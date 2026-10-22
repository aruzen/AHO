#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include <optional>
#include <cstdint>

namespace VSL_NAMESPACE::command {
    struct Draw : public Command {
        Draw(size_t size = 0);
        Draw(size_t size, size_t instance_count, size_t first_vertex = 0, size_t first_instance = 0);

        [[nodiscard]] std::uint32_t get_size() const noexcept { return _size; }

        void set_size(size_t size);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::uint32_t _size;
        std::uint32_t _instance_count, _first_vertex, _first_instance;
    };

    struct DrawManip : public Command, VertexSizeRequire {
        [[nodiscard]] std::optional<std::uint32_t> get_size() const noexcept { return _size; }

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;

        void set_vertex_size(std::optional<size_t> vertex_size) override;

    private:
        std::optional<std::uint32_t> _size;
    };

    extern DrawManip DRAW;
} // namespace VSL_NAMESPACE::command
