#pragma once
#include "../define.h"
#include "../consteval_string.h"
#include "VSL/utils/string.h"
#include "pv.h"

#include <filesystem>
#include <functional>

#include "device.h"

namespace vsl {
	enum class ShaderType : int {
        Error,
        Vertex = 0x00000001,
        TessellationControl = 0x00000002,
        TessellationEvaluation = 0x00000004,
        Geometry = 0x00000008,
        Fragment = 0x00000010,
        Compute = 0x00000020,
        AllGraphics = 0x0000001F,
        Raygen = 0x00000100,
        AnyHit = 0x00000200,
        ClosestHit = 0x00000400,
        Miss = 0x00000800,
        Intersection = 0x00001000,
        Callable = 0x00002000,
        Task = 0x00000040,
        Mesh = 0x00000080,
        SubpassShadingHuawei = 0x00004000,
        ClusterCullingHuawei = 0x00080000,
        All = 0x7FFFFFFF,
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
		} else if (std::string(Txt.value).contains(".comp.")) {
            return ShaderType::Compute;
        }
		return ShaderType::Error;
	}

	template<helper::constant_string_holder Path>
	auto make_shader(vsl::LogicalDeviceAccessor device) {
		return Shader<___Get_ShaderType<Path>()>(device, expand_environments(Path.value));
	}

	template<helper::constant_string_holder Path>
	auto make_shader(vsl::LogicalDeviceAccessor device, std::string name) {
		return Shader<___Get_ShaderType<Path>()>(device, expand_environments(Path.value), name);
	}
}