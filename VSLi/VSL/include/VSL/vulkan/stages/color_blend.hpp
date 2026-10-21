#pragma once
#include "../pipeline_layout.hpp"
#include <vector>
#include <utility>
namespace VSL_NAMESPACE::pipeline_layout {
    enum class ColorWriteMask : std::uint32_t {
        NONE = 0,
        R = 1,
        G = 2,
        B = 4,
        A = 8,
        RGBA = 15
    };
    ___VSL_EXPAND_FLAGS_OPERATOR(ColorWriteMask);
    enum class BlendFactor {
        ZERO = 0,
        ONE = 1,
        SRC_COLOR = 2,
        ONE_MINUS_SRC_COLOR = 3,
        DST_COLOR = 4,
        ONE_MINUS_DST_COLOR = 5,
        SRC_ALPHA = 6,
        ONE_MINUS_SRC_ALPHA = 7,
        DST_ALPHA = 8,
        ONE_MINUS_DST_ALPHA = 9
    };
    enum class BlendOp {
        ADD = 0,
        SUBTRACT = 1,
        REVERSE_SUBTRACT = 2,
        MIN_ = 3,
        MAX_ = 4
    };
    struct ColorBlendAttachment {
        bool enabled = true;
        ColorWriteMask write_mask = ColorWriteMask::RGBA;
        BlendFactor source_color = BlendFactor::SRC_ALPHA, destination_color = BlendFactor::ONE_MINUS_SRC_ALPHA;
        BlendOp color_op = BlendOp::ADD;
        BlendFactor source_alpha = BlendFactor::ONE, destination_alpha = BlendFactor::ZERO;
        BlendOp alpha_op = BlendOp::ADD;
    };
    struct ColorBlend {
        std::vector<ColorBlendAttachment> attachments{1};
        ColorBlend() = default;
        explicit ColorBlend(ColorBlendAttachment attachment) : attachments{attachment} {}
        explicit ColorBlend(std::vector<ColorBlendAttachment> values) : attachments(std::move(values)) {}
        void injection(PipelineLayoutEditAccessor pl);
    };
} // namespace VSL_NAMESPACE::pipeline_layout
