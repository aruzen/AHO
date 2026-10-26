#include <AHO/x/text/text_layout.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: aho-msdf-example font.ttf [UTF-8 text]\n";
        return 1;
    }
    try {
        aho::x::resource::MSDFAtlas atlas{aho::x::resource::FontFace(std::filesystem::path(argv[1]))};
        const auto text = aho::x::text::layout_text(atlas, argc == 3 ? std::string_view(argv[2]) : std::string_view("A日本語 AV"));
        const auto buffers = text.make_buffers();
        std::cout << "glyphs=" << text.glyphs().size() << " pages=" << atlas.page_count()
                  << " vertices=" << buffers.vertex_count << " indices=" << buffers.index_count
                  << " width=" << text.metrics().width << " height=" << text.metrics().height << '\n';
        for (const auto& update : atlas.updates_since(0))
            std::cout << "update=" << update.revision << " page=" << update.page
                      << " rect=" << update.rectangle.x << ',' << update.rectangle.y << ','
                      << update.rectangle.width << ',' << update.rectangle.height
                      << " bytes=" << update.pixels.size() << '\n';
        for (const auto& range : text.ranges())
            std::cout << "page=" << range.page << " first_index=" << range.first_index
                      << " index_count=" << range.index_count << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
