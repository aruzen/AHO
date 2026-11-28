#include <gtest/gtest.h>
#include <VSL/arena.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace {
    struct Recorder {
        std::vector<std::string>* log = nullptr;
        std::string name;
        ~Recorder() {
            if (log)
                log->push_back(name);
        }
    };

    struct Node {
        std::shared_ptr<Node> parent;
        std::vector<std::string>* log = nullptr;
        std::string name;
        ~Node() {
            // 構築時に結んだ親は、子の破棄中も生存している。
            if (log)
                log->push_back(name + (parent ? "<-" + parent->name : ""));
        }
    };

    auto record(std::vector<std::string>& log, std::string name) {
        return [&log, name = std::move(name)](Recorder& r) {
            r.log = &log;
            r.name = name;
        };
    }

    TEST(ArenaTest, PlacesObjectsInsideTheBlockAndCountsThem) {
        vsl::Arena arena(1024);
        auto a = arena.emplace<int>([](int& v) { v = 7; });
        auto b = arena.emplace<double>([](double& v) { v = 1.5; });
        EXPECT_EQ(*a, 7);
        EXPECT_EQ(*b, 1.5);
        EXPECT_EQ(arena.count(), 2u);
        EXPECT_GT(arena.used(), sizeof(int) + sizeof(double));
        EXPECT_LE(arena.used(), arena.capacity());
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(b.get()) % alignof(double), 0u);
    }

    TEST(ArenaTest, DestroysInReverseConstructionOrderWhenTheLastReferenceIsReleased) {
        std::vector<std::string> log;
        std::shared_ptr<Recorder> kept;
        {
            vsl::Arena arena(1024);
            arena.emplace<Recorder>(record(log, "first"));
            kept = arena.emplace<Recorder>(record(log, "second"));
            arena.emplace<Recorder>(record(log, "third"));
        }
        // Arena本体が消えても、配置した資源の参照が残る間は全体が生存する。
        EXPECT_TRUE(log.empty());
        kept.reset();
        EXPECT_EQ(log, (std::vector<std::string>{"third", "second", "first"}));
    }

    TEST(ArenaTest, FailedInitDestroysTheObjectAndRestoresTheBlock) {
        std::vector<std::string> log;
        vsl::Arena arena(1024);
        arena.emplace<Recorder>(record(log, "kept"));
        const auto used = arena.used();
        EXPECT_THROW(arena.emplace<Recorder>([&](Recorder& r) {
            r.log = &log;
            r.name = "failed";
            throw std::runtime_error("init failed");
        }),
                     std::runtime_error);
        EXPECT_EQ(log, (std::vector<std::string>{"failed"}));
        EXPECT_EQ(arena.used(), used);
        EXPECT_EQ(arena.count(), 1u);
        // 巻き戻した領域を再利用できる。
        arena.emplace<Recorder>(record(log, "after"));
        EXPECT_EQ(arena.count(), 2u);
    }

    TEST(ArenaTest, RejectsOverflowAndNestedEmplaceWithoutConstructing) {
        vsl::Arena small(vsl::Arena::bytes_for<int>(1));
        small.emplace<int>([](int&) {});
        bool constructed = false;
        EXPECT_THROW(small.emplace<int>([&](int&) { constructed = true; }), std::length_error);
        EXPECT_FALSE(constructed);

        vsl::Arena arena(1024);
        EXPECT_THROW(arena.emplace<int>([&](int&) { arena.emplace<int>([](int&) {}); }), std::logic_error);
        EXPECT_EQ(arena.count(), 0u);
    }

    TEST(ArenaTest, BytesForIsEnoughForTheRequestedCount) {
        vsl::Arena arena(vsl::Arena::bytes_for<Recorder>(8));
        for (int i = 0; i < 8; ++i)
            arena.emplace<Recorder>([](Recorder&) {});
        EXPECT_EQ(arena.count(), 8u);
    }

    TEST(ArenaTest, LinkInsideTheSameArenaDoesNotCreateACycle) {
        std::vector<std::string> log;
        std::weak_ptr<Node> weak_child;
        {
            vsl::Arena arena(1024);
            auto parent = arena.emplace<Node>([&](Node& n) {
                n.log = &log;
                n.name = "parent";
            });
            auto child = arena.emplace<Node>([&](Node& n) {
                n.log = &log;
                n.name = "child";
            });
            child->parent = vsl::link(child, parent);
            EXPECT_EQ(child->parent.get(), parent.get());
            EXPECT_EQ(child->parent.use_count(), 0) << "same arena link must be non-owning";
            weak_child = child;
        }
        EXPECT_TRUE(weak_child.expired());
        EXPECT_EQ(log, (std::vector<std::string>{"child<-parent", "parent"}));
    }

    TEST(ArenaTest, LinkAcrossOwnersKeepsTheTargetAlive) {
        std::vector<std::string> log;
        auto heap_child = vsl::place<Node>(nullptr, [&](Node& n) {
            n.log = &log;
            n.name = "heap";
        });
        {
            vsl::Arena arena(1024);
            auto parent = arena.emplace<Node>([&](Node& n) {
                n.log = &log;
                n.name = "arena";
            });
            heap_child->parent = vsl::link(heap_child, parent);
        }
        EXPECT_TRUE(log.empty()) << "heap child owns the other arena";
        heap_child.reset();
        EXPECT_EQ(log, (std::vector<std::string>{"heap<-arena", "arena"}));
    }

    TEST(ArenaTest, ExposeTurnsANonOwningLinkBackIntoAnOwner) {
        std::vector<std::string> log;
        std::shared_ptr<Node> exposed;
        {
            vsl::Arena arena(1024);
            auto parent = arena.emplace<Node>([&](Node& n) {
                n.log = &log;
                n.name = "parent";
            });
            auto child = arena.emplace<Node>([](Node&) {});
            child->parent = vsl::link(child, parent);
            exposed = vsl::expose(child, child->parent);
        }
        ASSERT_TRUE(exposed);
        EXPECT_EQ(exposed->name, "parent");
        EXPECT_TRUE(log.empty());
        exposed.reset();
        EXPECT_FALSE(log.empty());
    }

    TEST(ArenaTest, PlaceWithoutArenaUsesAnIndependentHeapOwner) {
        auto a = vsl::place<int>(nullptr, [](int& v) { v = 1; });
        auto b = vsl::place<int>(nullptr, [](int& v) { v = 2; });
        EXPECT_EQ(a.use_count(), 1);
        EXPECT_EQ(vsl::link(a, b).use_count(), 2) << "different heap objects are owned";
        EXPECT_THROW(vsl::place<int>(nullptr, [](int&) { throw std::runtime_error("x"); }), std::runtime_error);
    }
} // namespace
