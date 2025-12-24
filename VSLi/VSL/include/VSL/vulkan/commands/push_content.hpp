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
    struct PushConstant : public __Command {
        vsl::PipelineLayoutAccessor layout;
        vsl::ShaderFlag targetType;
        std::uint32_t size;
        std::uint32_t offset;
        void *value;

        PushConstant(PipelineLayoutAccessor layout, vsl::ShaderFlag targetType, std::uint32_t size,
                     std::uint32_t offset, void *value)
                : layout(std::move(layout)), targetType(targetType), size(size), offset(offset), value(value) {};

        template<typename T>
        PushConstant(PipelineLayoutAccessor layout, vsl::ShaderFlag targetType, T &t, std::uint32_t offset = 0);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    template<typename T>
    PushConstant::PushConstant(PipelineLayoutAccessor layout, vsl::ShaderFlag targetType, T &t, std::uint32_t offset)
            : PushConstant(layout, targetType, sizeof(T), offset, &t) {}
}


#endif //AHO_ALL_COMMAND_PUSH_CONTENTS_H
