#pragma once
#include "../pipeline_layout.hpp"
#include <vector>
#include <utility>
namespace VSL_NAMESPACE::pipeline_layout {
    enum class ColorWriteMask : std::uint32_t {
        None = 0,
        R = 1,
        G = 2,
        B = 4,
        A = 8,
        RGBA = 15
    };
    ___VSL_EXPAND_FLAGS_OPERATOR(ColorWriteMask);
    enum class BlendFactor {
        Zero = 0,
        One = 1,
        SrcColor = 2,
        OneMinusSrcColor = 3,
        DstColor = 4,
        OneMinusDstColor = 5,
        SrcAlpha = 6,
        OneMinusSrcAlpha = 7,
        DstAlpha = 8,
        OneMinusDstAlpha = 9
    };
    enum class BlendOp {
        Add = 0,
        Subtract = 1,
        ReverseSubtract = 2,
        Min = 3,
        Max = 4
    };
    struct ColorBlendAttachment {
        bool enabled = true;
        ColorWriteMask writeMask = ColorWriteMask::RGBA;
        BlendFactor sourceColor = BlendFactor::SrcAlpha, destinationColor = BlendFactor::OneMinusSrcAlpha;
        BlendOp colorOp = BlendOp::Add;
        BlendFactor sourceAlpha = BlendFactor::One, destinationAlpha = BlendFactor::Zero;
        BlendOp alphaOp = BlendOp::Add;
    };
    struct ColorBlend {
        std::vector<ColorBlendAttachment> attachments{1};
        ColorBlend() = default;
        explicit ColorBlend(ColorBlendAttachment attachment) : attachments{attachment} {}
        explicit ColorBlend(std::vector<ColorBlendAttachment> values) : attachments(std::move(values)) {}
        void injection(PipelineLayoutAccessor pl);
    };
}
