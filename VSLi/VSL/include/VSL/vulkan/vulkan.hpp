#pragma once
#include "../define.hpp"

#include "../utils/string.hpp"
#include <vector>
#include <any>

#include "../test.hpp"
#include "pv.hpp"
#include "../operation/instance_create_info.hpp"
#include "../lifetime/owner_ref.hpp"

#include <memory>
#include <concepts>
#include <utility>

namespace VSL_NAMESPACE {
    struct PhysicalDevice;
    struct PhysicalDevicesSercher;
    namespace lifetime::detail {
        struct StartupAccess;
    }

    template<typename S = VSL_NAMESPACE::PhysicalDevicesSercher>
    struct PhysicalDevices;

    static std::vector<const char*> VALIDATION_EXTENSIONS = {
        "VK_EXT_debug_utils",
        // "VK_EXT_layer_settings"
    };

    static std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"};

    struct VulkanAccessor {
    private:
        lifetime::OwnedRef<impl::VulkanImplAccessor> _owner;
        explicit VulkanAccessor(lifetime::OwnedRef<impl::VulkanImplAccessor> owner) noexcept : _owner(std::move(owner)) {}
        friend struct lifetime::detail::StartupAccess;

    public:
        VulkanAccessor() = default;
        VulkanAccessor(const VulkanAccessor&) = default;
        VulkanAccessor(VulkanAccessor&&) noexcept = default;
        VulkanAccessor& operator=(VulkanAccessor value) noexcept {
            swap(value);
            return *this;
        }
        void swap(VulkanAccessor& other) noexcept { _owner.swap(other._owner); }
        explicit operator bool() const noexcept { return bool(_owner); }
        static VulkanAccessor Create(InstanceCreateInfo info);
        lifetime::OwnerRef owner_ref() const;
    };

    template<bool validation = VSL_NAMESPACE::VALIDATION>
    struct Vulkan {
    private:
        VulkanAccessor _value;
        friend struct lifetime::detail::StartupAccess;

    public:
        bool check_validation_layer_support();

        Vulkan(const char* app_name, const std::vector<const char*>& require_extensions = {});
        // 移行用。配置済みInstanceを再生成せず旧facadeへ接続する。
        explicit Vulkan(VulkanAccessor instance);
        Vulkan(const Vulkan&) = default;
        Vulkan(Vulkan&&) noexcept = default;
        Vulkan& operator=(Vulkan value) noexcept {
            swap(value);
            return *this;
        }
        void swap(Vulkan& other) noexcept { _value.swap(other._value); }
        operator VulkanAccessor() const& { return _value; }
        operator VulkanAccessor() && { return std::move(_value); }
        explicit operator bool() const noexcept { return bool(_value); }
        lifetime::OwnerRef owner_ref() const { return _value.owner_ref(); }
        ~Vulkan();

        template<typename S = VSL_NAMESPACE::PhysicalDevicesSercher>
        PhysicalDevices<S> devices();

#ifdef VSL_NAMESPACE_TEST
        void test();
#endif // VSL_NAMESPACE_TEST
    };
} // namespace VSL_NAMESPACE
