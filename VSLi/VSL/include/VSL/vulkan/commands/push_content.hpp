//
// Created by morimoto_hibiki on 2025/10/17.
//

#ifndef AHO_ALL_COMMAND_PUSH_CONTENTS_H
#define AHO_ALL_COMMAND_PUSH_CONTENTS_H

#include <utility>

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../pipeline.hpp"
#include "../shader.hpp"

namespace VSL_NAMESPACE::command {
    /*
     * PushConstantsはset=1のように指定できず、offsetで管理するしかないので今後改良の余地あり
     */
    struct PushConstant : public Command {
        VSL_NAMESPACE::PipelineLayoutAccessor layout;
        VSL_NAMESPACE::ShaderFlag target_type;
        std::uint32_t size;
        std::uint32_t offset;
        const void* value;

        PushConstant(PipelineLayoutAccessor layout, VSL_NAMESPACE::ShaderFlag target_type, std::uint32_t size,
                     std::uint32_t offset, const void* value)
            : layout(std::move(layout)), target_type(target_type), size(size), offset(offset), value(value) {};

        template<typename T>
        PushConstant(PipelineLayoutAccessor layout, VSL_NAMESPACE::ShaderFlag target_type, T& t, std::uint32_t offset = 0);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    template<typename T>
    PushConstant::PushConstant(PipelineLayoutAccessor layout, VSL_NAMESPACE::ShaderFlag target_type, T& t, std::uint32_t offset)
        : PushConstant(layout, target_type, sizeof(T), offset, &t) {}
} // namespace VSL_NAMESPACE::command

#endif //AHO_ALL_COMMAND_PUSH_CONTENTS_H
