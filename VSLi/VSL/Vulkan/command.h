#pragma once
#include "../define.h"
#include "synchronize.h"
#include "device.h"
#include "swapchain.h"

namespace VSL_NAMESPACE {
	struct CommandManager;
	struct DefaultPhase;
	struct DefaultPhaseStreamOperator;

	static std::uint32_t DEFAULT_BUFFER_SIZE = 2;

	namespace defaults {
		extern std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> COMMAND_MANAGER;
	}

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

	namespace command {
		struct __Command {
			virtual void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) = 0;
		};

		template <typename T>
		concept is_command = requires(T t) {
			t.invoke(std::declval<CommandPool>(), std::declval<CommandBuffer>(), std::declval<CommandManager>());
		};

		struct __VertexHolder {
			virtual size_t get_vertex_size() = 0;
		};

		template<typename T>
		concept is_vertex_holder = requires(const T & t) {
			{ t.get_vertex_size() } -> std::convertible_to<size_t>;
		};

		struct __Manipulator {
			virtual void manipulate(DefaultPhaseStreamOperator* op, CommandPool pool, CommandBuffer buffer, CommandManager manager) = 0;
		};

		template <typename T>
		concept is_manipulator = requires(T t) {
			t.manipulate(std::declval<DefaultPhaseStreamOperator*>(), std::declval<CommandPool>(), std::declval<CommandBuffer>(), std::declval<CommandManager>());
		};
	}

	struct CommandManager {
		CommandManager(std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> data);
		CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> _data;

		template<class P = DefaultPhase, typename... Args>
		P startPhase(SwapchainAccessor swapchain, Args&&... args);

		CommandPool getPool();
		CommandBuffer getBuffer();
		std::uint32_t getCurrentBufferIdx();
		CommandBuffer makeExclusiveBuffer(size_t size = 1);
		void next();

		/*
		* 元のCommandBuffer{ defaults::COMMAND_MANAGER }を返す。
		*/
		CommandManager setDefault();
	};

	struct DefaultPhase {
		DefaultPhase(CommandManager manager, SwapchainAccessor swapchain,
			std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt,
			std::optional<SemaphoreHolder> calculationFinish = std::nullopt,
			std::optional<FenceHolder> inFlightFence = std::nullopt);

		CommandManager manager;
		SwapchainAccessor swapchain;
		std::optional<SemaphoreHolder> nextImageAvailable = std::nullopt, calculationFinish = std::nullopt;
		std::optional<FenceHolder> inFlightFence = std::nullopt;

		std::uint32_t imageIndex;

		DefaultPhaseStreamOperator operator<<(std::shared_ptr<command::__Command> cmd);
		DefaultPhaseStreamOperator operator<<(std::shared_ptr<command::__Manipulator> cmd);
		template <typename T>
			requires (command::is_command<T>) || (command::is_manipulator<T>)
		DefaultPhaseStreamOperator operator<<(T cmd);

		std::uint32_t getImageIndex();

		virtual ~DefaultPhase();
	};

	struct DefaultPhaseStreamOperator {
		DefaultPhase* parent;

		size_t vertexSize;

		DefaultPhaseStreamOperator& operator<<(std::shared_ptr<command::__Command> cmd);
		template <command::is_command T>
		DefaultPhaseStreamOperator& operator<<(T cmd);
		DefaultPhaseStreamOperator& operator<<(std::shared_ptr<command::__Manipulator> manip);
		template <command::is_manipulator T>
		DefaultPhaseStreamOperator& operator<<(T cmd);
	};

	// ============================================================================

	template<class P, typename... Args>
	P VSL_NAMESPACE::CommandManager::startPhase(SwapchainAccessor swapchain, Args&&... args)
	{
		return P(*this, swapchain, std::forward<Args>(args)...);
	}

	template <typename T>
		requires (command::is_command<T>) || (command::is_manipulator<T>)
	DefaultPhaseStreamOperator DefaultPhase::operator<<(T cmd)
	{
		DefaultPhaseStreamOperator op{ this };
		op << cmd;
		return op;
	}

	template<command::is_command T>
	DefaultPhaseStreamOperator& DefaultPhaseStreamOperator::operator<<(T cmd)
	{
		(&cmd)->invoke(parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
		if constexpr (command::is_vertex_holder<T>)
			this->vertexSize = (&cmd)->get_vertex_size();
		return *this;
	}

	template<command::is_manipulator T>
	DefaultPhaseStreamOperator& DefaultPhaseStreamOperator::operator<<(T manip)
	{
		(&manip)->manipulate(this, parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
		if constexpr (command::is_vertex_holder<T>)
			this->vertexSize = (&manip)->get_vertex_size();
		return *this;
	}
}