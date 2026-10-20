#pragma once
#include <AHO/define.hpp>
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
    GltfImage loadImage(const std::filesystem::path& path);

    struct GltfSampler {
        int magFilter = 9729, minFilter = 9987; // LINEAR / LINEAR_MIPMAP_LINEAR
        int wrapS = 10497, wrapT = 10497;       // REPEAT
    };
    struct GltfTexture {
        std::string name;
        int image = -1;
        GltfSampler sampler;
    };
    struct GltfTextureInfo {
        int texture = -1, texCoord = 0;
        std::array<float, 2> offset{0, 0}, scale{1, 1};
        float rotation = 0; // KHR_texture_transform、radian
    };
    enum class GltfAlphaMode {
        Opaque,
        Mask,
        Blend
    };
    struct GltfMaterial {
        std::string name;
        std::array<float, 4> baseColor{1, 1, 1, 1};
        std::array<float, 3> emissive{0, 0, 0};
        float metallic = 1, roughness = 1, alphaCutoff = 0.5f;
        float normalScale = 1, occlusionStrength = 1;
        GltfTextureInfo baseColorTexture, metallicRoughnessTexture, normalTexture, occlusionTexture, emissiveTexture;
        GltfAlphaMode alphaMode = GltfAlphaMode::Opaque;
        bool doubleSided = false, unlit = false;
    };
    struct GltfVertex {
        std::array<float, 3> position{}, normal{0, 0, 1};
        std::array<float, 4> tangent{1, 0, 0, 1}, color{1, 1, 1, 1};
        std::array<std::array<float, 2>, 2> texCoords{};
        std::array<std::uint16_t, 4> joints{};
        std::array<float, 4> weights{};
    };
    enum class GltfTopology {
        Points,
        Lines,
        LineLoop,
        LineStrip,
        Triangles,
        TriangleStrip,
        TriangleFan
    };
    struct GltfPrimitive {
        std::vector<GltfVertex> vertices;
        std::vector<std::uint32_t> indices; // 非indexed入力にも連番を生成する。
        int material = -1;                  // -1はglTFの既定material
        GltfTopology topology = GltfTopology::Triangles;
        bool hasNormals = false, hasTangents = false, hasSkin = false;
        std::array<bool, 2> hasTexCoords{};
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
        bool hasMatrix = false;
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
        std::vector<GltfMatrix> inverseBindMatrices;
    };
    enum class GltfInterpolation {
        Linear,
        Step,
        CubicSpline
    };
    enum class GltfAnimationPath {
        Translation,
        Rotation,
        Scale
    };
    struct GltfAnimationTrack {
        int node = -1;
        GltfAnimationPath path = GltfAnimationPath::Translation;
        GltfInterpolation interpolation = GltfInterpolation::Linear;
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
        std::vector<GltfMatrix> localMatrices, worldMatrices;
    };

    // GPU不要の不変asset。tinygltfの型・Vulkanの型は公開しない。
    // 読込/非対応形式はruntime_error、sampleのindex/time不正はout_of_range/invalid_argument。
    class GltfModel {
        struct Data;
        std::shared_ptr<const Data> _data;
        explicit GltfModel(std::shared_ptr<const Data> data);

    public:
        static GltfModel load(const std::filesystem::path& path);
        std::span<const GltfMesh> meshes() const;
        std::span<const GltfMaterial> materials() const;
        std::span<const GltfImage> images() const;
        std::span<const GltfTexture> textures() const;
        std::span<const GltfNode> nodes() const;
        std::span<const GltfScene> scenes() const;
        std::span<const GltfSkin> skins() const;
        std::span<const GltfAnimation> animations() const;
        const std::string& warnings() const;
        int defaultScene() const;
        GltfPose restPose() const;
        // timeはclipの時刻（秒）。loop=falseは端へclamp、trueは[start,end)へwrapする。
        GltfPose sample(std::size_t animation, double time, bool loop = true) const;
        // inverse(mesh world) * joint world * inverseBind。GPUのjoint paletteへ渡す。
        std::vector<GltfMatrix> skinMatrices(const GltfPose& pose, std::size_t meshNode) const;
    };
} // namespace AHO_NAMESPACE::x::resource
