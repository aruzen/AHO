//
// Created by morimoto_hibiki on 2025/10/17.
//

#ifndef AHO_ALL_PUSH_CONTENTS_H
#define AHO_ALL_PUSH_CONTENTS_H

#include "../../define.hpp"
#include "../pv.hpp"
#include "../pipeline_layout.hpp"
#include "../shader.hpp"

#include <utility>
#include <array>

namespace VSL_NAMESPACE::pipeline_layout {
    struct PushContentDefinition {
        VSL_NAMESPACE::ShaderFlag targetShader;
        std::uint32_t offset = (std::uint32_t)-1, size = (std::uint32_t)-1;
    };

    struct PushConstants {
        PushConstants() = default;

        constexpr PushConstants(VSL_NAMESPACE::ShaderFlag shader, std::initializer_list<PushContentDefinition> definitions) : definitions(definitions) {};

        void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);

        std::vector<PushContentDefinition> definitions;

        template<VSL_NAMESPACE::ShaderFlag ShaderFlag, typename... Types>
        constexpr static std::array<PushContentDefinition, sizeof...(Types)> MakeDefinitions(const Types &...);
    };

    template<vsl::ShaderFlag ShaderFlag, typename... Types>
    constexpr std::array<PushContentDefinition, sizeof...(Types)> PushConstants::MakeDefinitions(const Types &...) {
        std::uint32_t offset = 0;
        const auto next_definition = [&offset](std::uint32_t size) {
            const auto current = offset;
            offset += size;
            return PushContentDefinition{ShaderFlag, current, size};
        };
        return {{next_definition(sizeof(Types))...}};
    }
}

#endif //AHO_ALL_PUSH_CONTENTS_H
