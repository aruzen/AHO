#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"
#include "shader.h"

namespace VSL_NAMESPACE {
	struct PipelineLayoutAccessor {
		std::shared_ptr<_impl::PipelineLayout_impl> _data;
	protected:
		void init_start(LogicalDeviceAccessor device);
		void init_finish();
	};

	template<typename T>
	concept pipeline_layout_injecter = requires (T&& t) {
		std::forward<T>(t).injection(std::declval<VSL_NAMESPACE::PipelineLayoutAccessor>());
	};

	struct PipelineLayout : public PipelineLayoutAccessor {
		template<pipeline_layout_injecter... Args>
		PipelineLayout(LogicalDeviceAccessor device, Args&& ...args);

		PipelineLayout(std::shared_ptr<_impl::PipelineLayout_impl> _data);

		template <VSL_NAMESPACE::pipeline_layout_injecter Addition>
		PipelineLayout add(Addition&& a);

		PipelineLayout copy();
	};


	// ----------------------------------------------------------------------------------------------------------------------

	namespace helper {
		extern void expansionPipelineLayoutArgs(PipelineLayoutAccessor& pl);

		template<VSL_NAMESPACE::pipeline_layout_injecter T, typename... Args>
		void expansionPipelineLayoutArgs(PipelineLayoutAccessor& pl, T&& t, Args&&... args) {
			std::forward<T>(t).injection(pl);
			expansionPipelineLayoutArgs(pl, std::forward<Args>(args)...);
		}
	}

	template<pipeline_layout_injecter... Args>
	PipelineLayout::PipelineLayout(VSL_NAMESPACE::LogicalDeviceAccessor device, Args&& ...args) {
		init_start(device);

		helper::expansionPipelineLayoutArgs(*this, std::forward<Args>(args)...);

		init_finish();
	}/**/

	template <VSL_NAMESPACE::pipeline_layout_injecter Addition>
	PipelineLayout PipelineLayout::add(Addition&& a) {
		std::forward<Addition>(a).injection(*this);

		init_finish();

		return *this;
	}
}