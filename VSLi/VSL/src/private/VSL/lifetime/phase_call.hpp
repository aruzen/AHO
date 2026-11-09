#pragma once
#include <VSL/lifetime/phase_liveness.hpp>
namespace VSL_NAMESPACE::lifetime {
    class NativeCall {
        bool& _active;

    public:
        explicit NativeCall(bool& active);
        ~NativeCall();
        NativeCall(const NativeCall&) = delete;
        NativeCall& operator=(const NativeCall&) = delete;
    };
} // namespace VSL_NAMESPACE::lifetime
