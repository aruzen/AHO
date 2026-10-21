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
        General,
        GPUFriendly,
        OnGPU,
        Custom
    };

    struct DefaultBufferCustomization {};

    template<BufferType Type, typename Storage = void, typename Customization = DefaultBufferCustomization>
    class Buffer;

    // コピーはデータ・サイズ・容量を共有する。再確保後は取得済みのポインタを使わない。
    template<>
    class Buffer<BufferType::General> {
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
            bool hostReadable = false;
            VSL_NAMESPACE::Completion use;
            std::shared_ptr<BufferState> staging;

            bool busy() const;
            void requireIdle() const;
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
        bool hostReadable() const;

        void checkAvailable(const VSL_NAMESPACE::Completion& completion) const;
        void markUsed(VSL_NAMESPACE::Completion completion) const;
    };

    template<typename T>
    concept GPUBuffer = requires(const T& buffer) {
        { buffer.binding() } -> std::same_as<BufferBinding>;
    };

    namespace detail {
        template<typename T>
        concept VSLBuffer = std::derived_from<T, VSL_NAMESPACE::BufferAccessor> && std::move_constructible<T>;

        template<typename Storage>
        concept HostVisibleStorage = requires { Storage::Property; } &&
                                     VSL_NAMESPACE::contain(Storage::Property, VSL_NAMESPACE::MemoryProperty::HostVisible);

        template<typename Storage, typename Customization>
        concept CustomMap = requires(Storage& storage, Customization& customization) { customization.map(storage); };

        template<typename Storage, typename Customization>
        concept MappableStorage = CustomMap<Storage, Customization> || HostVisibleStorage<Storage>;

        template<typename Storage, typename Customization>
        auto mapStorage(Storage& storage, Customization& customization)
        requires MappableStorage<Storage, Customization>;

        template<typename Storage, typename Customization, bool = MappableStorage<Storage, Customization>>
        struct MappingType {
            using type = std::monostate;
        };

        template<typename Storage, typename Customization>
        struct MappingType<Storage, Customization, true> {
            using type = decltype(mapStorage(std::declval<Storage&>(), std::declval<Customization&>()));
        };

        template<typename Storage, typename Customization>
        struct BufferResource {
            using Mapping = typename MappingType<Storage, Customization>::type;

            // 割当管理などの依存先を、Storageの破棄後まで保持する。
            Customization customization;
            std::optional<Storage> storage;
            std::optional<Mapping> mapping;

            BufferResource(std::optional<Storage> storage, Customization customization);
        };

        template<typename Storage, typename Customization>
        concept ResizableStorage =
            MappableStorage<Storage, Customization> &&
            std::copy_constructible<Customization> &&
            requires(const Storage& storage, Customization& customization, std::size_t bytes) {
                { customization.reallocate(storage, bytes) } -> std::same_as<Storage>;
            };

        template<typename Storage, typename Customization>
        class GPUBufferBase {
            using Resource = BufferResource<Storage, Customization>;

            Resource& resource() const;

            void replace(std::size_t capacity, bool preserve)
            requires ResizableStorage<Storage, Customization>;

        protected:
            std::shared_ptr<BufferState> _state;

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
            requires MappableStorage<Storage, Customization>;

            std::span<const std::byte> read(std::size_t bytes, std::size_t offset = 0) const
            requires MappableStorage<Storage, Customization>;

            void write(std::span<const std::byte> bytes, std::size_t offset = 0)
            requires MappableStorage<Storage, Customization>;

            void reserve(std::size_t capacity)
            requires ResizableStorage<Storage, Customization>;

            void resize(std::size_t size)
            requires ResizableStorage<Storage, Customization>;

            void assign(std::span<const std::byte> bytes)
            requires MappableStorage<Storage, Customization>;

            AHO_NAMESPACE::command::BufferUpload upload(const Buffer<BufferType::General>& source, std::size_t destinationOffset = 0) const;
            template<AHO_NAMESPACE::resource::GPUBuffer Source>
            AHO_NAMESPACE::command::BufferUpload upload(const Source& source, std::size_t destinationOffset = 0, std::size_t sourceOffset = 0,
                                                        std::optional<std::size_t> bytes = std::nullopt) const;
        };

        inline constexpr auto StandardBufferUsage =
            VSL_NAMESPACE::MemoryType::TransferSource | VSL_NAMESPACE::MemoryType::TransferDestination |
            VSL_NAMESPACE::MemoryType::VertexBuffer | VSL_NAMESPACE::MemoryType::IndexBuffer |
            VSL_NAMESPACE::MemoryType::UniformBuffer | VSL_NAMESPACE::MemoryType::StorageBuffer;

        using HostBuffer = VSL_NAMESPACE::Buffer<StandardBufferUsage, VSL_NAMESPACE::MemoryProperty::HostVisible>;
        using DeviceBuffer = VSL_NAMESPACE::Buffer<StandardBufferUsage, VSL_NAMESPACE::MemoryProperty::DeviceLocal>;

        struct HostBufferCustomization {
            VSL_NAMESPACE::LogicalDeviceAccessor device;

            HostBuffer create(std::size_t bytes) const;

            HostBuffer reallocate(const HostBuffer&, std::size_t bytes) const;
        };
    } // namespace detail

    template<>
    class Buffer<BufferType::GPUFriendly> : public detail::GPUBufferBase<detail::HostBuffer, detail::HostBufferCustomization> {
        using Base = detail::GPUBufferBase<detail::HostBuffer, detail::HostBufferCustomization>;

        explicit Buffer(std::shared_ptr<detail::BufferState> state);
        friend class AHO_NAMESPACE::command::BufferUpload;

    public:
        explicit Buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t size = 0);
    };

    template<>
    class Buffer<BufferType::OnGPU> : public detail::GPUBufferBase<detail::DeviceBuffer, DefaultBufferCustomization> {
        using Base = detail::GPUBufferBase<detail::DeviceBuffer, DefaultBufferCustomization>;

    public:
        Buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t size);
    };

    template<typename Storage, typename Customization>
    requires detail::VSLBuffer<Storage>
    class Buffer<BufferType::Custom, Storage, Customization> : public detail::GPUBufferBase<Storage, Customization> {
        using Base = detail::GPUBufferBase<Storage, Customization>;

    public:
        explicit Buffer(Storage storage, Customization customization = {});
    };

    template<detail::VSLBuffer Storage, typename Customization = DefaultBufferCustomization>
    auto make_custom_buffer(Storage storage, Customization customization = {});

    inline BufferBinding::BufferBinding(std::shared_ptr<detail::BufferState> state) : _state(std::move(state)) {}

    namespace detail {
        template<typename Storage, typename Customization>
        auto mapStorage(Storage& storage, Customization& customization)
        requires MappableStorage<Storage, Customization>
        {
            if constexpr (CustomMap<Storage, Customization>)
                return customization.map(storage);
            else
                return storage.data();
        }

        template<typename Storage, typename Customization>
        BufferResource<Storage, Customization>::BufferResource(std::optional<Storage> storage, Customization customization)
            : customization(std::move(customization)), storage(std::move(storage)) {
            if constexpr (MappableStorage<Storage, Customization>) {
                static_assert(
                    requires(Mapping& map, std::span<const std::byte> bytes) {
                        { map.read(std::size_t{}, std::size_t{}) } -> std::convertible_to<std::span<const std::byte>>;
                        map.write(bytes, std::size_t{});
                    },
                    "Buffer mapping must provide read(bytes, offset) and write(bytes, offset)");

                if (this->storage)
                    mapping.emplace(mapStorage(*this->storage, this->customization));
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
        requires ResizableStorage<Storage, Customization>
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
            if (next->storage->getDevice() != _state->device ||
                next->storage->requestedSize() < capacity)
                throw std::invalid_argument("Custom reallocation requires sufficient storage on the same device");
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
            if (!device._data)
                throw std::invalid_argument("Buffer requires a device");
            auto owner = std::make_shared<Resource>(std::move(storage), std::move(customization));
            _state = std::make_shared<BufferState>();
            _state->device = device;
            _state->size = size;
            _state->hostReadable = MappableStorage<Storage, Customization>;
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
            const auto device = storage.getDevice();
            const auto size = storage.requestedSize();
            initialize(device, std::optional<Storage>(std::move(storage)), std::move(customization), size);
        }

        template<typename Storage, typename Customization>
        std::size_t GPUBufferBase<Storage, Customization>::size() const {
            return _state ? _state->size : 0;
        }

        template<typename Storage, typename Customization>
        std::size_t GPUBufferBase<Storage, Customization>::capacity() const {
            return _state && _state->native ? _state->native->requestedSize() : 0;
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
        requires MappableStorage<Storage, Customization>
        {
            auto& r = resource();
            _state->requireIdle();
            if (!r.mapping)
                throw std::logic_error("Buffer has no mapping");
            return mapStorage(*r.storage, r.customization);
        }

        template<typename Storage, typename Customization>
        std::span<const std::byte> GPUBufferBase<Storage, Customization>::read(std::size_t bytes, std::size_t offset) const
        requires MappableStorage<Storage, Customization>
        {
            auto& r = resource();
            _state->requireIdle();
            if (offset > size() || bytes > size() - offset)
                throw std::out_of_range("Buffer read range");
            if (!bytes)
                return {};
            return r.mapping->read(bytes, offset);
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::write(std::span<const std::byte> bytes, std::size_t offset)
        requires MappableStorage<Storage, Customization>
        {
            auto& r = resource();
            _state->requireIdle();
            if (offset > size() || bytes.size() > size() - offset)
                throw std::out_of_range("Buffer write range");
            if (!bytes.empty())
                r.mapping->write(bytes, offset);
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::reserve(std::size_t capacity)
        requires ResizableStorage<Storage, Customization>
        {
            (void)resource();
            _state->requireIdle();
            if (capacity <= this->capacity())
                return;
            replace(capacity, true);
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::resize(std::size_t size)
        requires ResizableStorage<Storage, Customization>
        {
            resource();
            _state->requireIdle();
            reserve(size);
            _state->size = size;
        }

        template<typename Storage, typename Customization>
        void GPUBufferBase<Storage, Customization>::assign(std::span<const std::byte> bytes)
        requires MappableStorage<Storage, Customization>
        {
            resource();
            _state->requireIdle();
            if (bytes.size() > capacity()) {
                if constexpr (ResizableStorage<Storage, Customization>)
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

    inline Buffer<BufferType::GPUFriendly>::Buffer(std::shared_ptr<detail::BufferState> state) : Base(std::move(state)) {}

    inline Buffer<BufferType::GPUFriendly>::Buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t size)
        : Base(device, size ? std::optional<detail::HostBuffer>(std::in_place, device, size) : std::nullopt,
               detail::HostBufferCustomization{device}, size) {}

    inline Buffer<BufferType::OnGPU>::Buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t size)
        : Base(device, size ? std::optional<detail::DeviceBuffer>(std::in_place, device, size) : std::nullopt,
               DefaultBufferCustomization{}, size) {}

    template<typename Storage, typename Customization>
    requires detail::VSLBuffer<Storage>
    Buffer<BufferType::Custom, Storage, Customization>::Buffer(Storage storage, Customization customization) : Base(std::move(storage), std::move(customization)) {}

    template<detail::VSLBuffer Storage, typename Customization>
    auto make_custom_buffer(Storage storage, Customization customization) {
        return Buffer<BufferType::Custom, Storage, Customization>(std::move(storage), std::move(customization));
    }
} // namespace AHO_NAMESPACE::resource
