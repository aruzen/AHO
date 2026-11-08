#include <VSL/lifetime/window_runtime.hpp>
#include <VSL/operation/window_ops.hpp>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <new>
#ifdef __APPLE__
#include <pthread.h>
#endif

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct GLFWState {
            std::mutex mutex;
            std::thread::id main_thread;
            std::weak_ptr<impl::GLFWRuntimeImpl> active;
            impl::GLFWRuntimeImpl* native_runtime = nullptr;
            GLFWRetirement* first = nullptr;
            GLFWRetirement* last = nullptr;
        };
        GLFWState& glfw_state();
        thread_local size_t CALL_DEPTH = 0, CALLBACK_DEPTH = 0;
        thread_local bool DRAINING = false;
        void require_main_thread();
        void drain_retirements() noexcept;
        GLFWState& glfw_state() {
            // metadataだけをprocess寿命に置き、native終了は明示Drainで行う。
            alignas(GLFWState) static std::byte STORAGE[sizeof(GLFWState)];
            static auto* const STATE = std::construct_at(reinterpret_cast<GLFWState*>(STORAGE));
            return *STATE;
        }
        void require_main_thread() {
#ifdef __APPLE__
            if (!pthread_main_np())
                throw std::logic_error("GLFW operation requires the main thread");
#endif
            std::lock_guard lock(glfw_state().mutex);
            if (glfw_state().main_thread == std::thread::id{})
                glfw_state().main_thread = std::this_thread::get_id();
            if (glfw_state().main_thread != std::this_thread::get_id())
                throw std::logic_error("GLFW operation requires its original main thread");
        }
        void drain_retirements() noexcept {
            if (CALL_DEPTH || CALLBACK_DEPTH || DRAINING)
                return;
            DRAINING = true;
            for (;;) {
                GLFWRetirement* retired;
                {
                    std::lock_guard lock(glfw_state().mutex);
                    retired = glfw_state().first;
                    if (!retired)
                        break;
                    glfw_state().first = retired->next;
                    if (!glfw_state().first)
                        glfw_state().last = nullptr;
                }
                retired->dispose(retired);
            }
            DRAINING = false;
        }
    } // namespace
    void retire_glfw(GLFWRetirement& retirement) noexcept {
        std::lock_guard lock(glfw_state().mutex);
        retirement.next = nullptr;
        if (glfw_state().last)
            glfw_state().last->next = &retirement;
        else
            glfw_state().first = &retirement;
        glfw_state().last = &retirement;
    }
    bool glfw_runtime_active() {
        std::lock_guard lock(glfw_state().mutex);
        return glfw_state().native_runtime != nullptr;
    }
    bool in_glfw_callback() noexcept {
        return CALLBACK_DEPTH != 0;
    }
    size_t pending_glfw_retirements() {
        std::lock_guard lock(glfw_state().mutex);
        size_t count = 0;
        for (auto* p = glfw_state().first; p; p = p->next)
            ++count;
        return count;
    }
    GLFWLease GLFWLease::Acquire() {
        GLFWCallScope scope;
        if (DRAINING)
            throw std::logic_error("GLFW runtime is draining");
        {
            std::lock_guard lock(glfw_state().mutex);
            if (auto runtime = glfw_state().active.lock())
                return GLFWLease(runtime->node.ref(std::move(runtime)));
            if (glfw_state().native_runtime)
                throw std::logic_error("GLFW runtime is retiring; drain after its final release");
        }
        if (CALLBACK_DEPTH)
            throw std::logic_error("Cannot initialize GLFW from a callback");
        auto runtime = std::shared_ptr<impl::GLFWRuntimeImpl>(new impl::GLFWRuntimeImpl, [](auto* value) noexcept {
            value->node.retire();
            retire_glfw(*value);
        });
        {
            std::lock_guard lock(glfw_state().mutex);
            glfw_state().native_runtime = runtime.get();
        }
        operation::initialize_glfw();
        runtime->initialized = true;
        runtime->node.publish(runtime);
        {
            std::lock_guard lock(glfw_state().mutex);
            glfw_state().active = runtime;
        }
        return GLFWLease(runtime->node.ref(std::move(runtime)));
    }
    void GLFWLease::Drain() {
        require_main_thread();
        if (CALL_DEPTH || CALLBACK_DEPTH || DRAINING)
            throw std::logic_error("GLFW drain requires a boundary outside native calls and callbacks");
        drain_retirements();
    }
    GLFWCallScope::GLFWCallScope() {
        require_main_thread();
        if (DRAINING)
            throw std::logic_error("Cannot enter GLFW operations during retirement");
        if (!CALL_DEPTH)
            drain_retirements();
        ++CALL_DEPTH;
    }
    GLFWCallScope::~GLFWCallScope() {
        if (!--CALL_DEPTH && _drain_on_exit)
            drain_retirements();
    }
    GLFWCallScope::GLFWCallScope(std::nothrow_t) noexcept : _drain_on_exit(false) {
        ++CALL_DEPTH;
    }
    GLFWCallbackScope::GLFWCallbackScope() noexcept {
        ++CALLBACK_DEPTH;
    }
    GLFWCallbackScope::~GLFWCallbackScope() {
        --CALLBACK_DEPTH;
    }
    OSWindowOwner OSWindowOwner::Create(std::string_view name, int width, int height) {
        GLFWCallScope scope;
        auto owner = std::shared_ptr<impl::OSWindowImpl>(new impl::OSWindowImpl(GLFWLease::Acquire()), [](auto* value) noexcept {
            value->node.retire();
            retire_glfw(*value);
        });
        owner->window = operation::create_os_window(name, width, height);
        owner->node.publish(owner, owner.get(), impl::OSWindowImpl::VisitDependencies);
        return OSWindowOwner(owner->node.ref(std::move(owner)));
    }
    void* OSWindowOwner::native_handle() const& noexcept {
        return _window ? _window->window : nullptr;
    }
} // namespace VSL_NAMESPACE::lifetime

void VSL_NAMESPACE::impl::GLFWRuntimeImpl::Dispose(lifetime::GLFWRetirement* retirement) noexcept {
    auto* runtime = static_cast<GLFWRuntimeImpl*>(retirement);
    bool current;
    {
        std::lock_guard lock(lifetime::glfw_state().mutex);
        current = lifetime::glfw_state().native_runtime == runtime;
    }
    if (current && runtime->initialized)
        operation::terminate_glfw();
    {
        std::lock_guard lock(lifetime::glfw_state().mutex);
        if (lifetime::glfw_state().native_runtime == runtime) {
            lifetime::glfw_state().native_runtime = nullptr;
            lifetime::glfw_state().active.reset();
        }
    }
    delete runtime;
}
void VSL_NAMESPACE::impl::OSWindowImpl::Dispose(lifetime::GLFWRetirement* retirement) noexcept {
    auto* owner = static_cast<OSWindowImpl*>(retirement);
    operation::destroy_os_window(owner->window);
    delete owner;
}
void VSL_NAMESPACE::impl::OSWindowImpl::VisitDependencies(const void* owner, lifetime::DependencyNode::EdgeVisitor visitor, void* context) {
    const auto lease = static_cast<const OSWindowImpl*>(owner)->glfw.owner_ref();
    visitor(context, lease);
}
