#pragma once
#include "../define.h"
#include "pv.h"

#include <memory>
#include <map>
#include <optional>

#include "device.h"

namespace VSL_NAMESPACE {
	struct SemaphoreHolder {
		std::shared_ptr<_impl::Semaphore_impl> _data;

		void destroy();
	};

	struct FenceHolder {
		std::shared_ptr<_impl::Fence_impl> _data;

		bool wait(std::uint64_t timeout_ns = UINT64_MAX);
		bool wait(std::uint32_t target_idx, std::uint64_t timeout_ns = UINT64_MAX);
		void reset();
		void reset(std::uint32_t target_idx);
		void destroy();
	};

	struct SynchroManager {
		std::shared_ptr<_impl::SynchroManager_impl> _data;

		SynchroManager(LogicalDeviceAccessor device);

		SemaphoreHolder createSemaphore(std::string name);
		SemaphoreHolder createSemaphore(std::string name, size_t amount);
		FenceHolder createFence(std::string name, bool signal = false);
		FenceHolder createFence(std::string name, size_t amount, bool signal = false);

		std::optional<SemaphoreHolder> getSemaphore(std::string name);
		std::optional<FenceHolder> getFence(std::string name);

		bool destroySemaphore(std::string name);
		bool destroyFence(std::string name);

		std::optional<FenceHolder> operator [](std::string name);
	};
}
