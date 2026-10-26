#include <AHO/x/resource/gltf.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <tiny_gltf.h>

namespace AHO_NAMESPACE::x::resource {
    namespace {
        struct AccessorData {
            std::size_t count = 0, components = 0;
            int type = 0;
            bool normalized = false;
            std::vector<double> values;
        };

        [[noreturn]] void fail(const std::string& reason) {
            throw std::runtime_error("glTF: " + reason);
        }
        std::size_t product(std::size_t a, std::size_t b) {
            if (b && a > std::numeric_limits<std::size_t>::max() / b)
                fail("data size overflow");
            return a * b;
        }
        template<class T>
        const T& at(const std::vector<T>& v, int i, const char* what) {
            if (i < 0 || std::size_t(i) >= v.size())
                fail(std::string("invalid ") + what + " index");
            return v[std::size_t(i)];
        }
        void optional_index(int i, std::size_t count, const char* what) {
            if (i < -1 || (i >= 0 && std::size_t(i) >= count))
                fail(std::string("invalid ") + what + " index");
        }
        float scalar(double x) {
            if (!std::isfinite(x) || std::abs(x) > std::numeric_limits<float>::max())
                fail("non-finite or oversized number");
            return static_cast<float>(x);
        }
        GltfMatrix identity() {
            return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        }
        GltfMatrix multiply(const GltfMatrix& a, const GltfMatrix& b) {
            GltfMatrix result{};
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r) {
                    double value = 0;
                    for (int k = 0; k < 4; ++k)
                        value += double(a[k * 4 + r]) * b[c * 4 + k];
                    result[c * 4 + r] = scalar(value);
                }
            return result;
        }
        GltfMatrix inverse(const GltfMatrix& m) {
            double rows[4][8]{};
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c) {
                    rows[r][c] = m[c * 4 + r];
                    rows[r][4 + c] = (r == c);
                }
            for (int c = 0; c < 4; ++c) {
                int pivot = c;
                for (int r = c + 1; r < 4; ++r)
                    if (std::abs(rows[r][c]) > std::abs(rows[pivot][c]))
                        pivot = r;
                if (rows[pivot][c] == 0)
                    fail("cannot build skin palette for a singular mesh transform");
                for (int k = 0; k < 8; ++k)
                    std::swap(rows[c][k], rows[pivot][k]);
                const double d = rows[c][c];
                for (double& x : rows[c])
                    x /= d;
                for (int r = 0; r < 4; ++r)
                    if (r != c) {
                        const double factor = rows[r][c];
                        for (int k = 0; k < 8; ++k)
                            rows[r][k] -= factor * rows[c][k];
                    }
            }
            GltfMatrix out;
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    out[c * 4 + r] = scalar(rows[r][c + 4]);
            return out;
        }
        std::array<float, 4> normalize(std::array<float, 4> q) {
            double length = 0;
            for (float v : q)
                length += double(v) * v;
            if (!(length > 0) || !std::isfinite(length))
                fail("invalid zero/non-finite quaternion");
            length = std::sqrt(length);
            for (float& v : q)
                v = scalar(v / length);
            return q;
        }
        GltfMatrix matrix(const GltfTransform& t) {
            auto q = normalize(t.rotation);
            const double x = q[0], y = q[1], z = q[2], w = q[3];
            GltfMatrix m = identity();
            m[0] = scalar((1 - 2 * (y * y + z * z)) * t.scale[0]);
            m[1] = scalar(2 * (x * y + z * w) * t.scale[0]);
            m[2] = scalar(2 * (x * z - y * w) * t.scale[0]);
            m[4] = scalar(2 * (x * y - z * w) * t.scale[1]);
            m[5] = scalar((1 - 2 * (x * x + z * z)) * t.scale[1]);
            m[6] = scalar(2 * (y * z + x * w) * t.scale[1]);
            m[8] = scalar(2 * (x * z + y * w) * t.scale[2]);
            m[9] = scalar(2 * (y * z - x * w) * t.scale[2]);
            m[10] = scalar((1 - 2 * (x * x + y * y)) * t.scale[2]);
            std::copy(t.translation.begin(), t.translation.end(), m.begin() + 12);
            return m;
        }
        template<std::size_t N>
        void copy_numbers(const std::vector<double>& src, std::array<float, N>& dst) {
            if (src.size() != N)
                fail("invalid vector/matrix component count");
            std::transform(src.begin(), src.end(), dst.begin(), scalar);
        }
        std::size_t component_size(int type) {
            switch (type) {
                case TINYGLTF_COMPONENT_TYPE_BYTE:
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    return 1;
                case TINYGLTF_COMPONENT_TYPE_SHORT:
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    return 2;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return 4;
                default:
                    fail("unsupported accessor component type");
            }
        }
        // バッファをnative型へreinterpret_castしない。非整列アドレスとlittle-endianを扱う。
        double read_component(const unsigned char* p, int type, bool normalized) {
            const auto n = component_size(type);
            std::uint32_t bits = 0;
            for (std::size_t i = 0; i < n; ++i)
                bits |= std::uint32_t(p[i]) << (8 * i);
            switch (type) {
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return scalar(std::bit_cast<float>(bits));
                case TINYGLTF_COMPONENT_TYPE_BYTE: {
                    auto x = std::bit_cast<std::int8_t>(std::uint8_t(bits));
                    return normalized ? std::max(-1.0, x / 127.0) : x;
                }
                case TINYGLTF_COMPONENT_TYPE_SHORT: {
                    auto x = std::bit_cast<std::int16_t>(std::uint16_t(bits));
                    return normalized ? std::max(-1.0, x / 32767.0) : x;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    return normalized ? bits / 255.0 : bits;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    return normalized ? bits / 65535.0 : bits;
                default:
                    return normalized ? bits / 4294967295.0 : bits;
            }
        }
        const unsigned char* storage(const tinygltf::Model& m, int view_index, std::size_t offset, std::size_t count, std::size_t stride, std::size_t bytes) {
            const auto& view = at(m.bufferViews, view_index, "bufferView");
            const auto& buffer = at(m.buffers, view.buffer, "buffer").data;
            if (view.byteOffset > buffer.size() || view.byteLength > buffer.size() - view.byteOffset || offset > view.byteLength)
                fail("bufferView outside buffer");
            const auto remaining = view.byteLength - offset;
            if (count && (bytes > remaining || stride < bytes || count - 1 > (remaining - bytes) / stride))
                fail("accessor outside bufferView");
            // 0件のviewでもnullptrへの算術を避ける。
            if (buffer.empty())
                return nullptr;
            return buffer.data() + view.byteOffset + offset;
        }
        AccessorData decode(const tinygltf::Model& m, int index, int shape) {
            const auto& a = at(m.accessors, index, "accessor");
            if (a.type != shape)
                fail("unexpected accessor shape");
            const int n = tinygltf::GetNumComponentsInType(std::uint32_t(a.type));
            const auto bytes = product(component_size(a.componentType), std::size_t(n));
            if (!a.count || a.count > std::numeric_limits<std::uint32_t>::max())
                fail("invalid accessor count");
            if (a.normalized && (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT || a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT))
                fail("invalid normalized accessor");
            AccessorData out{a.count, std::size_t(n), a.componentType, a.normalized, {}};
            const unsigned char* base = nullptr;
            std::size_t stride = bytes;
            if (a.bufferView >= 0) {
                const auto& view = at(m.bufferViews, a.bufferView, "bufferView");
                stride = view.byteStride ? view.byteStride : bytes;
                base = storage(m, a.bufferView, a.byteOffset, a.count, stride, bytes);
            } else if (a.bufferView != -1 || a.byteOffset || !a.sparse.isSparse)
                fail("accessor has no data");
            out.values.resize(product(a.count, std::size_t(n)), 0);
            if (base)
                for (std::size_t i = 0; i < a.count; ++i)
                    for (int c = 0; c < n; ++c)
                        out.values[i * n + c] = read_component(base + i * stride + c * component_size(a.componentType), a.componentType, a.normalized);
            if (a.sparse.isSparse) {
                const auto& s = a.sparse;
                if (s.count <= 0 || std::size_t(s.count) > a.count)
                    fail("invalid sparse count/offset");
                const auto type = s.indices.componentType;
                if (type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE && type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT && type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    fail("invalid sparse index type");
                if (at(m.bufferViews, s.indices.bufferView, "sparse indices").byteStride || at(m.bufferViews, s.values.bufferView, "sparse values").byteStride)
                    fail("sparse data must be tightly packed");
                auto size = component_size(type);
                auto* ids = storage(m, s.indices.bufferView, std::size_t(s.indices.byteOffset), s.count, size, size);
                auto* values = storage(m, s.values.bufferView, std::size_t(s.values.byteOffset), s.count, bytes, bytes);
                std::size_t previous = 0;
                for (int i = 0; i < s.count; ++i) {
                    auto id = std::size_t(read_component(ids + i * size, type, false));
                    if (id >= a.count || (i && id <= previous))
                        fail("sparse indices must be increasing and in range");
                    previous = id;
                    for (int c = 0; c < n; ++c)
                        out.values[id * n + c] = read_component(values + i * bytes + c * component_size(a.componentType), a.componentType, a.normalized);
                }
            }
            return out;
        }
        bool float_data(const AccessorData& a) {
            return a.type == TINYGLTF_COMPONENT_TYPE_FLOAT && !a.normalized;
        }
        bool normalized_data(const AccessorData& a) {
            return float_data(a) || (a.normalized && (a.type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || a.type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT));
        }
        bool image_loader(tinygltf::Image* image, int, std::string* err, std::string*, int req_width, int req_height, const unsigned char* bytes, int size, void*) {
            int width = 0, height = 0, components = 0;
            auto* pixels = stbi_load_from_memory(bytes, size, &width, &height, &components, 4);
            if (!pixels || width <= 0 || height <= 0 || (req_width && req_width != width) || (req_height && req_height != height)) {
                stbi_image_free(pixels);
                if (err)
                    *err += "Could not decode PNG/JPEG image";
                return false;
            }
            std::unique_ptr<unsigned char, decltype(&stbi_image_free)> owner(pixels, &stbi_image_free);
            image->width = width;
            image->height = height;
            image->component = 4;
            image->bits = 8;
            image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
            image->image.assign(pixels, pixels + product(product(std::size_t(width), std::size_t(height)), 4));
            return true;
        }
        std::string utf8(const std::filesystem::path& path) {
            const auto str = path.u8string();
            return {str.begin(), str.end()};
        }
        template<class Info>
        GltfTextureInfo texture_info(const Info& in, std::size_t textures) {
            GltfTextureInfo out;
            out.texture = in.index;
            out.tex_coord = in.texCoord;
            optional_index(in.index, textures, "texture");
            if (auto it = in.extensions.find("KHR_texture_transform"); it != in.extensions.end()) {
                const auto& transform = it->second;
                auto vector = [&](const char* name, std::array<float, 2>& dst) {
                    if (!transform.Has(name))
                        return;
                    const auto& value = transform.Get(name);
                    if (!value.IsArray() || value.ArrayLen() != 2)
                        fail("invalid texture transform vector");
                    for (int i = 0; i < 2; ++i) {
                        if (!value.Get(i).IsNumber())
                            fail("invalid texture transform value");
                        dst[i] = scalar(value.Get(i).GetNumberAsDouble());
                    }
                };
                vector("offset", out.offset);
                vector("scale", out.scale);
                if (transform.Has("rotation")) {
                    if (!transform.Get("rotation").IsNumber())
                        fail("invalid texture rotation");
                    out.rotation = scalar(transform.Get("rotation").GetNumberAsDouble());
                }
                if (transform.Has("texCoord")) {
                    if (!transform.Get("texCoord").IsInt())
                        fail("invalid texture texCoord");
                    out.tex_coord = transform.Get("texCoord").GetNumberAsInt();
                }
            }
            if (out.texture >= 0 && (out.tex_coord < 0 || out.tex_coord > 1))
                fail("only TEXCOORD_0/1 are supported");
            return out;
        }
        std::array<float, 4> evaluate(const GltfAnimationTrack& track, double time) {
            const auto& times = track.times;
            const auto factor = track.interpolation == GltfInterpolation::CUBIC_SPLINE ? 3u : 1u;
            auto value = [&](std::size_t i) { return track.values[i * factor + (factor == 3 ? 1 : 0)]; };
            if (time <= times.front())
                return value(0);
            if (time >= times.back())
                return value(times.size() - 1);
            const auto right = std::size_t(std::upper_bound(times.begin(), times.end(), time) - times.begin());
            const auto left = right - 1;
            const double dt = double(times[right]) - times[left], u = (time - times[left]) / dt;
            auto a = value(left), b = value(right), out = a;
            if (track.interpolation == GltfInterpolation::STEP)
                return a;
            if (track.interpolation == GltfInterpolation::CUBIC_SPLINE) {
                const double u2 = u * u, u3 = u2 * u;
                for (int c = 0; c < 4; ++c)
                    out[c] = scalar((2 * u3 - 3 * u2 + 1) * a[c] + (u3 - 2 * u2 + u) * dt * track.values[left * 3 + 2][c] + (-2 * u3 + 3 * u2) * b[c] + (u3 - u2) * dt * track.values[right * 3][c]);
            } else if (track.path == GltfAnimationPath::ROTATION) {
                a = normalize(a);
                b = normalize(b);
                double dot = 0;
                for (int c = 0; c < 4; ++c)
                    dot += double(a[c]) * b[c];
                if (dot < 0) {
                    for (auto& c : b)
                        c = -c;
                    dot = -dot;
                }
                double fa = 1 - u, fb = u;
                if (dot < 0.9995) {
                    const auto angle = std::acos(std::clamp(dot, -1.0, 1.0));
                    fa = std::sin((1 - u) * angle) / std::sin(angle);
                    fb = std::sin(u * angle) / std::sin(angle);
                }
                for (int c = 0; c < 4; ++c)
                    out[c] = scalar(fa * a[c] + fb * b[c]);
            } else
                for (int c = 0; c < 4; ++c)
                    out[c] = scalar((1 - u) * a[c] + u * b[c]);
            return track.path == GltfAnimationPath::ROTATION ? normalize(out) : out;
        }
    } // namespace

    struct GltfModel::Data {
        std::vector<GltfMesh> meshes;
        std::vector<GltfMaterial> materials;
        std::vector<GltfImage> images;
        std::vector<GltfTexture> textures;
        std::vector<GltfNode> nodes;
        std::vector<GltfScene> scenes;
        std::vector<GltfSkin> skins;
        std::vector<GltfAnimation> animations;
        std::vector<std::size_t> order;
        std::string warnings;
        int default_scene = -1;
        GltfPose pose(const GltfAnimation* animation, double time) const;
    };

    GltfPose GltfModel::Data::pose(const GltfAnimation* animation, double time) const {
        GltfPose out;
        out.local_matrices.reserve(nodes.size());
        out.world_matrices.resize(nodes.size());
        std::vector<GltfTransform> transforms;
        transforms.reserve(nodes.size());
        for (const auto& node : nodes)
            transforms.push_back(node.transform);
        if (animation)
            for (const auto& track : animation->tracks) {
                auto v = evaluate(track, time);
                auto& transform = transforms[std::size_t(track.node)];
                if (track.path == GltfAnimationPath::ROTATION)
                    transform.rotation = normalize(v);
                else {
                    auto& target = track.path == GltfAnimationPath::TRANSLATION ? transform.translation : transform.scale;
                    std::copy_n(v.begin(), 3, target.begin());
                }
            }
        for (std::size_t i = 0; i < nodes.size(); ++i)
            out.local_matrices.push_back(nodes[i].has_matrix ? nodes[i].matrix : matrix(transforms[i]));
        for (auto i : order)
            out.world_matrices[i] = nodes[i].parent < 0 ? out.local_matrices[i] : multiply(out.world_matrices[std::size_t(nodes[i].parent)], out.local_matrices[i]);
        return out;
    }

    GltfImage load_image(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input)
            fail("could not open image: " + path.string());
        const auto size = input.tellg();
        if (size <= 0 || size > std::numeric_limits<int>::max())
            fail("invalid encoded image size");
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
            fail("could not read image");
        tinygltf::Image image;
        std::string error;
        if (!image_loader(&image, 0, &error, nullptr, 0, 0, bytes.data(), int(bytes.size()), nullptr))
            fail(error);
        return {utf8(path.filename()), std::uint32_t(image.width), std::uint32_t(image.height), std::move(image.image)};
    }

    GltfModel::GltfModel(std::shared_ptr<const Data> data) : _data(std::move(data)) {}
    GltfModel GltfModel::Load(const std::filesystem::path& path) {
        tinygltf::TinyGLTF loader;
        loader.SetImageLoader(image_loader, nullptr);
        tinygltf::Model source;
        std::string error, warnings;
        std::ifstream input(path, std::ios::binary);
        char magic[4]{};
        input.read(magic, 4);
        if (!input)
            fail("could not read model: " + path.string());
        const bool binary = std::memcmp(magic, "glTF", 4) == 0;
        const bool ok = binary ? loader.LoadBinaryFromFile(&source, &error, &warnings, utf8(path)) : loader.LoadASCIIFromFile(&source, &error, &warnings, utf8(path));
        if (!ok)
            fail("could not load " + path.string() + ": " + error);
        auto data = std::make_shared<Data>();
        data->warnings = std::move(warnings);
        const std::set<std::string> supported{"KHR_materials_unlit", "KHR_texture_transform"};
        for (const auto& ext : source.extensionsRequired)
            if (!supported.contains(ext))
                fail("unsupported required extension: " + ext);
        for (const auto& ext : source.extensionsUsed)
            if (!supported.contains(ext))
                data->warnings += "Ignored optional extension: " + ext + "\n";
        if (source.asset.version != "2.0" || (!source.asset.minVersion.empty() && source.asset.minVersion != "2.0"))
            fail("only glTF 2.0 is supported");
        for (auto& image : source.images) {
            if (image.width <= 0 || image.height <= 0 || image.component != 4 || image.bits != 8 || image.image.size() != product(product(std::size_t(image.width), std::size_t(image.height)), 4))
                fail("invalid decoded image");
            data->images.push_back({std::move(image.name), std::uint32_t(image.width), std::uint32_t(image.height), std::move(image.image)});
        }
        for (const auto& texture : source.textures) {
            at(data->images, texture.source, "texture image");
            optional_index(texture.sampler, source.samplers.size(), "sampler");
            GltfTexture out;
            out.name = texture.name;
            out.image = texture.source;
            if (texture.sampler >= 0) {
                const auto& s = source.samplers[std::size_t(texture.sampler)];
                out.sampler = {s.magFilter < 0 ? 9729 : s.magFilter, s.minFilter < 0 ? 9987 : s.minFilter, s.wrapS, s.wrapT};
            }
            const auto& s = out.sampler;
            if ((s.mag_filter != 9728 && s.mag_filter != 9729) || (s.min_filter != 9728 && s.min_filter != 9729 && (s.min_filter < 9984 || s.min_filter > 9987)))
                fail("invalid texture filtering");
            auto wrap = [](int w) { return w == 10497 || w == 33071 || w == 33648; };
            if (!wrap(s.wrap_s) || !wrap(s.wrap_t))
                fail("invalid texture wrapping");
            data->textures.push_back(std::move(out));
        }
        for (const auto& m : source.materials) {
            GltfMaterial out;
            out.name = m.name;
            const auto& p = m.pbrMetallicRoughness;
            copy_numbers(p.baseColorFactor, out.base_color);
            copy_numbers(m.emissiveFactor, out.emissive);
            out.metallic = scalar(p.metallicFactor);
            out.roughness = scalar(p.roughnessFactor);
            out.alpha_cutoff = scalar(m.alphaCutoff);
            out.normal_scale = scalar(m.normalTexture.scale);
            out.occlusion_strength = scalar(m.occlusionTexture.strength);
            out.base_color_texture = texture_info(p.baseColorTexture, source.textures.size());
            out.metallic_roughness_texture = texture_info(p.metallicRoughnessTexture, source.textures.size());
            out.normal_texture = texture_info(m.normalTexture, source.textures.size());
            out.occlusion_texture = texture_info(m.occlusionTexture, source.textures.size());
            out.emissive_texture = texture_info(m.emissiveTexture, source.textures.size());
            if (m.alphaMode == "MASK")
                out.alpha_mode = GltfAlphaMode::MASK;
            else if (m.alphaMode == "BLEND")
                out.alpha_mode = GltfAlphaMode::BLEND;
            else if (m.alphaMode != "OPAQUE")
                fail("invalid alpha mode");
            out.double_sided = m.doubleSided;
            out.unlit = m.extensions.contains("KHR_materials_unlit");
            data->materials.push_back(std::move(out));
        }
        for (const auto& mesh : source.meshes) {
            if (!mesh.weights.empty())
                fail("morph weights are not supported");
            GltfMesh out;
            out.name = mesh.name;
            for (const auto& p : mesh.primitives) {
                if (!p.targets.empty())
                    fail("morph targets are not supported");
                if (p.extensions.contains("KHR_draco_mesh_compression"))
                    fail("Draco mesh compression is not supported");
                auto position = p.attributes.find("POSITION");
                if (position == p.attributes.end())
                    fail("primitive requires POSITION");
                const auto positions = decode(source, position->second, TINYGLTF_TYPE_VEC3);
                if (!float_data(positions))
                    fail("POSITION requires FLOAT");
                GltfPrimitive primitive;
                primitive.material = p.material;
                optional_index(p.material, data->materials.size(), "material");
                if (p.mode < -1)
                    fail("invalid primitive topology");
                const auto mode = p.mode < 0 ? 4 : p.mode;
                if (mode > 6)
                    fail("unsupported primitive topology");
                primitive.topology = static_cast<GltfTopology>(mode);
                std::vector<GltfGeometry::PointType> points;
                points.reserve(positions.count);
                std::vector<GltfVertex> vertices(positions.count);
                for (std::size_t i = 0; i < positions.count; ++i)
                    points.emplace_back(scalar(positions.values[i * 3]), scalar(positions.values[i * 3 + 1]), scalar(positions.values[i * 3 + 2]));
                auto attribute = [&](const char* name, int shape, auto write, bool normalized) {
                    auto it = p.attributes.find(name);
                    if (it == p.attributes.end())
                        return false;
                    auto a = decode(source, it->second, shape);
                    if (a.count != positions.count || !(normalized ? normalized_data(a) : float_data(a)))
                        fail(std::string("invalid attribute: ") + name);
                    for (std::size_t i = 0; i < a.count; ++i)
                        write(vertices[i], a.values.data() + i * a.components, a.components);
                    return true;
                };
                primitive.has_normals = attribute("NORMAL", TINYGLTF_TYPE_VEC3, [](auto& v, const double* x, std::size_t) { for (int c=0;c<3;++c) v.normal[c]=scalar(x[c]); }, false);
                primitive.has_tangents = attribute("TANGENT", TINYGLTF_TYPE_VEC4, [](auto& v, const double* x, std::size_t) { for (int c=0;c<4;++c) v.tangent[c]=scalar(x[c]); }, false);
                for (int channel = 0; channel < 2; ++channel)
                    primitive.has_tex_coords[channel] = attribute(channel ? "TEXCOORD_1" : "TEXCOORD_0", TINYGLTF_TYPE_VEC2, [channel](auto& v, const double* x, std::size_t) { for (int c=0;c<2;++c) v.tex_coords[channel][c]=scalar(x[c]); }, true);
                if (auto it = p.attributes.find("COLOR_0"); it != p.attributes.end()) {
                    const auto shape = at(source.accessors, it->second, "color").type;
                    if (shape != TINYGLTF_TYPE_VEC3 && shape != TINYGLTF_TYPE_VEC4)
                        fail("invalid color shape");
                    attribute("COLOR_0", shape, [](auto& v, const double* x, std::size_t n) { for (std::size_t c=0;c<n;++c) v.color[c]=scalar(x[c]); }, true);
                }
                const bool joints = p.attributes.contains("JOINTS_0"), weights = p.attributes.contains("WEIGHTS_0");
                if (joints != weights || p.attributes.contains("JOINTS_1") || p.attributes.contains("WEIGHTS_1"))
                    fail("skin requires exactly one JOINTS_0/WEIGHTS_0 pair");
                if (joints) {
                    auto a = decode(source, p.attributes.at("JOINTS_0"), TINYGLTF_TYPE_VEC4);
                    if (a.count != positions.count || a.normalized || (a.type != 5121 && a.type != 5123))
                        fail("invalid joints attribute");
                    for (std::size_t i = 0; i < a.count; ++i)
                        for (int c = 0; c < 4; ++c)
                            vertices[i].joints[c] = std::uint16_t(a.values[i * 4 + c]);
                    attribute("WEIGHTS_0", TINYGLTF_TYPE_VEC4, [](auto& v, const double* x, std::size_t) { double sum=0;for (int c=0;c<4;++c) { if (x[c]<0) fail("negative skin weight");sum+=x[c]; }if (!(sum>0)) fail("zero skin weights");for (int c=0;c<4;++c) v.weights[c]=scalar(x[c]/sum); }, true);
                    primitive.has_skin = true;
                }
                std::vector<std::uint32_t> indices;
                if (p.indices >= 0) {
                    auto a = decode(source, p.indices, TINYGLTF_TYPE_SCALAR);
                    if (a.normalized || (a.type != 5121 && a.type != 5123 && a.type != 5125))
                        fail("invalid index component type");
                    indices.reserve(a.count);
                    for (auto x : a.values) {
                        if (x >= positions.count)
                            fail("primitive index outside vertex array");
                        indices.push_back(std::uint32_t(x));
                    }
                } else {
                    if (p.indices != -1)
                        fail("invalid indices accessor");
                    indices.resize(positions.count);
                    std::iota(indices.begin(), indices.end(), 0u);
                }
                const auto count = indices.size();
                if ((mode == 1 && count % 2) || (mode == 4 && count % 3) || ((mode == 2 || mode == 3) && count < 2) || ((mode == 5 || mode == 6) && count < 3))
                    fail("invalid primitive index count");
                if (p.material >= 0) {
                    const auto& material = data->materials[std::size_t(p.material)];
                    for (const auto* info : {&material.base_color_texture, &material.metallic_roughness_texture, &material.normal_texture, &material.occlusion_texture, &material.emissive_texture})
                        if (info->texture >= 0 && !primitive.has_tex_coords[info->tex_coord])
                            fail("material references absent texture coordinates");
                }
                std::vector<GltfGeometry::TriangleType> triangles;
                if (mode == 4) {
                    triangles.reserve(count / 3);
                    for (std::size_t i = 0; i < count; i += 3)
                        triangles.push_back({{indices[i]}, {indices[i + 1]}, {indices[i + 2]}});
                } else if (mode == 5 || mode == 6) {
                    triangles.reserve(count - 2);
                    for (std::size_t i = 2; i < count; ++i) {
                        const auto a = mode == 6 ? indices[0] : indices[i - 2 + (i % 2)];
                        const auto b = mode == 6 ? indices[i - 1] : indices[i - 1 - (i % 2)];
                        triangles.push_back({{a}, {b}, {indices[i]}});
                    }
                } else
                    primitive.elements = std::move(indices);
                primitive.mesh = GltfMeshData(GltfGeometry(points, triangles), vertices);
                out.primitives.push_back(std::move(primitive));
            }
            data->meshes.push_back(std::move(out));
        }
        data->nodes.resize(source.nodes.size());
        for (std::size_t i = 0; i < source.nodes.size(); ++i) {
            const auto& n = source.nodes[i];
            auto& out = data->nodes[i];
            out.name = n.name;
            out.mesh = n.mesh;
            out.skin = n.skin;
            out.children = n.children;
            optional_index(n.mesh, data->meshes.size(), "mesh");
            optional_index(n.skin, source.skins.size(), "skin");
            if (!n.weights.empty())
                fail("node morph weights are not supported");
            if (!n.matrix.empty()) {
                if (!n.translation.empty() || !n.rotation.empty() || !n.scale.empty())
                    fail("node cannot have matrix and TRS together");
                copy_numbers(n.matrix, out.matrix);
                out.has_matrix = true;
            } else {
                if (!n.translation.empty())
                    copy_numbers(n.translation, out.transform.translation);
                if (!n.rotation.empty()) {
                    copy_numbers(n.rotation, out.transform.rotation);
                    out.transform.rotation = normalize(out.transform.rotation);
                }
                if (!n.scale.empty())
                    copy_numbers(n.scale, out.transform.scale);
                out.matrix = matrix(out.transform);
            }
            for (int child : n.children) {
                at(data->nodes, child, "child node");
                auto& target = data->nodes[std::size_t(child)];
                if (target.parent != -1 || std::size_t(child) == i)
                    fail("node graph has duplicate parents or cycle");
                target.parent = int(i);
            }
        }
        // 親→子の順を保存し、sampleごとの探索と再帰を不要にする。
        for (std::size_t i = 0; i < data->nodes.size(); ++i)
            if (data->nodes[i].parent < 0)
                data->order.push_back(i);
        for (std::size_t i = 0; i < data->order.size(); ++i)
            for (int child : data->nodes[data->order[i]].children)
                data->order.push_back(std::size_t(child));
        if (data->order.size() != data->nodes.size())
            fail("cyclic node graph");
        for (const auto& s : source.scenes) {
            GltfScene scene{s.name, s.nodes};
            std::set<int> roots;
            for (int node : s.nodes)
                if (at(data->nodes, node, "scene root").parent >= 0 || !roots.insert(node).second)
                    fail("invalid scene root");
            data->scenes.push_back(std::move(scene));
        }
        optional_index(source.defaultScene, data->scenes.size(), "default scene");
        data->default_scene = source.defaultScene >= 0 ? source.defaultScene : (data->scenes.empty() ? -1 : 0);
        for (const auto& s : source.skins) {
            GltfSkin out;
            out.name = s.name;
            out.skeleton = s.skeleton;
            out.joints = s.joints;
            optional_index(s.skeleton, data->nodes.size(), "skeleton");
            if (s.joints.empty())
                fail("skin has no joints");
            std::set<int> unique;
            for (int joint : s.joints) {
                at(data->nodes, joint, "joint node");
                if (!unique.insert(joint).second)
                    fail("duplicate skin joint");
            }
            out.inverse_bind_matrices.resize(s.joints.size(), identity());
            if (s.inverseBindMatrices >= 0) {
                auto a = decode(source, s.inverseBindMatrices, TINYGLTF_TYPE_MAT4);
                if (!float_data(a) || a.count < s.joints.size())
                    fail("invalid inverse bind matrices");
                for (std::size_t i = 0; i < s.joints.size(); ++i)
                    for (int c = 0; c < 16; ++c)
                        out.inverse_bind_matrices[i][c] = scalar(a.values[i * 16 + c]);
            } else if (s.inverseBindMatrices != -1)
                fail("invalid inverse bind accessor");
            data->skins.push_back(std::move(out));
        }
        for (const auto& n : data->nodes)
            if (n.skin >= 0) {
                const auto& mesh = at(data->meshes, n.mesh, "skinned mesh");
                const auto& skin = data->skins[std::size_t(n.skin)];
                for (const auto& p : mesh.primitives) {
                    if (!p.has_skin)
                        fail("skinned mesh has no joint attributes");
                    for (const auto& joints : p.mesh.vertex_column<&GltfVertex::joints>())
                        for (int c = 0; c < 4; ++c)
                            if (joints[c] >= skin.joints.size())
                                fail("vertex joint outside skin");
                }
            }
        for (const auto& a : source.animations) {
            GltfAnimation out;
            out.name = a.name;
            out.start = std::numeric_limits<float>::max();
            std::set<std::pair<int, int>> targets;
            for (const auto& channel : a.channels) {
                GltfAnimationTrack track;
                track.node = channel.target_node;
                if (at(data->nodes, track.node, "animation node").has_matrix)
                    fail("animation cannot target matrix node");
                int shape = TINYGLTF_TYPE_VEC3;
                if (channel.target_path == "translation")
                    track.path = GltfAnimationPath::TRANSLATION;
                else if (channel.target_path == "scale")
                    track.path = GltfAnimationPath::SCALE;
                else if (channel.target_path == "rotation") {
                    track.path = GltfAnimationPath::ROTATION;
                    shape = TINYGLTF_TYPE_VEC4;
                } else
                    fail("unsupported animation target: " + channel.target_path);
                if (!targets.insert({track.node, int(track.path)}).second)
                    fail("duplicate animation target");
                const auto& sampler = at(a.samplers, channel.sampler, "animation sampler");
                if (sampler.interpolation == "STEP")
                    track.interpolation = GltfInterpolation::STEP;
                else if (sampler.interpolation == "CUBICSPLINE")
                    track.interpolation = GltfInterpolation::CUBIC_SPLINE;
                else if (sampler.interpolation != "LINEAR")
                    fail("unsupported animation interpolation");
                auto times = decode(source, sampler.input, TINYGLTF_TYPE_SCALAR);
                auto values = decode(source, sampler.output, shape);
                const auto factor = track.interpolation == GltfInterpolation::CUBIC_SPLINE ? 3u : 1u;
                const bool rotation = track.path == GltfAnimationPath::ROTATION;
                const bool rotation_format = values.normalized &&
                                             (values.type == TINYGLTF_COMPONENT_TYPE_BYTE || values.type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
                                              values.type == TINYGLTF_COMPONENT_TYPE_SHORT || values.type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
                if (!float_data(times) || !(float_data(values) || (rotation && rotation_format)) || values.count != product(times.count, factor) || (factor == 3 && times.count < 2))
                    fail("invalid animation accessor");
                for (auto t : times.values) {
                    const auto time = scalar(t);
                    if (time < 0 || (!track.times.empty() && time <= track.times.back()))
                        fail("animation times must increase");
                    track.times.push_back(time);
                }
                for (std::size_t i = 0; i < values.count; ++i) {
                    std::array<float, 4> v{};
                    for (std::size_t c = 0; c < values.components; ++c)
                        v[c] = scalar(values.values[i * values.components + c]);
                    if (track.path == GltfAnimationPath::ROTATION && (factor == 1 || i % 3 == 1))
                        (void)normalize(v);
                    track.values.push_back(v);
                }
                out.start = std::min(out.start, track.times.front());
                out.end = std::max(out.end, track.times.back());
                out.tracks.push_back(std::move(track));
            }
            if (out.tracks.empty())
                out.start = 0;
            data->animations.push_back(std::move(out));
        }
        return GltfModel(std::move(data));
    }

    std::span<const GltfMesh> GltfModel::meshes() const {
        return _data->meshes;
    }
    std::span<const GltfMaterial> GltfModel::materials() const {
        return _data->materials;
    }
    std::span<const GltfImage> GltfModel::images() const {
        return _data->images;
    }
    std::span<const GltfTexture> GltfModel::textures() const {
        return _data->textures;
    }
    std::span<const GltfNode> GltfModel::nodes() const {
        return _data->nodes;
    }
    std::span<const GltfScene> GltfModel::scenes() const {
        return _data->scenes;
    }
    std::span<const GltfSkin> GltfModel::skins() const {
        return _data->skins;
    }
    std::span<const GltfAnimation> GltfModel::animations() const {
        return _data->animations;
    }
    const std::string& GltfModel::warnings() const {
        return _data->warnings;
    }
    int GltfModel::default_scene() const {
        return _data->default_scene;
    }
    GltfPose GltfModel::rest_pose() const {
        return _data->pose(nullptr, 0);
    }
    GltfPose GltfModel::sample(std::size_t animation, double time, bool loop) const {
        if (animation >= _data->animations.size())
            throw std::out_of_range("glTF animation index");
        if (!std::isfinite(time))
            throw std::invalid_argument("glTF animation time must be finite");
        const auto& a = _data->animations[animation];
        const auto duration = double(a.end) - a.start;
        if (loop && duration > 0) {
            time = std::fmod(time - a.start, duration);
            if (time < 0)
                time += duration;
            time += a.start;
        } else
            time = std::clamp(time, double(a.start), double(a.end));
        return _data->pose(&a, time);
    }
    std::vector<GltfMatrix> GltfModel::skin_matrices(const GltfPose& pose, std::size_t node) const {
        if (node >= _data->nodes.size())
            throw std::out_of_range("glTF mesh node index");
        if (pose.local_matrices.size() != _data->nodes.size() || pose.world_matrices.size() != _data->nodes.size())
            throw std::invalid_argument("glTF pose size does not match model");
        const auto& n = _data->nodes[node];
        if (n.skin < 0)
            return {};
        const auto& skin = _data->skins[std::size_t(n.skin)];
        const auto mesh_inverse = inverse(pose.world_matrices[node]);
        std::vector<GltfMatrix> out;
        out.reserve(skin.joints.size());
        for (std::size_t i = 0; i < skin.joints.size(); ++i)
            out.push_back(multiply(multiply(mesh_inverse, pose.world_matrices[std::size_t(skin.joints[i])]), skin.inverse_bind_matrices[i]));
        return out;
    }
} // namespace AHO_NAMESPACE::x::resource
