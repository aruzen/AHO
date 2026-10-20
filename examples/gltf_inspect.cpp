#include <AHO/x/resource/gltf.hpp>
#include <AHO/x/resource/shader.hpp>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: gltf-inspect model.gltf|model.glb [time-seconds] [shader.spv]\n";
        return 1;
    }
    try {
        using namespace AHO_NAMESPACE::x::resource;
        const auto model = GltfModel::load(argv[1]);
        std::size_t vertices = 0, indices = 0;
        for (const auto& mesh : model.meshes())
            for (const auto& primitive : mesh.primitives) {
                vertices += primitive.vertices.size();
                indices += primitive.indices.size();
            }
        std::cout << "meshes=" << model.meshes().size() << " vertices=" << vertices << " indices=" << indices
                  << " materials=" << model.materials().size() << " images=" << model.images().size()
                  << " nodes=" << model.nodes().size() << " skins=" << model.skins().size()
                  << " animations=" << model.animations().size() << '\n';
        const auto pose = model.animations().empty() ? model.restPose() : model.sample(0, argc > 2 ? std::stod(argv[2]) : 0);
        for (std::size_t node = 0; node < model.nodes().size(); ++node) {
            if (model.nodes()[node].skin >= 0)
                std::cout << "node=" << node << " joint-matrices=" << model.skinMatrices(pose, node).size() << '\n';
        }
        if (argc > 3)
            std::cout << "shader-words=" << loadSpirv(argv[3]).size() << '\n';
        if (!model.warnings().empty())
            std::cerr << model.warnings();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
