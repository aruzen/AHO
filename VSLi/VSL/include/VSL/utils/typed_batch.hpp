#pragma once
#include "typed_pipeline.hpp"
#include "typed_compute.hpp"

namespace VSL_NAMESPACE::typed {
    struct BatchStatistics {
        size_t pipeline_binds = 0, vertex_binds = 0, index_binds = 0, descriptor_binds = 0;
        size_t draws = 0, dispatches = 0;
    };
    namespace detail {
        enum class BatchDomain {
            UNKNOWN,
            GRAPHICS,
            COMPUTE
        };
        template<typename T>
        struct BatchTraits {
            static constexpr auto COMMAND_DOMAIN = BatchDomain::UNKNOWN;
            static constexpr size_t VERTEX_COUNT = 0, RESOURCE_COUNT = 0;
        };
        template<typename Input, typename Resources, typename Pushes>
        struct BatchTraits<TypedDraw<Input, Resources, Pushes>> {
            static constexpr auto COMMAND_DOMAIN = BatchDomain::GRAPHICS;
            static constexpr size_t VERTEX_COUNT = Input::BINDING_COUNT, RESOURCE_COUNT = Resources::COUNT;
        };
        template<typename Input, typename Resources, typename Pushes>
        struct BatchTraits<TypedIndexedDraw<Input, Resources, Pushes>> : BatchTraits<TypedDraw<Input, Resources, Pushes>> {};
        template<typename Resources, typename Pushes>
        struct BatchTraits<TypedDispatch<Resources, Pushes>> {
            static constexpr auto COMMAND_DOMAIN = BatchDomain::COMPUTE;
            static constexpr size_t VERTEX_COUNT = 0, RESOURCE_COUNT = Resources::COUNT;
        };
        template<typename Command>
        struct BatchPlan {
            bool pipeline = false, index = false;
            std::array<bool, BatchTraits<Command>::VERTEX_COUNT> vertices{};
            std::array<bool, BatchTraits<Command>::RESOURCE_COUNT> resources{};
        };
        struct BatchVertexKey {
            std::uint32_t binding;
            const void* source;
            size_t offset;
            bool operator==(const BatchVertexKey&) const = default;
        };
        struct BatchResourceKey {
            std::uint32_t set;
            const void* source;
            bool operator==(const BatchResourceKey&) const = default;
        };
        struct BatchIndexKey {
            const void* source = nullptr;
            size_t offset = 0;
            command::IndexType type = command::IndexType::UINT32;
            bool operator==(const BatchIndexKey&) const = default;
        };
        template<size_t vertices, size_t resources>
        struct BatchState {
            const void* pipeline = nullptr;
            std::array<BatchVertexKey, vertices> vertex_keys{};
            std::array<BatchResourceKey, resources> resource_keys{};
            BatchIndexKey index;
            size_t vertex_count = 0, resource_count = 0;

            bool update_pipeline(const void* next);
            bool update_vertex(const command::BindVertexBufferAt& binding);
            bool update_index(const command::BindIndexBufferAt& binding);
            bool update_resource(const command::BindGraphicResource& binding);
        };
        template<typename... Commands>
        consteval bool valid_batch() {
            if constexpr (!sizeof...(Commands))
                return false;
            else {
                constexpr auto domain = BatchTraits<std::tuple_element_t<0, std::tuple<Commands...>>>::COMMAND_DOMAIN;
                return domain != BatchDomain::UNKNOWN && ((BatchTraits<Commands>::COMMAND_DOMAIN == domain) && ...);
            }
        }
    } // namespace detail

    template<typename... Commands>
    class TypedBatch final : public command::Command, public command::PipelineHolder {
        static_assert(detail::valid_batch<Commands...>(), "Typed batch requires nonempty graphics-only or compute-only typed commands");
        static constexpr size_t VERTEX_CAPACITY = (detail::BatchTraits<Commands>::VERTEX_COUNT + ... + 0);
        static constexpr size_t RESOURCE_CAPACITY = (detail::BatchTraits<Commands>::RESOURCE_COUNT + ... + 0);
        using State = detail::BatchState<VERTEX_CAPACITY, RESOURCE_CAPACITY>;
        std::tuple<Commands...> _commands;
        std::tuple<detail::BatchPlan<Commands>...> _plans;
        BatchStatistics _statistics;
        template<typename CommandType>
        void plan_one(const CommandType& command, detail::BatchPlan<CommandType>& plan, State& state);
        template<typename CommandType>
        void invoke_one(CommandType& command, const detail::BatchPlan<CommandType>& plan, CommandPool pool, CommandBuffer buffer, CommandManager manager);
        template<size_t... indices>
        void build_plans(std::index_sequence<indices...>);
        template<size_t... indices>
        void invoke_all(CommandPool pool, CommandBuffer buffer, CommandManager manager, std::index_sequence<indices...>);

    public:
        explicit TypedBatch(Commands... commands);
        const BatchStatistics& statistics() const { return _statistics; }
        PipelineAccessor get_pipeline() override { return std::get<sizeof...(Commands) - 1>(_commands).get_pipeline(); }
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };
    template<typename... Commands>
    TypedBatch(Commands...) -> TypedBatch<Commands...>;

    namespace detail {
        template<size_t vertices, size_t resources>
        bool BatchState<vertices, resources>::update_pipeline(const void* next) {
            if (pipeline == next)
                return false;
            pipeline = next;
            // Descriptor compatibility is not inferred across different pipeline layouts.
            resource_count = 0;
            return true;
        }

        template<size_t vertices, size_t resources>
        bool BatchState<vertices, resources>::update_vertex(const command::BindVertexBufferAt& binding) {
            const BatchVertexKey key{binding.binding(), binding.source().get(), binding.offset()};
            auto keys = std::span(vertex_keys).first(vertex_count);
            const auto found = std::ranges::find(keys, key.binding, &BatchVertexKey::binding);
            if (found != keys.end() && *found == key)
                return false;
            if (found == keys.end())
                vertex_keys[vertex_count++] = key;
            else
                *found = key;
            return true;
        }

        template<size_t vertices, size_t resources>
        bool BatchState<vertices, resources>::update_index(const command::BindIndexBufferAt& binding) {
            const BatchIndexKey key{binding.source().get(), binding.offset(), binding.type()};
            if (index == key)
                return false;
            index = key;
            return true;
        }

        template<size_t vertices, size_t resources>
        bool BatchState<vertices, resources>::update_resource(const command::BindGraphicResource& binding) {
            const BatchResourceKey key{binding.first_binding, binding.resources.front().data_.get()};
            auto keys = std::span(resource_keys).first(resource_count);
            const auto found = std::ranges::find(keys, key.set, &BatchResourceKey::set);
            if (found != keys.end() && *found == key)
                return false;
            if (found == keys.end())
                resource_keys[resource_count++] = key;
            else
                *found = key;
            return true;
        }
    } // namespace detail

    template<typename... Commands>
    TypedBatch<Commands...>::TypedBatch(Commands... commands) : _commands(std::move(commands)...) {
        const auto device = std::get<0>(_commands)._device.data_;
        const bool same_device = std::apply([&](const auto&... value) { return ((value._device.data_ == device) && ...); }, _commands);
        if (!same_device)
            throw std::invalid_argument("Typed batch commands use different devices");
        build_plans(std::index_sequence_for<Commands...>{});
    }
    template<typename... Commands>
    template<size_t... indices>
    void TypedBatch<Commands...>::build_plans(std::index_sequence<indices...>) {
        State state;
        (plan_one(std::get<indices>(_commands), std::get<indices>(_plans), state), ...);
    }
    template<typename... Commands>
    template<typename CommandType>
    void TypedBatch<Commands...>::plan_one(const CommandType& command, detail::BatchPlan<CommandType>& plan, State& state) {
        plan.pipeline = state.update_pipeline(command._pipeline.data_.get());
        _statistics.pipeline_binds += plan.pipeline;

        if constexpr (requires { command._buffers; }) {
            for (size_t i = 0; i < command._buffers.size(); ++i) {
                plan.vertices[i] = state.update_vertex(command._buffers[i]);
                _statistics.vertex_binds += plan.vertices[i];
            }
        }
        if constexpr (requires { command._indices; }) {
            plan.index = state.update_index(command._indices);
            _statistics.index_binds += plan.index;
        }
        for (size_t i = 0; i < command._resources.size(); ++i) {
            plan.resources[i] = state.update_resource(command._resources[i]);
            _statistics.descriptor_binds += plan.resources[i];
        }

        if constexpr (detail::BatchTraits<CommandType>::COMMAND_DOMAIN == detail::BatchDomain::GRAPHICS)
            ++_statistics.draws;
        else
            ++_statistics.dispatches;
    }
    template<typename... Commands>
    template<typename CommandType>
    void TypedBatch<Commands...>::invoke_one(CommandType& command, const detail::BatchPlan<CommandType>& plan, CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        if (plan.pipeline)
            command._pipeline.invoke(pool, buffer, manager);
        if constexpr (requires { command._buffers; }) {
            for (size_t i = 0; i < command._buffers.size(); ++i)
                if (plan.vertices[i])
                    command._buffers[i].invoke(pool, buffer, manager);
                else
                    command._buffers[i].track(buffer);
        }
        if constexpr (requires { command._indices; }) {
            if (plan.index)
                command._indices.invoke(pool, buffer, manager);
            else
                command._indices.track(buffer);
        }
        for (size_t i = 0; i < command._resources.size(); ++i)
            if (plan.resources[i])
                command._resources[i].invoke(pool, buffer, manager);
            else
                command._resources[i].track(buffer);
        if constexpr (!CommandType::PushLayout::EMPTY)
            detail::invoke_push_values(command._pipeline, buffer, CommandType::PushLayout::REQUIREMENTS, command._pushes->bytes());
        if constexpr (requires { command._draw; })
            command._draw.invoke(pool, buffer, manager);
        else
            command._dispatch.invoke(pool, buffer, manager);
    }
    template<typename... Commands>
    template<size_t... indices>
    void TypedBatch<Commands...>::invoke_all(CommandPool pool, CommandBuffer buffer, CommandManager manager, std::index_sequence<indices...>) {
        (invoke_one(std::get<indices>(_commands), std::get<indices>(_plans), pool, buffer, manager), ...);
    }
    template<typename... Commands>
    void TypedBatch<Commands...>::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        if (std::get<0>(_commands)._device.data_ != manager.get_device().data_)
            throw std::invalid_argument("Typed batch and command manager use different devices");
        invoke_all(pool, buffer, manager, std::index_sequence_for<Commands...>{});
    }
} // namespace VSL_NAMESPACE::typed
