#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include <optional>
#include <cstdint>

namespace VSL_NAMESPACE::command {
    struct Draw : public __Command {
        Draw(size_t size = 0);

        [[nodiscard]] std::uint32_t getSize() const noexcept { return size_; }

        void setSize(size_t size);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::uint32_t size_;
    };

    struct DrawManip : public __Command, __VertexSizeRequire {
        [[nodiscard]] std::optional<std::uint32_t> getSize() const noexcept { return size_; }

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;

        void setVertexSize(std::optional<size_t> vertexSize) override;

    private:
        std::optional<std::uint32_t> size_;
    };

    extern DrawManip draw;
} // namespace VSL_NAMESPACE::command
