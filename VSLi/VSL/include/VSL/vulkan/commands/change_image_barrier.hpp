#ifndef AHO_ALL_CHANGE_IMAGE_BARRIER_HPP
#define AHO_ALL_CHANGE_IMAGE_BARRIER_HPP

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../descriptor.hpp"
#include "../pipeline.hpp"

#include "../image.hpp"

namespace VSL_NAMESPACE::command {
    struct ChangeImageBarrier : public __Command {
        ChangeImageBarrier(ImageAccessor image, ImageLayout oldLayout, ImageLayout newLayout);
        // layerCountが0ならbaseLayer以降の全レイヤーを対象にする。ミップレベルは0のみ。
        ChangeImageBarrier(ImageAccessor image, ImageLayout oldLayout, ImageLayout newLayout,
                           std::uint32_t baseLayer, std::uint32_t layerCount);

        template<vsl::ImageType Usage, vsl::MemoryProperty MemProp, vsl::ImageLayout Layout>
        ChangeImageBarrier(Image<Usage, MemProp, Layout> image, ImageLayout newLayout)
            : ChangeImageBarrier(image, Layout, newLayout){};

        ImageLayout oldLayout, newLayout;
        ImageAccessor image;
        std::uint32_t baseLayer = 0, layerCount = 0;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
}

#endif
