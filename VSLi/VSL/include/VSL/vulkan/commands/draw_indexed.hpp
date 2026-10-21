#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include <optional>
#include <cstdint>

namespace VSL_NAMESPACE::command {
    struct DrawIndexed : public __Command {
        DrawIndexed(size_t size = 0);

        [[nodiscard]] std::uint32_t getSize() const noexcept { return size_; }

        void setSize(size_t size);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::uint32_t size_;
    };

    struct DrawIndexedManip : public DrawIndexed, public __VertexSizeRequire {
        void setVertexSize(std::optional<size_t> vertexSize) override;
    };

    extern DrawIndexedManip drawIndexed;
} // namespace VSL_NAMESPACE::command
