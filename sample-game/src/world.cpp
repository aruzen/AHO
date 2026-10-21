#include "world.hpp"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>

namespace wander {
    namespace {
        constexpr float spacing = 4.f;
        float lakeDistance(float x, float z) { return std::sqrt(x * x / (55 * 55) + (z + 8) * (z + 8) / (72 * 72)); }
        float height(float x, float z) {
            const float d = lakeDistance(x, z);
            const float hills = 2.8f * std::sin(x * .029f + z * .014f) + 1.9f * std::cos(z * .045f) + .7f * std::sin(x * .11f + z * .08f);
            return glm::mix(-3.f, 5.5f + hills, glm::smoothstep(.83f, 1.6f, d)) +
                   20.f * glm::smoothstep(130.f, 280.f, -x) + 5.f * glm::smoothstep(150.f, 280.f, z);
        }
        void triangle(std::vector<Vertex>& out, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color, float material = 0) {
            auto n = glm::normalize(glm::cross(b - a, c - a));
            if (n.y < 0 && material == 0)
                n = -n;
            out.insert(out.end(), {{a, n, color, material}, {b, n, color, material}, {c, n, color, material}});
        }
        void cone(std::vector<Vertex>& out, glm::vec3 base, float radius, float h, glm::vec3 color, int sides = 6, float turn = 0) {
            for (int i = 0; i < sides; ++i) {
                float a = turn + i * glm::two_pi<float>() / sides, b = turn + (i + 1) * glm::two_pi<float>() / sides;
                triangle(out, base + glm::vec3(std::cos(a) * radius, 0, std::sin(a) * radius), base + glm::vec3(0, h, 0),
                         base + glm::vec3(std::cos(b) * radius, 0, std::sin(b) * radius), color * (0.91f + .09f * (i % 3)));
            }
        }
        void branch(std::vector<Vertex>& out, glm::vec3 a, glm::vec3 b, float radius, glm::vec3 color) {
            auto axis = glm::normalize(b - a);
            auto u = glm::normalize(glm::cross(axis, glm::vec3(.01f, .01f, 1))) * radius;
            auto v = glm::cross(axis, u);
            for (int i = 0; i < 5; ++i) {
                float t = i * glm::two_pi<float>() / 5, t1 = (i + 1) * glm::two_pi<float>() / 5;
                auto p = u * std::cos(t) + v * std::sin(t), q = u * std::cos(t1) + v * std::sin(t1);
                triangle(out, a + p, b + p * .55f, a + q, color);
                triangle(out, a + q, b + p * .55f, b + q * .55f, color);
            }
        }
    } // namespace
    float ground(float x, float z) {
        float ax = std::floor(x / spacing) * spacing, az = std::floor(z / spacing) * spacing;
        float u = (x - ax) / spacing, v = (z - az) / spacing;
        float a = height(ax, az), b = height(ax + spacing, az), c = height(ax, az + spacing), d = height(ax + spacing, az + spacing);
        return u + v < 1 ? a + u * (b - a) + v * (c - a) : d + (1 - v) * (b - d) + (1 - u) * (c - d);
    }
    float surface(float x, float z) { return std::max(ground(x, z), waterLevel); }
    World::World() {
        std::mt19937 random(0xA40);
        auto unit = [&]() { return std::generate_canonical<float, 24>(random); };
        auto range = [&](float lo, float hi) { return glm::mix(lo, hi, unit()); };
        vertices.reserve(1100000);
        for (int z = -320; z < 320; z += 4)
            for (int x = -320; x < 320; x += 4) {
                glm::vec3 a(x, height(x, z), z), b(x + 4, height(x + 4, z), z), c(x, height(x, z + 4), z + 4), d(x + 4, height(x + 4, z + 4), z + 4);
                float snow = glm::smoothstep(75.f, 155.f, -float(x));
                glm::vec3 color = glm::mix(glm::vec3(.24f, .39f, .32f), glm::vec3(.79f, .88f, .84f), snow);
                float shore = 1 - glm::smoothstep(.7f, 3.f, a.y);
                color = glm::mix(color, glm::vec3(.53f, .58f, .48f), shore * .8f);
                color *= range(.91f, 1.04f);
                triangle(vertices, a, c, b, color);
                triangle(vertices, b, c, d, color * .985f);
            }
        // A distant ring of angular peaks, with snow caps sharing the same silhouette.
        for (int i = 0; i < 54; ++i) {
            float angle = i * glm::two_pi<float>() / 54;
            glm::vec3 base(std::sin(angle) * range(370, 460), -14, std::cos(angle) * range(370, 460));
            float r = range(50, 100), h = range(95, 200), cap = range(.36f, .55f), rotation = range(0, 6);
            auto peak = base + glm::vec3(0, h, 0);
            for (int j = 0; j < 5; ++j) {
                float a = rotation + j * glm::two_pi<float>() / 5, b = rotation + (j + 1) * glm::two_pi<float>() / 5;
                auto p = base + glm::vec3(std::cos(a) * r, 0, std::sin(a) * r);
                auto q = base + glm::vec3(std::cos(b) * r, 0, std::sin(b) * r);
                auto cp = glm::mix(peak, p, cap), cq = glm::mix(peak, q, cap);
                triangle(vertices, p, cp, q, {.28f, .4f, .39f});
                triangle(vertices, q, cp, cq, {.28f, .4f, .39f});
                triangle(vertices, cp, peak, cq, {.85f, .91f, .85f});
            }
        }
        // Continuous walkable shoreline: trunks and boulders are solid; water stays at the shore.
        for (int i = 0; i < 2100; ++i) {
            float x = range(-280, 280), z = range(-280, 280), y = ground(x, z);
            if (y < 2.4f || glm::distance(glm::vec2(x, z), glm::vec2(0, 87)) < 12 ||
                glm::distance(glm::vec2(x, z), glm::vec2(-115, 70)) < 9 || glm::distance(glm::vec2(x, z), glm::vec2(120, 55)) < 9)
                continue;
            // Leave a winding trail around the lake.
            if (std::abs(lakeDistance(x, z) - 1.54f) < .09f)
                continue;
            float h = range(7, 18), r = h * range(.19f, .27f);
            glm::vec3 base(x, y - .2f, z);
            obstacles.push_back({{x, z}, .65f});
            bool dead = x > 75 && z < 100;
            branch(vertices, base, base + glm::vec3(0, h * .88f, 0), h * .035f, {.22f, .27f, .23f});
            if (dead) {
                for (int j = 0; j < 4; ++j) {
                    float a = range(0, 6.28f), by = h * (.3f + j * .13f);
                    glm::vec3 p = base + glm::vec3(0, by, 0), q = p + glm::vec3(std::cos(a) * r, h * .2f, std::sin(a) * r);
                    branch(vertices, p, q, .18f, {.23f, .3f, .27f});
                    branch(vertices, q, q + glm::vec3(.2f, h * .12f, 0), .09f, {.27f, .33f, .29f});
                }
            } else {
                float snow = glm::smoothstep(60.f, 145.f, -x);
                glm::vec3 foliage = glm::mix(glm::vec3(.14f, .35f, .27f), glm::vec3(.72f, .84f, .8f), snow);
                foliage *= range(.87f, 1.15f);
                for (int layer = 0; layer < 3; ++layer) {
                    float f = 1 - layer * .23f;
                    cone(vertices, base + glm::vec3(0, h * (.19f + layer * .22f), 0), r * f, h * .55f * f, foliage * (1 + layer * .045f));
                }
            }
            // Soft contact shadow, draped over the exact terrain triangles.
            const glm::vec3 tint(.12f, .21f, .19f);
            for (int j = 0; j < 8; ++j) {
                float a = j * glm::two_pi<float>() / 8, b = (j + 1) * glm::two_pi<float>() / 8;
                glm::vec3 p(x + std::cos(a) * r * 1.2f, 0, z + std::sin(a) * r * .65f), q(x + std::cos(b) * r * 1.2f, 0, z + std::sin(b) * r * .65f);
                p.y = ground(p.x, p.z) + .035f;
                q.y = ground(q.x, q.z) + .035f;
                triangle(vertices, {x, y + .04f, z}, p, q, tint, 4);
            }
        }
        for (int i = 0; i < 270; ++i) {
            float x = range(-260, 260), z = range(-260, 260), y = ground(x, z), r = range(.7f, 2.8f);
            if (y < 1.5f || glm::distance(glm::vec2(x, z), glm::vec2(0, 87)) < 7)
                continue;
            obstacles.push_back({{x, z}, r * .85f});
            cone(vertices, {x, y - .3f, z}, r, r * .95f, x < -80 ? glm::vec3(.78f, .86f, .83f) : glm::vec3(.39f, .49f, .46f), 7);
        }
        // Water follows the lake basin; submerged terrain naturally clips its outer edge.
        triangle(vertices, {-100, waterLevel, -110}, {-100, waterLevel, 100}, {100, waterLevel, -110}, {.22f, .46f, .44f}, 1);
        triangle(vertices, {100, waterLevel, -110}, {-100, waterLevel, 100}, {100, waterLevel, 100}, {.22f, .46f, .44f}, 1);
        for (int i = 0; i < 1100; ++i) {
            glm::vec3 p(range(-300, 300), range(3, 45), range(-300, 300));
            float r = range(.025f, .075f);
            triangle(vertices, p + glm::vec3(-r, 0, 0), p + glm::vec3(r, 0, 0), p + glm::vec3(0, r * 1.8f, 0), {.9f, 1, .95f}, 3);
        }
        // Sky dome centered on the camera in the vertex shader.
        const auto skyStart = vertices.size();
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 64; ++x) {
                auto sphere = [](int x, int y) {
                    float a = x * glm::two_pi<float>() / 64, b = -.5f * glm::pi<float>() + y * glm::pi<float>() / 24;
                    return glm::vec3(std::cos(b) * std::cos(a), std::sin(b), std::cos(b) * std::sin(a));
                };
                auto a = sphere(x, y), b = sphere(x + 1, y), c = sphere(x, y + 1), d = sphere(x + 1, y + 1);
                // Pole triangles may be degenerate; normals are unused for the sky.
                for (auto p : {a, b, c, b, d, c})
                    vertices.push_back({p, {0, 1, 0}, {1, 1, 1}, 2});
            }
        std::rotate(vertices.begin(), vertices.begin() + skyStart, vertices.end());
        std::cout << "World: " << vertices.size() / 3 << " triangles, " << obstacles.size() << " colliders\n";
    }
    bool World::blocked(glm::vec2 p) const {
        if (std::abs(p.x) > worldLimit || std::abs(p.y) > worldLimit || ground(p.x, p.y) < waterLevel + .15f)
            return true;
        for (auto obstacle : obstacles)
            if (glm::distance(p, obstacle.center) < obstacle.radius + .35f)
                return true;
        return false;
    }
    glm::vec2 World::move(glm::vec2 from, glm::vec2 delta) const {
        // Substeps prevent running through trunks after a slow frame; axis separation slides along them.
        int count = std::max(1, int(std::ceil(glm::length(delta) / .2f)));
        delta /= float(count);
        for (int i = 0; i < count; ++i) {
            glm::vec2 next = from + glm::vec2(delta.x, 0);
            if (!blocked(next))
                from = next;
            next = from + glm::vec2(0, delta.y);
            if (!blocked(next))
                from = next;
        }
        return from;
    }
    void Walker::view(int index) {
        if (index == 0) {
            position = {0, 87};
            yaw = 0;
            pitch = .03f;
            night = 0;
        }
        if (index == 1) {
            position = {-115, 70};
            yaw = -.65f;
            pitch = .12f;
            night = .15f;
        }
        if (index == 2) {
            position = {120, 55};
            yaw = .75f;
            pitch = .25f;
            night = 1;
        }
        stride = 0;
    }
    void Walker::walk(const World& world, glm::vec2 input, float seconds, bool running) {
        float length = glm::length(input);
        if (length > 1)
            input /= length;
        auto delta = glm::vec2(std::cos(yaw), std::sin(yaw)) * input.x + glm::vec2(std::sin(yaw), -std::cos(yaw)) * input.y;
        auto next = world.move(position, delta * seconds * (running ? 11.f : 4.6f));
        stride += glm::distance(next, position);
        position = next;
    }
    glm::vec3 Walker::eye() const { return {position.x, surface(position.x, position.y) + 1.78f + std::sin(stride * 2.6f) * .035f, position.y}; }
    glm::vec3 Walker::direction() const { return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)}; }
    void selfTest(const World& world) {
        auto require = [](bool test, const char* message) { if (!test) throw std::runtime_error(message); };
        for (int i = 0; i < 3; ++i) {
            Walker walker;
            walker.view(i);
            require(!world.blocked(walker.position), "Viewpoint is obstructed");
            for (int step = 0; step < 600; ++step) {
                walker.yaw += .011f;
                walker.walk(world, {0, 1}, 1.f / 60, true);
                require(!world.blocked(walker.position), "Walker entered a collider or water");
                require(std::isfinite(walker.eye().y), "Invalid eye height");
            }
        }
        require(world.blocked({0, -8}), "Lake must block walking");
        require(world.blocked({worldLimit + 1, 150}), "World boundary must block walking");
        auto stop = world.move({0, 87}, {0, -150});
        require(!world.blocked(stop) && stop.y > 0, "Large step tunneled through lake");
        for (auto obstacle : world.obstacles)
            require(world.blocked(obstacle.center), "Missing obstacle collider");
        require(world.vertices.size() % 3 == 0, "Incomplete triangle");
        for (const auto& v : world.vertices)
            require(std::isfinite(v.position.x + v.position.y + v.position.z + v.normal.x + v.normal.y + v.normal.z), "Invalid vertex");
        std::cout << "World verified: 1800 walking steps, shore, boundaries, obstacles and mesh\n";
    }
} // namespace wander
