#pragma once
#include <AHO/define.hpp>
#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/command.hpp>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace AHO_NAMESPACE::command {
    class BufferUpload;
}

namespace AHO_NAMESPACE::resource {
    enum class BufferType {
        GENERAL,
        GPU_FRIENDLY,
        ON_GPU,
        CUSTOM
    };

    struct DefaultBufferCustomization {};

    template<BufferType type, typename Storage = void, typename Customization = DefaultBufferCustomization>
    class Buffer;

    // コピーはデータ・サイズ・容量を共有する。再確保後は取得済みのポインタを使わない。
    template<>
    class Buffer<BufferType::GENERAL> {
        struct Data;
        std::shared_ptr<Data> _data;

    public:
        explicit Buffer(std::size_t size = 0);

        void* data();
        const void* data() const;
        std::span<std::byte> bytes();
        std::span<const std::byte> bytes() const;

        std::size_t size() const;
        std::size_t capacity() const;

        void reserve(std::size_t capacity);
        void resize(std::size_t size);
        void assign(std::span<const std::byte> bytes);
        void write(std::span<const std::byte> bytes, std::size_t offset = 0);
    };

    namespace detail {
        // Submissionはresourceだけを保持し、Completionを持つこの状態は保持しない。
        struct BufferState {
            VSL_NAMESPACE::LogicalDeviceAccessor device;
            std::shared_ptr<void> resource;
            VSL_NAMESPACE::BufferAccessor* native = nullptr;
            std::size_t size = 0;
            bool host_readable = false;
            VSL_NAMESPACE::Completion use;
            std::shared_ptr<BufferState> staging;

            bool busy() const;
            void require_idle() const;
        };

        template<typename Storage, typename Customization>
        class GPUBufferBase;
    } // namespace detail

    // 型を消すのは転送に必要な参照だけ。CustomのStorage/Customizationは元の型で保持する。
    class BufferBinding {
        std::shared_ptr<detail::BufferState> _state;

        explicit BufferBinding(std::shared_ptr<detail::BufferState> state);

        template<typename, typename>
        friend class detail::GPUBufferBase;
        friend class AHO_NAMESPACE::command::BufferUpload;

    public:
        std::size_t size() const;
        VSL_NAMESPACE::LogicalDeviceAccessor device() const;
        VSL_NAMESPACE::BufferAccessor& native() const;
        std::shared_ptr<void> owner() const;
        bool host_readable() const;

        void check_available(const VSL_NAMESPACE::Completion& completion) const;
        void mark_used(VSL_NAMESPACE::Completion completion) const;
    };

    template<typename T>
    concept gpu_buffer = requires(const T& buffer) {
        { buffer.binding() } -> std::same_as<BufferBinding>;
    };

    namespace detail {
        template<typename T>
        concept vsl_buffer = std::derived_from<T, VSL_NAMESPACE::BufferAccessor> && std::move_constructible<T>;

        template<typename Storage>
        concept host_visible_storage = requires { Storage::PROPERTY; } &&
                                       VSL_NAMESPACE::contain(Storage::PROPERTY, VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE);

        template<typename Storage, typename Customization>
        concept custom_map = requires(Storage& storage, Customization& customization) { customization.map(storage); };

        template<typename Storage, typename Customization>
        concept mappable_storage = custom_map<Storage, Customization> || host_visible_storage<Storage>;

        template<typename Storage, typename Customization>
        auto map_storage(Storage& storage, Customization& customization)
        requires mappable_storage<Storage, Customization>;

        template<typename Storage, typename Customization, bool = mappable_storage<Storage, Customization>>
        struct MappingType {
            using Type = std::monostate;
        };

        template<typename Storage, typename Customization>
        struct MappingType<Storage, Customization, true> {
            using Type = decltype(map_storage(std::declval<Storage&>(), std::declval<Customization&>()));
        };

        template<typename Storage, typename Customization>
        struct BufferResource {
            using Mapping = typename MappingType<Storage, Customization>::Type;

            // 割当管理などの依存先を、Storageの破棄後まで保持する。
            Customization customization;
            std::optional<Storage> storage;
            std::optional<Mapping> mapping;

            BufferResource(std::optional<Storage> storage, Customization customization);
        };

        template<typename Storage, typename Customization>
        concept resizable_storage =
            mappable_storage<Storage, Customization> &&
            std::copy_constructible<Customization> &&
            requires(const Storage& storage, Customization& customization, std::size_t bytes) {
                { customization.reallocate(storage, bytes) } -> std::same_as<Storage>;
            };

        template<typename Storage, typename Customization>
        class GPUBufferBase {
            using Resource = BufferResource<Storage, Customization>;
            std::shared_ptr<BufferState> _state;

            Resource& resource() const;

            void replace(std::size_t capacity, bool preserve)
            requires resizable_storage<Storage, Customization>;

        protected:
            explicit GPUBufferBase(std::shared_ptr<BufferState> state);

            void initialize(VSL_NAMESPACE::LogicalDeviceAccessor device, std::optional<Storage> storage,
                            Customization customization, std::size_t size);

            GPUBufferBase(VSL_NAMESPACE::LogicalDeviceAccessor device, std::optional<Storage> storage,
                          Customization customization, std::size_t size);

        public:
            GPUBufferBase(Storage storage, Customization customization);

            std::size_t size() const;

            std::size_t capacity() const;

            bool busy() const;

            bool wait(std::uint64_t timeout = UINT64_MAX) const;

            BufferBinding binding() const;

            Storage& native();

            const Storage& native() const;

            auto map()
            requires mappable_storage<Storage, Customization>;

            std::span<const std::byte> read(std::size_t bytes, std::size_t offset = 0) const
            requires mappable_storage<Storage, Customization>;

            void write(std::span<const std::byte> bytes, std::size_t offset = 0)
            requires mappable_storage<Storage, Customization>;

            void reserve(std::size_t capacity)
            requires resizable_storage<Storage, Customization>;

            void resize(std::size_t size)
            requires resizable_storage<Storage, Customization>;

            void assign(std::span<const std::byte> bytes)
            requires mappable_storage<Storage, Customization>;

            AHO_NAMESPACE::command::BufferUpload upload(const Buffer<BufferType::GENERAL>& source, std::size_t destination_offset = 0) const;
            template<AHO_NAMESPACE::resource::gpu_buffer Source>
            AHO_NAMESPACE::command::BufferUpload upload(const Source& source, std::size_t destination_offset = 0, std::size_t source_offset = 0,
                                                        std::optional<std::size_t> bytes = std::nullopt) const;
        };

        inline constexpr auto STANDARD_BUFFER_USAGE =
            VSL_NAMESPACE::MemoryType::TRANSFER_SOURCE | VSL_NAMESPACE::MemoryType::TRANSFER_DESTINATION |
            VSL_NAMESPACE::MemoryType::VERTEX_BUFFER | VSL_NAMESPACE::MemoryType::INDEX_BUFFER |
            VSL_NAMESPACE::MemoryType::UNIFORM_BUFFER | VSL_NAMESPACE::MemoryType::STORAGE_BUFFER;

        using HostBuffer = VSL_NAMESPACE::Buffer<STANDARD_BUFFER_USAGE, VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE>;
        using DeviceBuffer = VSL_NAMESPACE::Buffer<STANDARD_BUFFER_USAGE, VSL_NAMESPACE::MemoryProperty::DEVICE_LOCAL>;

        struct HostBufferCustomization {
            VSL_NAMESPACE::LogicalDeviceAccessor device;

            HostBuffer create(std::size_t bytes) const;

            HostBuffer reallocate(const HostBuffer&, std::size_t bytes) const;
        };
    } // namespace detail

    template<>
    class Buffer<BufferType::GPU_FRIENDLY> : public detail::GPUBufferBase<detail::HostBuffer, detail::HostBufferCustomization> {
        using Base = detail::GPUBufferBase<detail::HostBuffer, detail::HostBufferCustomization>;

        explicit Buffer(std::shared_ptr<detail::BufferState> state);
        friend class AHO_NAMESPACE::command::BufferUpload;

    public:
        explicit Buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t size = 0);
    };

    template<>
    class Buffer<BufferType::ON_GPU> : public detail::GPUBufferBase<detail::DeviceBuffer, DefaultBufferCustomization> {
        using Base = detail::GPUBufferBase<detail::DeviceBuffer, DefaultBufferCustomization>;

    public:
        Buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t size);
    };

    template<typename Storage, typename Customization>
    requires detail::vsl_buffer<Storage>
    class Buffer<BufferType::CUSTOM, Storage, Customization> : public detail::GPUBufferBase<Storage, Customization> {
        using Base = detail::GPUBufferBase<Storage, Customization>;

    public:
        explicit Buffer(Storage storage, Customization customization = {});
    };

    template<detail::vsl_buffer Storage, typename Customization = DefaultBufferCustomization>
    auto make_custom_buffer(Storage storage, Customization customization = {});

    inline BufferBinding::BufferBinding(std::shared_ptr<detail::BufferState> state) : _state(std::move(state)) {}

    namespace detail {
        template<typename Storage, typename Customization>
        auto map_storage(Storage& storage, Customization& customization)
        requires mappable_storage<Storage, Customization>
        {
            if constexpr (custom_map<Storage, Customization>)
                return customization.map(storage);
            else
                return storage.data();
        }

        template<typename Storage, typename Customization>
        BufferResource<Storage, Customization>::BufferResource(std::optional<Storage> storage, Customization customization)
            : customization(std::move(customization)), storage(std::move(storage)) {
            if constexpr (mappable_storage<Storage, Customization>) {
                static_assert(
                    requires(Mapping& map, std::span<const std::byte> bytes) {
                        { map.read(std::size_t{}, std::size_t{}) } -> std::convertible_to<std::span<const std::byte>>;
                        map.write(bytes, std::size_t{});
                    },
                    "Buffer mapping must provide read(bytes, offset) and write(bytes, offset)");

                if (this->storage)
                    mapping.emplace(map_storage(*this->storage, this->customization));
            }
        }

        template<typename Storage, typename Customization>
        auto GPUBufferBase<Storage, Customization>::resource() const -> Resource& {
            if (!_state)
                throw std::logic_error("Buffer is empty");
            return *static_cast<Resource*>(_state->resource.get());
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::replace(std::size_t capacity, bool preserve)
        requires resizable_storage<Storage, Customization>
        {
            auto& old = resource();
            auto storage = [&]() -> Storage {
                if (old.storage)
                    return old.customization.reallocate(*old.storage, capacity);
                if constexpr (requires { old.customization.create(capacity); })
                    return old.customization.create(capacity);
                else
                    throw std::logic_error("Buffer has no storage to reallocate");
            }();
            auto next = std::make_shared<Resource>(std::optional<Storage>(std::move(storage)), old.customization);
            if (next->storage->get_device() != _state->device ||
                next->storage->requested_size() < capacity)
                throw std::invalid_argument("CUSTOM reallocation requires sufficient storage on the same device");
            if (preserve && _state->size)
                next->mapping->write(old.mapping->read(_state->size), 0);
            _state->native = &*next->storage;
            _state->resource = std::move(next);
        }

        template<typename Storage, typename Customization>
        GPUBufferBase<Storage, Customization>::GPUBufferBase(std::shared_ptr<BufferState> state) : _state(std::move(state)) {}

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::initialize(VSL_NAMESPACE::LogicalDeviceAccessor device, std::optional<Storage> storage,
                                                               Customization customization, std::size_t size) {
            if (!device.data_)
                throw std::invalid_argument("Buffer requires a device");
            auto owner = std::make_shared<Resource>(std::move(storage), std::move(customization));
            _state = std::make_shared<BufferState>();
            _state->device = device;
            _state->size = size;
            _state->host_readable = mappable_storage<Storage, Customization>;
            _state->native = owner->storage ? &*owner->storage : nullptr;
            _state->resource = std::move(owner);
        }

        template<typename Storage, typename Customization>
        GPUBufferBase<Storage, Customization>::GPUBufferBase(VSL_NAMESPACE::LogicalDeviceAccessor device, std::optional<Storage> storage,
                                                             Customization customization, std::size_t size) {
            initialize(device, std::move(storage), std::move(customization), size);
        }

        template<typename Storage, typename Customization>
        GPUBufferBase<Storage, Customization>::GPUBufferBase(Storage storage, Customization customization) {
            const auto device = storage.get_device();
            const auto size = storage.requested_size();
            initialize(device, std::optional<Storage>(std::move(storage)), std::move(customization), size);
        }

        template<typename Storage, typename Customization>
        std::size_t GPUBufferBase<Storage, Customization>::size() const {
            return _state ? _state->size : 0;
        }

        template<typename Storage, typename Customization>
        std::size_t GPUBufferBase<Storage, Customization>::capacity() const {
            return _state && _state->native ? _state->native->requested_size() : 0;
        }

        template<typename Storage, typename Customization>
        bool GPUBufferBase<Storage, Customization>::busy() const {
            return _state && _state->busy();
        }

        template<typename Storage, typename Customization>
        bool GPUBufferBase<Storage, Customization>::wait(std::uint64_t timeout) const {
            (void)resource();
            return !busy() || _state->use.wait(timeout);
        }

        template<typename Storage, typename Customization>
        BufferBinding GPUBufferBase<Storage, Customization>::binding() const {
            (void)resource();
            return BufferBinding(_state);
        }

        template<typename Storage, typename Customization>
        Storage& GPUBufferBase<Storage, Customization>::native() {
            auto& r = resource();
            if (!r.storage)
                throw std::logic_error("Buffer has no storage");
            return *r.storage;
        }

        template<typename Storage, typename Customization>
        const Storage& GPUBufferBase<Storage, Customization>::native() const {
            auto& r = resource();
            if (!r.storage)
                throw std::logic_error("Buffer has no storage");
            return *r.storage;
        }

        template<typename Storage, typename Customization>
        auto GPUBufferBase<Storage, Customization>::map()
        requires mappable_storage<Storage, Customization>
        {
            auto& r = resource();
            _state->require_idle();
            if (!r.mapping)
                throw std::logic_error("Buffer has no mapping");
            return map_storage(*r.storage, r.customization);
        }

        template<typename Storage, typename Customization>
        std::span<const std::byte> GPUBufferBase<Storage, Customization>::read(std::size_t bytes, std::size_t offset) const
        requires mappable_storage<Storage, Customization>
        {
            auto& r = resource();
            _state->require_idle();
            if (offset > size() || bytes > size() - offset)
                throw std::out_of_range("Buffer read range");
            if (!bytes)
                return {};
            return r.mapping->read(bytes, offset);
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::write(std::span<const std::byte> bytes, std::size_t offset)
        requires mappable_storage<Storage, Customization>
        {
            auto& r = resource();
            _state->require_idle();
            if (offset > size() || bytes.size() > size() - offset)
                throw std::out_of_range("Buffer write range");
            if (!bytes.empty())
                r.mapping->write(bytes, offset);
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::reserve(std::size_t capacity)
        requires resizable_storage<Storage, Customization>
        {
            (void)resource();
            _state->require_idle();
            if (capacity <= this->capacity())
                return;
            replace(capacity, true);
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::resize(std::size_t size)
        requires resizable_storage<Storage, Customization>
        {
            resource();
            _state->require_idle();
            reserve(size);
            _state->size = size;
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::assign(std::span<const std::byte> bytes)
        requires mappable_storage<Storage, Customization>
        {
            resource();
            _state->require_idle();
            if (bytes.size() > capacity()) {
                if constexpr (resizable_storage<Storage, Customization>)
                    replace(bytes.size(), false);
                else
                    throw std::out_of_range("Fixed Buffer assignment exceeds capacity");
            }
            if (!bytes.empty())
                resource().mapping->write(bytes, 0);
            _state->size = bytes.size();
        }

        inline HostBuffer HostBufferCustomization::create(std::size_t bytes) const {
            return HostBuffer(device, bytes);
        }

        inline HostBuffer HostBufferCustomization::reallocate(const HostBuffer&, std::size_t bytes) const {
            return create(bytes);
        }
    } // namespace detail

    inline Buffer<BufferType::GPU_FRIENDLY>::Buffer(std::shared_ptr<detail::BufferState> state) : Base(std::move(state)) {}

    inline Buffer<BufferType::GPU_FRIENDLY>::Buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t size)
        : Base(device, size ? std::optional<detail::HostBuffer>(std::in_place, device, size) : std::nullopt,
               detail::HostBufferCustomization{device}, size) {}

    inline Buffer<BufferType::ON_GPU>::Buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t size)
        : Base(device, size ? std::optional<detail::DeviceBuffer>(std::in_place, device, size) : std::nullopt,
               DefaultBufferCustomization{}, size) {}

    template<typename Storage, typename Customization>
    requires detail::vsl_buffer<Storage>
    Buffer<BufferType::CUSTOM, Storage, Customization>::Buffer(Storage storage, Customization customization) : Base(std::move(storage), std::move(customization)) {}

    template<detail::vsl_buffer Storage, typename Customization>
    auto make_custom_buffer(Storage storage, Customization customization) {
        return Buffer<BufferType::CUSTOM, Storage, Customization>(std::move(storage), std::move(customization));
    }
} // namespace AHO_NAMESPACE::resource
