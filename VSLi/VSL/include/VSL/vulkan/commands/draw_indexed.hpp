#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include <optional>
#include <cstdint>

namespace VSL_NAMESPACE::command {
    struct DrawIndexed : public Command {
        DrawIndexed(size_t size = 0);

        [[nodiscard]] std::uint32_t get_size() const noexcept { return _size; }

        void set_size(size_t size);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::uint32_t _size;
    };

    struct DrawIndexedManip : public DrawIndexed, public VertexSizeRequire {
        void set_vertex_size(std::optional<size_t> vertex_size) override;
    };

    extern DrawIndexedManip DRAW_INDEXED;
} // namespace VSL_NAMESPACE::command
