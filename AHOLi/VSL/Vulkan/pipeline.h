#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"
#include "shader.h"

namespace VSL_NAMESPACE {
	enum class PipelineStageType : unsigned char {
		Vertex = 1,
		Fragment = 2
	};

	struct _PipelineStageData {
		PipelineStageType type;
		void* _info;
	};

	template<bool V = VSL_NAMESPACE::validation>
	struct Pipeline {
		Pipeline();

		std::shared_ptr<VSL_NAMESPACE::_impl::Pipeline_impl> _data;
	};

	template<PipelineStageType Type>
	struct PipelineStage {};

	namespace pipeline_stage {
		template<bool V, PipelineStageType Type>
		Pipeline<V>& operator <<(Pipeline<V>& out, const PipelineStage<Type>& y);
	}
}

template<>
struct VSL_NAMESPACE::PipelineStage<VSL_NAMESPACE::PipelineStageType::Vertex> : public _PipelineStageData {
	PipelineStage(std::string name, VSL_NAMESPACE::Shader shader);

	std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
};

template<>
struct VSL_NAMESPACE::PipelineStage<VSL_NAMESPACE::PipelineStageType::Fragment> : public _PipelineStageData {
	PipelineStage(std::string name, VSL_NAMESPACE::Shader shader);

	std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
};