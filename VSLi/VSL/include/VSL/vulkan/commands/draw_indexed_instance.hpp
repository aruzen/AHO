#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include <optional>
#include <cstdint>

namespace VSL_NAMESPACE::command {
    struct DrawInstance : public Command {
        DrawInstance(size_t index_size, size_t instance_size);
        DrawInstance(size_t instance_size);

        [[nodiscard]] std::uint32_t get_index_size() const noexcept { return _index_size; }

        [[nodiscard]] std::uint32_t get_instance_size() const noexcept { return _instance_size; }

        void set_index_size(size_t size);

        void set_instance_size(size_t size);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::uint32_t _index_size, _instance_size;
    };

    /* FIXME
	struct DrawInstanceManip : public DrawInstance, public __VertexSizeRequire{
        void set_vertex_size(std::optional<size_t> vertex_size) override;
    };

	extern DrawInstanceManip drawInstance;
     */
} // namespace VSL_NAMESPACE::command
