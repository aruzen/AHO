#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"
#include "shader.h"

namespace VSL_NAMESPACE {
	// struct Pipeline;

	/* enum class PipelineLayoutStageType : unsigned char {
		Error,
		Stages,
		VertexInput,
		InputAssemblyState,
		Viewport,
		Rasterization,
		Multisample,
		DepthStencil,
		ColorBlend
	};*/

	/*
	struct _ShaderPipelineLayoutStageData {
		// ShaderType type;
		std::shared_ptr<_impl::ShaderStage_impl> _data;
	};
	*/

	template<typename T>
	concept pipeline_layout_createinfo_injecter = requires (T t) {
		t.injection(std::declval<_impl::CreateInfo>());
	};

	template<typename T>
	concept pipeline_layout_data_injecter = requires (T t) {
		t.injection(std::declval<std::shared_ptr<_impl::PipelineLayout_impl>>());
	};

	template<typename T>
	concept pipeline_layout_injecter = pipeline_layout_createinfo_injecter<T>
		|| pipeline_layout_data_injecter<T>;

	struct PipelineLayoutAccesor {
		std::shared_ptr<_impl::PipelineLayout_impl> _data;
	};

	template<pipeline_layout_injecter... Args>
	struct PipelineLayout : public PipelineLayoutAccesor {
		PipelineLayout(LogicalDeviceAccessor device, const Args& ...args);

		template <typename Addition>
		PipelineLayout<Addition, Args...> add(const Addition& a);
	};

	/*
	template<ShaderType Type>
	struct ShaderPipelineLayoutStage {
		ShaderPipelineLayoutStage(std::string name, Shader<Type> shader);

		std::shared_ptr<_impl::ShaderStage_impl> _data;
	};
	*/
}