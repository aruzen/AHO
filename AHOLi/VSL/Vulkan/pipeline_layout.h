#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"
#include "shader.h"

namespace VSL_NAMESPACE {
	enum class PipelineLayoutStageType : unsigned char {
		Error,
		Stages,
		VertexInput,
		InputAssemblyState,
		Viewport,
		Rasterization,
		Multisample,
		DepthStencil,
		ColorBlend
	};

	enum class ShaderPipelineLayoutStageType : unsigned char {
		Error,
		Vertex,
		Fragment
	};

	struct _ShaderPipelineLayoutStageData {
		ShaderPipelineLayoutStageType type;
		std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
	};

	template<typename T>
	concept pipeline_layout_createinfo_injecter = requires (T t) {
		t.injection(std::declval<VSL_NAMESPACE::_impl::CreateInfo>());
	};

	template<typename T>
	concept pipeline_layout_data_injecter = requires (T t) {
		t.injection(std::declval<std::shared_ptr<VSL_NAMESPACE::_impl::PipelineLayout_impl>>());
	};

	template<typename T>
	concept pipeline_layout_injecter = pipeline_layout_createinfo_injecter<T> 
								|| pipeline_layout_data_injecter<T>;

	template<pipeline_layout_injecter... Args>
	struct PipelineLayout {
		PipelineLayout(VSL_NAMESPACE::LogicalDeviceAcsessor device, const Args& ...args);

		std::shared_ptr<VSL_NAMESPACE::_impl::PipelineLayout_impl> _data;

		template <typename Addition>
		PipelineLayout<Addition, Args...> add(const Addition& a);
	};

	template<ShaderPipelineLayoutStageType Type>
	struct ShaderPipelineLayoutStage {};

	struct ShaderPipelineLayoutStages {

	};

	namespace pipeline_stage {
		template<ShaderPipelineLayoutStageType Type, typename... Args>
		PipelineLayout<Args...>& operator <<(PipelineLayout<Args...>& out, const ShaderPipelineLayoutStage<Type>& y);
	}
}

template<>
struct VSL_NAMESPACE::ShaderPipelineLayoutStage<VSL_NAMESPACE::ShaderPipelineLayoutStageType::Vertex> : public _ShaderPipelineLayoutStageData {
	ShaderPipelineLayoutStage(std::string name, VSL_NAMESPACE::Shader shader);

	struct ShaderPipelineLayoutStageResult {

	};

	ShaderPipelineLayoutStageResult operator ()();
	ShaderPipelineLayoutStages operator+();
};

template<>
struct VSL_NAMESPACE::ShaderPipelineLayoutStage<VSL_NAMESPACE::ShaderPipelineLayoutStageType::Fragment> : public _ShaderPipelineLayoutStageData {
	ShaderPipelineLayoutStage(std::string name, VSL_NAMESPACE::Shader shader);

	std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
};