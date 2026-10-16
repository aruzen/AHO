#include <gtest/gtest.h>
#include <AHO/engine/graphical_engine.hpp>
#include <type_traits>
static_assert(std::is_abstract_v<aho::engine::GraphicalEngine>);
static_assert(std::is_base_of_v<aho::engine::EngineAccessor, aho::engine::GraphicalEngine>);
