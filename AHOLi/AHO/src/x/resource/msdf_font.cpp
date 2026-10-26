#include <AHO/x/resource/msdf_font.hpp>
#include "../text/utf8.hpp"
#include <msdfgen.h>
#include <msdfgen-ext.h>
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace AHO_NAMESPACE::x::resource {
    namespace {
        struct PackingPage {
            MSDFAtlasPage value;
            std::uint32_t x = 0, y = 0, row_height = 0;
            explicit PackingPage(const MSDFSettings& settings);
        };
        MSDFPixelBuffer copy_font_bytes(std::span<const std::byte> bytes);
        MSDFPixelBuffer read_font(const std::filesystem::path& path);
        void validate_settings(const MSDFSettings& settings);
        GlyphBounds bounds_of(const msdfgen::Shape& shape);

        PackingPage::PackingPage(const MSDFSettings& settings)
            : value{settings.page_width, settings.page_height, settings.pixel_range,
                    MSDFPixelBuffer(std::size_t(settings.page_width) * settings.page_height * 4), 0} {}

        MSDFPixelBuffer copy_font_bytes(std::span<const std::byte> bytes) {
            if (bytes.empty() || bytes.size() > INT_MAX)
                throw std::invalid_argument("Font data must fit the msdfgen font loader");
            MSDFPixelBuffer result;
            result.assign(bytes);
            return result;
        }

        MSDFPixelBuffer read_font(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
                throw std::runtime_error("Could not open font: " + path.string());
            const std::streamoff size = file.tellg();
            if (size <= 0 || size > INT_MAX)
                throw std::invalid_argument("Invalid font file size");
            MSDFPixelBuffer result(static_cast<std::size_t>(size));
            file.seekg(0);
            if (!file.read(static_cast<char*>(result.data()), size))
                throw std::runtime_error("Could not read font: " + path.string());
            return result;
        }

        void validate_settings(const MSDFSettings& s) {
            if (!std::isfinite(s.pixels_per_em) || s.pixels_per_em <= 0 ||
                !std::isfinite(s.pixel_range) || s.pixel_range <= 0 ||
                !s.page_width || !s.page_height || !s.max_pages ||
                s.page_width > INT_MAX || s.page_height > INT_MAX ||
                s.pixel_range + 4 >= std::min(s.page_width, s.page_height) ||
                std::size_t(s.page_width) > std::numeric_limits<std::size_t>::max() / 4 / s.page_height ||
                (s.missing_glyph != MissingGlyphPolicy::REPLACE && s.missing_glyph != MissingGlyphPolicy::ERROR_))
                throw std::invalid_argument("Invalid MSDF atlas settings");
        }

        GlyphBounds bounds_of(const msdfgen::Shape& shape) {
            const auto b = shape.getBounds();
            for (const double value : {b.l, b.b, b.r, b.t})
                if (!std::isfinite(value))
                    throw std::runtime_error("Font glyph has non-finite bounds");
            return {b.l, b.b, b.r, b.t};
        }
    } // namespace

    struct FontFace::Data {
        MSDFPixelBuffer bytes;
        std::unique_ptr<msdfgen::FreetypeHandle, decltype(&msdfgen::deinitializeFreetype)> library{nullptr, msdfgen::deinitializeFreetype};
        std::unique_ptr<msdfgen::FontHandle, decltype(&msdfgen::destroyFont)> font{nullptr, msdfgen::destroyFont};
        FontMetrics metrics;
        std::mutex mutex;
        explicit Data(MSDFPixelBuffer bytes);
    };

    struct MSDFAtlas::Data {
        FontFace font;
        MSDFSettings settings;
        std::unordered_map<std::uint32_t, MSDFGlyph> glyphs;
        std::vector<std::unique_ptr<PackingPage>> pages;
        std::vector<MSDFAtlasUpdate> updates;
        std::uint64_t revision = 0;
        Data(FontFace font, MSDFSettings settings) : font(std::move(font)), settings(settings) {}
    };

    FontFace::Data::Data(MSDFPixelBuffer bytes) : bytes(std::move(bytes)) {
        library.reset(msdfgen::initializeFreetype());
        if (!library)
            throw std::runtime_error("Could not initialize msdfgen FreeType support");
        font.reset(msdfgen::loadFontData(library.get(), static_cast<const msdfgen::byte*>(this->bytes.data()), static_cast<int>(this->bytes.size())));
        if (!font)
            throw std::invalid_argument("Could not load an outline font");
        msdfgen::FontMetrics m;
        if (!msdfgen::getFontMetrics(m, font.get(), msdfgen::FONT_SCALING_EM_NORMALIZED))
            throw std::runtime_error("Could not load font metrics");
        for (const double value : {m.emSize, m.ascenderY, m.descenderY, m.lineHeight, m.underlineY, m.underlineThickness})
            if (!std::isfinite(value))
                throw std::invalid_argument("Font metrics must be finite");
        if (m.lineHeight <= 0 || m.emSize <= 0)
            throw std::invalid_argument("Font must have positive em and line height");
        metrics = {m.ascenderY, m.descenderY, m.lineHeight, m.underlineY, m.underlineThickness};
    }

    FontFace::FontFace(const std::filesystem::path& path) : _data(std::make_shared<Data>(read_font(path))) {}
    FontFace::FontFace(std::span<const std::byte> bytes) : _data(std::make_shared<Data>(copy_font_bytes(bytes))) {}

    auto FontFace::data() const -> Data& {
        if (!_data)
            throw std::logic_error("FontFace is empty");
        return *_data;
    }

    FontMetrics FontFace::metrics() const {
        return data().metrics;
    }

    std::optional<std::uint32_t> FontFace::glyph_index(char32_t codepoint) const {
        detail::require_scalar(codepoint);
        auto& d = data();
        std::lock_guard lock(d.mutex);
        msdfgen::GlyphIndex index;
        if (!msdfgen::getGlyphIndex(index, d.font.get(), codepoint))
            return std::nullopt;
        return index.getIndex();
    }

    double FontFace::kerning(std::uint32_t left, std::uint32_t right) const {
        auto& d = data();
        std::lock_guard lock(d.mutex);
        double result = 0;
        if (!msdfgen::getKerning(result, d.font.get(), msdfgen::GlyphIndex(left), msdfgen::GlyphIndex(right), msdfgen::FONT_SCALING_EM_NORMALIZED))
            throw std::runtime_error("Could not read font kerning");
        if (!std::isfinite(result))
            throw std::runtime_error("Font kerning must be finite");
        return result;
    }

    MSDFAtlas::MSDFAtlas(FontFace font, MSDFSettings settings) {
        validate_settings(settings);
        (void)font.metrics();
        _data = std::make_shared<Data>(std::move(font), settings);
    }

    auto MSDFAtlas::data() const -> Data& {
        if (!_data)
            throw std::logic_error("MSDFAtlas is empty");
        return *_data;
    }

    const FontFace& MSDFAtlas::font() const {
        return data().font;
    }
    const MSDFSettings& MSDFAtlas::settings() const {
        return data().settings;
    }
    std::size_t MSDFAtlas::glyph_count() const {
        return data().glyphs.size();
    }
    std::size_t MSDFAtlas::page_count() const {
        return data().pages.size();
    }
    const MSDFAtlasPage& MSDFAtlas::page(std::size_t index) const {
        return data().pages.at(index)->value;
    }
    std::uint64_t MSDFAtlas::revision() const {
        return data().revision;
    }

    std::array<float, 2> MSDFAtlasPage::unit_range() const {
        if (!width || !height)
            throw std::logic_error("Atlas page is empty");
        return {static_cast<float>(pixel_range / width), static_cast<float>(pixel_range / height)};
    }

    std::vector<MSDFAtlasUpdate> MSDFAtlas::updates_since(std::uint64_t revision) const {
        const auto& updates = data().updates;
        const auto first = std::upper_bound(updates.begin(), updates.end(), revision,
                                            [](auto value, const MSDFAtlasUpdate& update) { return value < update.revision; });
        return {first, updates.end()};
    }

    void MSDFAtlas::prepare(std::string_view utf8) {
        for (const auto codepoint : detail::decode_utf8(utf8))
            if (codepoint.value != U'\n' && codepoint.value != U'\r' && codepoint.value != U'\t')
                (void)glyph(codepoint.value);
    }
    void MSDFAtlas::prepare(std::u8string_view utf8) {
        prepare(std::string_view(reinterpret_cast<const char*>(utf8.data()), utf8.size()));
    }

    MSDFGlyph MSDFAtlas::glyph(char32_t codepoint) {
        auto& d = data();
        auto index = d.font.glyph_index(codepoint);
        const bool fallback = !index;
        if (!index) {
            if (d.settings.missing_glyph == MissingGlyphPolicy::ERROR_)
                throw std::invalid_argument("Font has no glyph for codepoint " + std::to_string(std::uint32_t(codepoint)));
            index = d.font.glyph_index(U'\ufffd');
            if (!index)
                index = d.font.glyph_index(U'?');
        }
        const auto glyph_index = index.value_or(0);
        if (const auto found = d.glyphs.find(glyph_index); found != d.glyphs.end()) {
            auto result = found->second;
            result.fallback = fallback;
            return result;
        }
        msdfgen::Shape shape;
        MSDFGlyph glyph;
        glyph.index = glyph_index;
        {
            auto& face = d.font.data();
            std::lock_guard lock(face.mutex);
            if (!msdfgen::loadGlyph(shape, face.font.get(), msdfgen::GlyphIndex(glyph_index), msdfgen::FONT_SCALING_EM_NORMALIZED, &glyph.advance))
                throw std::runtime_error("Could not load font glyph");
        }
        if (!std::isfinite(glyph.advance))
            throw std::runtime_error("Font glyph advance must be finite");
        if (!shape.edgeCount()) {
            d.glyphs.emplace(glyph_index, glyph);
            glyph.fallback = fallback;
            return glyph;
        }
        shape.normalize();
        if (!shape.validate())
            throw std::runtime_error("Font glyph has an invalid outline");
        shape.setYAxisOrientation(msdfgen::Y_UPWARD);
        glyph.ink_bounds = bounds_of(shape);
        const auto scale = d.settings.pixels_per_em;
        const auto padding = d.settings.pixel_range / 2 + 1;
        const double left = std::floor(glyph.ink_bounds.left * scale - padding);
        const double bottom = std::floor(glyph.ink_bounds.bottom * scale - padding);
        const double right = std::ceil(glyph.ink_bounds.right * scale + padding);
        const double top = std::ceil(glyph.ink_bounds.top * scale + padding);
        const auto width = right - left, height = top - bottom;
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 ||
            width > double(d.settings.page_width) - 2 || height > double(d.settings.page_height) - 2)
            throw std::length_error("MSDF glyph exceeds atlas page dimensions");
        const auto w = static_cast<std::uint32_t>(width), h = static_cast<std::uint32_t>(height);
        // msdfgen::Bitmapはチャンネル数を含む画素数をintで計算する。
        if (std::size_t(w) > std::size_t(INT_MAX) / 3 / h)
            throw std::length_error("MSDF glyph exceeds the bitmap element limit");
        std::size_t page_index = 0;
        std::uint32_t x = 0, y = 0, row_height = 0;
        for (; page_index < d.pages.size(); ++page_index) {
            const auto& page = *d.pages[page_index];
            x = page.x;
            y = page.y;
            row_height = page.row_height;
            if (w + 2 > d.settings.page_width - x) {
                x = 0;
                y += row_height;
                row_height = 0;
            }
            if (h + 2 <= d.settings.page_height - y)
                break;
        }
        std::unique_ptr<PackingPage> next_page;
        if (page_index == d.pages.size()) {
            if (d.pages.size() >= d.settings.max_pages)
                throw std::length_error("MSDF atlas page limit reached");
            next_page = std::make_unique<PackingPage>(d.settings);
            x = 0;
            y = 0;
            row_height = 0;
        }
        if (d.revision == UINT64_MAX)
            throw std::overflow_error("MSDF atlas revision exhausted");
        glyph.page = static_cast<std::uint32_t>(page_index);
        glyph.rectangle = {x + 1, y + 1, w, h};
        glyph.plane_bounds = {left / scale, bottom / scale, right / scale, top / scale};
        glyph.uv = {float(x + 1) / d.settings.page_width, float(y + 1) / d.settings.page_height,
                    float(x + 1 + w) / d.settings.page_width, float(y + 1 + h) / d.settings.page_height};
        msdfgen::edgeColoringSimple(shape, 3.0, glyph_index);
        msdfgen::Bitmap<float, 3> bitmap(static_cast<int>(w), static_cast<int>(h), msdfgen::Y_UPWARD);
        msdfgen::generateMSDF(bitmap, shape, msdfgen::SDFTransformation(msdfgen::Projection(msdfgen::Vector2(scale), msdfgen::Vector2(-left / scale, -bottom / scale)), msdfgen::Range(d.settings.pixel_range / scale)));
        MSDFPixelBuffer pixels(std::size_t(w) * h * 4);
        auto bytes = pixels.bytes();
        for (std::uint32_t row = 0; row < h; ++row)
            for (std::uint32_t column = 0; column < w; ++column) {
                const auto* pixel = bitmap(static_cast<int>(column), static_cast<int>(h - 1 - row));
                const auto offset = (std::size_t(row) * w + column) * 4;
                for (std::size_t c = 0; c < 3; ++c) {
                    const auto value = std::clamp(pixel[c], 0.f, 1.f);
                    if (!std::isfinite(value))
                        throw std::runtime_error("MSDF generation produced a non-finite distance");
                    bytes[offset + c] = static_cast<std::byte>(static_cast<unsigned char>(value * 255 + .5f));
                }
                bytes[offset + 3] = std::byte{255};
            }
        // 失敗し得る確保を終えてから、キャッシュ・画素・配置をまとめて公開する。
        if (d.updates.size() == d.updates.capacity())
            d.updates.reserve(std::max(d.updates.size() + 1, d.updates.capacity() * 2));
        if (next_page && d.pages.size() == d.pages.capacity())
            d.pages.reserve(std::max(d.pages.size() + 1, d.pages.capacity() * 2));
        d.glyphs.emplace(glyph_index, glyph);
        if (next_page)
            d.pages.push_back(std::move(next_page));
        auto& page = *d.pages[page_index];
        auto destination = page.value.pixels.bytes();
        for (std::uint32_t row = 0; row < h; ++row)
            std::memcpy(destination.data() + (std::size_t(y + 1 + row) * page.value.width + x + 1) * 4,
                        bytes.data() + std::size_t(row) * w * 4, std::size_t(w) * 4);
        page.x = x + w + 2;
        page.y = y;
        page.row_height = std::max(row_height, h + 2);
        page.value.revision = ++d.revision;
        d.updates.push_back({d.revision, glyph.page, glyph.rectangle, std::move(pixels)});
        glyph.fallback = fallback;
        return glyph;
    }
} // namespace AHO_NAMESPACE::x::resource
