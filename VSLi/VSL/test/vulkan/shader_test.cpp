#include "support/vulkan_fixture.hpp"
#include "support/temporary_directory.hpp"
#include "support/scoped_environment.hpp"
#include "support/render_fixture.hpp"
#include "support/compute_fixture.hpp"
#include <VSL/lifetime/resource_arena.hpp>

namespace {
    using ShaderVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    class ShaderArenaVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    class ShaderArenaRenderVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    class ShaderArenaComputeVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    std::vector<std::uint32_t> shader_words(const char* file);
    vsl::LogicalDeviceAccessor another_device(vsl::PhysicalDevice physical, std::uint32_t queue);
    template<typename T>
    concept temporary_shader_code = requires(T&& value) { std::move(value).code(); };
    static_assert(!temporary_shader_code<vsl::ShaderAccessor>);
    static_assert(!temporary_shader_code<vsl::Shader<>>);
    static_assert(vsl::get_shader_type<"fixture.vert.spv">() == vsl::ShaderType::VERTEX);
    static_assert(vsl::get_shader_type<"fixture.frag.spv">() == vsl::ShaderType::FRAGMENT);
    static_assert(vsl::get_shader_type<"fixture.comp.spv">() == vsl::ShaderType::COMPUTE);
    static_assert(vsl::get_shader_type<"fixture.unknown">() == vsl::ShaderType::ERROR_);
    std::vector<std::uint32_t> shader_words(const char* file) {
        const auto bytes = vsl_test::read_binary(vsl_test::shader_path(file));
        std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
        std::memcpy(words.data(), bytes.data(), bytes.size());
        return words;
    }
    vsl::LogicalDeviceAccessor another_device(vsl::PhysicalDevice physical, std::uint32_t queue) {
        vsl::DeviceCreateInfo info;
        info.graphics_family = queue;
        return vsl::LogicalDeviceAccessor::Create(std::move(physical), std::move(info));
    }
    TEST(ShaderAccessorTest, EmptyAccessorRejectsOperationsAndHasNoOwner) {
        const vsl::ShaderAccessor empty;
        EXPECT_EQ(empty.owner_ref().use_count(), 0);
        EXPECT_THROW(empty.name(), std::logic_error);
        EXPECT_THROW(empty.type(), std::logic_error);
        EXPECT_THROW(empty.code(), std::logic_error);
        EXPECT_THROW(empty.get_device(), std::logic_error);
    }
    TEST(ShaderPlacementTest, SameTypeSlotsUseActualImplAlignmentWithoutGpuUseState) {
        namespace t = vsl::typed;
        namespace s = vsl::storage;
        constexpr std::array<t::ResourceRequest, 3> requests{{{{11}, s::ResourceKind::SHADER}, {{12}, s::ResourceKind::SHADER}, {{13}, s::ResourceKind::SHADER, true}}};
        constexpr t::ResourcePlan<3> plan(requests);
        static_assert(s::ResourceSlotLayout<vsl::impl::ShaderImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        static_assert(s::ResourceSlotLayout<vsl::impl::ShaderImpl>::SIZE == sizeof(vsl::impl::ShaderImpl));
        static_assert(plan.slots() == 2 && plan.size() == 3);
        EXPECT_EQ(plan.alignment(), alignof(vsl::impl::ShaderImpl));
        EXPECT_EQ(plan.placement({12}).offset, sizeof(vsl::impl::ShaderImpl));
        EXPECT_EQ(plan.bytes(), 2 * sizeof(vsl::impl::ShaderImpl));
        EXPECT_EQ(plan.placement({13}).slot, t::ResourcePlacement::NO_SLOT);
    }
    TEST_F(ShaderArenaVulkanIntegrationTest, PlacedModulesOwnInputsAndShareOneCanonicalOwner) {
        namespace t = vsl::typed;
        namespace l = vsl::lifetime;
        constexpr std::array<t::ResourceRequest, 2> requests{{{{11}, vsl::storage::ResourceKind::SHADER}, {{12}, vsl::storage::ResourceKind::SHADER}}};
        constexpr t::ResourcePlan<2> plan(requests);
        const auto code = shader_words("triangle.vert.spv");
        std::optional<vsl::ShaderAccessor> kept;
        std::weak_ptr<void> owner;
        {
            std::array<l::ResourceInit, 2> inputs{vsl::ShaderCreateInfo{vsl::ShaderType::VERTEX, code},
                                                  vsl::ShaderCreateInfo{vsl::ShaderType::FRAGMENT, shader_words("triangle.frag.spv")}};
            l::ResourceArena arena(device, plan, inputs);
            owner = arena.owner();
            const auto first = arena.shader({11}), second = arena.shader({12});
            EXPECT_TRUE(first.owner_ref().managed());
            EXPECT_FALSE(first.data_.owner_before(second.data_));
            EXPECT_FALSE(second.data_.owner_before(first.data_));
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second.data_.get()) - reinterpret_cast<std::uintptr_t>(first.data_.get()), plan.placement({12}).offset);
            EXPECT_EQ(first.get_device(), device);
            EXPECT_EQ(first.type(), vsl::ShaderType::VERTEX);
            EXPECT_EQ(second.type(), vsl::ShaderType::FRAGMENT);
            std::get<vsl::ShaderCreateInfo>(inputs[0]).code.clear();
            std::get<vsl::ShaderCreateInfo>(inputs[0]).name = "changed";
            kept.emplace(first);
            EXPECT_THROW(arena.shader({404}), std::out_of_range);
            EXPECT_THROW(arena.buffer({11}), std::invalid_argument);
        }
        EXPECT_FALSE(owner.expired());
        EXPECT_EQ(kept->name(), "main");
        EXPECT_TRUE(std::ranges::equal(kept->code(), code));
        EXPECT_TRUE(kept->data_->shader_module);
        kept.reset();
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(ShaderArenaVulkanIntegrationTest, LaterShaderFailureRollsBackTheRegionAndLeavesImportedModuleAlive) {
        namespace t = vsl::typed;
        namespace l = vsl::lifetime;
        constexpr std::array<t::ResourceRequest, 3> requests{{{{11}, vsl::storage::ResourceKind::SHADER}, {{12}, vsl::storage::ResourceKind::SHADER}, {{13}, vsl::storage::ResourceKind::SHADER, true}}};
        constexpr t::ResourcePlan<3> plan(requests);
        vsl::storage::ArenaMemory memory(plan.bytes(), plan.alignment());
        vsl::Shader<vsl::ShaderType::VERTEX> imported(device, vsl_test::shader_path("triangle.vert.spv"));
        const vsl::ShaderCreateInfo valid{vsl::ShaderType::VERTEX, shader_words("triangle.vert.spv")};
        std::array<l::ResourceInit, 3> inputs{valid, valid, vsl::ShaderAccessor{imported}};
        const auto references = imported.data_.use_count();
        for (const auto error : {0, 1, 2, 3}) {
            auto invalid = valid;
            if (error == 0)
                invalid.code.clear();
            else if (error == 1)
                invalid.code.front() = 0;
            else if (error == 2)
                invalid.name.clear();
            else
                invalid.name = std::string("ma\0in", 5);
            inputs[1] = std::move(invalid);
            EXPECT_THROW((l::ResourceArena(device, plan, inputs, memory)), vsl::exceptions::RuntimeException);
            EXPECT_EQ(imported.data_.use_count(), references);
            EXPECT_TRUE(imported.data_->shader_module);
            inputs[1] = valid;
            EXPECT_NO_THROW((l::ResourceArena(device, plan, inputs, memory)));
            EXPECT_EQ(imported.data_.use_count(), references);
        }
    }
    TEST_F(ShaderArenaVulkanIntegrationTest, ImportAndShaderGroupsRejectMissingOwnersAndOtherDevices) {
        namespace t = vsl::typed;
        namespace l = vsl::lifetime;
        constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, vsl::storage::ResourceKind::SHADER, true}}};
        constexpr t::ResourcePlan<1> plan(requests);
        vsl::Shader<> source(device, vsl_test::shader_path("triangle.vert.spv"));
        const vsl::ShaderAccessor borrowed{std::shared_ptr<vsl::impl::ShaderImpl>(std::shared_ptr<void>{}, source.data_.get())};
        for (const auto& input : {vsl::ShaderAccessor{}, borrowed}) {
            const std::array<l::ResourceInit, 1> inputs{input};
            EXPECT_THROW((l::ResourceArena(device, plan, inputs)), std::invalid_argument);
            EXPECT_THROW((vsl::pipeline_layout::ShaderGroup("invalid", {input})), std::invalid_argument);
        }
        const auto other = another_device(physical, queue_family);
        const std::array<l::ResourceInit, 1> inputs{vsl::ShaderAccessor{source}};
        EXPECT_THROW((l::ResourceArena(other, plan, inputs)), std::invalid_argument);
        EXPECT_THROW((vsl::PipelineLayout(other, vsl::pipeline_layout::ShaderGroup("foreign", {source}))), std::invalid_argument);
        vsl::Shader<vsl::ShaderType::ALL_GRAPHICS> aggregate(device, vsl_test::shader_path("triangle.vert.spv"));
        EXPECT_THROW((vsl::PipelineLayout(device, vsl::pipeline_layout::ShaderGroup("aggregate stage", {aggregate}))), std::invalid_argument);
    }
    TEST_F(ShaderArenaRenderVulkanIntegrationTest, FrozenRecipeRebuildsAndDrawsAfterTheShaderArenaFacadeIsGone) {
        namespace t = vsl::typed;
        namespace l = vsl::lifetime;
        namespace pl = vsl::pipeline_layout;
        constexpr std::array<t::ResourceRequest, 2> requests{{{{11}, vsl::storage::ResourceKind::SHADER}, {{12}, vsl::storage::ResourceKind::SHADER}}};
        constexpr t::ResourcePlan<2> plan(requests);
        pipeline.reset();
        layout.reset();
        vertex.reset();
        fragment.reset();
        std::weak_ptr<void> owner;
        {
            {
                const std::array<l::ResourceInit, 2> inputs{vsl::ShaderCreateInfo{vsl::ShaderType::VERTEX, shader_words("triangle.vert.spv")},
                                                            vsl::ShaderCreateInfo{vsl::ShaderType::FRAGMENT, shader_words("triangle.frag.spv")}};
                l::ResourceArena arena(device, plan, inputs);
                owner = arena.owner();
                layout.emplace(device, pl::ShaderGroup("placed", {arena.shader({11}), arena.shader({12})}), pl::VertexInput{}, pl::ColorBlend{},
                               pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
            }
            EXPECT_FALSE(owner.expired());
            auto frozen = layout->copy().freeze();
            layout.reset();
            pipeline.emplace(frozen, *pass);
            ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
            pipeline.reset();
            EXPECT_FALSE(owner.expired());
        }
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(ShaderArenaComputeVulkanIntegrationTest, ImportedShaderKeepsItsOriginalOwnerAndBuildsComputeAfterImportArenaDestruction) {
        namespace t = vsl::typed;
        namespace l = vsl::lifetime;
        namespace pl = vsl::pipeline_layout;
        const auto original = std::weak_ptr<void>(shader->data_);
        constexpr std::array<t::ResourceRequest, 2> requests{{{{1}, vsl::storage::ResourceKind::SHADER, true}, {{2}, vsl::storage::ResourceKind::BUFFER}}};
        constexpr t::ResourcePlan<2> plan(requests);
        vsl::ShaderAccessor kept;
        std::weak_ptr<void> importer;
        {
            const std::array<l::ResourceInit, 2> inputs{vsl::ShaderAccessor{*shader}, vsl::BufferCreateInfo{4, vsl::MemoryType::TRANSFER_SOURCE}};
            l::ResourceArena arena(device, plan, inputs);
            importer = arena.owner();
            kept = arena.shader({1});
            EXPECT_EQ(kept.data_.get(), shader->data_.get());
            EXPECT_FALSE(kept.data_.owner_before(shader->data_));
            EXPECT_FALSE(shader->data_.owner_before(kept.data_));
            EXPECT_TRUE(kept.owner_ref().managed());
        }
        EXPECT_TRUE(importer.expired());
        pipeline.reset();
        layout.reset();
        shader.reset();
        {
            layout.emplace(device, pl::ShaderGroup("imported", {kept}), pl::ResourceBinding{{*binding}},
                           pl::PushConstants(vsl::ShaderFlag::COMPUTE, {{vsl::ShaderFlag::COMPUTE, 0, 4}}));
            auto frozen = layout->copy().freeze();
            pipeline.emplace(frozen);
            ExpectSequence(execute(313), 313);
            pipeline.reset();
            layout.reset();
        }
        EXPECT_FALSE(original.expired());
        kept = {};
        EXPECT_TRUE(original.expired());
    }
    TEST(ShaderPathTest, DirectoryNamesDoNotChooseTheShaderStage) {
        EXPECT_EQ(vsl::get_shader_type<"cache.vert.data/lighting.frag.spv">(), vsl::ShaderType::FRAGMENT);
        EXPECT_EQ(vsl::get_shader_type<"cache.frag.data\\simulation.comp.spv">(), vsl::ShaderType::COMPUTE);
        EXPECT_EQ(vsl::get_shader_type<"cache.vert.data/unknown.spv">(), vsl::ShaderType::ERROR_);
    }
    TEST_F(ShaderVulkanIntegrationTest, LoadsVertexAndFragmentModulesWithEntryPointNames) {
        const vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        const vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"),
                                                              "main");
        EXPECT_EQ(vertex.name(), "main");
        EXPECT_EQ(fragment.name(), "main");
        EXPECT_TRUE(vertex.data_->shader_module);
        EXPECT_TRUE(fragment.data_->shader_module);
        EXPECT_EQ(vertex.type(), vsl::ShaderType::VERTEX);
        EXPECT_EQ(fragment.type(), vsl::ShaderType::FRAGMENT);
        EXPECT_EQ(vertex.name(), "main");
    }
    TEST_F(ShaderVulkanIntegrationTest, RejectsMissingShaderFiles) {
        EXPECT_THROW((vsl::Shader<vsl::ShaderType::COMPUTE>(device, vsl_test::shader_path("missing.comp.spv"))),
                     vsl::exceptions::RuntimeException);
    }
    TEST_F(ShaderVulkanIntegrationTest, MemoryModulesOwnCodeAndEntryPointAfterSourceChanges) {
        const auto bytes = vsl_test::read_binary(vsl_test::shader_path("triangle.vert.spv"));
        std::vector<std::uint32_t> code(bytes.size() / sizeof(std::uint32_t));
        std::memcpy(code.data(), bytes.data(), bytes.size());
        const auto original = code;
        std::string entry = "main";
        const vsl::Shader<vsl::ShaderType::VERTEX> named(device, std::span(code), entry);
        const vsl::Shader<vsl::ShaderType::VERTEX> unnamed(device, std::span(code));
        std::fill(code.begin(), code.end(), 0);
        entry = "changed";
        EXPECT_EQ(named.data_->code, original);
        EXPECT_EQ(unnamed.data_->code, original);
        EXPECT_EQ(named.name(), "main");
        EXPECT_EQ(unnamed.name(), "main");
        EXPECT_TRUE(named.data_->shader_module);
        EXPECT_EQ(named.name(), "main");
        EXPECT_EQ(named.type(), vsl::ShaderType::VERTEX);
        EXPECT_THROW((vsl::Shader<vsl::ShaderType::VERTEX>(device, std::span(original), "")), vsl::exceptions::RuntimeException);
        EXPECT_THROW((vsl::Shader<vsl::ShaderType::VERTEX>(device, std::span(original), std::string("ma\0in", 5))), vsl::exceptions::RuntimeException);
    }
    TEST(ShaderMemoryTest, InvalidHeadersAndMissingDeviceAreRejectedBeforeModuleCreation) {
        const vsl::LogicalDeviceAccessor empty;
        const std::array<std::uint32_t, 5> invalid{};
        EXPECT_THROW((vsl::Shader<>(empty, std::span<const std::uint32_t>{})), vsl::exceptions::RuntimeException);
        EXPECT_THROW((vsl::Shader<>(empty, std::span(invalid).first(4))), vsl::exceptions::RuntimeException);
        EXPECT_THROW((vsl::Shader<>(empty, std::span(invalid))), vsl::exceptions::RuntimeException);
        const std::array<std::uint32_t, 5> header{0x07230203, 0x10000, 0, 1, 0};
        EXPECT_THROW((vsl::Shader<>(empty, std::span(header))), vsl::exceptions::RuntimeException);
    }
    TEST(ShaderFlagsTest, BitwiseStageCombinationsPreserveMembership) {
        using namespace vsl;
        const auto stages = ShaderType::VERTEX | ShaderType::FRAGMENT;
        EXPECT_EQ(stages, ShaderFlag::VERTEX | ShaderFlag::FRAGMENT);
        EXPECT_EQ(ShaderType::VERTEX & ShaderType::FRAGMENT, ShaderFlag::ERROR_);
        EXPECT_EQ(ShaderType::VERTEX ^ ShaderType::FRAGMENT, stages);
        EXPECT_EQ(convert(ShaderType::COMPUTE), ShaderFlag::COMPUTE);
    }
} // namespace
namespace {
    TEST_F(ShaderVulkanIntegrationTest, FactoriesInferStageAndHonorExplicitEntryName) {
        const auto vertex = vsl::make_shader<VSL_TEST_SHADER_DIR "/triangle.vert.spv">(device);
        const auto fragment = vsl::make_shader<VSL_TEST_SHADER_DIR "/triangle.frag.spv">(device, "main");
        static_assert(decltype(vertex)::TYPE == vsl::ShaderType::VERTEX);
        static_assert(decltype(fragment)::TYPE == vsl::ShaderType::FRAGMENT);
        EXPECT_EQ(vertex.name(), "main");
        EXPECT_EQ(fragment.name(), "main");
        EXPECT_TRUE(static_cast<bool>(vertex.data_->shader_module));
        EXPECT_TRUE(static_cast<bool>(fragment.data_->shader_module));
    }

    TEST_F(ShaderVulkanIntegrationTest, FactoryLoadsAFragmentShaderInsideADirectoryWithAVertexMarker) {
        vsl_test::TemporaryDirectory temporary;
        const auto directory = temporary.path() / "cache.vert.data";
        std::filesystem::create_directory(directory);
        std::filesystem::copy_file(vsl_test::shader_path("triangle.frag.spv"), directory / "triangle.frag.spv");
        vsl_test::ScopedEnvironment environment("VSL_SHADER_STAGE_TEST_ROOT", temporary.path().string().c_str());
        const auto shader = vsl::make_shader<"${VSL_SHADER_STAGE_TEST_ROOT}/cache.vert.data/triangle.frag.spv">(device);
        EXPECT_EQ(decltype(shader)::TYPE, vsl::ShaderType::FRAGMENT);
        EXPECT_EQ(shader.type(), vsl::ShaderType::FRAGMENT);
        EXPECT_TRUE(static_cast<bool>(shader.data_->shader_module));
    }
} // namespace

namespace {
    TEST_F(ShaderVulkanIntegrationTest, RejectsIncompleteHeadersAndPartialWordsBeforeCreatingAModule) {
        vsl_test::TemporaryDirectory temporary;
        for (const auto byte_count : {0u, 3u, 7u, 4u, 16u, 21u}) {
            const auto path = temporary.path() / (std::to_string(byte_count) + ".vert.spv");
            {
                std::ofstream output(path, std::ios::binary);
                output << std::string(byte_count, '\0');
            }
            EXPECT_THROW((vsl::Shader<vsl::ShaderType::VERTEX>(device, path)), vsl::exceptions::RuntimeException);
            EXPECT_THROW((vsl::Shader<vsl::ShaderType::VERTEX>(device, path, "main")), vsl::exceptions::RuntimeException);
        }
    }
} // namespace
