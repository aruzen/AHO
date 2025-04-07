#pragma once
#include "../define.h"
#include "../consteval_string.h"
#include "pv.h"

#include <filesystem>
#include <functional>

#include "device.h"

namespace vsl {
	enum class ShaderType : unsigned char {
		Error,
		Vertex,
		Fragment
	};

	struct ShaderAccessor {
		std::shared_ptr<vsl::_impl::Shader_impl> _data;
	};

	template<VSL_NAMESPACE::ShaderType _Type = ShaderType::Vertex>
	struct Shader : public ShaderAccessor {
		constexpr static ShaderType Type = _Type;

		Shader(vsl::LogicalDeviceAccessor device, std::filesystem::path path);
		Shader(vsl::LogicalDeviceAccessor device, std::filesystem::path path, std::string name);
		
		template<helper::constant_string_holder Path>
		Shader(vsl::LogicalDeviceAccessor device);

		template<helper::constant_string_holder Path>
		Shader(vsl::LogicalDeviceAccessor device, std::string name);
	};

	template<helper::constant_string_holder Txt>
	consteval ShaderType ___Get_ShaderType() {
		if (std::string(Txt.value).contains(".vert.")) {
			return ShaderType::Vertex;
		} else if (std::string(Txt.value).contains(".frag.")) {
			return ShaderType::Fragment;
		}
		return ShaderType::Error;
	}

	template<helper::constant_string_holder Path>
	auto make_shader(vsl::LogicalDeviceAccessor device) {
		return Shader<___Get_ShaderType<Path>()>(device, Path.value);
	}

	template<helper::constant_string_holder Path>
	auto make_shader(vsl::LogicalDeviceAccessor device, std::string name) {
		return Shader<___Get_ShaderType<Path>()>(device, Path.value, name);
	}
}