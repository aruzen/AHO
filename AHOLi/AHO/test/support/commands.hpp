#pragma once
#include <VSL/vulkan/command.hpp>

namespace aho_test {
    struct RecordingCommand : vsl::command::Command {
        int* calls;
        explicit RecordingCommand(int* calls);
        void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override;
    };
    struct RecordingDelegate : vsl::command::DelegateCommand {
        int* calls;
        explicit RecordingDelegate(int* calls);
        void invoke(vsl::DefaultPhaseStream&, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override;
    };
    struct RecordingStream {
        int calls = 0;
        template<class T>
        RecordingStream& operator<<(T&&);
    };

    inline RecordingCommand::RecordingCommand(int* calls) : calls(calls) {}

    inline void RecordingCommand::invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) {
        ++*calls;
    }

    inline RecordingDelegate::RecordingDelegate(int* calls) : calls(calls) {}

    inline void RecordingDelegate::invoke(vsl::DefaultPhaseStream&, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) {
        ++*calls;
    }

    template<class T>
    RecordingStream& RecordingStream::operator<<(T&&) {
        ++calls;
        return *this;
    }
} // namespace aho_test
