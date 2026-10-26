#include <AHO/x/core/mesh_attributes.hpp>
#include <gtest/gtest.h>
#include <string>

namespace {
    struct Attribute {
        std::array<float, 2> uv0{}, uv1{1, 1};
        std::string name = "default";
        bool visible = true;
    };
    using Members = aho::x::MeshAttributeMembers<&Attribute::uv0, &Attribute::uv1, &Attribute::name, &Attribute::visible>;
    using Attributes = aho::x::MeshAttributes<Attribute, Members>;

    template<typename Layout, typename Fields = Members>
    void check_bounded_attributes() {
        using Bounded = aho::x::MeshAttributes<Attribute, Fields, Layout>;
        Bounded empty;
        EXPECT_TRUE(empty.empty());
        Bounded attributes(1);
        EXPECT_EQ(attributes.size(), 1u);
        EXPECT_EQ(attributes.template column<&Attribute::name>().size(), 1u);
        EXPECT_EQ(attributes.read(0).name, "default");
        EXPECT_EQ(attributes.read(0).uv1[0], 1);
        auto copy = attributes;
        copy[0].write(Attribute{{2, 3}, {4, 5}, "changed", false});
        EXPECT_EQ(attributes.read(0).name, "default");
        EXPECT_FALSE(copy.read(0).visible);
        auto moved = std::move(copy);
        EXPECT_TRUE(copy.empty());
        EXPECT_EQ(moved.read(0).uv1[1], 5);
        const auto& read_only = attributes;
        static_assert(std::same_as<decltype(read_only.template column<&Attribute::name>()), std::span<const std::string>>);
        Bounded full(2);
        EXPECT_EQ(full.size(), 2u);
        EXPECT_THROW((void)Bounded(3), std::length_error);
        const std::array rows{Attribute{}, Attribute{}, Attribute{}};
        EXPECT_THROW((void)Bounded(rows), std::length_error);
        EXPECT_EQ(attributes.read(0).name, "default");
    }
} // namespace

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
namespace {
    TEST(MeshAttributesTest, ReflectedFixedAndBoundedStoragePreserveRowsAndRejectCapacityOverflow) {
        check_bounded_attributes<kk::storage::fixed_layout<2>, void>();
        check_bounded_attributes<kk::storage::dynamic_layout<2>, void>();
    }

    TEST(MeshAttributesTest, ReflectionUsesUnregisteredStructWithRepeatedTypesAndDefaults) {
        aho::x::MeshAttributes<Attribute> attributes(2);
        EXPECT_EQ(attributes.read(1).name, "default");
        EXPECT_EQ(attributes.read(1).uv1[0], 1);
        EXPECT_TRUE(attributes.read(1).visible);
        attributes.column<&Attribute::uv0>()[0] = {2, 3};
        attributes.column<&Attribute::uv1>()[0] = {4, 5};
        attributes[0].get<&Attribute::visible>() = false;
        EXPECT_EQ(attributes.at(0).read().uv0[1], 3);
        EXPECT_EQ(attributes.read(0).uv1[1], 5);
        EXPECT_FALSE(attributes.read(0).visible);
        const auto& read_only = attributes;
        static_assert(std::same_as<decltype(read_only.column<&Attribute::name>()), std::span<const std::string>>);
        EXPECT_EQ(read_only.at(0).get<&Attribute::name>(), "default");
        EXPECT_THROW((void)read_only.at(2), std::out_of_range);
    }

    TEST(MeshAttributesTest, ReflectionWritesCopiesAndBoundsChecksCompleteRows) {
        const std::array rows{Attribute{{2, 3}, {4, 5}, "first", false}};
        aho::x::MeshAttributes<Attribute> attributes(rows);
        auto copy = attributes;
        copy.at(0).write(Attribute{{6, 7}, {8, 9}, "updated", true});
        EXPECT_EQ(attributes.read(0).name, "first");
        EXPECT_EQ(copy.read(0).name, "updated");
        EXPECT_EQ(copy.read(0).uv1[1], 9);
        EXPECT_TRUE(copy.read(0).visible);
        EXPECT_THROW(copy.write(1, rows[0]), std::out_of_range);
        EXPECT_THROW((void)copy.read(1), std::out_of_range);
        aho::x::MeshAttributes<Attribute> empty;
        EXPECT_TRUE(empty.empty());
    }
} // namespace
#endif

namespace {
    TEST(MeshAttributesTest, ExplicitFixedAndBoundedStoragePreserveRowsAndRejectCapacityOverflow) {
        check_bounded_attributes<kk::storage::fixed_layout<2>>();
        check_bounded_attributes<kk::storage::dynamic_layout<2>>();
    }

    TEST(MeshAttributesTest, NativeSoAColumnsKeepDefaultsConstnessAndProxyFields) {
        Attributes attributes(2);
        EXPECT_EQ(attributes.size(), 2u);
        EXPECT_FALSE(attributes.empty());
        EXPECT_EQ(attributes.read(1).name, "default");
        EXPECT_EQ(attributes.read(1).uv1[0], 1);
        attributes.column<&Attribute::uv0>()[0] = {2, 3};
        attributes[0].get<&Attribute::visible>() = false;
        EXPECT_FALSE(attributes.read(0).visible);
        EXPECT_EQ(attributes.read(0).uv1[0], 1);
        EXPECT_EQ(attributes.at(0).read().uv0[1], 3);
        const auto& read_only = attributes;
        static_assert(std::same_as<decltype(read_only.column<&Attribute::name>()), std::span<const std::string>>);
        EXPECT_EQ(read_only[0].get<&Attribute::uv0>()[1], 3);
        EXPECT_FALSE(read_only.at(0).read().visible);
        EXPECT_THROW((void)attributes.at(2), std::out_of_range);
        EXPECT_THROW((void)read_only.at(2), std::out_of_range);
    }

    TEST(MeshAttributesTest, StructInputAndRowWriteKeepRepeatedFieldTypesSeparate) {
        const std::array rows{Attribute{{2, 3}, {4, 5}, "first", false}};
        Attributes attributes(rows);
        EXPECT_EQ(attributes.read(0).uv0[0], 2);
        attributes.at(0).write(Attribute{{6, 7}, {8, 9}, "updated", true});
        EXPECT_EQ(attributes.read(0).uv1[1], 9);
        EXPECT_EQ(attributes.read(0).name, "updated");
        EXPECT_TRUE(attributes.read(0).visible);
        EXPECT_THROW(attributes.write(1, rows[0]), std::out_of_range);
        Attributes empty;
        EXPECT_TRUE(empty.empty());
    }
} // namespace
