#pragma once
#include "../define.h"
#include "synchronize.h"
#include "device.h"
#include "swapchain.h"

namespace VSL_NAMESPACE {
	static std::uint32_t DEFAULT_BUFFER_SIZE = 2;

	struct CommandPool {
		CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device);
		CommandPool(std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl> data);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl> _data;
	};

	struct CommandBuffer {
		CommandBuffer(VSL_NAMESPACE::CommandPool pool, std::uint32_t size = DEFAULT_BUFFER_SIZE);
		CommandBuffer(std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl> data);

		std::uint32_t getCurrentBufferIdx();
		size_t getSize();
		void next();
		void reset();
		void reset(std::uint32_t idx);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl> _data;
	};

	struct CommandManager;
	namespace command {
		struct __Command {
			virtual void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) = 0;
		};

		template <typename T>
		concept is_command = requires(T t) {
			t.invoke(std::declval<CommandPool>(), std::declval<CommandBuffer>(), std::declval<CommandManager>());
		};
	}

	struct DefaultPhase {
		DefaultPhase(CommandManager manager, SwapchainAccessor swapchain,
			std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt,
			std::optional<SemaphoreHolder> calculationFinish = std::nullopt,
			std::optional<FenceHolder> inFlightFence = std::nullopt);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> manager;
		SwapchainAccessor swapchain;
		std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt, calculationFinish = std::nullopt;
		std::optional<FenceHolder> inFlightFence = std::nullopt;

		std::uint32_t imageIndex;

		DefaultPhase& operator<<(std::shared_ptr<command::__Command> cmd);
		template <command::is_command T>
		DefaultPhase& operator<<(T cmd);

		std::uint32_t getImageIndex();

		virtual ~DefaultPhase();
	};

	struct CommandManager {
		CommandManager(std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> data);
		CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> _data;

		template<class P = DefaultPhase, typename... Args>
		P startPhase(SwapchainAccessor swapchain, Args&&... args);

		CommandPool getPool();
		CommandBuffer getBuffer();
		std::uint32_t getCurrentBufferIdx();
		void next();
	};

	// ============================================================================

	template<class P, typename... Args>
	P VSL_NAMESPACE::CommandManager::startPhase(SwapchainAccessor swapchain, Args&&... args)
	{
		return P(*this, swapchain, std::forward<Args>(args)...);
	}

	template<command::is_command T>
	inline DefaultPhase& DefaultPhase::operator<<(T cmd)
	{
		CommandManager cm(manager);
		(&cmd)->invoke(cm.getPool(), cm.getBuffer(), cm);

		return *this;
	}
}