#ifdef AHO_CONTRACT_ENGINE_VSL_FIRST
#include <VSL/vsl.hpp>
#endif
#include <AHO/engine.hpp>
#if defined(AHO_CONTRACT_ENGINE_VSL_FIRST) || defined(AHO_CONTRACT_ENGINE_AHO_FIRST)
#ifdef VSL_ENABLE_SPIRV_REFLECTOR
#error AHO headers must not define VSL_ENABLE_SPIRV_REFLECTOR
#endif
#include <VSL/utils/spir_reflector.hpp>
static_assert(sizeof(vsl::utils::SPIRVReflector) > 0);
#else
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <AHO/engine.hpp>
namespace {
    TEST(EngineAccessorTest, EmptyHandlesDoNotRegisterADefaultEngine) {
        const auto previous = aho::engine::DefaultEngineRegistration::Current().data();
        aho::engine::EngineAccessor first;
        aho::engine::EngineAccessor second;
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), previous);
        EXPECT_EQ(first.data(), nullptr);
        EXPECT_EQ(second.data(), nullptr);
        EXPECT_THROW((aho::engine::DefaultEngineRegistration{first}), std::invalid_argument);
    }
    TEST(EngineAccessorLifetimeTest, CopiesShareDataAndMovesKeepTransferringOwnership) {
        using Accessor = aho::engine::EngineAccessor;
        static_assert(std::is_nothrow_move_constructible_v<Accessor>);
        static_assert(std::is_nothrow_move_assignable_v<Accessor>);
        // GPU資源を作らず、共有所有権のcontrol blockだけを検証する。
        auto token = std::make_shared<int>(7);
        Accessor source(std::shared_ptr<aho::engine::EngineData>(token, nullptr));
        static_assert(std::is_const_v<std::remove_reference_t<decltype(source.data())>>);
        Accessor copy(source);
        EXPECT_EQ(token.use_count(), 3);
        Accessor moved(std::move(copy));
        EXPECT_EQ(copy.data().use_count(), 0);
        EXPECT_EQ(moved.data().use_count(), 3);
        Accessor assigned;
        assigned = source;
        EXPECT_EQ(token.use_count(), 4);
        Accessor move_assigned;
        move_assigned = std::move(assigned);
        EXPECT_EQ(assigned.data().use_count(), 0);
        EXPECT_EQ(move_assigned.data().use_count(), 4);
    }

} // namespace
#endif
