//
// Created by morimoto_hibiki on 2025/07/30.
//

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
        // layerCount == 0 selects all layers from baseLayer onward. Only mip 0 is used.
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

#endif //AHO_ALL_CHANGE_IMAGE_BARRIER_HPP
