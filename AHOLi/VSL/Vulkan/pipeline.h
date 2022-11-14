#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"
#include "shader.h"

namespace VSL_NAMESPACE {
	enum class StageID : unsigned char {
		Vertex = 1,
		Fragment = 2
	};

	template<bool V>
	struct Pipeline {
		template<VSL_NAMESPACE::StageID, bool _V = V>
		struct Stage {};
		
		Pipeline();

		template <VSL_NAMESPACE::StageID ID, bool _V = V>
		Pipeline<V>& operator <<(VSL_NAMESPACE::Pipeline<V>::Stage<ID, _V> stage);

		std::shared_ptr<VSL_NAMESPACE::_impl::Pipeline_impl> _data;
	};
}

template<bool V>
template<bool _V>
struct VSL_NAMESPACE::Pipeline<V>::Stage<VSL_NAMESPACE::StageID::Vertex, _V> {
	static constexpr VSL_NAMESPACE::StageID StageID = VSL_NAMESPACE::StageID::Vertex;

	Stage(std::string name, VSL_NAMESPACE::Shader<V> shader);

	std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
};

template<bool V>
template<bool _V>
struct VSL_NAMESPACE::Pipeline<V>::Stage<VSL_NAMESPACE::StageID::Fragment, _V> {
	static constexpr VSL_NAMESPACE::StageID StageID = VSL_NAMESPACE::StageID::Fragment;

	Stage(std::string name, VSL_NAMESPACE::Shader<V> shader);

	std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl> _data;
};