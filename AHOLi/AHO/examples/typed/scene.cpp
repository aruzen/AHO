#include "scene.hpp"
#include <array>
#include <iostream>

namespace typed_example {
    namespace {
        // The first record is deliberately excluded by the vertex view.
        const std::array<Vertex, 5> VERTICES{{{{99, 99}, {0, 0, 0}},
                                              {{-.6f, -.6f}, {1, .2f, .35f}},
                                              {{.6f, -.6f}, {.2f, .8f, 1}},
                                              {{.6f, .6f}, {.4f, 1, .55f}},
                                              {{-.6f, .6f}, {1, .8f, .2f}}}};
        const std::array<std::uint16_t, 6> INDICES{0, 1, 2, 2, 3, 0};
        const SceneUniform UNIFORM{{.9f, .9f, 1, 1}};
    } // namespace

    Scene::Scene(aho::Window window, const std::filesystem::path& shaders)
        : _window(std::move(window)), _device(_window.engine.data_->logical_device),
          _vertices(_device, VERTICES), _indices(_device, INDICES), _uniform(_device, UNIFORM),
          _set(_device, *_window.engine.data_->graphic_resource_manager),
          _vertex_shader(_device, shaders / "scene.vert.spv"), _fragment_shader(_device, shaders / "scene.frag.spv") {
        _set.write<0>(_uniform);
        rebuild_pipeline();
        _resize_hook = _window.add_plugin<aho::window::WindowResizeHookPlugin>([this](aho::Window*) { rebuild_pipeline(); });
    }

    Scene::~Scene() {
        _window.remove_plugin(_resize_hook);
    }

    void Scene::rebuild_pipeline() {
        const auto format = _window.data2->swapchain.get_format();
        if (_pipeline && _format == format)
            return;
        namespace pl = vsl::pipeline_layout;
        _pipeline.emplace(_device, _window.data2->render_pass,
                          pl::ShaderGroup("Typed example", {_vertex_shader, _fragment_shader}),
                          pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE},
                          pl::Multisample{}, pl::DynamicState{}, vsl::Viewport{}, vsl::Scissor{});
        _format = format;
        ++_pipeline_builds;
    }

    void Scene::record(aho::DrawPhase& phase, float time) {
        const auto vertices = _vertices.view(1, 4);
        const auto bound = _pipeline->bind_vertices(vertices).bind_resources(_set);
        const DrawTransform left{{-.48f, 0, .48f, time * .7f}};
        const DrawTransform right{{.48f, 0, .48f, -time * .9f}};
        auto batch = t::TypedBatch(
            bound.push_constants(Pushes::Values(left)).draw_indexed(_indices, INDICES.size()),
            bound.push_constants(Pushes::Values(right)).draw_indexed(_indices, INDICES.size()));
        if (time == 0)
            std::cout << "Typed batch: " << batch.statistics().draws << " draws, "
                      << batch.statistics().pipeline_binds << " pipeline, "
                      << batch.statistics().vertex_binds << " vertex, "
                      << batch.statistics().index_binds << " index, "
                      << batch.statistics().descriptor_binds << " descriptor binds\n";
        phase << vsl::Viewport(_window.data2->swapchain) << vsl::Scissor(_window.data2->swapchain) << batch;
    }
} // namespace typed_example
