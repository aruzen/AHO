#include <AHO/x/resource/gltf.hpp>
#include <gtest/gtest.h>
#include <json.hpp>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>

namespace {
    using namespace AHO_NAMESPACE::x::resource;
    using Json = nlohmann::json;

    constexpr unsigned char PNG[]{137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 1, 8, 6, 0, 0, 0, 244, 34, 127, 138, 0, 0, 0, 17, 73, 68, 65, 84, 120, 156, 99, 248, 207, 192, 208, 192, 240, 159, 225, 63, 0, 14, 125, 3, 126, 234, 246, 147, 192, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130};
    class GltfTest : public ::testing::Test {
    protected:
        std::filesystem::path dir;
        Json doc;
        std::vector<unsigned char> bytes;
        void SetUp() override;
        void TearDown() override {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }
        void write(const std::filesystem::path& p, std::span<const unsigned char> data);
        int view(std::span<const unsigned char> b);
        int accessor(const std::vector<float>& numbers, const char* type, int components);
        void triangle();
        std::filesystem::path save(bool glb = false);
        void track(const char* path, const char* interpolation, std::vector<float> times, std::vector<float> values, int node = 0);
    };

    std::vector<std::uint32_t> triangle_indices(const GltfPrimitive& primitive) {
        std::vector<std::uint32_t> result;
        for (const auto& triangle : primitive.mesh.indices())
            result.insert(result.end(), {triangle.idx1.index, triangle.idx2.index, triangle.idx3.index});
        return result;
    }

    void GltfTest::SetUp() {
        static std::atomic<unsigned> SERIAL{};
        dir = std::filesystem::temp_directory_path() / std::filesystem::path("aho-gltf-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(SERIAL++));
        std::filesystem::create_directories(dir);
        doc = {{"asset", {{"version", "2.0"}}}, {"bufferViews", Json::array()}, {"accessors", Json::array()}, {"nodes", Json::array()}, {"scenes", Json::array()}};
    }

    void GltfTest::write(const std::filesystem::path& p, std::span<const unsigned char> data) {
        std::ofstream file(p, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
        ASSERT_TRUE(file.good());
    }

    int GltfTest::view(std::span<const unsigned char> b) {
        while (bytes.size() % 4)
            bytes.push_back(0);
        int index = int(doc["bufferViews"].size());
        doc["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", bytes.size()}, {"byteLength", b.size()}});
        bytes.insert(bytes.end(), b.begin(), b.end());
        return index;
    }

    int GltfTest::accessor(const std::vector<float>& numbers, const char* type, int components) {
        std::vector<unsigned char> b;
        for (float number : numbers) {
            auto value = std::bit_cast<std::uint32_t>(number);
            for (int i = 0; i < 4; ++i)
                b.push_back((value >> (8 * i)) & 255);
        }
        int v = view(b), id = int(doc["accessors"].size());
        doc["accessors"].push_back({{"bufferView", v}, {"componentType", 5126}, {"count", numbers.size() / components}, {"type", type}});
        return id;
    }

    void GltfTest::triangle() {
        int p = accessor({0, 0, 0, 1, 0, 0, 0, 1, 0}, "VEC3", 3);
        doc["meshes"] = {{{"name", "triangle"}, {"primitives", {{{"attributes", {{"POSITION", p}}}}}}}};
        doc["nodes"] = {{{"mesh", 0}}};
        doc["scenes"] = {{{"nodes", {0}}}};
        doc["scene"] = 0;
    }

    std::filesystem::path GltfTest::save(bool glb) {
        doc["buffers"] = {{{"byteLength", bytes.size()}}};
        auto path = dir / (glb ? "model.glb" : "model.gltf");
        if (!glb) {
            doc["buffers"][0]["uri"] = "data.bin";
            write(dir / "data.bin", bytes);
            std::ofstream(path) << doc.dump();
            return path;
        }
        std::string json = doc.dump();
        while (json.size() % 4)
            json += ' ';
        auto binary = bytes;
        while (binary.size() % 4)
            binary.push_back(0);
        std::vector<unsigned char> out;
        auto word = [&](std::uint32_t x) {for (int i=0;i<4;++i) out.push_back((x>>(8*i))&255); };
        word(0x46546c67);
        word(2);
        word(std::uint32_t(12 + 8 + json.size() + 8 + binary.size()));
        word(std::uint32_t(json.size()));
        word(0x4e4f534a);
        out.insert(out.end(), json.begin(), json.end());
        word(std::uint32_t(binary.size()));
        word(0x004e4942);
        out.insert(out.end(), binary.begin(), binary.end());
        write(path, out);
        return path;
    }

    void GltfTest::track(const char* path, const char* interpolation, std::vector<float> times, std::vector<float> values, int node) {
        int input = accessor(times, "SCALAR", 1), output = accessor(values, std::string(path) == "rotation" ? "VEC4" : "VEC3", std::string(path) == "rotation" ? 4 : 3);
        if (!doc.contains("animations"))
            doc["animations"] = {{{"name", "motion"}, {"samplers", Json::array()}, {"channels", Json::array()}}};
        auto& a = doc["animations"][0];
        int sampler = int(a["samplers"].size());
        a["samplers"].push_back({{"input", input}, {"output", output}, {"interpolation", interpolation}});
        a["channels"].push_back({{"sampler", sampler}, {"target", {{"node", node}, {"path", path}}}});
    }

    TEST_F(GltfTest, ExternalGltfOwnsMeshMaterialAndTexturePixels) {
        triangle();
        auto& p = doc["meshes"][0]["primitives"][0];
        int uv = accessor({0, 0, 1, 0, 0, 1}, "VEC2", 2);
        p["attributes"]["TEXCOORD_0"] = uv;
        unsigned char index[]{0, 1, 2};
        int iv = view(index);
        doc["accessors"].push_back({{"bufferView", iv}, {"componentType", 5121}, {"count", 3}, {"type", "SCALAR"}});
        p["indices"] = int(doc["accessors"].size()) - 1;
        p["material"] = 0;
        write(dir / "色.png", PNG);
        doc["images"] = {{{"name", "pixels"}, {"uri", "色.png"}}};
        // URIはUTF-8 byte列としてJSONへ記録する。
        doc["images"][0]["uri"] = "色.png";
        doc["samplers"] = {{{"magFilter", 9728}, {"minFilter", 9984}, {"wrapS", 33071}, {"wrapT", 33648}}};
        doc["textures"] = {{{"source", 0}, {"sampler", 0}}};
        doc["extensionsUsed"] = {"KHR_materials_unlit", "KHR_texture_transform"};
        doc["materials"] = {{{"name", "paint"}, {"alphaMode", "MASK"}, {"alphaCutoff", 0.25}, {"doubleSided", true}, {"emissiveFactor", {0.1, 0.2, 0.3}}, {"pbrMetallicRoughness", {{"baseColorFactor", {0.2, 0.4, 0.6, 0.8}}, {"metallicFactor", 0.3}, {"roughnessFactor", 0.7}, {"baseColorTexture", {{"index", 0}, {"extensions", {{"KHR_texture_transform", {{"offset", {0.25, 0.5}}, {"scale", {2, 3}}, {"rotation", 0.5}}}}}}}}}, {"extensions", {{"KHR_materials_unlit", Json::object()}}}}};
        auto model = GltfModel::Load(save());
        std::filesystem::remove_all(dir);
        ASSERT_EQ(model.meshes().size(), 1);
        const auto& primitive = model.meshes()[0].primitives[0];
        ASSERT_EQ(primitive.mesh.vertex_count(), 3);
        EXPECT_EQ(triangle_indices(primitive), (std::vector<std::uint32_t>{0, 1, 2}));
        EXPECT_EQ(primitive.mesh.points()[1], aho::d3::PointF(1, 0, 0));
        EXPECT_TRUE(primitive.elements.empty());
        EXPECT_TRUE(primitive.is_triangular());
        EXPECT_EQ(primitive.index_count(), 3);
        EXPECT_EQ(primitive.material, 0);
        ASSERT_EQ(model.images().size(), 1);
        EXPECT_EQ(model.images()[0].width, 2);
        EXPECT_EQ(model.images()[0].height, 1);
        EXPECT_EQ(model.images()[0].rgba, (std::vector<std::uint8_t>{255, 0, 0, 128, 0, 255, 0, 255}));
        EXPECT_EQ(model.textures()[0].image, 0);
        EXPECT_EQ(model.textures()[0].sampler.mag_filter, 9728);
        EXPECT_EQ(model.textures()[0].sampler.wrap_t, 33648);
        const auto& m = model.materials()[0];
        EXPECT_EQ(m.name, "paint");
        EXPECT_TRUE(m.unlit);
        EXPECT_TRUE(m.double_sided);
        EXPECT_EQ(m.alpha_mode, GltfAlphaMode::MASK);
        EXPECT_FLOAT_EQ(m.alpha_cutoff, 0.25f);
        EXPECT_FLOAT_EQ(m.metallic, 0.3f);
        EXPECT_FLOAT_EQ(m.roughness, 0.7f);
        EXPECT_FLOAT_EQ(m.base_color[3], 0.8f);
        EXPECT_FLOAT_EQ(m.emissive[2], 0.3f);
        EXPECT_EQ(m.base_color_texture.offset, (std::array<float, 2>{0.25f, 0.5f}));
        EXPECT_EQ(m.base_color_texture.scale, (std::array<float, 2>{2, 3}));
        EXPECT_FLOAT_EQ(m.base_color_texture.rotation, 0.5f);
        EXPECT_EQ(model.default_scene(), 0);
        EXPECT_EQ(model.scenes()[0].roots, (std::vector<int>{0}));
        auto copy = model;
        EXPECT_EQ(copy.meshes().data(), model.meshes().data());
        EXPECT_TRUE(model.warnings().empty());
    }
    TEST_F(GltfTest, GlbLoadsEmbeddedBuffersAndEmbeddedPng) {
        triangle();
        int image = view(PNG);
        doc["images"] = {{{"bufferView", image}, {"mimeType", "image/png"}}};
        doc["textures"] = {{{"source", 0}}};
        auto model = GltfModel::Load(save(true));
        EXPECT_EQ(triangle_indices(model.meshes()[0].primitives[0]), (std::vector<std::uint32_t>{0, 1, 2}));
        EXPECT_EQ(model.images()[0].rgba[3], 128);
        EXPECT_EQ(model.textures()[0].sampler.wrap_s, 10497);
    }
    TEST_F(GltfTest, InterleavedPositionsAndNormalizedColorsAndUvs) {
        triangle();
        int position = accessor({0, 0, 0, 99, 1, 0, 0, 99, 0, 1, 0, 99}, "VEC4", 4);
        doc["accessors"][position]["type"] = "VEC3";
        doc["bufferViews"][doc["accessors"][position]["bufferView"].get<int>()]["byteStride"] = 16;
        auto& p = doc["meshes"][0]["primitives"][0];
        p["attributes"]["POSITION"] = position;
        unsigned char colors[]{255, 0, 128, 255, 0, 255, 0, 128, 0, 0, 255, 0};
        int v = view(colors), a = int(doc["accessors"].size());
        doc["accessors"].push_back({{"bufferView", v}, {"componentType", 5121}, {"count", 3}, {"type", "VEC4"}, {"normalized", true}});
        p["attributes"]["COLOR_0"] = a;
        unsigned char uv[]{0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0};
        v = view(uv);
        a = int(doc["accessors"].size());
        doc["accessors"].push_back({{"bufferView", v}, {"componentType", 5123}, {"count", 3}, {"type", "VEC2"}, {"normalized", true}});
        p["attributes"]["TEXCOORD_1"] = a;
        auto model = GltfModel::Load(save());
        const auto& mesh = model.meshes()[0].primitives[0].mesh;
        EXPECT_EQ(mesh.points()[1], aho::d3::PointF(1, 0, 0));
        EXPECT_FLOAT_EQ(mesh.vertex_column<&GltfVertex::color>()[0][2], 128.f / 255);
        EXPECT_FLOAT_EQ(mesh.vertex_column<&GltfVertex::color>()[1][3], 128.f / 255);
        EXPECT_EQ(mesh.vertex_column<&GltfVertex::tex_coords>()[0][1], (std::array<float, 2>{0, 1}));
    }
    TEST_F(GltfTest, SparseAccessorWithZeroInitializedBase) {
        triangle();
        unsigned char index[]{1};
        int ids = view(index), values = accessor({5, 6, 7}, "VEC3", 3);
        int vv = doc["accessors"][values]["bufferView"];
        doc["accessors"][0] = {{"componentType", 5126}, {"count", 3}, {"type", "VEC3"}, {"sparse", {{"count", 1}, {"indices", {{"bufferView", ids}, {"componentType", 5121}}}, {"values", {{"bufferView", vv}}}}}};
        auto model = GltfModel::Load(save());
        const auto points = model.meshes()[0].primitives[0].mesh.points();
        EXPECT_EQ(points[0], aho::d3::PointF(0, 0, 0));
        EXPECT_EQ(points[1], aho::d3::PointF(5, 6, 7));
        EXPECT_EQ(points[2], aho::d3::PointF(0, 0, 0));
    }
    TEST_F(GltfTest, AllUnsignedIndexWidthsAndTopologiesKeepTheirMeaning) {
        triangle();
        const auto original = doc;
        const auto original_bytes = bytes;
        for (int width : {1, 2, 4})
            for (int mode : {0, 2, 3, 4, 5, 6}) {
                doc = original;
                bytes = original_bytes;
                std::vector<unsigned char> b;
                for (int index : {0, 2, 1}) {
                    b.push_back(index);
                    for (int c = 1; c < width; ++c)
                        b.push_back(0);
                }
                int v = view(b);
                doc["accessors"].push_back({{"bufferView", v}, {"componentType", width == 1 ? 5121 : width == 2 ? 5123
                                                                                                                : 5125},
                                            {"count", 3},
                                            {"type", "SCALAR"}});
                auto& p = doc["meshes"][0]["primitives"][0];
                p["indices"] = 1;
                p["mode"] = mode;
                auto model = GltfModel::Load(save());
                const auto& primitive = model.meshes()[0].primitives[0];
                EXPECT_EQ(primitive.is_triangular() ? triangle_indices(primitive) : primitive.elements, (std::vector<std::uint32_t>{0, 2, 1}));
                EXPECT_EQ(int(primitive.topology), mode);
                EXPECT_EQ(primitive.is_triangular(), mode >= 4);
                EXPECT_EQ(primitive.index_count(), 3u);
                EXPECT_EQ(primitive.mesh.vertex_count(), 3u);
                EXPECT_EQ(primitive.mesh.size(), mode >= 4 ? 1u : 0u);
            }
    }
    TEST_F(GltfTest, LinesKeepTheirElementPairsWithoutCreatingTriangles) {
        triangle();
        const auto positions = accessor({0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0}, "VEC3", 3);
        auto& input = doc["meshes"][0]["primitives"][0];
        input["attributes"]["POSITION"] = positions;
        input["mode"] = 1;
        const auto model = GltfModel::Load(save());
        const auto& primitive = model.meshes()[0].primitives[0];
        EXPECT_FALSE(primitive.is_triangular());
        EXPECT_EQ(primitive.mesh.vertex_count(), 4u);
        EXPECT_EQ(primitive.mesh.size(), 0u);
        EXPECT_TRUE(primitive.mesh.triangle_data().empty());
        EXPECT_EQ(primitive.elements, (std::vector<std::uint32_t>{0, 1, 2, 3}));
        EXPECT_EQ(primitive.index_count(), 4u);
        EXPECT_EQ(primitive.mesh.vertex_column<&GltfVertex::normal>().size(), 4u);
    }

    TEST_F(GltfTest, TriangleStripAndFanBecomeOwnedMeshesWithConsistentWinding) {
        triangle();
        const auto positions = accessor({0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0}, "VEC3", 3);
        auto& input = doc["meshes"][0]["primitives"][0];
        input["attributes"]["POSITION"] = positions;
        input["mode"] = 5;
        auto strip_model = GltfModel::Load(save());
        const auto& strip = strip_model.meshes()[0].primitives[0];
        EXPECT_EQ(triangle_indices(strip), (std::vector<std::uint32_t>{0, 1, 2, 2, 1, 3}));
        EXPECT_EQ(strip.mesh.vertex_count(), 4u);
        EXPECT_EQ(strip.mesh.size(), 2u);
        EXPECT_EQ(strip.index_count(), 6u);
        EXPECT_TRUE(strip.elements.empty());
        EXPECT_DOUBLE_EQ(strip.mesh.area(), 1);
        const auto normals = strip.mesh.vertex_column<&GltfVertex::normal>();
        static_assert(std::same_as<decltype(normals), const std::span<const std::array<float, 3>>>);
        ASSERT_EQ(normals.size(), 4u);
        EXPECT_EQ(normals[3], (std::array<float, 3>{0, 0, 1}));
        EXPECT_EQ(normals.data() + 1, &normals[1]);
        input["mode"] = 6;
        auto fan_model = GltfModel::Load(save());
        const auto& fan = fan_model.meshes()[0].primitives[0];
        EXPECT_EQ(triangle_indices(fan), (std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3}));
        EXPECT_EQ(fan.mesh.triangle_data().size(), 2u);
        EXPECT_TRUE(fan.elements.empty());
        EXPECT_EQ(fan.index_count(), 6u);
        EXPECT_EQ(triangle_indices(strip), (std::vector<std::uint32_t>{0, 1, 2, 2, 1, 3}));
    }

    TEST_F(GltfTest, ParentRotationAndMatrixNodesUseColumnMajorTransforms) {
        triangle();
        const double root_half = std::sqrt(0.5);
        doc["nodes"] = {{{"translation", {10, 0, 0}}, {"rotation", {0, 0, root_half, root_half}}, {"children", {1}}}, {{"mesh", 0}, {"matrix", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1}}}};
        auto model = GltfModel::Load(save());
        auto pose = model.rest_pose();
        EXPECT_EQ(model.nodes()[1].parent, 0);
        EXPECT_TRUE(model.nodes()[1].has_matrix);
        EXPECT_NEAR(pose.world_matrices[1][12], 10, 1e-5);
        EXPECT_NEAR(pose.world_matrices[1][13], 2, 1e-5);
        EXPECT_NEAR(pose.world_matrices[1][1], 1, 1e-5);
    }
    TEST_F(GltfTest, LinearTrsInterpolatesAndRotationUsesShortestSlerp) {
        triangle();
        track("translation", "LINEAR", {0, 2}, {0, 0, 0, 4, 2, 0});
        track("scale", "LINEAR", {0, 2}, {1, 1, 1, 3, 3, 3});
        track("rotation", "LINEAR", {0, 2}, {0, 0, 0, 1, 0, 0, -1, 0});
        auto model = GltfModel::Load(save());
        auto p = model.sample(0, 1, false);
        EXPECT_FLOAT_EQ(p.world_matrices[0][12], 2);
        EXPECT_FLOAT_EQ(p.world_matrices[0][13], 1);
        EXPECT_NEAR(p.world_matrices[0][0], 0, 1e-5);
        EXPECT_NEAR(p.world_matrices[0][1], -2, 1e-5);
        auto rest = model.rest_pose();
        EXPECT_FLOAT_EQ(rest.world_matrices[0][12], 0);
        EXPECT_FLOAT_EQ(rest.world_matrices[0][0], 1);
        EXPECT_EQ(model.animations()[0].name, "motion");
    }
    TEST_F(GltfTest, StepAndLoopHandleNonzeroStartNegativeTimesAndEndpoints) {
        triangle();
        track("translation", "STEP", {1, 3}, {2, 0, 0, 8, 0, 0});
        auto model = GltfModel::Load(save());
        EXPECT_FLOAT_EQ(model.sample(0, 2, false).world_matrices[0][12], 2);
        EXPECT_FLOAT_EQ(model.sample(0, 3, false).world_matrices[0][12], 8);
        EXPECT_FLOAT_EQ(model.sample(0, 3, true).world_matrices[0][12], 2);
        EXPECT_FLOAT_EQ(model.sample(0, -1, true).world_matrices[0][12], 2);
        EXPECT_FLOAT_EQ(model.sample(0, 100, false).world_matrices[0][12], 8);
        EXPECT_FLOAT_EQ(model.sample(0, -100, false).world_matrices[0][12], 2);
        EXPECT_THROW(model.sample(1, 0), std::out_of_range);
        EXPECT_THROW(model.sample(0, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    }
    TEST_F(GltfTest, CubicSplineScalesTangentsByKeyInterval) {
        triangle();
        track("translation", "CUBICSPLINE", {0, 2}, {0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0});
        auto model = GltfModel::Load(save());
        EXPECT_FLOAT_EQ(model.sample(0, 1, false).world_matrices[0][12], 1.5f);
    }
    TEST_F(GltfTest, SingleKeyAnimationAndQuaternionSignDoNotChangeThePose) {
        triangle();
        track("translation", "LINEAR", {2}, {3, 4, 5});
        track("rotation", "LINEAR", {2}, {0, 0, 0, -1});
        auto model = GltfModel::Load(save());
        auto p = model.sample(0, -100, true);
        EXPECT_FLOAT_EQ(p.world_matrices[0][12], 3);
        EXPECT_FLOAT_EQ(p.world_matrices[0][0], 1);
        EXPECT_FLOAT_EQ(model.animations()[0].start, 2);
        EXPECT_FLOAT_EQ(model.animations()[0].end, 2);
    }
    TEST_F(GltfTest, SkinPaletteIncludesMeshInverseJointWorldAndInverseBind) {
        triangle();
        auto& p = doc["meshes"][0]["primitives"][0];
        unsigned char joints[12]{};
        int v = view(joints), a = int(doc["accessors"].size());
        doc["accessors"].push_back({{"bufferView", v}, {"componentType", 5121}, {"count", 3}, {"type", "VEC4"}});
        p["attributes"]["JOINTS_0"] = a;
        p["attributes"]["WEIGHTS_0"] = accessor({2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0}, "VEC4", 4);
        int inverse_bind = accessor({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 1}, "MAT4", 16);
        doc["nodes"] = {{{"mesh", 0}, {"skin", 0}, {"translation", {2, 0, 0}}}, {{"translation", {5, 0, 0}}}};
        doc["scenes"][0]["nodes"] = {0, 1};
        doc["skins"] = {{{"joints", {1}}, {"inverseBindMatrices", inverse_bind}, {"skeleton", 1}}};
        track("translation", "LINEAR", {0, 2}, {5, 0, 0, 9, 0, 0}, 1);
        auto model = GltfModel::Load(save());
        EXPECT_FLOAT_EQ(model.meshes()[0].primitives[0].mesh.vertex_column<&GltfVertex::weights>()[0][0], 1);
        EXPECT_FLOAT_EQ(model.skin_matrices(model.rest_pose(), 0)[0][12], 2);
        EXPECT_FLOAT_EQ(model.skin_matrices(model.sample(0, 1, false), 0)[0][12], 4);
        EXPECT_TRUE(model.skin_matrices(model.rest_pose(), 1).empty());
        EXPECT_THROW(model.skin_matrices({}, 0), std::invalid_argument);
        EXPECT_THROW(model.skin_matrices(model.rest_pose(), 2), std::out_of_range);
        EXPECT_EQ(model.skins()[0].skeleton, 1);
    }
    TEST_F(GltfTest, StandaloneImageDecodeOwnsRgbaAndRejectsBadFiles) {
        write(dir / "image.png", PNG);
        auto image = load_image(dir / "image.png");
        EXPECT_EQ(image.rgba, (std::vector<std::uint8_t>{255, 0, 0, 128, 0, 255, 0, 255}));
        EXPECT_EQ(image.width, 2);
        EXPECT_THROW(load_image(dir / "missing.png"), std::runtime_error);
        unsigned char bad[]{1, 2, 3};
        write(dir / "bad.png", bad);
        EXPECT_THROW(load_image(dir / "bad.png"), std::runtime_error);
    }
    TEST_F(GltfTest, UnsupportedRequiredExtensionsAndMorphTargetsFailExplicitly) {
        triangle();
        auto base = doc;
        doc["extensionsRequired"] = {"KHR_draco_mesh_compression"};
        EXPECT_THROW(GltfModel::Load(save()), std::runtime_error);
        doc = base;
        doc["meshes"][0]["primitives"][0]["targets"] = {{{"POSITION", 0}}};
        EXPECT_THROW(GltfModel::Load(save()), std::runtime_error);
        doc = base;
        doc["extensionsUsed"] = {"CUSTOM_optional"};
        auto model = GltfModel::Load(save());
        EXPECT_NE(model.warnings().find("CUSTOM_optional"), std::string::npos);
    }
    TEST_F(GltfTest, InvalidBoundsIndicesTypesAndNonfiniteValuesAreRejected) {
        triangle();
        auto base = doc;
        auto original = bytes;
        for (auto change : {0, 1, 2, 3, 4, 5}) {
            SCOPED_TRACE(change);
            doc = base;
            bytes = original;
            if (change == 0)
                doc["accessors"][0]["byteOffset"] = 1000;
            if (change == 1)
                doc["bufferViews"][0]["byteStride"] = 4;
            if (change == 2)
                doc["accessors"][0]["componentType"] = 5123;
            if (change == 3)
                doc["accessors"][0]["type"] = "VEC2";
            if (change == 4) {
                unsigned char b[]{0, 1, 3};
                int v = view(b);
                doc["accessors"].push_back({{"bufferView", v}, {"componentType", 5121}, {"count", 3}, {"type", "SCALAR"}});
                doc["meshes"][0]["primitives"][0]["indices"] = 1;
            }
            if (change == 5) {
                bytes[0] = 0;
                bytes[1] = 0;
                bytes[2] = 128;
                bytes[3] = 127;
            }
            EXPECT_THROW(GltfModel::Load(save()), std::runtime_error);
        }
    }
    TEST_F(GltfTest, CyclesDuplicateParentsAndInvalidSceneRootsAreRejected) {
        triangle();
        auto base = doc;
        for (int change = 0; change < 4; ++change) {
            doc = base;
            if (change == 0) {
                doc["nodes"] = {{{"children", {1}}}, {{"children", {0}}}};
                doc["scenes"] = Json::array();
                doc.erase("scene");
            }
            if (change == 1)
                doc["nodes"] = {{{"children", {1, 1}}}, Json::object()};
            if (change == 2) {
                doc["nodes"] = {{{"children", {1}}}, Json::object()};
                doc["scenes"][0]["nodes"] = {1};
            }
            if (change == 3)
                doc["nodes"][0]["mesh"] = 2;
            EXPECT_THROW(GltfModel::Load(save()), std::runtime_error);
        }
    }
    TEST_F(GltfTest, InvalidAnimationKeysTargetsAndOutputCountsAreRejected) {
        triangle();
        track("translation", "LINEAR", {0, 2}, {0, 0, 0, 1, 0, 0});
        auto base = doc;
        auto binary = bytes;
        for (int change = 0; change < 5; ++change) {
            doc = base;
            bytes = binary;
            auto& a = doc["animations"][0];
            if (change == 0) {
                int input = a["samplers"][0]["input"];
                int view_index = doc["accessors"][input]["bufferView"];
                std::size_t offset = doc["bufferViews"][view_index]["byteOffset"];
                std::fill_n(bytes.begin() + offset + 4, 4, 0);
            }
            if (change == 1)
                a["channels"][0]["target"]["node"] = 5;
            if (change == 2)
                doc["accessors"][a["samplers"][0]["output"].get<int>()]["count"] = 1;
            if (change == 3)
                a["channels"][0]["target"]["path"] = "weights";
            if (change == 4)
                a["channels"].push_back(a["channels"][0]);
            EXPECT_THROW(GltfModel::Load(save()), std::runtime_error);
        }
    }
    TEST_F(GltfTest, MissingMalformedAndTruncatedModelFilesAreRejected) {
        EXPECT_THROW(GltfModel::Load(dir / "absent.glb"), std::runtime_error);
        std::ofstream(dir / "bad.gltf") << "not JSON";
        EXPECT_THROW(GltfModel::Load(dir / "bad.gltf"), std::runtime_error);
        triangle();
        auto path = save(true);
        std::filesystem::resize_file(path, 16);
        EXPECT_THROW(GltfModel::Load(path), std::runtime_error);
    }

    TEST_F(GltfTest, CubicRotationNormalizesAndLinearRotationTakesTheShortArc) {
        triangle();
        track("rotation", "CUBICSPLINE", {0, 2}, {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0});
        auto cubic = GltfModel::Load(save());
        auto pose = cubic.sample(0, 1, false);
        EXPECT_NEAR(pose.world_matrices[0][0], 0, 1e-5);
        EXPECT_NEAR(pose.world_matrices[0][1], 1, 1e-5);
        doc.erase("animations");
        const float h = std::sqrt(0.5f);
        track("rotation", "LINEAR", {0, 2}, {0, 0, 0, 1, 0, 0, -h, -h});
        auto linear = GltfModel::Load(save());
        pose = linear.sample(0, 1, false);
        EXPECT_NEAR(pose.world_matrices[0][0], h, 1e-5);
        EXPECT_NEAR(pose.world_matrices[0][1], h, 1e-5);
    }
    TEST_F(GltfTest, DataUriImageAndAllPbrTextureBindingsRetainTheirMetadata) {
        triangle();
        auto& primitive = doc["meshes"][0]["primitives"][0];
        primitive["attributes"]["NORMAL"] = accessor({0, 0, 1, 0, 0, 1, 0, 0, 1}, "VEC3", 3);
        primitive["attributes"]["TANGENT"] = accessor({1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1}, "VEC4", 4);
        primitive["attributes"]["TEXCOORD_0"] = accessor({0, 0, 1, 0, 0, 1}, "VEC2", 2);
        primitive["material"] = 0;
        doc["images"] = {{{"uri", "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAYAAAD0In+KAAAAEUlEQVR4nGP4z8DQwPCf4T8ADn0Dfur2k8AAAAAASUVORK5CYII="}}};
        doc["textures"] = {{{"source", 0}}};
        doc["materials"] = {{{"alphaMode", "BLEND"}, {"normalTexture", {{"index", 0}, {"scale", 0.25}}}, {"occlusionTexture", {{"index", 0}, {"strength", 0.75}}}, {"emissiveTexture", {{"index", 0}}}, {"pbrMetallicRoughness", {{"metallicRoughnessTexture", {{"index", 0}}}}}}};
        auto model = GltfModel::Load(save());
        EXPECT_TRUE(model.meshes()[0].primitives[0].has_normals);
        EXPECT_TRUE(model.meshes()[0].primitives[0].has_tangents);
        EXPECT_FLOAT_EQ(model.meshes()[0].primitives[0].mesh.vertex_column<&GltfVertex::tangent>()[2][3], -1);
        EXPECT_EQ(model.materials()[0].alpha_mode, GltfAlphaMode::BLEND);
        EXPECT_FLOAT_EQ(model.materials()[0].normal_scale, 0.25f);
        EXPECT_FLOAT_EQ(model.materials()[0].occlusion_strength, 0.75f);
        EXPECT_EQ(model.materials()[0].normal_texture.texture, 0);
        EXPECT_EQ(model.materials()[0].occlusion_texture.texture, 0);
        EXPECT_EQ(model.materials()[0].emissive_texture.texture, 0);
        EXPECT_EQ(model.materials()[0].metallic_roughness_texture.texture, 0);
        EXPECT_EQ(model.images()[0].rgba[3], 128);
    }
    TEST_F(GltfTest, SparseOutOfRangeUnorderedAndTruncatedDataAreRejected) {
        triangle();
        unsigned char ids[]{2, 1};
        int index_view = view(ids);
        int values = accessor({1, 2, 3, 4, 5, 6}, "VEC3", 3);
        int value_view = doc["accessors"][values]["bufferView"];
        doc["accessors"][0]["sparse"] = {{"count", 2}, {"indices", {{"bufferView", index_view}, {"componentType", 5121}}}, {"values", {{"bufferView", value_view}}}};
        auto original = doc;
        auto original_bytes = bytes;
        EXPECT_THROW(GltfModel::Load(save()), std::runtime_error);
        auto offset = doc["bufferViews"][index_view]["byteOffset"].get<std::size_t>();
        bytes[offset] = 0;
        bytes[offset + 1] = 3;
        EXPECT_THROW(GltfModel::Load(save()), std::runtime_error);
        bytes = original_bytes;
        bytes[offset] = 0;
        bytes[offset + 1] = 1;
        doc["accessors"][0]["sparse"]["values"]["byteOffset"] = 1000;
        EXPECT_THROW(GltfModel::Load(save()), std::runtime_error);
    }

    TEST_F(GltfTest, InverseBindAccessorMayContainTrailingUnusedMatrices) {
        triangle();
        doc["skins"] = {{{"joints", {0}}, {"inverseBindMatrices", accessor({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1}, "MAT4", 16)}}};
        auto model = GltfModel::Load(save());
        ASSERT_EQ(model.skins().size(), 1);
        ASSERT_EQ(model.skins()[0].inverse_bind_matrices.size(), 1);
        EXPECT_FLOAT_EQ(model.skins()[0].inverse_bind_matrices[0][12], 0);
    }
    TEST_F(GltfTest, NormalizedSignedQuaternionKeysAreDecodedBeforeSlerp) {
        triangle();
        int input = accessor({0, 2}, "SCALAR", 1);
        unsigned char encoded[]{0, 0, 0, 127, 0, 0, 127, 0};
        int bv = view(encoded), output = int(doc["accessors"].size());
        doc["accessors"].push_back({{"bufferView", bv}, {"componentType", 5120}, {"normalized", true}, {"count", 2}, {"type", "VEC4"}});
        doc["animations"] = {{{"samplers", {{{"input", input}, {"output", output}}}}, {"channels", {{{"sampler", 0}, {"target", {{"node", 0}, {"path", "rotation"}}}}}}}};
        auto model = GltfModel::Load(save());
        EXPECT_NEAR(model.sample(0, 1, false).world_matrices[0][0], 0, 1e-5);
        EXPECT_NEAR(model.sample(0, 1, false).world_matrices[0][1], 1, 1e-5);
    }
} // namespace
