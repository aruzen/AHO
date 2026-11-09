#ifdef AHO_GPU_TESTS
#include <AHO/x/resource/texture.hpp>
#include <AHO/x/command/texture_update.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/operation/submission_ops.hpp>

namespace {
    class ExperimentalTextureVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool ExperimentalTextureVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(ExperimentalTextureVulkanIntegrationTest, ConstructorReportsMetadataAndRejectsUnsupportedExtentsAndFormats) {
        namespace resource = aho::x::resource;
        resource::Texture texture(device, 4, 3);
        EXPECT_EQ(texture.width(), 4u);
        EXPECT_EQ(texture.height(), 3u);
        EXPECT_EQ(texture.format(), vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        EXPECT_THROW((resource::Texture(device, 0, 1)), std::invalid_argument);
        resource::Texture color(device, 1, 1, vsl::data_format::SRGB8_RGBA);
        EXPECT_EQ(color.format(), vsl::data_format::SRGB8_RGBA);
        EXPECT_THROW((resource::Texture(device, 1, 1, vsl::data_format::FLOAT_RGBA)), std::invalid_argument);
    }
    TEST_F(ExperimentalTextureVulkanIntegrationTest, NativeImageUseBlocksAHOUpdatesAndBothEntrypointsSharePhysicalState) {
        aho::x::resource::Texture texture(device, 1, 1);
        aho::resource::Buffer<aho::resource::BufferType::GENERAL> pixels(4);
        auto image = texture.image();
        const auto use = texture.resource_use();
        EXPECT_EQ(use.state.get(), &image.use_state());
        ASSERT_TRUE(use.constraint_state);
        EXPECT_NE(use.constraint_state, use.state);
        auto native_manager = make_manager(), aho_manager = make_manager();
        auto native = native_manager.start_phase<vsl::ComputePhase>();
        image.resource_use().record(native_manager.get_buffer());
        auto update = aho_manager.start_phase<vsl::ComputePhase>();
        EXPECT_THROW(update << texture.update(pixels), std::logic_error);
        EXPECT_FALSE(use.constraint_state->busy());
        native.cancel();
        EXPECT_NO_THROW(update << texture.update(pixels));
        EXPECT_TRUE(image.use_state().busy());
        EXPECT_TRUE(use.constraint_state->busy());
        update.cancel();
        EXPECT_FALSE(image.use_state().busy());
        EXPECT_FALSE(use.constraint_state->busy());
    }
    TEST_F(ExperimentalTextureVulkanIntegrationTest, ManagedLogicalOwnerKeepsTheImageWithoutAnOwnershipCycle) {
        std::optional<aho::x::resource::Texture> texture(std::in_place, device, 1, 1);
        std::optional<vsl::ImageAccessor> image(texture->image());
        auto use = texture->resource_use();
        EXPECT_TRUE(use.owner.managed());
        EXPECT_TRUE(use.constraint_state.managed());
        EXPECT_TRUE(use.constraint_owner.managed());
        EXPECT_FALSE(use.owner.owner_before(image->resource_use().owner));
        EXPECT_FALSE(image->resource_use().owner.owner_before(use.owner));
        EXPECT_TRUE(use.owner.owner_before(use.constraint_owner) || use.constraint_owner.owner_before(use.owner));
        EXPECT_FALSE(use.constraint_state.owner_before(use.constraint_owner));
        EXPECT_FALSE(use.constraint_owner.owner_before(use.constraint_state));
        auto physical = use.owner.weak(), logical = use.constraint_owner.weak();
        texture.reset();
        EXPECT_FALSE(logical.expired());
        use = {};
        EXPECT_TRUE(logical.expired());
        EXPECT_FALSE(physical.expired());
        image.reset();
        EXPECT_TRUE(physical.expired());
    }
    TEST_F(ExperimentalTextureVulkanIntegrationTest, UnknownUpdateRejectsCachedBindingsAfterIdleAndBeforeAnotherNativeSubmit) {
        aho::x::resource::Texture texture(device, 1, 1);
        aho::resource::Buffer<aho::resource::BufferType::GENERAL> pixels(4);
        auto commands = make_manager().independent(1), other = make_manager().independent(1);
        auto cached = std::make_shared<vsl::ResourceUseGroup>(1);
        cached->replace(0, {texture.resource_use()});
        auto update = commands.start_phase<vsl::ComputePhase>();
        update << texture.update(pixels);
        auto already_recorded = other.start_phase<vsl::ComputePhase>();
        cached->record(other.get_buffer());
        unsigned calls = 0;
        const vsl::operation::ScopedSubmissionNativeCalls native({.context = &calls,
                                                                  .queue_submit = +[](void* context, VkQueue, const VkSubmitInfo*, VkFence) noexcept {
                                                                    ++*static_cast<unsigned*>(context);
                                                                    return VK_ERROR_UNKNOWN; },
                                                                  .fence_status = +[](void*, VkDevice, VkFence) noexcept { return VK_NOT_READY; }});
        EXPECT_THROW(update.submit(), vk::SystemError);
        EXPECT_EQ(calls, 1u);
        EXPECT_TRUE(update.completion().busy());
        commands.wait_idle();
        EXPECT_FALSE(update.completion().busy());
        EXPECT_TRUE(update.completion().native_uncertain());
        EXPECT_THROW(already_recorded.submit(), std::logic_error);
        EXPECT_EQ(calls, 1u);
        auto next = other.start_phase<vsl::ComputePhase>();
        EXPECT_THROW(cached->record(other.get_buffer()), std::logic_error);
        EXPECT_THROW(next << texture.update(pixels), std::runtime_error);
        EXPECT_NO_THROW(texture.image().resource_use().record(other.get_buffer()));
        next.cancel();
    }
} // namespace
#endif
