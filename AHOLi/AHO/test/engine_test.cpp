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
#include "../src/engine/engine_access.hpp"

namespace {
    using AHO_NAMESPACE::engine::detail::EngineAccess;
}

#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <AHO/engine.hpp>

namespace {
    TEST(EngineAccessorTest, EmptyHandlesDoNotRegisterADefaultEngine) {
        const auto previous = EngineAccess::State(aho::engine::DefaultEngineRegistration::Current());
        aho::engine::EngineAccessor first;
        aho::engine::EngineAccessor second;
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), previous);
        EXPECT_EQ(EngineAccess::State(first), nullptr);
        EXPECT_EQ(EngineAccess::State(second), nullptr);
        EXPECT_THROW((aho::engine::DefaultEngineRegistration{first}), std::invalid_argument);
        EXPECT_THROW(aho::engine::EngineAccessor::Create({}), std::invalid_argument);
        EXPECT_THROW(first.device(), std::logic_error);
        EXPECT_THROW(first.commands(), std::logic_error);
        EXPECT_THROW(first.descriptor_manager(), std::logic_error);
        EXPECT_THROW(first.synchronization(), std::logic_error);
        EXPECT_THROW(first.instance(), std::logic_error);
        EXPECT_THROW(first.physical_device(), std::logic_error);
        EXPECT_THROW(first.present_fences(), std::logic_error);
    }

    TEST(EngineAccessorLifetimeTest, CopiesShareDataAndMovesKeepTransferringOwnership) {
        using Accessor = aho::engine::EngineAccessor;
        static_assert(std::is_nothrow_move_constructible_v<Accessor>);
        static_assert(std::is_nothrow_move_assignable_v<Accessor>);
        // GPU資源を作らず、共有所有権のcontrol blockだけを検証する。
        auto token = std::make_shared<int>(7);
        auto source = EngineAccess::FromState(std::shared_ptr<aho::engine::EngineData>(token, nullptr));
        static_assert(std::is_const_v<std::remove_reference_t<decltype(EngineAccess::State(std::as_const(source)))>>);
        Accessor copy(source);
        EXPECT_EQ(token.use_count(), 3);
        Accessor moved(std::move(copy));
        EXPECT_EQ(EngineAccess::State(copy).use_count(), 0);
        EXPECT_EQ(EngineAccess::State(moved).use_count(), 3);
        Accessor assigned;
        assigned = source;
        EXPECT_EQ(token.use_count(), 4);
        Accessor move_assigned;
        move_assigned = std::move(assigned);
        EXPECT_EQ(EngineAccess::State(assigned).use_count(), 0);
        EXPECT_EQ(EngineAccess::State(move_assigned).use_count(), 4);
    }

} // namespace
#endif
