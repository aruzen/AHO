//
// Created by morimoto_hibiki on 2025/10/07.
//

#include <AHO/resource/buffer.hpp>



template class AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::General>;
template class AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::GPUFriendly>;
template class AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::OnGPU>;