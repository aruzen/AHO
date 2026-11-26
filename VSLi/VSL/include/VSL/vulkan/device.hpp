#pragma once
#include "../define.hpp"
#include "vulkan.hpp"
#include "surface.hpp"
#include "../operation/device_create_info.hpp"

#include "../type.hpp"
#include "../concepts.hpp"

#include <any>
#include <optional>
#include <utility>
#include "../exceptions.hpp"

namespace VSL_NAMESPACE {
    struct Surface;

    namespace physical_device_type {
        class PhysicalDeviceType : public Type_<PhysicalDeviceType> {
        public:
            consteval PhysicalDeviceType(const char* name, char index) : Type_(name, index) {}
        };

        constexpr PhysicalDeviceType OTHER = PhysicalDeviceType("OTHER", 0);
        constexpr PhysicalDeviceType INTEGRATED_GPU = PhysicalDeviceType("INTEGRATED_GPU", 1);
        constexpr PhysicalDeviceType DISCRETE_GPU = PhysicalDeviceType("DISCRETE_GPU", 2);
        constexpr PhysicalDeviceType VIRTUAL_GPU = PhysicalDeviceType("VIRTUAL_GPU", 3);
        constexpr PhysicalDeviceType CPU = PhysicalDeviceType("CPU", 4);

        PhysicalDeviceType get_type(int a);
    }; // namespace physical_device_type

    struct PhysicalDevicesSercher {
        int benchmark(PhysicalDevice device);
    };

    struct PhysicalDevice {
    private:
        lifetime::OwnedRef<impl::PhysicalDeviceImpl> _owner;

        explicit PhysicalDevice(lifetime::OwnedRef<impl::PhysicalDeviceImpl> owner) noexcept : _owner(std::move(owner)) {}
        friend struct lifetime::detail::StartupAccess;

    public:
        PhysicalDevice() = default;
        PhysicalDevice(const PhysicalDevice&) = default;
        PhysicalDevice(PhysicalDevice&&) noexcept = default;

        PhysicalDevice& operator=(PhysicalDevice value) noexcept {
            swap(value);
            return *this;
        }

        void swap(PhysicalDevice& other) noexcept { _owner.swap(other._owner); }
        explicit operator bool() const noexcept { return bool(_owner); }

        ~PhysicalDevice();

        static PhysicalDevice Select(VulkanAccessor instance);
        VulkanAccessor get_instance() const;
        lifetime::OwnerRef owner_ref() const;
        DeviceFeature supported_features() const;
        uint32_t api_version();
        uint32_t driver_version();
        uint32_t vendor_id();
        uint32_t device_id();
        physical_device_type::PhysicalDeviceType type();
        std::string name();
    };

    namespace lifetime::detail {
        class PhysicalDeviceCollection {
            std::shared_ptr<impl::PhysicalDevicesImpl> _state;
            explicit PhysicalDeviceCollection(VulkanAccessor instance);
            std::vector<PhysicalDevice> get() const&;
            std::vector<PhysicalDevice> get() &&;
            PhysicalDevice select() const;
            template<typename>
            friend struct ::VSL_NAMESPACE::PhysicalDevices;
            friend struct StartupAccess;

        public:
            PhysicalDeviceCollection(const PhysicalDeviceCollection&) = default;
            PhysicalDeviceCollection(PhysicalDeviceCollection&&) noexcept = default;

            PhysicalDeviceCollection& operator=(PhysicalDeviceCollection value) noexcept {
                swap(value);
                return *this;
            }

            void swap(PhysicalDeviceCollection& other) noexcept { _state.swap(other._state); }
        };
    } // namespace lifetime::detail

    template<typename S>
    struct PhysicalDevices {
    private:
        lifetime::detail::PhysicalDeviceCollection _collection;
        friend struct lifetime::detail::StartupAccess;

    public:
        using BenchMarcker = S;
        PhysicalDevices(VulkanAccessor vulkan);
        PhysicalDevices(const PhysicalDevices&) = default;
        PhysicalDevices(PhysicalDevices&&) noexcept = default;

        PhysicalDevices& operator=(PhysicalDevices value) noexcept {
            swap(value);
            return *this;
        }

        void swap(PhysicalDevices& other) noexcept { _collection.swap(other._collection); }

        ~PhysicalDevices();
        std::vector<PhysicalDevice> get();
        PhysicalDevice search();
#ifdef VSL_NAMESPACE_TEST
        void test();
#endif
    };

    class WeakLogicalDevice;

    struct LogicalDeviceAccessor {
    private:
        lifetime::OwnedRef<impl::LogicalDeviceImpl> _owner;

        explicit LogicalDeviceAccessor(lifetime::OwnedRef<impl::LogicalDeviceImpl> owner) noexcept : _owner(std::move(owner)) {}

        static LogicalDeviceAccessor CheckRuntime(LogicalDeviceAccessor value);
        static LogicalDeviceAccessor CheckCreatorResult(const PhysicalDevice& physical, LogicalDeviceAccessor value);
        static Surface ReadSurface(std::shared_ptr<Surface> surface);
        template<typename>
        friend struct LogicalDevice;
        friend struct lifetime::detail::StartupAccess;
        friend class WeakLogicalDevice;

    public:
        LogicalDeviceAccessor() = default;
        LogicalDeviceAccessor(const LogicalDeviceAccessor&) = default;
        LogicalDeviceAccessor(LogicalDeviceAccessor&&) noexcept = default;

        LogicalDeviceAccessor& operator=(LogicalDeviceAccessor value) noexcept {
            swap(value);
            return *this;
        }

        void swap(LogicalDeviceAccessor& other) noexcept { _owner.swap(other._owner); }
        explicit operator bool() const noexcept { return bool(_owner); }

        bool operator==(const LogicalDeviceAccessor&) const = default;
        static LogicalDeviceAccessor Create(PhysicalDevice physical, DeviceCreateInfo info = {});
        static DeviceCreateInfo GraphicsInfo(PhysicalDevice physical, Surface surface, bool validation = VALIDATION,
                                             PresentFencePolicy present_fences = PresentFencePolicy::LEGACY);
        static LogicalDeviceAccessor CreateGraphics(PhysicalDevice physical, Surface surface, bool validation = VALIDATION,
                                                    PresentFencePolicy present_fences = PresentFencePolicy::LEGACY);
        lifetime::OwnerRef owner_ref() const;
        PhysicalDevice get_physical_device() const;
        DeviceFeature enabled_features() const;
        WeakLogicalDevice weak() const noexcept;
    };

    // lockは元のstorageを保持する。Native操作の可否は各操作で検証する。
    class WeakLogicalDevice {
        lifetime::WeakOwnedRef<impl::LogicalDeviceImpl> _owner;

        explicit WeakLogicalDevice(lifetime::WeakOwnedRef<impl::LogicalDeviceImpl> owner) noexcept : _owner(std::move(owner)) {}
        friend struct LogicalDeviceAccessor;

    public:
        WeakLogicalDevice() = default;
        WeakLogicalDevice(const WeakLogicalDevice&) = default;
        WeakLogicalDevice(WeakLogicalDevice&&) noexcept = default;

        WeakLogicalDevice& operator=(WeakLogicalDevice value) noexcept {
            swap(value);
            return *this;
        }

        void swap(WeakLogicalDevice& other) noexcept { _owner.swap(other._owner); }
        void reset() noexcept { _owner.reset(); }
        bool expired() const noexcept { return _owner.expired(); }

        LogicalDeviceAccessor lock() const noexcept;
    };

    template<bool validation>
    struct GraphicsDeviceCreator {
        LogicalDeviceAccessor create(PhysicalDevice physical, Surface surface) {
            return LogicalDeviceAccessor::CreateGraphics(std::move(physical), std::move(surface), validation);
        }
    };

    template<typename C = GraphicsDeviceCreator<VALIDATION>>
    struct LogicalDevice {
    private:
        LogicalDeviceAccessor _value;
        friend struct lifetime::detail::StartupAccess;
        template<typename>
        friend struct LogicalDevice;

    public:
        LogicalDevice(PhysicalDevice physical, Surface surface);
        LogicalDevice(PhysicalDevice physical, std::shared_ptr<Surface> surface);

        explicit LogicalDevice(LogicalDeviceAccessor source) : _value(LogicalDeviceAccessor::CheckRuntime(std::move(source))) {}

        LogicalDevice(const LogicalDevice&) = default;
        LogicalDevice(LogicalDevice&&) noexcept = default;

        LogicalDevice& operator=(LogicalDevice value) noexcept {
            swap(value);
            return *this;
        }

        void swap(LogicalDevice& other) noexcept { _value.swap(other._value); }
        operator LogicalDeviceAccessor() const& { return _value; }
        operator LogicalDeviceAccessor() && { return std::move(_value); }
        explicit operator bool() const noexcept { return bool(_value); }

        bool operator==(const LogicalDevice&) const = default;

        bool operator==(const LogicalDeviceAccessor& other) const noexcept { return _value == other; }

        template<typename Other>
        bool operator==(const LogicalDevice<Other>& other) const noexcept { return _value == other._value; }
        lifetime::OwnerRef owner_ref() const { return _value.owner_ref(); }
        PhysicalDevice get_physical_device() const { return _value.get_physical_device(); }
        DeviceFeature enabled_features() const { return _value.enabled_features(); }
        WeakLogicalDevice weak() const noexcept { return _value.weak(); }
    };

    template<typename C>
    LogicalDevice<C>::LogicalDevice(PhysicalDevice physical, Surface surface) {
        C creator;
        _value = LogicalDeviceAccessor::CheckCreatorResult(physical, creator.create(PhysicalDevice(physical), Surface(surface)));
    }

    template<typename C>
    LogicalDevice<C>::LogicalDevice(PhysicalDevice physical, std::shared_ptr<Surface> surface)
        : LogicalDevice(std::move(physical), LogicalDeviceAccessor::ReadSurface(std::move(surface))) {}
} // namespace VSL_NAMESPACE

template<bool validation>
template<typename S>
VSL_NAMESPACE::PhysicalDevices<S> VSL_NAMESPACE::Vulkan<validation>::devices() {
    return VSL_NAMESPACE::PhysicalDevices<S>(*this);
}

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::PhysicalDevices(VulkanAccessor vulkan)
    : _collection(std::move(vulkan)) {}

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::~PhysicalDevices() = default;

template<typename S>
std::vector<VSL_NAMESPACE::PhysicalDevice> VSL_NAMESPACE::PhysicalDevices<S>::get() {
    return _collection.get();
}

template<typename S>
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::PhysicalDevices<S>::search() {
    if constexpr (std::same_as<S, PhysicalDevicesSercher>) {
        return _collection.select();
    } else {
        auto snapshot = _collection;
        S searcher;
        const auto candidates = std::move(snapshot).get();
        int max_score = -1;
        std::optional<PhysicalDevice> selected;
        for (auto device : candidates) {
            const int score = searcher.benchmark(device);
            if (max_score < score) {
                max_score = score;
                selected = std::move(device);
            }
        }
        if (!selected)
            throw exceptions::RuntimeException("PhysicalDevices", "PhysicalDevices is not suitable.");
        return std::move(*selected);
    }
}
