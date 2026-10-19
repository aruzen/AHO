//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_POST_DEFINITION_H
#define AHO_ALL_POST_DEFINITION_H

#include "image.hpp"
#include "phase.hpp"
#include "commands/copy_buffer_to_image.hpp"
#include "commands/change_image_barrier.hpp"

template<VSL_NAMESPACE::ImageType Usage, VSL_NAMESPACE::MemoryProperty MemProp, VSL_NAMESPACE::ImageLayout Layout>
template<VSL_NAMESPACE::MemoryType MemType, VSL_NAMESPACE::MemoryProperty MemProperty, VSL_NAMESPACE::SharingMode ShareMode>
bool VSL_NAMESPACE::Image<Usage, MemProp, Layout>::copyByBuffer(CommandManager manager,
                                                                Buffer<MemType, MemProperty, ShareMode>& buffer) {
    auto phase = manager.startPhase<ComputePhase>();
    if (Layout != ImageLayout::TransferDstOptimal)
        phase << command::ChangeImageBarrier(*this, ImageLayout::TransferDstOptimal);
    phase << command::CopyBufferToImage(*this, &buffer, ImageLayout::TransferDstOptimal);
    return true;
}

#endif //AHO_ALL_POST_DEFINITION_H
