#pragma once
#include <VSL/define.hpp>

namespace VSL_NAMESPACE::lifetime {
    // 同期callbackをまたぐ呼出しだけをstack上で追跡する。対象を所有しない。
    class FacadeCall {
        static thread_local FacadeCall* CURRENT;
        const void* _subject;
        FacadeCall* _previous;
        bool _alive = true;

    public:
        explicit FacadeCall(const void* subject) noexcept;
        ~FacadeCall();
        FacadeCall(const FacadeCall&) = delete;
        FacadeCall& operator=(const FacadeCall&) = delete;
        bool alive() const noexcept { return _alive; }
        static void Invalidate(const void* subject) noexcept;
    };
} // namespace VSL_NAMESPACE::lifetime
