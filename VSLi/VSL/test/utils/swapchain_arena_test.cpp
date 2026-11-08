#include "support/window_fixture.hpp"
#include "support/fixed_parent.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/lifetime/swapchain_storage.hpp>
#include <VSL/storage/cpu_region.hpp>
#include <limits>

namespace {
    namespace l = vsl::lifetime;
    namespace o = vsl::operation;
    namespace s = vsl::storage;
    class SwapchainArenaWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
        void SetUp() override;
    };
    enum class Failure {
        NONE,
        CREATE,
        IMAGES,
        VIEW
    };
    struct NativeProbe {
        Failure failure = Failure::NONE;
        unsigned creates = 0, queries = 0, destroys = 0, temporary_destroys = 0, views = 0;
        static VkResult Create(void* context, VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR* output);
        static VkResult Images(void* context, VkDevice device, VkSwapchainKHR chain, std::uint32_t* count, VkImage* images);
        static void Destroy(void* context, VkDevice device, VkSwapchainKHR chain) noexcept;
        static VkResult View(void* context, VkDevice device, const VkImageViewCreateInfo* info, VkImageView* output);
        o::SwapchainNativeCalls calls() { return {this, Create, Images, Destroy}; }
        o::ImageViewNativeCalls image_calls() { return {this, View}; }
    };
    template<typename Handle>
    Handle sentinel();
    vsl::SwapchainCreateInfo create_info();

    void SwapchainArenaWindowIntegrationTest::SetUp() {
        vsl_test::WindowIntegrationTest::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;
        make_surface();
        if (HasFatalFailure())
            return;
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected queue cannot present to the Surface";
    }
    template<typename Handle>
    Handle sentinel() {
        if constexpr (std::is_pointer_v<Handle>)
            return reinterpret_cast<Handle>(std::uintptr_t{1});
        else
            return static_cast<Handle>(1);
    }
    vsl::SwapchainCreateInfo create_info() {
        return {.width = 64, .height = 48};
    }
    VkResult NativeProbe::Create(void* context, VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR* output) {
        auto& probe = *static_cast<NativeProbe*>(context);
        ++probe.creates;
        const auto result = vkCreateSwapchainKHR(device, info, nullptr, output);
        if (result == VK_SUCCESS && probe.failure == Failure::CREATE) {
            // 実際に旧handleを退役させてから、失敗の未定義出力を再現する。
            vkDestroySwapchainKHR(device, *output, nullptr);
            ++probe.temporary_destroys;
            *output = sentinel<VkSwapchainKHR>();
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        return result;
    }
    VkResult NativeProbe::Images(void* context, VkDevice device, VkSwapchainKHR chain, std::uint32_t* count, VkImage* images) {
        auto& probe = *static_cast<NativeProbe*>(context);
        ++probe.queries;
        return probe.failure == Failure::IMAGES ? VK_ERROR_OUT_OF_HOST_MEMORY : vkGetSwapchainImagesKHR(device, chain, count, images);
    }
    void NativeProbe::Destroy(void* context, VkDevice device, VkSwapchainKHR chain) noexcept {
        ++static_cast<NativeProbe*>(context)->destroys;
        vkDestroySwapchainKHR(device, chain, nullptr);
    }
    VkResult NativeProbe::View(void* context, VkDevice device, const VkImageViewCreateInfo* info, VkImageView* output) {
        auto& probe = *static_cast<NativeProbe*>(context);
        if (++probe.views == 2 && probe.failure == Failure::VIEW) {
            *output = sentinel<VkImageView>();
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        return vkCreateImageView(device, info, nullptr, output);
    }
    TEST(SwapchainLayoutTest, RuntimeCountProducesAlignedDistinctSlotsAndChecksBounds) {
        constexpr s::SwapchainLayout layout(3);
        static_assert(layout.images() == 3);
        static_assert(layout.image_offset(0) >= s::SWAPCHAIN_SLOT_SIZE);
        static_assert(layout.image_offset(1) > layout.image_offset(0));
        static_assert(layout.image_offset(2) % s::IMAGE_IMPL_ALIGNMENT == 0);
        static_assert(layout.bytes() % layout.alignment() == 0);
        EXPECT_THROW((s::SwapchainLayout{0}), std::invalid_argument);
        EXPECT_THROW(layout.image_offset(3), std::out_of_range);
        EXPECT_THROW(layout.require_region(layout.bytes() - 1, layout.alignment()), std::invalid_argument);
        EXPECT_THROW(layout.require_region(layout.bytes(), 3), std::invalid_argument);
        if constexpr (sizeof(size_t) > sizeof(std::uint32_t))
            EXPECT_THROW((s::SwapchainLayout{size_t{std::numeric_limits<std::uint32_t>::max()} + 1}), std::invalid_argument);
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, NormalConstructorPlacesEveryImageWithItsOwnStateUnderOneOwner) {
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        const s::SwapchainLayout layout(chain.images.size());
        ASSERT_FALSE(chain.images.empty());
        EXPECT_EQ(chain.state(), vsl::SwapchainState::ACTIVE);
        EXPECT_TRUE(chain.owner_ref().managed());
        for (size_t i = 0; i < chain.images.size(); ++i) {
            const auto& image = chain.images[i];
            EXPECT_FALSE(image.data_.owner_before(chain.data_));
            EXPECT_FALSE(chain.data_.owner_before(image.data_));
            EXPECT_EQ(&image.use_state(), image.data_->use);
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(image.data_.get()) - reinterpret_cast<std::uintptr_t>(chain.data_.get()), layout.image_offset(i));
            if (i) {
                EXPECT_NE(image.data_, chain.images[i - 1].data_);
                EXPECT_NE(image.data_->use, chain.images[i - 1].data_->use);
            }
        }
        vsl_test::expect_fixed_parent(chain.owner_ref(), device.owner_ref());
        vsl_test::expect_fixed_parent(chain.owner_ref(), surface->owner_ref());
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, SavedImageRetainsItsGenerationWhileParentGettersReturnOriginalOwners) {
        vsl::ImageAccessor saved;
        l::WeakOwnerRef generation;
        vsl::LogicalDeviceAccessor parent;
        {
            l::SwapchainArena arena(device, *surface, create_info());
            generation = arena.owner_ref().weak();
            auto chain = arena.view();
            parent = chain.get_device();
            EXPECT_EQ(parent.data_, device.data_);
            EXPECT_FALSE(parent.data_.owner_before(device.data_));
            EXPECT_FALSE(device.data_.owner_before(parent.data_));
            auto original = chain.get_surface();
            EXPECT_EQ(original.data_, surface->data_);
            EXPECT_FALSE(original.data_.owner_before(surface->data_));
            EXPECT_FALSE(surface->data_.owner_before(original.data_));
            saved = chain.images.front();
        }
        EXPECT_FALSE(generation.expired());
        EXPECT_TRUE(saved.data_->view);
        saved.data_.reset();
        EXPECT_TRUE(generation.expired());
        EXPECT_TRUE(parent.owner_ref().managed());
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, MemoryCannotBeReusedUntilTheLastImageIsReleased) {
        std::optional<s::ArenaMemory> memory;
        const l::SwapchainMemoryFactory allocate = [&](const auto& layout) {
            if (!memory)
                memory.emplace(layout.bytes(), layout.alignment());
            return *memory;
        };
        auto first = std::make_optional<l::SwapchainArena>(device, *surface, create_info(), std::nullopt, allocate);
        auto image = first->view().images.front();
        EXPECT_THROW((l::SwapchainArena{device, *surface, create_info(), first->view(), allocate}), std::invalid_argument);
        EXPECT_EQ(first->view().state(), vsl::SwapchainState::RETIRED);
        first.reset();
        EXPECT_THROW((l::SwapchainArena{device, *surface, create_info(), {}, allocate}), std::invalid_argument);
        image.data_.reset();
        EXPECT_NO_THROW((l::SwapchainArena{device, *surface, create_info(), {}, allocate}));
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, ImageEnumerationFailureDestroysPendingAndNeverRevivesTheOldGeneration) {
        vsl::Swapchain old(device, surface, {}, 64, 48);
        const auto alias = static_cast<vsl::SwapchainAccessor>(old);
        NativeProbe probe;
        probe.failure = Failure::IMAGES;
        unsigned allocations = 0;
        const l::SwapchainRegionFactory memory = [&](const auto& layout) {
            ++allocations;
            return std::make_shared<s::detail::CPURegion>(layout.bytes(), layout.alignment());
        };
        EXPECT_THROW(l::make_swapchain(device, *surface, create_info(), old, memory, probe.calls()), vk::SystemError);
        EXPECT_EQ(probe.creates, 1u);
        EXPECT_EQ(probe.queries, 1u);
        EXPECT_EQ(probe.destroys, 1u);
        EXPECT_EQ(allocations, 0u);
        EXPECT_EQ(old.state(), vsl::SwapchainState::RETIRED);
        EXPECT_EQ(alias.state(), vsl::SwapchainState::RETIRED);
        EXPECT_THROW(l::make_swapchain(device, *surface, create_info(), old, memory, probe.calls()), std::logic_error);
        EXPECT_EQ(probe.creates, 1u);
        auto manager = make_manager();
        EXPECT_THROW((vsl::DefaultPhase{manager, old}), std::logic_error);
        EXPECT_NO_THROW((vsl::Swapchain{device, surface, {}, 64, 48}));
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, FailedNativeOutputIsNotAdoptedAndRetryOmitsTheRetiredHandle) {
        vsl::Swapchain old(device, surface, {}, 64, 48);
        NativeProbe probe;
        probe.failure = Failure::CREATE;
        const l::SwapchainRegionFactory memory = [](const auto& layout) {
            return std::make_shared<s::detail::CPURegion>(layout.bytes(), layout.alignment());
        };
        EXPECT_THROW(l::make_swapchain(device, *surface, create_info(), old, memory, probe.calls()), vsl::exceptions::RuntimeException);
        EXPECT_EQ(probe.creates, 1u);
        EXPECT_EQ(probe.temporary_destroys, 1u);
        EXPECT_EQ(probe.queries, 0u);
        EXPECT_EQ(probe.destroys, 0u);
        EXPECT_EQ(old.state(), vsl::SwapchainState::RETIRED);
        vsl::Swapchain retry(device, surface, {}, 64, 48);
        EXPECT_EQ(retry.state(), vsl::SwapchainState::ACTIVE);
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, LaterImageViewFailureRollsBackSlotsAndReleasesTheCPUClaim) {
        const auto caps = physical.data_->device.getSurfaceCapabilitiesKHR(surface->data_->surface);
        if (caps.maxImageCount == 1)
            GTEST_SKIP() << "Surface supports only one image";
        NativeProbe probe;
        probe.failure = Failure::VIEW;
        std::shared_ptr<s::detail::CPURegion> region;
        const l::SwapchainRegionFactory memory = [&](const auto& layout) {
            if (!region)
                region = std::make_shared<s::detail::CPURegion>(layout.bytes(), layout.alignment());
            return region;
        };
        EXPECT_THROW(l::make_swapchain(device, *surface, create_info(), {}, memory, probe.calls(), probe.image_calls()), vk::SystemError);
        EXPECT_EQ(probe.views, 2u);
        EXPECT_EQ(probe.destroys, 1u);
        ASSERT_TRUE(region);
        EXPECT_FALSE(region->claimed);
        auto retry = l::make_swapchain(device, *surface, create_info(), {}, memory);
        EXPECT_TRUE(region->claimed);
        retry = {};
        EXPECT_FALSE(region->claimed);
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, UnsupportedPresentFencePolicyIsRejectedWithoutRetiringTheOldGeneration) {
        ASSERT_FALSE(vsl::has_device_features(device.enabled_features(), vsl::DeviceFeature::SWAPCHAIN_MAINTENANCE1));
        vsl::Swapchain old(device, surface, {}, 64, 48);
        auto info = create_info();
        info.present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE;
        EXPECT_THROW((l::SwapchainArena{device, *surface, info, old}), std::invalid_argument);
        EXPECT_EQ(old.state(), vsl::SwapchainState::ACTIVE);
        auto acquired = device.data_->device.createFenceUnique({});
        const auto index = device.data_->device.acquireNextImageKHR(old.data_->swap_chain, UINT64_MAX, nullptr, *acquired).value;
        ASSERT_EQ(device.data_->device.waitForFences(*acquired, true, UINT64_MAX), vk::Result::eSuccess);
        EXPECT_LT(index, old.images.size());
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, PreparationCallbackCannotAcquireARetiredOrReplacedGeneration) {
        for (const bool replace : {false, true}) {
            vsl::Swapchain old(device, surface, {}, 64, 48);
            auto manager = make_manager().independent(1);
            std::optional<vsl::Swapchain> next;
            std::unique_ptr<vsl::DefaultPhase> phase;
            bool invoked = false;
            const vsl_test::DeferredAction callback([&] {
                invoked = true;
                EXPECT_NO_THROW(next.emplace(device, surface, old, 64, 48));
                if (replace && next)
                    EXPECT_NO_THROW(phase->swapchain = *next);
            });
            {
                auto previous = manager.start_phase<vsl::ComputePhase>();
                manager.get_buffer().retain_until_complete(callback.owner());
                ASSERT_EQ(previous.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            }
            vsl::SynchroManager sync(device);
            auto available = sync.create_semaphore("prepare available");
            auto finished = sync.create_semaphore("prepare finished", old.images.size());
            phase = std::make_unique<vsl::DefaultPhase>(manager, old, available, finished, std::nullopt, true);
            EXPECT_THROW(phase->setup(), std::logic_error);
            EXPECT_TRUE(invoked);
            EXPECT_EQ(old.state(), vsl::SwapchainState::RETIRED);
            EXPECT_TRUE(old.data_->presentation_semaphores.empty());
            EXPECT_NO_THROW(available.destroy());
            EXPECT_NO_THROW(finished.destroy());
        }
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, ReturningImageDoesNotPollEarlierImageUseBeforeTheReturnIsSubmitted) {
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        auto earlier = make_manager().independent(1);
        auto returning = make_manager().independent(1);
        std::unique_ptr<vsl::DefaultPhase> phase;
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            phase.reset();
        });
        vsl::Completion completed;
        {
            auto previous = earlier.start_phase<vsl::ComputePhase>();
            for (const auto& image : chain.images)
                vsl::ResourceUse::ForImage(image.data_).record(earlier.get_buffer(), vsl::ResourceRetention::COMPLETION);
            earlier.get_buffer().retain_until_complete(callback.owner());
            completed = previous.completion();
            ASSERT_EQ(previous.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        device.data_->device.waitIdle(); // native完了だけを待ち、過去Workのcallbackは未観測にする。
        phase = std::make_unique<vsl::DefaultPhase>(returning, chain);
        EXPECT_NO_THROW(phase->cancel());
        ASSERT_FALSE(invoked);
        ASSERT_TRUE(phase);
        EXPECT_TRUE(completed.wait());
        EXPECT_TRUE(invoked);
        EXPECT_FALSE(phase);
        returning.wait_idle();
    }
    TEST_F(SwapchainArenaWindowIntegrationTest, CancelUsesTheAcquiredGenerationAndIndexAfterPublicFieldsChange) {
        auto original = std::make_optional<vsl::Swapchain>(device, surface, std::nullopt, 64, 48);
        const std::weak_ptr<vsl::impl::SwapchainImpl> owner = original->data_;
        auto manager = make_manager().independent(1);
        {
            auto phase = manager.start_phase<vsl::DefaultPhase>(*original);
            original.reset();
            phase.swapchain = {};
            phase.image_index = UINT32_MAX;
            EXPECT_NO_THROW(phase.cancel());
            EXPECT_FALSE(owner.expired());
        }
        manager.wait_idle();
        EXPECT_TRUE(owner.expired());
    }
} // namespace
