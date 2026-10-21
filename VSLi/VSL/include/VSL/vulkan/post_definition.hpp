//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_POST_DEFINITION_H
#define AHO_ALL_POST_DEFINITION_H

#include "image.hpp"
#include "phase.hpp"
#include "commands/copy_buffer_to_image.hpp"
#include "commands/change_image_barrier.hpp"

template<VSL_NAMESPACE::ImageType usage, VSL_NAMESPACE::MemoryProperty mem_prop, VSL_NAMESPACE::ImageLayout layout>
template<VSL_NAMESPACE::MemoryType mem_type, VSL_NAMESPACE::MemoryProperty mem_property, VSL_NAMESPACE::SharingMode share_mode>
bool VSL_NAMESPACE::Image<usage, mem_prop, layout>::copy_by_buffer(CommandManager manager,
                                                                Buffer<mem_type, mem_property, share_mode>& buffer) {
    auto phase = manager.start_phase<ComputePhase>();
    if (layout != ImageLayout::TRANSFER_DST_OPTIMAL)
        phase << command::ChangeImageBarrier(*this, ImageLayout::TRANSFER_DST_OPTIMAL);
    phase << command::CopyBufferToImage(*this, &buffer, ImageLayout::TRANSFER_DST_OPTIMAL);
    return true;
}

#endif //AHO_ALL_POST_DEFINITION_H
