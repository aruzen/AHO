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

		Pipeline<V>& operator <<(_PipelineStageData stage);

		std::shared_ptr<VSL_NAMESPACE::_impl::Pipeline_impl> _data;
	};

	template<PipelineStageType Type, bool V>
	struct PipelineStage : public Pipeline<V>::Stage {};
}

template<bool V>
struct VSL_NAMESPACE::PipelineStage<VSL_NAMESPACE::PipelineStageType::Vertex, V> : public _PipelineStageData {
	PipelineStage(std::string name, VSL_NAMESPACE::Shader shader);

	std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
};

template<bool V>
struct VSL_NAMESPACE::PipelineStage<VSL_NAMESPACE::PipelineStageType::Fragment, V> : public _PipelineStageData {
	PipelineStage(std::string name, VSL_NAMESPACE::Shader shader);

	std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
};