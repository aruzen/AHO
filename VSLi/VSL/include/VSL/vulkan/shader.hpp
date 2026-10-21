#pragma once
#include "../define.hpp"
#include "../consteval_string.hpp"
#include "../utils/string.hpp"
#include "pv.hpp"

#include <filesystem>
#include <functional>

#include "device.hpp"
#include "../utils/flags.hpp"

namespace VSL_NAMESPACE {
    enum class ShaderType : int {
        ERROR_,
        VERTEX = 0x00000001,
        TESSELLATION_CONTROL = 0x00000002,
        TESSELLATION_EVALUATION = 0x00000004,
        GEOMETRY = 0x00000008,
        FRAGMENT = 0x00000010,
        COMPUTE = 0x00000020,
        ALL_GRAPHICS = 0x0000001F,
        RAYGEN = 0x00000100,
        ANY_HIT = 0x00000200,
        CLOSEST_HIT = 0x00000400,
        MISS = 0x00000800,
        INTERSECTION = 0x00001000,
        CALLABLE = 0x00002000,
        TASK = 0x00000040,
        MESH = 0x00000080,
        //        All = 0x7FFFFFFF,
    };

    enum class ShaderFlag : int {
        ERROR_,
        VERTEX = 0x00000001,
        TESSELLATION_CONTROL = 0x00000002,
        TESSELLATION_EVALUATION = 0x00000004,
        GEOMETRY = 0x00000008,
        FRAGMENT = 0x00000010,
        COMPUTE = 0x00000020,
        ALL_GRAPHICS = 0x0000001F,
        RAYGEN = 0x00000100,
        ANY_HIT = 0x00000200,
        CLOSEST_HIT = 0x00000400,
        MISS = 0x00000800,
        INTERSECTION = 0x00001000,
        CALLABLE = 0x00002000,
        TASK = 0x00000040,
        MESH = 0x00000080,
        //        All = 0x7FFFFFFF,
    };

    ___VSL_EXPAND_FLAGS_OPERATOR(ShaderFlag);

    constexpr ShaderFlag operator|(ShaderType l, ShaderType r) {
        return static_cast<ShaderFlag>(static_cast<std::underlying_type_t<ShaderType>>(l) | static_cast<std::underlying_type_t<ShaderType>>(r));
    }

    constexpr ShaderFlag operator&(ShaderType l, ShaderType r) {
        return static_cast<ShaderFlag>(static_cast<std::underlying_type_t<ShaderType>>(l) & static_cast<std::underlying_type_t<ShaderType>>(r));
    }

    constexpr ShaderFlag operator^(ShaderType l, ShaderType r) {
        return static_cast<ShaderFlag>(static_cast<std::underlying_type_t<ShaderType>>(l) ^ static_cast<std::underlying_type_t<ShaderType>>(r));
    }

    constexpr ShaderFlag convert(const ShaderType& t) { return (ShaderFlag)t; };

    struct ShaderAccessor {
        std::shared_ptr<VSL_NAMESPACE::impl::ShaderImpl> data_;
    };

    template<VSL_NAMESPACE::ShaderType type = ShaderType::VERTEX>
    struct Shader : public ShaderAccessor {
        constexpr static ShaderType TYPE = type;

        Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::filesystem::path path);
        Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::filesystem::path path, std::string name);

        std::string name() const;
    };

    template<helper::ConstantStringHolder txt>
    consteval ShaderType get_shader_type() {
        if (std::string(txt.value).contains(".vert.")) {
            return ShaderType::VERTEX;
        } else if (std::string(txt.value).contains(".frag.")) {
            return ShaderType::FRAGMENT;
        } else if (std::string(txt.value).contains(".comp.")) {
            return ShaderType::COMPUTE;
        }
        return ShaderType::ERROR_;
    }

    template<helper::ConstantStringHolder path>
    auto make_shader(VSL_NAMESPACE::LogicalDeviceAccessor device) {
        return Shader<get_shader_type<path>()>(device, expand_environments(path.value));
    }

    template<helper::ConstantStringHolder path>
    auto make_shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::string name) {
        return Shader<get_shader_type<path>()>(device, expand_environments(path.value), name);
    }
} // namespace VSL_NAMESPACE
