#ifndef AHO_ALL_CHANGE_IMAGE_BARRIER_HPP
#define AHO_ALL_CHANGE_IMAGE_BARRIER_HPP

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../descriptor.hpp"
#include "../pipeline.hpp"

#include "../image.hpp"
#include <optional>

namespace VSL_NAMESPACE::command {
    namespace detail {
        constexpr bool valid_shader_read_stages(ShaderFlag stages) noexcept {
            constexpr auto known = ShaderFlag::ALL_GRAPHICS | ShaderFlag::COMPUTE | ShaderFlag::TASK | ShaderFlag::MESH |
                                   ShaderFlag::RAYGEN | ShaderFlag::ANY_HIT | ShaderFlag::CLOSEST_HIT |
                                   ShaderFlag::MISS | ShaderFlag::INTERSECTION | ShaderFlag::CALLABLE;
            return stages != ShaderFlag::ERROR_ && (stages & known) == stages;
        }
    } // namespace detail

    struct ChangeImageBarrier : public Command {
        ChangeImageBarrier(ImageAccessor image, ImageLayout old_layout, ImageLayout new_layout);
        // 転送後のshader読み取り先を指定する。未指定の呼び出しはAllCommandsを使う。
        ChangeImageBarrier(ImageAccessor image, ImageLayout old_layout, ImageLayout new_layout,
                           ShaderFlag shader_read_stages);
        // layer_countが0ならbase_layer以降の全レイヤーを対象にする。ミップレベルは0のみ。
        ChangeImageBarrier(ImageAccessor image, ImageLayout old_layout, ImageLayout new_layout,
                           std::uint32_t base_layer, std::uint32_t layer_count);
        ChangeImageBarrier(ImageAccessor image, ImageLayout old_layout, ImageLayout new_layout,
                           std::uint32_t base_layer, std::uint32_t layer_count, ShaderFlag shader_read_stages);

        template<VSL_NAMESPACE::ImageType usage, VSL_NAMESPACE::MemoryProperty mem_prop, VSL_NAMESPACE::ImageLayout layout>
        ChangeImageBarrier(Image<usage, mem_prop, layout> image, ImageLayout new_layout)
            : ChangeImageBarrier(image, layout, new_layout){};

        ImageLayout old_layout, new_layout;
        ImageAccessor image;
        std::uint32_t base_layer = 0, layer_count = 0;

        [[nodiscard]] std::optional<ShaderFlag> shader_read_stages() const noexcept;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::uint32_t _shader_read_pipeline_stages;
        ShaderFlag _shader_read_stages = ShaderFlag::ERROR_;
    };

    inline std::optional<ShaderFlag> ChangeImageBarrier::shader_read_stages() const noexcept {
        if (_shader_read_stages == ShaderFlag::ERROR_)
            return std::nullopt;
        return _shader_read_stages;
    }
} // namespace VSL_NAMESPACE::command

#endif
