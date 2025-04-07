#pragma once
#include "../define.h"

#include "device.h"

namespace VSL_NAMESPACE {
	struct CommandPool {
		CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl> _data;
	};

	struct CommandBuffer {
		CommandBuffer(VSL_NAMESPACE::CommandPool pool);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl> _data;
	};

	namespace command {
		struct __Command {
			virtual void invoke(CommandPool pool, CommandBuffer buffer) = 0;
		};
	}

	struct DefaultPhase {
		DefaultPhase(CommandPool pool, CommandBuffer buffer);

		CommandPool pool;
		CommandBuffer buffer;

		DefaultPhase& operator<<(std::shared_ptr<command::__Command> cmd);

		virtual ~DefaultPhase();
	};

	struct CommandManager {
		CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> _data;
		CommandPool pool;
		CommandBuffer buffer;

        template<class P = DefaultPhase>
		P startPhase();
	};

    template<class P>
    P VSL_NAMESPACE::CommandManager::startPhase()
    {
        return P(pool, buffer);
    }

	// ============================================================================
}