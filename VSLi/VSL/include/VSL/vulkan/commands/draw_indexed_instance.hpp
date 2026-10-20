#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include <optional>
#include <cstdint>

namespace VSL_NAMESPACE::command {
    struct DrawInstance : public __Command {
        DrawInstance(size_t index_size, size_t instance_size);
        DrawInstance(size_t instance_size);

        [[nodiscard]] std::uint32_t getIndexSize() const noexcept { return index_size_; }

        [[nodiscard]] std::uint32_t getInstanceSize() const noexcept { return instance_size_; }

        void setIndexSize(size_t size);

        void setInstanceSize(size_t size);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::uint32_t index_size_, instance_size_;
    };

    /* FIXME
	struct DrawInstanceManip : public DrawInstance, public __VertexSizeRequire{
        void setVertexSize(std::optional<size_t> vertexSize) override;
    };

	extern DrawInstanceManip drawInstance;
     */
} // namespace VSL_NAMESPACE::command
