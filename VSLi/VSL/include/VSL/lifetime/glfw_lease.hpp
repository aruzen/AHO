#pragma once
#include "../operation/instance_create_info.hpp"
#include "../operation/device_create_info.hpp"
#include "owner_ref.hpp"
#include <new>

namespace VSL_NAMESPACE::impl {
    struct GLFWRuntimeImpl;
}
namespace VSL_NAMESPACE::lifetime {
    // GLFWの文字列をlease内でコピーする。Instanceの生成／永久所有は行わない。
    InstanceCreateInfo glfw_instance_info(InstanceCreateInfo info, PresentFencePolicy present_fences = PresentFencePolicy::LEGACY);
    // Acquire／DrainとGLFW操作はmain threadで行う。最後のreleaseは退役だけを登録する。
    class GLFWLease {
        OwnedRef<impl::GLFWRuntimeImpl> _runtime;
        explicit GLFWLease(OwnedRef<impl::GLFWRuntimeImpl> runtime) : _runtime(std::move(runtime)) {}

    public:
        GLFWLease() = default;
        static GLFWLease Acquire();
        static void Drain();
        OwnerRef owner_ref() const { return _runtime; }
        explicit operator bool() const noexcept { return bool(_runtime); }
    };
    // GLFW呼出しを含む操作全体を囲う。外側scopeの終了時に退役を回収する。
    class GLFWCallScope {
        bool _drain_on_exit = true;

    public:
        GLFWCallScope();
        // noexceptの登録解除用。回収は外側scope／明示Drainへ任せる。
        explicit GLFWCallScope(std::nothrow_t) noexcept;
        ~GLFWCallScope();
        GLFWCallScope(const GLFWCallScope&) = delete;
        GLFWCallScope& operator=(const GLFWCallScope&) = delete;
    };
    // raw GLFWと併用するcallbackにも使う。callbackから復帰する前には回収しない。
    class GLFWCallbackScope {
    public:
        GLFWCallbackScope() noexcept;
        ~GLFWCallbackScope();
        GLFWCallbackScope(const GLFWCallbackScope&) = delete;
        GLFWCallbackScope& operator=(const GLFWCallbackScope&) = delete;
    };
} // namespace VSL_NAMESPACE::lifetime
