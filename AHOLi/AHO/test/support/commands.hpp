#pragma once
#include <VSL/vulkan/command.hpp>

namespace aho_test {
struct RecordingCommand : vsl::command::__Command {
    int* calls;
    explicit RecordingCommand(int* calls) : calls(calls) {}
    void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override { ++*calls; }
};
struct RecordingDelegate : vsl::command::__DelegateCommand {
    int* calls;
    explicit RecordingDelegate(int* calls) : calls(calls) {}
    void invoke(vsl::DefaultPhaseStream&, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override { ++*calls; }
};
struct RecordingStream {
    int calls = 0;
    template<class T> RecordingStream& operator<<(T&&) { ++calls; return *this; }
};
}
