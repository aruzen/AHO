#include <VSL/lifetime/instance_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/storage/resource_layout.hpp>
#include <VSL/vulkan/vulkan.hpp>
#include <gtest/gtest.h>
#include <bit>
#include <cstring>
#include <atomic>
#include <cstdio>

namespace {
    namespace l = vsl::lifetime;
    namespace s = vsl::storage;
    namespace o = vsl::operation;
    enum class Event {
        CREATE_INSTANCE,
        CREATE_MESSENGER,
        DESTROY_MESSENGER,
        DESTROY_INSTANCE
    };
    struct NativeProbe {
        std::vector<Event> events;
        std::atomic<size_t> validation_errors = 0;
        bool fail_instance = false, fail_messenger = false;
        NativeProbe() { events.reserve(8); }
        ~NativeProbe() { EXPECT_EQ(validation_errors.load(), 0u); }
        o::InstanceNativeCalls calls();
        static VkResult CreateInstance(void* context, const VkInstanceCreateInfo* info, VkInstance* instance);
        static VkResult CreateMessenger(void* context, VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* info, VkDebugUtilsMessengerEXT* messenger);
        static void DestroyInstance(void* context, VkInstance instance) noexcept;
        static void DestroyMessenger(void* context, VkInstance instance, VkDebugUtilsMessengerEXT messenger) noexcept;
        static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                                                                 const VkDebugUtilsMessengerCallbackDataEXT* data, void* context) noexcept;
    };
    struct SlotOwner {
        alignas(l::InstanceSlotStorage) std::byte memory[sizeof(l::InstanceSlotStorage)];
        bool constructed = false;
        ~SlotOwner();
        void construct(vsl::InstanceCreateInfo info, const o::InstanceNativeCalls& calls = {});
        vsl::impl::VulkanImplAccessor& value() { return l::instance_at(memory); }
    };
    struct PairOwner {
        l::DependencyNode node;
        alignas(l::InstanceSlotStorage) std::byte memory[2 * sizeof(l::InstanceSlotStorage)];
        size_t constructed = 0;
        explicit PairOwner(vsl::InstanceCreateInfo info);
        ~PairOwner();
        void clear() noexcept;
        void* slot(size_t index) { return memory + index * sizeof(l::InstanceSlotStorage); }
    };
    class InstanceStorageVulkanIntegrationTest : public testing::Test {
    protected:
        void SetUp() override;
        vsl::InstanceCreateInfo info(bool validation = false) const;
        bool validation_available() const;
    };

    o::InstanceNativeCalls NativeProbe::calls() {
        return {this, CreateInstance, CreateMessenger, DestroyInstance, DestroyMessenger};
    }
    VkResult NativeProbe::CreateInstance(void* context, const VkInstanceCreateInfo* info, VkInstance* instance) {
        auto& probe = *static_cast<NativeProbe*>(context);
        probe.events.push_back(Event::CREATE_INSTANCE);
        if (probe.fail_instance) {
            *instance = reinterpret_cast<VkInstance>(std::uintptr_t{1});
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        auto actual = *info;
        VkDebugUtilsMessengerCreateInfoEXT debug{};
        if (info->pNext && static_cast<const VkBaseInStructure*>(info->pNext)->sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT) {
            debug = *static_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(info->pNext);
            debug.pfnUserCallback = ValidationCallback;
            debug.pUserData = context;
            actual.pNext = &debug;
        }
        return vkCreateInstance(&actual, nullptr, instance);
    }
    VkResult NativeProbe::CreateMessenger(void* context, VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* info, VkDebugUtilsMessengerEXT* messenger) {
        auto& probe = *static_cast<NativeProbe*>(context);
        probe.events.push_back(Event::CREATE_MESSENGER);
        if (probe.fail_messenger) {
            *messenger = std::bit_cast<VkDebugUtilsMessengerEXT>(std::uint64_t{1});
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        auto actual = *info;
        actual.pfnUserCallback = ValidationCallback;
        actual.pUserData = context;
        return create(instance, &actual, nullptr, messenger);
    }
    void NativeProbe::DestroyInstance(void* context, VkInstance instance) noexcept {
        static_cast<NativeProbe*>(context)->events.push_back(Event::DESTROY_INSTANCE);
        vkDestroyInstance(instance, nullptr);
    }
    void NativeProbe::DestroyMessenger(void* context, VkInstance instance, VkDebugUtilsMessengerEXT messenger) noexcept {
        static_cast<NativeProbe*>(context)->events.push_back(Event::DESTROY_MESSENGER);
        const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        destroy(instance, messenger, nullptr);
    }
    VKAPI_ATTR VkBool32 VKAPI_CALL NativeProbe::ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                                                                   const VkDebugUtilsMessengerCallbackDataEXT* data, void* context) noexcept {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            ++static_cast<NativeProbe*>(context)->validation_errors;
            std::fprintf(stderr, "%s\n", data && data->pMessage ? data->pMessage : "Instance validation error");
        }
        return VK_FALSE;
    }
    SlotOwner::~SlotOwner() {
        if (constructed)
            l::destroy_instance(memory);
    }
    void SlotOwner::construct(vsl::InstanceCreateInfo info, const o::InstanceNativeCalls& calls) {
        if (constructed)
            throw std::logic_error("Test slot is already constructed");
        l::construct_instance(memory, std::move(info), calls);
        constructed = true;
    }
    PairOwner::PairOwner(vsl::InstanceCreateInfo info) {
        try {
            for (; constructed < 2; ++constructed)
                l::construct_instance(slot(constructed), info);
        } catch (...) {
            clear();
            throw;
        }
    }
    PairOwner::~PairOwner() {
        node.retire();
        clear();
    }
    void PairOwner::clear() noexcept {
        while (constructed)
            l::destroy_instance(slot(--constructed));
    }
    vsl::InstanceCreateInfo InstanceStorageVulkanIntegrationTest::info(bool validation) const {
        vsl::InstanceCreateInfo result;
        result.application_name = "instance placement test";
        result.api_version = VK_API_VERSION_1_0;
        result.validation = validation;
        for (const auto& extension : vk::enumerateInstanceExtensionProperties())
            if (std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
                result.extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        return result;
    }
    bool InstanceStorageVulkanIntegrationTest::validation_available() const {
        const std::array<std::string, 1> layers{"VK_LAYER_KHRONOS_validation"};
        if (!o::instance_layers_supported(layers))
            return false;
        auto extensions = vk::enumerateInstanceExtensionProperties();
        const auto extra = vk::enumerateInstanceExtensionProperties(layers.front());
        extensions.insert(extensions.end(), extra.begin(), extra.end());
        return std::ranges::any_of(extensions, [](const auto& extension) { return std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0; });
    }
    void InstanceStorageVulkanIntegrationTest::SetUp() {
        try {
            (void)vsl::VulkanAccessor::Create(info());
        } catch (const vk::IncompatibleDriverError& error) {
            GTEST_SKIP() << "Vulkan driver unavailable: " << error.what();
        }
    }
    TEST(InstancePlacementTest, RuntimeAliasesShareOneExactNonPolymorphicSlotLayout) {
        static_assert(std::same_as<vsl::impl::VulkanImpl<true>, vsl::impl::VulkanImpl<false>>);
        static_assert(!std::is_polymorphic_v<vsl::impl::VulkanImplAccessor>);
        static_assert(!std::is_copy_constructible_v<vsl::impl::VulkanImplAccessor> && !std::is_move_constructible_v<vsl::impl::VulkanImplAccessor>);
        static_assert(sizeof(vsl::impl::VulkanImplAccessor) == s::INSTANCE_IMPL_SIZE && alignof(vsl::impl::VulkanImplAccessor) == s::INSTANCE_IMPL_ALIGNMENT);
        static_assert(sizeof(l::InstanceSlotStorage) == s::INSTANCE_SLOT_SIZE && alignof(l::InstanceSlotStorage) == s::INSTANCE_SLOT_ALIGNMENT);
        static_assert(s::ResourceSlotLayout<vsl::impl::VulkanImplAccessor>::USE_OFFSET == s::NO_SEPARATE_USE);
        EXPECT_EQ(s::INSTANCE_SLOT_SIZE, s::checked_align(s::INSTANCE_IMPL_SIZE, s::INSTANCE_SLOT_ALIGNMENT));
        EXPECT_THROW(vsl::VulkanAccessor{}.owner_ref(), std::invalid_argument);
    }
    TEST_F(InstanceStorageVulkanIntegrationTest, HeadlessHeapCreationKeepsItsCanonicalOwnerAfterTheOriginalFacadeIsGone) {
        std::weak_ptr<void> owner;
        vsl::VulkanAccessor saved;
        {
            auto configuration = info();
            auto original = vsl::VulkanAccessor::Create(configuration);
            owner = original.accessor;
            saved = original;
            configuration.application_name.clear();
            configuration.extensions.clear();
            EXPECT_TRUE(original.owner_ref().managed());
            EXPECT_FALSE(original.accessor->debug_messenger);
        }
        EXPECT_FALSE(owner.expired());
        EXPECT_NO_THROW((void)saved.accessor->instance.enumeratePhysicalDevices());
        saved = {};
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(InstanceStorageVulkanIntegrationTest, TwoSameTypeSlotsUseDirectOffsetsAndOneExternalOwner) {
        auto owner = std::make_shared<PairOwner>(info());
        owner->node.publish(owner);
        auto& first = l::instance_at(owner->slot(0));
        auto& second = l::instance_at(owner->slot(1));
        first.owner_node = second.owner_node = &owner->node;
        vsl::VulkanAccessor a{{owner, &first}}, b{{owner, &second}};
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&second) - reinterpret_cast<std::uintptr_t>(&first), s::INSTANCE_SLOT_SIZE);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&first) % s::INSTANCE_SLOT_ALIGNMENT, 0u);
        EXPECT_NE(first.instance, second.instance);
        EXPECT_FALSE(a.accessor.owner_before(b.accessor));
        EXPECT_FALSE(b.accessor.owner_before(a.accessor));
        EXPECT_TRUE(a.owner_ref().managed());
        EXPECT_TRUE(b.owner_ref().managed());
        std::weak_ptr<void> weak = owner;
        owner.reset();
        a = {};
        EXPECT_FALSE(weak.expired());
        EXPECT_NO_THROW((void)b.accessor->instance.enumeratePhysicalDevices());
        b = {};
        EXPECT_TRUE(weak.expired());
    }
    TEST_F(InstanceStorageVulkanIntegrationTest, ValidationCreationAndExplicitDestructionUseNativeDependencyOrderExactlyOnce) {
        if (!validation_available())
            GTEST_SKIP() << "VK_LAYER_KHRONOS_validation is unavailable";
        NativeProbe probe;
        {
            SlotOwner slot;
            slot.construct(info(true), probe.calls());
            EXPECT_TRUE(slot.value().instance);
            EXPECT_TRUE(slot.value().debug_messenger);
            o::destroy_instance(slot.value(), probe.calls());
            EXPECT_FALSE(slot.value().instance);
            EXPECT_FALSE(slot.value().debug_messenger);
            EXPECT_FALSE(slot.value().destroy_debug_messenger);
            o::destroy_instance(slot.value(), probe.calls());
        }
        EXPECT_EQ(probe.events, (std::vector<Event>{Event::CREATE_INSTANCE, Event::CREATE_MESSENGER, Event::DESTROY_MESSENGER, Event::DESTROY_INSTANCE}));
    }
    TEST_F(InstanceStorageVulkanIntegrationTest, FailedInstanceOutputIsNotPublishedAndTheSameCpuSlotCanBeRetried) {
        NativeProbe probe;
        probe.fail_instance = true;
        SlotOwner slot;
        EXPECT_THROW(slot.construct(info(), probe.calls()), vk::OutOfHostMemoryError);
        EXPECT_FALSE(slot.constructed);
        EXPECT_EQ(probe.events, (std::vector<Event>{Event::CREATE_INSTANCE}));
        probe.fail_instance = false;
        slot.construct(info(), probe.calls());
        EXPECT_TRUE(slot.value().instance);
        o::destroy_instance(slot.value(), probe.calls());
        EXPECT_EQ(probe.events, (std::vector<Event>{Event::CREATE_INSTANCE, Event::CREATE_INSTANCE, Event::DESTROY_INSTANCE}));
    }
    TEST_F(InstanceStorageVulkanIntegrationTest, FailedMessengerOutputIsIgnoredAndRollsBackTheRealInstanceBeforeRetry) {
        if (!validation_available())
            GTEST_SKIP() << "VK_LAYER_KHRONOS_validation is unavailable";
        NativeProbe probe;
        probe.fail_messenger = true;
        SlotOwner slot;
        EXPECT_THROW(slot.construct(info(true), probe.calls()), vk::OutOfHostMemoryError);
        EXPECT_FALSE(slot.constructed);
        EXPECT_EQ(probe.events, (std::vector<Event>{Event::CREATE_INSTANCE, Event::CREATE_MESSENGER, Event::DESTROY_INSTANCE}));
        probe.fail_messenger = false;
        slot.construct(info(true), probe.calls());
        EXPECT_TRUE(slot.value().debug_messenger);
        o::destroy_instance(slot.value(), probe.calls());
        EXPECT_EQ(probe.events, (std::vector<Event>{Event::CREATE_INSTANCE, Event::CREATE_MESSENGER, Event::DESTROY_INSTANCE, Event::CREATE_INSTANCE, Event::CREATE_MESSENGER, Event::DESTROY_MESSENGER, Event::DESTROY_INSTANCE}));
    }
    TEST_F(InstanceStorageVulkanIntegrationTest, InvalidInputIsRejectedBeforeNativeCreationWithoutClaimingTheCpuSlot) {
        std::vector<vsl::InstanceCreateInfo> invalid;
        for (const auto& name : {std::string{}, std::string("bad\0extension", 13), std::string("VK_AHO_extension_that_does_not_exist")}) {
            auto value = info();
            value.extensions.push_back(name);
            invalid.push_back(std::move(value));
        }
        auto layer = info();
        layer.layers.emplace_back("VK_LAYER_AHO_missing");
        invalid.push_back(std::move(layer));
        auto printf_without_validation = info();
        printf_without_validation.debug_printf = true;
        invalid.push_back(std::move(printf_without_validation));
        NativeProbe probe;
        SlotOwner slot;
        for (const auto& value : invalid) {
            EXPECT_THROW(slot.construct(value, probe.calls()), std::invalid_argument);
            EXPECT_FALSE(slot.constructed);
        }
        EXPECT_TRUE(probe.events.empty());
        EXPECT_NO_THROW(slot.construct(info(), probe.calls()));
    }
    TEST_F(InstanceStorageVulkanIntegrationTest, DuplicateExtensionsAndLayersAreNormalizedAndOwnedDuringCreation) {
        if (!validation_available())
            GTEST_SKIP() << "VK_LAYER_KHRONOS_validation is unavailable";
        auto configuration = info(true);
        configuration.extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        configuration.extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        configuration.layers = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_validation"};
        auto accessor = vsl::VulkanAccessor::Create(configuration);
        EXPECT_TRUE(accessor.accessor->debug_messenger);
        configuration = {};
        EXPECT_NO_THROW((void)accessor.accessor->instance.enumeratePhysicalDevices());
    }
    TEST_F(InstanceStorageVulkanIntegrationTest, ReinitializationAndForeignControlBlocksCannotReplaceALiveInstance) {
        auto accessor = vsl::VulkanAccessor::Create(info());
        const auto handle = accessor.accessor->instance;
        NativeProbe probe;
        EXPECT_THROW(o::initialize_instance(*accessor.accessor, info(), probe.calls()), std::logic_error);
        EXPECT_TRUE(probe.events.empty());
        EXPECT_EQ(accessor.accessor->instance, handle);
        const vsl::VulkanAccessor forged{{std::make_shared<int>(0), accessor.accessor.get()}};
        const vsl::VulkanAccessor borrowed{{std::shared_ptr<void>{}, accessor.accessor.get()}};
        EXPECT_THROW(forged.owner_ref(), std::invalid_argument);
        EXPECT_THROW(borrowed.owner_ref(), std::invalid_argument);
    }
} // namespace
