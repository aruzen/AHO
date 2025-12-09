//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_POST_DEFINITION_H
#define AHO_ALL_POST_DEFINITION_H

#include "image.hpp"
#include "phase.hpp"
#include "commands/copy_buffer_to_image.hpp"
#include "commands/change_image_barrier.hpp"

template<vsl::ImageType Usage, vsl::MemoryProperty MemProp, vsl::ImageLayout Layout>
template<vsl::MemoryType MemType, vsl::MemoryProperty MemProperty, vsl::SharingMode ShareMode>
bool vsl::Image<Usage, MemProp, Layout>::copyByBuffer(CommandManager manager,
                                                      Buffer<MemType, MemProperty, ShareMode> &buffer) {
    auto phase = manager.startPhase<ComputePhase>();
    if (Layout != ImageLayout::TransferDstOptimal)
        phase << command::ChangeImageBarrier(*this, ImageLayout::TransferDstOptimal);
    phase << command::CopyBufferToImage(*this, &buffer, ImageLayout::TransferDstOptimal);
}

#endif //AHO_ALL_POST_DEFINITION_H
