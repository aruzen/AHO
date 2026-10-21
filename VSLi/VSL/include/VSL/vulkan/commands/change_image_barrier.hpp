#ifndef AHO_ALL_CHANGE_IMAGE_BARRIER_HPP
#define AHO_ALL_CHANGE_IMAGE_BARRIER_HPP

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../descriptor.hpp"
#include "../pipeline.hpp"

#include "../image.hpp"

namespace VSL_NAMESPACE::command {
    struct ChangeImageBarrier : public Command {
        ChangeImageBarrier(ImageAccessor image, ImageLayout old_layout, ImageLayout new_layout);
        // layerCountが0ならbaseLayer以降の全レイヤーを対象にする。ミップレベルは0のみ。
        ChangeImageBarrier(ImageAccessor image, ImageLayout old_layout, ImageLayout new_layout,
                           std::uint32_t base_layer, std::uint32_t layer_count);

        template<VSL_NAMESPACE::ImageType usage, VSL_NAMESPACE::MemoryProperty mem_prop, VSL_NAMESPACE::ImageLayout layout>
        ChangeImageBarrier(Image<usage, mem_prop, layout> image, ImageLayout new_layout)
            : ChangeImageBarrier(image, layout, new_layout){};

        ImageLayout old_layout, new_layout;
        ImageAccessor image;
        std::uint32_t base_layer = 0, layer_count = 0;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
} // namespace VSL_NAMESPACE::command

#endif
