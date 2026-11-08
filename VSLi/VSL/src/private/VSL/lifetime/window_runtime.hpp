#pragma once
#include <VSL/lifetime/glfw_lease.hpp>
#include <VSL/lifetime/window_owner.hpp>
#include <VSL/lifetime/dependency_node.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct GLFWRetirement {
        GLFWRetirement* next = nullptr;
        void (*dispose)(GLFWRetirement*) noexcept = nullptr;
    };
    void retire_glfw(GLFWRetirement& retirement) noexcept;
    bool glfw_runtime_active();
    bool in_glfw_callback() noexcept;
    size_t pending_glfw_retirements();
    namespace detail {
        struct WindowOwnerAccess {
            static const OwnedRef<impl::OSWindowImpl>& Ref(const OSWindowOwner& owner) { return owner._window; }
            static OSWindowOwner From(OwnedRef<impl::OSWindowImpl> owner) { return OSWindowOwner(std::move(owner)); }
        };
    } // namespace detail
} // namespace VSL_NAMESPACE::lifetime

namespace VSL_NAMESPACE::impl {
    struct GLFWRuntimeImpl : lifetime::GLFWRetirement {
        lifetime::DependencyNode node;
        bool initialized = false;
        static void Dispose(lifetime::GLFWRetirement* retirement) noexcept;
        GLFWRuntimeImpl() { dispose = Dispose; }
    };
    struct OSWindowImpl : lifetime::GLFWRetirement {
        lifetime::DependencyNode node;
        lifetime::GLFWLease glfw;
        void* window = nullptr;
        explicit OSWindowImpl(lifetime::GLFWLease glfw) : glfw(std::move(glfw)) { dispose = Dispose; }
        static void Dispose(lifetime::GLFWRetirement* retirement) noexcept;
        static void VisitDependencies(const void* owner, lifetime::DependencyNode::EdgeVisitor visitor, void* context);
    };
} // namespace VSL_NAMESPACE::impl
