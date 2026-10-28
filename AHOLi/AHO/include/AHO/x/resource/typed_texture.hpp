#pragma once
#include "texture.hpp"
#include <VSL/utils/typed_resources.hpp>

namespace AHO_NAMESPACE::x::resource {
    inline VSL_NAMESPACE::typed::TypedTexture typed_texture(const Texture& texture, VSL_NAMESPACE::SamplerAccessor sampler) {
        return VSL_NAMESPACE::typed::TypedTexture(texture.image(), std::move(sampler), texture.resource_use());
    }
} // namespace AHO_NAMESPACE::x::resource
