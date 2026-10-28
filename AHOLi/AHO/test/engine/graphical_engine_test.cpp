#include <gtest/gtest.h>
#include <AHO/engine/graphical_engine.hpp>
#include <type_traits>
#include <memory>
#include <stdexcept>

static_assert(std::is_abstract_v<aho::engine::GraphicalEngine>);
static_assert(std::is_base_of_v<aho::engine::EngineAccessor, aho::engine::GraphicalEngine>);

static_assert(std::has_virtual_destructor_v<aho::engine::GraphicalEngine>);
namespace {
    struct DeferredGraphicalEngine : aho::engine::GraphicalEngine {
        DeferredGraphicalEngine() = default;
        explicit DeferredGraphicalEngine(std::shared_ptr<aho::engine::EngineData> data) { reset_data(std::move(data)); }
        aho::Window get_window() override { throw std::logic_error("no window"); }
    };
    static_assert(!std::is_copy_constructible_v<DeferredGraphicalEngine>);
    static_assert(!std::is_move_constructible_v<DeferredGraphicalEngine>);
    struct TrackedGraphicalEngine : DeferredGraphicalEngine {
        int& destructions;
        std::shared_ptr<int> resource = std::make_shared<int>(7);
        explicit TrackedGraphicalEngine(int& destructions) : destructions(destructions) {}
        ~TrackedGraphicalEngine() override { ++destructions; }
    };
    TEST(GraphicalEngineTest, DeletingThroughBaseDestroysDerivedEngineAndReleasesItsResources) {
        int destructions = 0;
        std::weak_ptr<int> resource;
        {
            auto derived = std::make_unique<TrackedGraphicalEngine>(destructions);
            resource = derived->resource;
            std::unique_ptr<aho::engine::GraphicalEngine> engine = std::move(derived);
            EXPECT_FALSE(resource.expired());
            EXPECT_EQ(destructions, 0);
            EXPECT_THROW(engine->get_window(), std::logic_error);
        }
        EXPECT_EQ(destructions, 1);
        EXPECT_TRUE(resource.expired());
    }
    TEST(GraphicalEngineTest, SharedHandleOutlivesItsOwner) {
        const auto backing = std::make_shared<int>(7);
        aho::engine::EngineAccessor handle;
        {
            DeferredGraphicalEngine original(std::shared_ptr<aho::engine::EngineData>(backing, nullptr));
            handle = original;
            EXPECT_EQ(backing.use_count(), 3);
        }
        EXPECT_EQ(handle.data().use_count(), 2);
    }
} // namespace
