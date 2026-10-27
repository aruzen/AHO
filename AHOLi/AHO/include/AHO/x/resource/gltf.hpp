#pragma once
#include <AHO/define.hpp>
#include <AHO/x/core/mesh.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace AHO_NAMESPACE::x::resource {
    using GltfMatrix = std::array<float, 16>; // column-major、列ベクトル。glTFの右手系を維持する。

    struct GltfImage {
        std::string name;
        std::uint32_t width = 0, height = 0;
        std::vector<std::uint8_t> rgba; // RGBA8。反転・gamma変換なし。
    };
    // PNG/JPEGをRGBA8へ展開する。ファイルを閉じた後も画素を所有する。
    GltfImage load_image(const std::filesystem::path& path);

    struct GltfSampler {
        int mag_filter = 9729, min_filter = 9987; // LINEAR / LINEAR_MIPMAP_LINEAR
        int wrap_s = 10497, wrap_t = 10497;       // REPEAT
    };
    struct GltfTexture {
        std::string name;
        int image = -1;
        GltfSampler sampler;
    };
    struct GltfTextureInfo {
        int texture = -1, tex_coord = 0;
        std::array<float, 2> offset{0, 0}, scale{1, 1};
        float rotation = 0; // KHR_texture_transform、radian
    };
    enum class GltfAlphaMode {
        OPAQUE,
        MASK,
        BLEND
    };
    struct GltfMaterial {
        std::string name;
        std::array<float, 4> base_color{1, 1, 1, 1};
        std::array<float, 3> emissive{0, 0, 0};
        float metallic = 1, roughness = 1, alpha_cutoff = 0.5f;
        float normal_scale = 1, occlusion_strength = 1;
        GltfTextureInfo base_color_texture, metallic_roughness_texture, normal_texture, occlusion_texture, emissive_texture;
        GltfAlphaMode alpha_mode = GltfAlphaMode::OPAQUE;
        bool double_sided = false, unlit = false;
    };
    struct GltfVertex {
        std::array<float, 3> normal{0, 0, 1};
        std::array<float, 4> tangent{1, 0, 0, 1}, color{1, 1, 1, 1};
        std::array<std::array<float, 2>, 2> tex_coords{};
        std::array<std::uint16_t, 4> joints{};
        std::array<float, 4> weights{};
    };
    struct GltfTriangle {};
    using GltfGeometry = AHO_NAMESPACE::Polygon<
        float, VSL_NAMESPACE::D3, AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D3>::Value, std::uint32_t>;
    // glTFの固定schemaは明示し、reflection設定によらず同じ公開型を使う。
    using GltfVertexMembers = MeshAttributeMembers<
        &GltfVertex::normal, &GltfVertex::tangent, &GltfVertex::color,
        &GltfVertex::tex_coords, &GltfVertex::joints, &GltfVertex::weights>;
    using GltfVertexStorage = MeshAttributes<GltfVertex, GltfVertexMembers>;
    using GltfTriangleStorage = MeshAttributes<GltfTriangle, MeshAttributeMembers<>>;
    using GltfMeshData = Mesh<GltfGeometry, GltfVertex, GltfTriangle, GltfVertexStorage, GltfTriangleStorage>;
    enum class GltfTopology {
        POINTS,
        LINES,
        LINE_LOOP,
        LINE_STRIP,
        TRIANGLES,
        TRIANGLE_STRIP,
        TRIANGLE_FAN
    };
    struct GltfPrimitive {
        GltfMeshData mesh;
        // 点・線の要素順。三角形はmeshへ展開し、strip/fanの頂点順も保持する。
        std::vector<std::uint32_t> elements;
        int material = -1; // -1はglTFの既定material
        GltfTopology topology = GltfTopology::TRIANGLES;
        bool has_normals = false, has_tangents = false, has_skin = false;
        std::array<bool, 2> has_tex_coords{};

        bool is_triangular() const noexcept;
        std::size_t index_count() const noexcept;
    };
    struct GltfMesh {
        std::string name;
        std::vector<GltfPrimitive> primitives;
    };
    struct GltfTransform {
        std::array<float, 3> translation{0, 0, 0}, scale{1, 1, 1};
        std::array<float, 4> rotation{0, 0, 0, 1}; // quaternion x/y/z/w
    };
    struct GltfNode {
        std::string name;
        int parent = -1, mesh = -1, skin = -1;
        std::vector<int> children;
        GltfTransform transform;
        bool has_matrix = false;
        GltfMatrix matrix{};
    };
    struct GltfScene {
        std::string name;
        std::vector<int> roots;
    };
    struct GltfSkin {
        std::string name;
        int skeleton = -1;
        std::vector<int> joints;
        std::vector<GltfMatrix> inverse_bind_matrices;
    };
    enum class GltfInterpolation {
        LINEAR,
        STEP,
        CUBIC_SPLINE
    };
    enum class GltfAnimationPath {
        TRANSLATION,
        ROTATION,
        SCALE
    };
    struct GltfAnimationTrack {
        int node = -1;
        GltfAnimationPath path = GltfAnimationPath::TRANSLATION;
        GltfInterpolation interpolation = GltfInterpolation::LINEAR;
        std::vector<float> times;
        // TRSはxyz/xyzw。CubicSplineは各keyのin tangent/value/out tangentの順。
        std::vector<std::array<float, 4>> values;
    };
    struct GltfAnimation {
        std::string name;
        float start = 0, end = 0;
        std::vector<GltfAnimationTrack> tracks;
    };
    struct GltfPose {
        std::vector<GltfMatrix> local_matrices, world_matrices;
    };

    // GPU不要の不変asset。tinygltfの型・Vulkanの型は公開しない。
    // 読込/非対応形式はruntime_error、sampleのindex/time不正はout_of_range/invalid_argument。
    class GltfModel {
        struct Data;
        std::shared_ptr<const Data> _data;
        explicit GltfModel(std::shared_ptr<const Data> data);

    public:
        static GltfModel Load(const std::filesystem::path& path);
        std::span<const GltfMesh> meshes() const;
        std::span<const GltfMaterial> materials() const;
        std::span<const GltfImage> images() const;
        std::span<const GltfTexture> textures() const;
        std::span<const GltfNode> nodes() const;
        std::span<const GltfScene> scenes() const;
        std::span<const GltfSkin> skins() const;
        std::span<const GltfAnimation> animations() const;
        const std::string& warnings() const;
        int default_scene() const;
        GltfPose rest_pose() const;
        // timeはclipの時刻（秒）。loop=falseは端へclamp、trueは[start,end)へwrapする。
        GltfPose sample(std::size_t animation, double time, bool loop = true) const;
        // inverse(mesh world) * joint world * inverseBind。GPUのjoint paletteへ渡す。
        std::vector<GltfMatrix> skin_matrices(const GltfPose& pose, std::size_t mesh_node) const;
    };

    inline bool GltfPrimitive::is_triangular() const noexcept {
        return topology == GltfTopology::TRIANGLES || topology == GltfTopology::TRIANGLE_STRIP || topology == GltfTopology::TRIANGLE_FAN;
    }

    inline std::size_t GltfPrimitive::index_count() const noexcept {
        return is_triangular() ? mesh.size() * 3 : elements.size();
    }
} // namespace AHO_NAMESPACE::x::resource
