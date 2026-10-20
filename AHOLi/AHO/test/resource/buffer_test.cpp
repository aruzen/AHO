#ifndef AHO_GPU_TESTS
#include <gtest/gtest.h>
#include <AHO/resource/buffer.hpp>
#include <cstring>
#include <optional>
#include <utility>

namespace {
    using GeneralBuffer = aho::resource::Buffer<aho::resource::BufferType::General>;
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
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define AHO_BUFFER_TEST_ASAN 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define AHO_BUFFER_TEST_ASAN 1
#endif
#ifdef AHO_BUFFER_TEST_ASAN
#include <sanitizer/asan_interface.h>
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

#endif

#include <AHO/resource/buffer.hpp>
#include <gtest/gtest.h>
#include <array>
#include <utility>

#ifndef AHO_GPU_TESTS
namespace {
    namespace resource = aho::resource;
    TEST(GeneralBufferCapacityTest, ReserveResizeAndAliasesShareStorageWithoutGrowingWithinCapacity) {
        resource::Buffer<resource::BufferType::General> buffer(4);
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
        resource::Buffer<resource::BufferType::General> buffer(4);
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
#else
#include "support/vulkan_fixture.hpp"
namespace {
    class GpuFriendlyBufferVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, LogicalLengthAndReservedCapacityAreIndependent) {
        aho::resource::Buffer<aho::resource::BufferType::GPUFriendly> buffer(device);
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
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, InvalidWritesDoNotChangeSizeAndEmptyAssignmentKeepsCapacity) {
        aho::resource::Buffer<aho::resource::BufferType::GPUFriendly> buffer(device, 16);
        std::array<std::byte, 4> payload{};
        buffer.assign(payload);
        EXPECT_THROW(buffer.write(payload, 1), std::out_of_range);
        EXPECT_THROW(buffer.write({}, SIZE_MAX), std::out_of_range);
        EXPECT_NO_THROW(buffer.write({}, 4));
        EXPECT_EQ(buffer.size(), 4u);
        buffer.assign({});
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_EQ(buffer.capacity(), 16u);
        EXPECT_THROW((aho::resource::Buffer<aho::resource::BufferType::GPUFriendly>(vsl::LogicalDeviceAccessor{})), std::invalid_argument);
    }
} // namespace
#endif

namespace {
    template<typename T>
    concept CompleteBuffer = requires { sizeof(T); };
    template<typename T>
    concept HasCpuWrite = requires(T& b, std::span<const std::byte> bytes) { b.write(bytes); };
    template<typename T>
    concept HasCpuMap = requires(T& b) { b.map(); };
    template<typename T>
    concept HasReserve = requires(T& b) { b.reserve(std::size_t{}); };
    using General = aho::resource::Buffer<aho::resource::BufferType::General>;
    using Host = aho::resource::Buffer<aho::resource::BufferType::GPUFriendly>;
    using Device = aho::resource::Buffer<aho::resource::BufferType::OnGPU>;
    using CustomHost = decltype(aho::resource::make_custom_buffer(std::declval<vsl::StagingBuffer<>>()));
    using CustomDevice = decltype(aho::resource::make_custom_buffer(std::declval<vsl::DeviceLocalBuffer<>>()));
    static_assert(!CompleteBuffer<aho::resource::Buffer<aho::resource::BufferType::Custom>>);
    static_assert(aho::resource::GpuBuffer<Host> && aho::resource::GpuBuffer<Device> && aho::resource::GpuBuffer<CustomHost>);
    static_assert(!aho::resource::GpuBuffer<General>);
    static_assert(HasCpuWrite<Host> && HasCpuMap<Host> && HasReserve<Host>);
    static_assert(!HasCpuWrite<Device> && !HasCpuMap<Device> && !HasReserve<Device>);
    static_assert(HasCpuWrite<CustomHost> && !HasReserve<CustomHost>);
    static_assert(!HasCpuWrite<CustomDevice> && !HasCpuMap<CustomDevice>);
} // namespace
#ifndef AHO_GPU_TESTS
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
#else
#include <VSL/vulkan/commands/copy_buffer.hpp>
namespace {
    struct CustomCounters {
        unsigned maps = 0, allocations = 0;
    };
    using NativeHost = vsl::Buffer<vsl::MemoryType::TransferSource | vsl::MemoryType::TransferDestination, vsl::MemoryProperty::HostVisible>;
    struct CountingCustomization {
        vsl::LogicalDeviceAccessor device;
        std::shared_ptr<CustomCounters> counters;
        auto map(NativeHost& storage) {
            ++counters->maps;
            return storage.data();
        }
        NativeHost reallocate(const NativeHost&, std::size_t size) {
            ++counters->allocations;
            return NativeHost(device, size);
        }
    };
    struct MoveOnlyCustomization {
        std::unique_ptr<int> value = std::make_unique<int>(7);
        MoveOnlyCustomization() = default;
        MoveOnlyCustomization(MoveOnlyCustomization&&) = default;
        MoveOnlyCustomization(const MoveOnlyCustomization&) = delete;
    };
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, ConstructorsExposeStandardUsageAndReuseTheirMapping) {
        Host source(device, 8);
        EXPECT_EQ(source.size(), 8u);
        EXPECT_EQ(source.capacity(), 8u);
        EXPECT_EQ(source.native().getDevice(), device);
        EXPECT_TRUE(vsl::contain(source.native().getUsage(), vsl::MemoryType::VertexBuffer | vsl::MemoryType::UniformBuffer));
        EXPECT_TRUE(vsl::contain(source.native().getMemoryProperties(), vsl::MemoryProperty::HostVisible));
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
        EXPECT_EQ(view.native().requestedSize(), 32u);
        auto moved = std::move(source);
        EXPECT_EQ(source.size(), 0u);
        EXPECT_THROW(source.binding(), std::logic_error);
        EXPECT_THROW(source.read(0), std::logic_error);
        EXPECT_THROW(source.wait(0), std::logic_error);
        EXPECT_THROW(source.native(), std::logic_error);
        EXPECT_THROW(moved.read(1, 4), std::out_of_range);
        EXPECT_TRUE(moved.read(0, 4).empty());
    }
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, CpuUploadAndGpuCopiesShareTheSameCommandsAndRetainContents) {
        std::array<std::byte, 8> expected{std::byte{11}, std::byte{22}, std::byte{33}, std::byte{44}, std::byte{55}, std::byte{66}, std::byte{77}, std::byte{88}};
        General cpu;
        cpu.assign(expected);
        Device destination(device, 8);
        Host readback(device, 8);
        auto owner = destination.binding().owner();
        auto manager = make_manager();
        for (unsigned frame = 0; frame < 3; ++frame) {
            expected[0] = std::byte{static_cast<unsigned char>(11 + frame)};
            cpu.assign(expected);
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << destination.upload(cpu) << readback.upload(destination);
            EXPECT_TRUE(destination.busy());
            EXPECT_TRUE(readback.busy());
            EXPECT_FALSE(destination.wait(0));
            EXPECT_THROW(readback.write({}), std::logic_error);
            EXPECT_THROW(readback.read(0), std::logic_error);
            EXPECT_THROW(readback.map(), std::logic_error);
            EXPECT_THROW(phase << destination.upload(cpu), std::logic_error);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
            ASSERT_TRUE(phase.completion().wait());
            EXPECT_TRUE(destination.wait(0));
            EXPECT_EQ(destination.binding().owner(), owner);
            auto pixels = readback.read(8);
            EXPECT_EQ(std::memcmp(pixels.data(), expected.data(), 8), 0);
        }
    }
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, FixedCustomBuffersAndOverlappingRangesUseCapabilityChecks) {
        auto custom = aho::resource::make_custom_buffer(NativeHost(device, 8));
        const std::array<std::byte, 4> expected{std::byte{17}, std::byte{29}, std::byte{43}, std::byte{71}};
        custom.assign(expected);
        EXPECT_EQ(custom.size(), 4u);
        EXPECT_EQ(custom.capacity(), 8u);
        EXPECT_EQ(custom.read(4)[2], expected[2]);
        std::array<std::byte, 9> tooLarge{};
        EXPECT_THROW(custom.assign(tooLarge), std::out_of_range);
        auto manager = make_manager();
        auto phase = manager.startPhase<vsl::ComputePhase>();
        Host target(device, 8);
        phase << target.upload(custom, 4);
        EXPECT_THROW(phase << target.upload(target, 1, 0, 4), std::invalid_argument);
        EXPECT_THROW(phase << target.upload(custom, 0, SIZE_MAX), std::out_of_range);
        EXPECT_THROW(phase << target.upload(custom, 6), std::out_of_range);
        EXPECT_THROW(phase << target.upload(custom, 0, 1, SIZE_MAX), std::out_of_range);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_EQ(std::memcmp(target.read(4, 4).data(), expected.data(), 4), 0);
    }
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, CustomMappingAndReallocationHooksPreserveDataAndSharedState) {
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
        auto moveOnly = aho::resource::make_custom_buffer(NativeHost(device, 4), MoveOnlyCustomization{});
        moveOnly.assign(expected);
        EXPECT_EQ(moveOnly.read(1)[0], expected[0]);
    }
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, InvalidCustomReallocationLeavesTheOriginalStorageIntact) {
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
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, PendingTransfersRejectOtherPhasesAndCancellationAllowsReuse) {
        auto manager = make_manager();
        auto other = manager.independent();
        Host source(device, 4);
        Device destination(device, 4);
        const std::array<std::byte, 4> bytes{};
        source.write(bytes);
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << destination.upload(source);
            auto second = other.startPhase<vsl::ComputePhase>();
            EXPECT_THROW(second << destination.upload(source), std::logic_error);
            second.cancel();
            phase.cancel();
        }
        EXPECT_FALSE(source.busy());
        EXPECT_TRUE(source.wait(0));
        source.assign(bytes);
        auto phase = manager.startPhase<vsl::ComputePhase>();
        phase << destination.upload(source);
        phase.submit();
        ASSERT_TRUE(phase.completion().wait());
    }
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, CancelledCpuUploadReusesStagingWithTheNewPayload) {
        auto manager = make_manager();
        General cpu(4);
        Device destination(device, 4);
        Host result(device, 4);
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << destination.upload(cpu);
            phase.cancel();
        }
        const std::array<std::byte, 4> expected{std::byte{17}, std::byte{37}, std::byte{73}, std::byte{255}};
        cpu.assign(expected);
        auto phase = manager.startPhase<vsl::ComputePhase>();
        phase << destination.upload(cpu) << result.upload(destination);
        phase.submit();
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_EQ(std::memcmp(result.read(4).data(), expected.data(), 4), 0);
    }
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, SubmissionOwnsCustomizationWithoutKeepingCompletionInACycle) {
        auto manager = make_manager();
        std::weak_ptr<CustomCounters> weak;
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            {
                auto counters = std::make_shared<CustomCounters>();
                weak = counters;
                auto source = aho::resource::make_custom_buffer(NativeHost(device, 4), CountingCustomization{device, counters});
                source.write(std::array<std::byte, 4>{});
                Device destination(device, 4);
                phase << destination.upload(source);
            }
            EXPECT_FALSE(weak.expired());
            phase.submit();
            ASSERT_TRUE(phase.completion().wait());
        }
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase.submit();
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(weak.expired());
    }
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, CustomizationContextOutlivesTheWrappedStorageDestructor) {
        struct ContextStorage : NativeHost {
            std::weak_ptr<int> context;
            std::shared_ptr<bool> destroyedWithoutContext;
            ContextStorage(vsl::LogicalDeviceAccessor device, const std::shared_ptr<int>& context, std::shared_ptr<bool> result)
                : NativeHost(device, 4), context(context), destroyedWithoutContext(std::move(result)) {}
            ContextStorage(ContextStorage&&) = default;
            ContextStorage(const ContextStorage&) = delete;
            ~ContextStorage() override {
                if (_data && context.expired())
                    *destroyedWithoutContext = true;
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
    TEST_F(GpuFriendlyBufferVulkanIntegrationTest, EmptyUploadsAndInvalidRecordingDoNotSubmitOrClaimResources) {
        auto manager = make_manager();
        Host source(device);
        Device destination(device, 0);
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << destination.upload(source) << destination.upload(General{});
            EXPECT_FALSE(destination.busy());
            EXPECT_FALSE(source.busy());
            phase.cancel();
        }
        EXPECT_THROW(destination.native(), std::logic_error);
        auto command = destination.upload(source);
        EXPECT_THROW(command.invoke(manager.getPool(), manager.getBuffer(), manager), std::logic_error);
    }
} // namespace
#endif
