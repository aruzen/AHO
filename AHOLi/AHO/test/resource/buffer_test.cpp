#include <AHO/resource/buffer.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <optional>
#include <utility>

#ifdef AHO_GPU_TESTS
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/utils/typed_resources.hpp>
#include <functional>
#include "../support/buffer_customization.hpp"
#include "support/vulkan_fixture.hpp"
#else
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define AHO_BUFFER_TEST_ASAN 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define AHO_BUFFER_TEST_ASAN 1
#endif
#ifdef AHO_BUFFER_TEST_ASAN
#include <sanitizer/asan_interface.h>
#endif
#endif

namespace {
    template<typename T>
    concept complete_buffer = requires { sizeof(T); };

    template<typename T>
    concept has_cpu_write = requires(T& b, std::span<const std::byte> bytes) { b.write(bytes); };

    template<typename T>
    concept has_cpu_map = requires(T& b) { b.map(); };

    template<typename T>
    concept has_reserve = requires(T& b) { b.reserve(std::size_t{}); };

    using General = aho::resource::Buffer<aho::resource::BufferType::GENERAL>;
    using Host = aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY>;
    using Device = aho::resource::Buffer<aho::resource::BufferType::ON_GPU>;
    using CustomHost = decltype(aho::resource::make_custom_buffer(std::declval<vsl::StagingBuffer<>>()));
    using CustomDevice = decltype(aho::resource::make_custom_buffer(std::declval<vsl::DeviceLocalBuffer<>>()));
    static_assert(!complete_buffer<aho::resource::Buffer<aho::resource::BufferType::CUSTOM>>);
    static_assert(aho::resource::gpu_buffer<Host> && aho::resource::gpu_buffer<Device> && aho::resource::gpu_buffer<CustomHost>);
    static_assert(!aho::resource::gpu_buffer<General>);
    static_assert(has_cpu_write<Host> && has_cpu_map<Host> && has_reserve<Host>);
    static_assert(!has_cpu_write<Device> && !has_cpu_map<Device> && !has_reserve<Device>);
    static_assert(has_cpu_write<CustomHost> && !has_reserve<CustomHost>);
    static_assert(!has_cpu_write<CustomDevice> && !has_cpu_map<CustomDevice>);
} // namespace

#ifndef AHO_GPU_TESTS
namespace {
    using GeneralBuffer = aho::resource::Buffer<aho::resource::BufferType::GENERAL>;
    class GeneralBufferTest : public testing::TestWithParam<std::size_t> {};

    TEST_P(GeneralBufferTest, ConstructorReportsRequestedSizeAndWritableStorage) {
        const auto size = GetParam();
        GeneralBuffer buffer(size);
        EXPECT_EQ(buffer.size(), size);
        if (size > 0) {
            auto* data = static_cast<unsigned char*>(buffer.data());
            ASSERT_NE(data, nullptr);
            std::memset(data, 0x5a, size);
            EXPECT_EQ(data[0], 0x5a);
            EXPECT_EQ(data[size - 1], 0x5a);
            EXPECT_EQ(buffer.data(), data);
        }
    }

    INSTANTIATE_TEST_SUITE_P(AllocationBoundaries, GeneralBufferTest, testing::Values(std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{257}));
    TEST(GeneralBufferOwnershipTest, OwningAllocationRequiresNontrivialDestruction) {
        EXPECT_FALSE(std::is_trivially_destructible_v<GeneralBuffer>);
    }
} // namespace

namespace {
    TEST(GeneralBufferOwnershipTest, CopiesKeepTheExistingSharedStorageSemantics) {
        GeneralBuffer source(8);
        auto* storage = static_cast<unsigned char*>(source.data());
        storage[0] = 17;
        GeneralBuffer copy(source);
        GeneralBuffer assigned(3);
        assigned = source;
        EXPECT_EQ(copy.data(), source.data());
        EXPECT_EQ(assigned.data(), source.data());
        EXPECT_EQ(copy.size(), 8u);
        EXPECT_EQ(assigned.size(), 8u);
        static_cast<unsigned char*>(copy.data())[0] = 29;
        EXPECT_EQ(storage[0], 29);
        EXPECT_EQ(static_cast<unsigned char*>(assigned.data())[0], 29);
    }

    TEST(GeneralBufferOwnershipTest, SharedStorageOutlivesTheOriginalBuffer) {
        std::optional<GeneralBuffer> copy;
        {
            GeneralBuffer source(4);
            std::memset(source.data(), 0x5a, source.size());
            copy.emplace(source);
        }
        ASSERT_EQ(copy->size(), 4u);
        for (std::size_t i = 0; i < copy->size(); ++i)
            EXPECT_EQ(static_cast<unsigned char*>(copy->data())[i], 0x5a);
    }

    TEST(GeneralBufferOwnershipTest, MovesKeepTheStorageInTheDestination) {
        GeneralBuffer source(8);
        auto* storage = source.data();
        std::memset(storage, 0x6b, 8);
        GeneralBuffer moved(std::move(source));
        EXPECT_EQ(moved.data(), storage);
        EXPECT_EQ(moved.size(), 8u);
        EXPECT_EQ(source.size(), 0u);
        EXPECT_EQ(source.data(), nullptr);
        GeneralBuffer assigned(2);
        assigned = std::move(moved);
        EXPECT_EQ(assigned.data(), storage);
        EXPECT_EQ(moved.size(), 0u);
        EXPECT_EQ(moved.data(), nullptr);
        EXPECT_EQ(assigned.size(), 8u);
        EXPECT_EQ(static_cast<unsigned char*>(assigned.data())[7], 0x6b);
    }
} // namespace
namespace {
    namespace resource = aho::resource;
    TEST(GeneralBufferCapacityTest, ReserveResizeAndAliasesShareStorageWithoutGrowingWithinCapacity) {
        resource::Buffer<resource::BufferType::GENERAL> buffer(4);
        EXPECT_EQ(buffer.size(), 4u);
        buffer.reserve(32);
        auto* storage = buffer.bytes().data();
        auto alias = buffer;
        buffer.resize(8);
        EXPECT_EQ(buffer.bytes().data(), storage);
        EXPECT_EQ(alias.size(), 8u);
        EXPECT_GE(alias.capacity(), 32u);
        const std::array<std::byte, 2> bytes{std::byte{17}, std::byte{29}};

        alias.write(bytes, 2);
        EXPECT_EQ(buffer.bytes()[2], bytes[0]);
        const auto& view = buffer;
        EXPECT_EQ(view.bytes()[3], bytes[1]);
        EXPECT_EQ(view.bytes()[7], std::byte{});
        auto moved = std::move(buffer);
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_TRUE(buffer.bytes().empty());
        EXPECT_EQ(moved.bytes().data(), storage);
    }

    TEST(GeneralBufferCapacityTest, BoundsAndOverlappingWritesPreserveThePayload) {
        resource::Buffer<resource::BufferType::GENERAL> buffer(4);
        const std::array<std::byte, 4> initial{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};

        buffer.write(initial);
        buffer.write(buffer.bytes().first(3), 1);
        EXPECT_EQ(buffer.bytes()[0], std::byte{1});
        EXPECT_EQ(buffer.bytes()[1], std::byte{1});
        EXPECT_EQ(buffer.bytes()[2], std::byte{2});
        EXPECT_EQ(buffer.bytes()[3], std::byte{3});
        EXPECT_THROW(buffer.write(initial, 1), std::out_of_range);
        EXPECT_THROW(buffer.write({}, SIZE_MAX), std::out_of_range);
        EXPECT_NO_THROW(buffer.write({}, buffer.size()));
        buffer.resize(0);
        EXPECT_TRUE(buffer.bytes().empty());
        EXPECT_GE(buffer.capacity(), 4u);
    }
} // namespace
namespace {
    TEST(GeneralBufferCapacityTest, AssignmentFromOwnSubrangeAndConstAccessKeepSharedMetadata) {
        General buffer(8);
        const std::array<std::byte, 4> payload{std::byte{11}, std::byte{22}, std::byte{33}, std::byte{44}};

        buffer.assign(payload);
        auto alias = buffer;
        buffer.assign(buffer.bytes().subspan(1, 2));
        EXPECT_EQ(alias.size(), 2u);
        EXPECT_EQ(alias.bytes()[0], payload[1]);
        EXPECT_EQ(alias.bytes()[1], payload[2]);
        const auto& view = alias;
        EXPECT_EQ(view.data(), buffer.data());
        EXPECT_EQ(view.bytes().size(), 2u);
        auto moved = std::move(buffer);
        EXPECT_THROW(buffer.assign(payload), std::logic_error);
        EXPECT_THROW(buffer.reserve(8), std::logic_error);
        EXPECT_THROW(buffer.resize(8), std::logic_error);
        EXPECT_THROW(buffer.write(payload), std::logic_error);
    }
} // namespace
#ifdef AHO_BUFFER_TEST_ASAN
TEST(GeneralBufferOwnershipTest, LastOwnerDestructionReleasesTheArray) {
    void* storage = nullptr;
    {
        GeneralBuffer source(8);
        storage = source.data();
        {
            GeneralBuffer copy(source);
            EXPECT_EQ(__asan_address_is_poisoned(storage), 0);
        }
        EXPECT_EQ(__asan_address_is_poisoned(storage), 0);
    }
    EXPECT_NE(__asan_address_is_poisoned(storage), 0);
}
#undef AHO_BUFFER_TEST_ASAN
#endif

#else
namespace {
    using aho_test::CustomCounters;
    using aho_test::NativeHost;
    using aho_test::CountingCustomization;

    struct MoveOnlyCustomization {
        std::unique_ptr<int> value = std::make_unique<int>(7);
        MoveOnlyCustomization() = default;
        MoveOnlyCustomization(MoveOnlyCustomization&&) = default;
        MoveOnlyCustomization(const MoveOnlyCustomization&) = delete;
    };

    struct ObservedStorage : NativeHost {
        std::shared_ptr<std::function<void()>> released;
        ObservedStorage(vsl::LogicalDeviceAccessor device, size_t bytes, std::shared_ptr<std::function<void()>> released)
            : NativeHost(device, bytes), released(std::move(released)) {}
        ObservedStorage(ObservedStorage&& other) noexcept
            : NativeHost(std::move(other)), released(std::move(other.released)) { other.data_.reset(); }
        ~ObservedStorage() override {
            if (data_ && released && *released)
                (*released)();
        }
    };
    struct ObservedCustomization {
        vsl::LogicalDeviceAccessor device;
        std::shared_ptr<std::function<void()>> released;
        std::function<void()> preparing;
        ObservedStorage reallocate(const ObservedStorage&, size_t bytes) {
            if (preparing)
                preparing();
            return {device, bytes, released};
        }
    };

    class GPUFriendlyBufferVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool GPUFriendlyBufferVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, LogicalLengthAndReservedCapacityAreIndependent) {
        aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY> buffer(device);
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_EQ(buffer.capacity(), 0u);
        EXPECT_FALSE(buffer.busy());
        EXPECT_TRUE(buffer.wait(0));
        buffer.reserve(64);
        const std::array<std::byte, 8> payload{};

        buffer.assign(payload);
        EXPECT_EQ(buffer.size(), 8u);
        EXPECT_EQ(buffer.capacity(), 64u);
        auto alias = buffer;
        alias.assign(std::span(payload).first(4));
        EXPECT_EQ(buffer.size(), 4u);
        EXPECT_EQ(buffer.capacity(), 64u);
        buffer.reserve(128);
        EXPECT_EQ(alias.capacity(), 128u);
        EXPECT_EQ(alias.size(), 4u);
        buffer.write({});
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, InvalidWritesDoNotChangeSizeAndEmptyAssignmentKeepsCapacity) {
        aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY> buffer(device, 16);
        std::array<std::byte, 4> payload{};

        buffer.assign(payload);
        EXPECT_THROW(buffer.write(payload, 1), std::out_of_range);
        EXPECT_THROW(buffer.write({}, SIZE_MAX), std::out_of_range);
        EXPECT_NO_THROW(buffer.write({}, 4));
        EXPECT_EQ(buffer.size(), 4u);
        buffer.assign({});
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_EQ(buffer.capacity(), 16u);
        EXPECT_THROW((aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY>(vsl::LogicalDeviceAccessor{})), std::invalid_argument);
    }
} // namespace
namespace {
    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, OldGenerationCallbacksSeeTheCommittedOwnerSizeAndContents) {
        for (const auto operation : {0, 1, 2}) {
            auto released = std::make_shared<std::function<void()>>();
            auto buffer = aho::resource::make_custom_buffer(ObservedStorage(device, 4, released), ObservedCustomization{device, released, {}});
            const std::uint32_t value = 37;
            buffer.write(std::as_bytes(std::span(&value, 1)));
            const std::array<std::uint32_t, 2> assigned{41, 43};
            const auto old = buffer.binding().native().data_.get();
            auto weak = buffer.binding().owner_ref().weak();
            size_t calls = 0;
            *released = [&] {
                ++calls;
                const auto snapshot = buffer.binding().snapshot();
                EXPECT_NE(snapshot.storage().ref().data, old);
                EXPECT_TRUE(snapshot.constraints().owner.managed());
                EXPECT_TRUE(buffer.binding().owner_ref().managed());
                EXPECT_EQ(snapshot.size(), operation ? 8u : 4u);
                std::uint32_t actual = 0;
                buffer.read_into(std::as_writable_bytes(std::span(&actual, 1)));
                EXPECT_EQ(actual, operation == 2 ? assigned[0] : value);
            };
            if (operation == 0)
                buffer.reserve(8);
            else if (operation == 1)
                buffer.resize(8);
            else
                buffer.assign(std::as_bytes(std::span(assigned)));
            EXPECT_EQ(calls, 1u);
            EXPECT_TRUE(weak.expired());
            *released = {};
        }
    }
    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, CustomPreparationCannotReenterTheSameLogicalBufferMutation) {
        auto released = std::make_shared<std::function<void()>>();
        std::function<void()> reenter;
        auto buffer = aho::resource::make_custom_buffer(ObservedStorage(device, 4, released),
                                                        ObservedCustomization{device, released, [&] { reenter(); }});
        const std::uint32_t value = 17;
        buffer.write(std::as_bytes(std::span(&value, 1)));
        const auto old = buffer.binding().snapshot();
        reenter = [&] {
            const auto current = buffer.binding().snapshot();
            EXPECT_EQ(current.storage().identity(), old.storage().identity());
            buffer.resize(0);
        };
        EXPECT_THROW(buffer.reserve(8), std::logic_error);
        EXPECT_EQ(buffer.size(), 4u);
        EXPECT_EQ(buffer.capacity(), 4u);
        const auto after = buffer.binding().snapshot();
        EXPECT_EQ(after.storage().identity(), old.storage().identity());
        reenter = [] {};
        EXPECT_NO_THROW(buffer.reserve(8));
        EXPECT_EQ(buffer.read(4)[0], std::as_bytes(std::span(&value, 1))[0]);
    }
    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, FailedCandidateAssignmentKeepsTheOriginalGenerationAndData) {
        struct Customization {
            vsl::LogicalDeviceAccessor device;
            std::shared_ptr<bool> fail;
            struct Mapping {
                NativeHost::LocalBufferHolder mapped;
                std::shared_ptr<bool> fail;
                auto read(size_t bytes, size_t offset = 0) { return mapped.read(bytes, offset); }
                void write(std::span<const std::byte> bytes, size_t offset) {
                    if (*fail)
                        throw std::runtime_error("Injected candidate write failure");
                    mapped.write(bytes, offset);
                }
            };
            Mapping map(NativeHost& storage) { return {storage.data(), fail}; }
            NativeHost reallocate(const NativeHost&, size_t bytes) { return {device, bytes}; }
        };
        auto fail = std::make_shared<bool>(false);
        auto buffer = aho::resource::make_custom_buffer(NativeHost(device, 4), Customization{device, fail});
        const std::array<std::uint32_t, 2> values{17, 31};
        buffer.write(std::as_bytes(std::span(values).first(1)));
        const auto old = buffer.binding().snapshot();
        *fail = true;
        EXPECT_THROW(buffer.assign(std::as_bytes(std::span(values))), std::runtime_error);
        EXPECT_EQ(buffer.capacity(), 4u);
        EXPECT_EQ(buffer.size(), 4u);
        const auto after = buffer.binding().snapshot();
        EXPECT_EQ(after.storage().identity(), old.storage().identity());
        EXPECT_EQ(buffer.binding().owner_ref().get(), old.constraints().owner.get());
        *fail = false;
        std::uint32_t actual = 0;
        buffer.read_into(std::as_writable_bytes(std::span(&actual, 1)));
        EXPECT_EQ(actual, values[0]);
        EXPECT_NO_THROW(buffer.assign(std::as_bytes(std::span(values))));
        EXPECT_EQ(buffer.size(), 8u);
    }
    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, GenerationStorageDependencyRejectsAnIndirectDescriptorCycle) {
        namespace t = vsl::typed;
        namespace l = vsl::lifetime;
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        using enum vsl::storage::ResourceKind;
        constexpr std::array<t::ResourceRequest, 4> requests{{{{1}, BUFFER}, {{2}, DESCRIPTOR_LAYOUT}, {{3}, DESCRIPTOR_POOL}, {{4}, DESCRIPTOR_SET}}};
        const t::ResourcePlan<4> plan(requests);
        const vsl::DescriptorLayoutCreateInfo layout{{Set::POINTS.begin(), Set::POINTS.end()}};
        const vsl::DescriptorPoolCreateInfo pool{{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}}, 1};
        const std::array<l::ResourceInit, 4> initializers{vsl::BufferCreateInfo{4, aho::resource::detail::STANDARD_BUFFER_USAGE}, layout, pool, l::DescriptorSetInit{{3}, {2}}};
        l::ResourceArena arena(device, plan, initializers, l::DependencyPolicy::DYNAMIC);
        auto storage = NativeHost(device, 4);
        storage.data_ = arena.buffer({1}).legacy_owner();
        auto buffer = aho::resource::make_custom_buffer(std::move(storage));
        const auto snapshot = buffer.binding().snapshot();
        EXPECT_TRUE(snapshot.constraints().owner.managed());
        EXPECT_FALSE(snapshot.constraints().state.managed());
        auto set = t::TypedDescriptorSet<Set>::FromResource(device, arena.descriptor_set({4}));
        set.write<0>(View::FromView(arena.buffer({1}), 1));
        Host independent(device, 4);
        // 物理入力は別owner。論理制約のR→arenaだけで循環を検出する。
        EXPECT_THROW(set.write<0>(View::FromView(vsl::BufferView::FromBuffer(independent.native()), 1, 0, snapshot.constraints())), std::invalid_argument);
        EXPECT_NO_THROW(set.write<0>(View::FromView(vsl::BufferView::FromBuffer(independent.native()), 1)));
    }
    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, PreparationMayReleaseTheFacadeWhileTheTransactionKeepsItsStateAlive) {
        for (const auto operation : {0, 1, 2}) {
            auto released = std::make_shared<std::function<void()>>();
            std::function<void()> preparing;
            using Buffer = decltype(aho::resource::make_custom_buffer(std::declval<ObservedStorage>(), std::declval<ObservedCustomization>()));
            std::optional<Buffer> buffer;
            buffer.emplace(ObservedStorage(device, 4, released), ObservedCustomization{device, released, [&] { preparing(); }});
            const auto weak = buffer->binding().owner_ref().weak();
            preparing = [&] { buffer.reset(); };
            const std::array<std::byte, 8> bytes{};
            if (operation == 0)
                EXPECT_NO_THROW(buffer->reserve(8));
            else if (operation == 1)
                EXPECT_NO_THROW(buffer->resize(8));
            else
                EXPECT_NO_THROW(buffer->assign(bytes));
            EXPECT_FALSE(buffer);
            EXPECT_TRUE(weak.expired());
        }
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, ConstructorsExposeStandardUsageAndReuseTheirMapping) {
        Host source(device, 8);
        EXPECT_EQ(source.size(), 8u);
        EXPECT_EQ(source.capacity(), 8u);
        EXPECT_EQ(source.native().get_device(), device);
        EXPECT_TRUE(vsl::contain(source.native().get_usage(), vsl::MemoryType::VERTEX_BUFFER | vsl::MemoryType::UNIFORM_BUFFER));
        EXPECT_TRUE(vsl::contain(source.native().get_memory_properties(), vsl::MemoryProperty::HOST_VISIBLE));
        std::array<std::byte, 8> expected{};
        expected[3] = std::byte{71};

        source.write(expected);
        const auto* address = source.read(8).data();
        auto alias = source;
        alias.resize(4);
        EXPECT_EQ(source.size(), 4u);
        EXPECT_EQ(alias.read(4).data(), address);
        source.reserve(32);
        EXPECT_EQ(alias.capacity(), 32u);
        EXPECT_EQ(alias.read(4)[3], std::byte{71});
        EXPECT_EQ(source.map().data, source.read(4).data());
        const auto& view = source;
        EXPECT_EQ(view.native().requested_size(), 32u);
        auto moved = std::move(source);
        EXPECT_EQ(source.size(), 0u);
        EXPECT_THROW(source.binding(), std::logic_error);
        EXPECT_THROW(source.read(0), std::logic_error);
        EXPECT_THROW(source.wait(0), std::logic_error);
        EXPECT_THROW(source.native(), std::logic_error);
        EXPECT_THROW(moved.read(1, 4), std::out_of_range);
        EXPECT_TRUE(moved.read(0, 4).empty());
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, CustomMappingAndReallocationHooksPreserveDataAndSharedState) {
        auto counters = std::make_shared<CustomCounters>();
        auto custom = aho::resource::make_custom_buffer(NativeHost(device, 4), CountingCustomization{device, counters});
        const std::array<std::byte, 4> expected{std::byte{5}, std::byte{13}, std::byte{29}, std::byte{71}};

        custom.write(expected);
        auto alias = custom;
        custom.reserve(16);
        EXPECT_EQ(counters->maps, 2u);
        EXPECT_EQ(counters->allocations, 1u);
        EXPECT_EQ(alias.capacity(), 16u);
        EXPECT_EQ(alias.size(), 4u);
        EXPECT_EQ(std::memcmp(alias.read(4).data(), expected.data(), 4), 0);
        alias.resize(8);
        EXPECT_EQ(custom.size(), 8u);
        auto move_only = aho::resource::make_custom_buffer(NativeHost(device, 4), MoveOnlyCustomization{});
        move_only.assign(expected);
        EXPECT_EQ(move_only.read(1)[0], expected[0]);
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, InvalidCustomReallocationLeavesTheOriginalStorageIntact) {
        struct TooSmallAllocation {
            vsl::LogicalDeviceAccessor device;
            NativeHost reallocate(const NativeHost&, std::size_t) { return NativeHost(device, 1); }
        };

        auto custom = aho::resource::make_custom_buffer(NativeHost(device, 4), TooSmallAllocation{device});
        const std::array<std::byte, 4> payload{std::byte{11}, std::byte{22}, std::byte{33}, std::byte{44}};

        custom.write(payload);
        const auto owner = custom.binding().owner();
        EXPECT_THROW(custom.reserve(8), std::invalid_argument);
        EXPECT_EQ(custom.binding().owner(), owner);
        EXPECT_EQ(custom.capacity(), 4u);
        EXPECT_EQ(std::memcmp(custom.read(4).data(), payload.data(), 4), 0);
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, CustomizationContextOutlivesTheWrappedStorageDestructor) {
        struct ContextStorage : NativeHost {
            std::weak_ptr<int> context;
            std::shared_ptr<bool> destroyed_without_context;
            ContextStorage(vsl::LogicalDeviceAccessor device, const std::shared_ptr<int>& context, std::shared_ptr<bool> result)
                : NativeHost(device, 4), context(context), destroyed_without_context(std::move(result)) {}
            ContextStorage(ContextStorage&&) = default;
            ContextStorage(const ContextStorage&) = delete;
            ~ContextStorage() override {
                if (data_ && context.expired())
                    *destroyed_without_context = true;
            }
        };

        struct ContextCustomization {
            std::shared_ptr<int> context;
        };

        auto result = std::make_shared<bool>(false);
        {
            auto context = std::make_shared<int>(7);
            auto custom = aho::resource::make_custom_buffer(ContextStorage(device, context, result), ContextCustomization{context});
            context.reset();
        }
        EXPECT_FALSE(*result);
    }

} // namespace
#endif
