#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"
#include "shader.h"

namespace VSL_NAMESPACE {
	enum class PipelineStageType : unsigned char {
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

	enum class ShaderPipelineStageType : unsigned char {
		Error,
		Vertex,
		Fragment
	};

	struct _ShaderPipelineStageData {
		ShaderPipelineStageType type;
		std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
	};

	template<typename... Args>
	struct Pipeline {
		Pipeline(const Args& ...args);

		std::shared_ptr<VSL_NAMESPACE::_impl::Pipeline_impl> _data;

		template <typename Addition>
		Pipeline<Addition, Args...> add(const Addition& a);
	};

	template<ShaderPipelineStageType Type>
	struct ShaderPipelineStage {};

	struct ShaderPipelineStages {

	};

	namespace pipeline_stage {
		template<ShaderPipelineStageType Type, typename... Args>
		Pipeline<Args...>& operator <<(Pipeline<Args...>& out, const ShaderPipelineStage<Type>& y);
	}
}

template<>
struct VSL_NAMESPACE::ShaderPipelineStage<VSL_NAMESPACE::ShaderPipelineStageType::Vertex> : public _ShaderPipelineStageData {
	ShaderPipelineStage(std::string name, VSL_NAMESPACE::Shader shader);

	struct ShaderPipelineStageResult {

	};

	ShaderPipelineStageResult operator ()();
	ShaderPipelineStages operator+();
};

template<>
struct VSL_NAMESPACE::ShaderPipelineStage<VSL_NAMESPACE::ShaderPipelineStageType::Fragment> : public _ShaderPipelineStageData {
	ShaderPipelineStage(std::string name, VSL_NAMESPACE::Shader shader);

	std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
};