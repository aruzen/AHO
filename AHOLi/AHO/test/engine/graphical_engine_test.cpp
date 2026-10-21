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
        aho::Window get_window() override { throw std::logic_error("no window"); }
    };
    static_assert(std::is_nothrow_move_constructible_v<DeferredGraphicalEngine>);
    static_assert(std::is_nothrow_move_assignable_v<DeferredGraphicalEngine>);
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
    TEST(GraphicalEngineTest, CopyAndMoveKeepExistingSharedHandleSemantics) {
        const auto backing = std::make_shared<int>(7);
        DeferredGraphicalEngine original;
        original.data_ = std::shared_ptr<aho::engine::EngineData>(backing, nullptr);
        const DeferredGraphicalEngine copy(original);
        EXPECT_EQ(backing.use_count(), 3);
        DeferredGraphicalEngine moved(std::move(original));
        EXPECT_EQ(original.data_.use_count(), 0);
        EXPECT_EQ(moved.data_.use_count(), 3);
        DeferredGraphicalEngine assigned;
        assigned = std::move(moved);
        EXPECT_EQ(moved.data_.use_count(), 0);
        EXPECT_EQ(assigned.data_.use_count(), 3);
        DeferredGraphicalEngine copy_assigned;
        copy_assigned = copy;
        EXPECT_EQ(backing.use_count(), 4);
    }
} // namespace
