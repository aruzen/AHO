#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <AHO/x/scene/mesh_renderer.hpp>

namespace wander {
    constexpr float worldLimit = 290.f;
    constexpr float waterLevel = 0.6f;
    using Vertex = aho::x::scene::MeshVertex;
    struct Obstacle {
        glm::vec2 center;
        float radius;
    };
    // The rendered triangles and the walking surface use the same height samples.
    float ground(float x, float z);
    float surface(float x, float z);
    struct World {
        std::vector<Vertex> vertices;
        std::vector<Obstacle> obstacles;
        World();
        bool blocked(glm::vec2 p) const;
        glm::vec2 move(glm::vec2 from, glm::vec2 delta) const;
    };
    struct Walker {
        glm::vec2 position{0, 87};
        float yaw = 0, pitch = 0.03f, night = 0;
        float stride = 0;
        void view(int index);
        void walk(const World& world, glm::vec2 input, float seconds, bool running);
        glm::vec3 eye() const;
        glm::vec3 direction() const;
    };
    void selfTest(const World& world);
} // namespace wander
