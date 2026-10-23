#pragma once
#include "../../define.hpp"
#include <saitekikuukan.hpp>
#include <array>
#include <algorithm>
#include <concepts>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace AHO_NAMESPACE::x {
    // reflection非対応のツールチェーンでも、Mesh属性を型パック列へ接続する。
    template<typename Row>
    struct MeshAttributeSchema;

    template<auto... Members>
    struct MeshAttributeMembers {
        static_assert((std::is_member_object_pointer_v<decltype(Members)> && ...));
    };

    namespace detail {
        template<typename T>
        struct MeshAttributeMemberType;

        template<typename Owner, typename Value>
        struct MeshAttributeMemberType<Value Owner::*> {
            using OwnerType = Owner;
            using ValueType = Value;
        };

        struct MeshAttributeIndex {
            template<auto Left, auto Right>
            static consteval bool SameMember() {
                if constexpr (std::same_as<decltype(Left), decltype(Right)>)
                    return Left == Right;
                else
                    return false;
            }

            template<auto Target, auto... Members>
            static consteval std::size_t Get() {
                constexpr std::array<bool, sizeof...(Members)> matches{SameMember<Target, Members>()...};
                static_assert(std::count(matches.begin(), matches.end(), true) == 1,
                              "The field must occur exactly once in MeshAttributeSchema");
                return static_cast<std::size_t>(std::find(matches.begin(), matches.end(), true) - matches.begin());
            }
        };

        template<typename Row, typename List>
        struct MeshAttributeColumns;

        template<typename Row, auto... Members>
        struct MeshAttributeColumns<Row, MeshAttributeMembers<Members...>> {
            static_assert((std::same_as<Row, typename MeshAttributeMemberType<decltype(Members)>::OwnerType> && ...),
                          "MeshAttributeSchema fields must belong to the row type");
            static_assert(((MeshAttributeIndex::Get<Members, Members...>() < sizeof...(Members)) && ...));
            using Columns = SAITEKIKUUKAN_NAMESPACE::soa<typename MeshAttributeMemberType<decltype(Members)>::ValueType...>;
        };
    } // namespace detail

    template<typename Row, typename Members = typename MeshAttributeSchema<Row>::Members>
    class MeshAttributes;

    template<typename Row, auto... Members>
    class MeshAttributes<Row, MeshAttributeMembers<Members...>> {
        static_assert(std::default_initializable<Row> && std::copy_constructible<Row>);
        using Columns = typename detail::MeshAttributeColumns<Row, MeshAttributeMembers<Members...>>::Columns;
        Columns _columns;

    public:
        template<bool IS_CONST>
        class RowRef {
            using Parent = std::conditional_t<IS_CONST, const MeshAttributes, MeshAttributes>;
            Parent* _parent;
            std::size_t _index;

        public:
            RowRef(Parent& parent, std::size_t index) noexcept;
            RowRef(const RowRef&) = default;
            RowRef& operator=(const RowRef&) = delete;

            template<auto Member>
            [[nodiscard]] decltype(auto) get() const noexcept;
            [[nodiscard]] Row read() const;
            void write(const Row& row) const
            requires(!IS_CONST);
        };

        using ValueType = Row;
        MeshAttributes() = default;
        explicit MeshAttributes(std::span<const Row> rows);
        explicit MeshAttributes(std::size_t count);

        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        template<auto Member>
        [[nodiscard]] auto column() noexcept;
        template<auto Member>
        [[nodiscard]] auto column() const noexcept;

        [[nodiscard]] RowRef<false> operator[](std::size_t index) noexcept;
        [[nodiscard]] RowRef<true> operator[](std::size_t index) const noexcept;
        [[nodiscard]] RowRef<false> at(std::size_t index);
        [[nodiscard]] RowRef<true> at(std::size_t index) const;
        [[nodiscard]] Row read(std::size_t index) const;
        void write(std::size_t index, const Row& row);
    };

    template<typename Row, auto... Members>
    MeshAttributes<Row, MeshAttributeMembers<Members...>>::MeshAttributes(std::span<const Row> rows) {
        _columns.reserve(rows.size());
        for (const auto& row : rows)
            _columns.push_back(row.*Members...);
    }

    template<typename Row, auto... Members>
    MeshAttributes<Row, MeshAttributeMembers<Members...>>::MeshAttributes(std::size_t count) {
        _columns.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const Row row{};
            _columns.push_back(row.*Members...);
        }
    }

    template<typename Row, auto... Members>
    std::size_t MeshAttributes<Row, MeshAttributeMembers<Members...>>::size() const noexcept {
        return _columns.size();
    }

    template<typename Row, auto... Members>
    bool MeshAttributes<Row, MeshAttributeMembers<Members...>>::empty() const noexcept {
        return _columns.empty();
    }

    template<typename Row, auto... Members>
    template<auto Member>
    auto MeshAttributes<Row, MeshAttributeMembers<Members...>>::column() noexcept {
        return _columns.template column<detail::MeshAttributeIndex::Get<Member, Members...>()>();
    }

    template<typename Row, auto... Members>
    template<auto Member>
    auto MeshAttributes<Row, MeshAttributeMembers<Members...>>::column() const noexcept {
        return _columns.template column<detail::MeshAttributeIndex::Get<Member, Members...>()>();
    }

    template<typename Row, auto... Members>
    auto MeshAttributes<Row, MeshAttributeMembers<Members...>>::operator[](std::size_t index) noexcept -> RowRef<false> {
        return {*this, index};
    }

    template<typename Row, auto... Members>
    auto MeshAttributes<Row, MeshAttributeMembers<Members...>>::operator[](std::size_t index) const noexcept -> RowRef<true> {
        return {*this, index};
    }

    template<typename Row, auto... Members>
    auto MeshAttributes<Row, MeshAttributeMembers<Members...>>::at(std::size_t index) -> RowRef<false> {
        if (index >= size())
            throw std::out_of_range("MeshAttributes row index exceeds size");
        return (*this)[index];
    }

    template<typename Row, auto... Members>
    auto MeshAttributes<Row, MeshAttributeMembers<Members...>>::at(std::size_t index) const -> RowRef<true> {
        if (index >= size())
            throw std::out_of_range("MeshAttributes row index exceeds size");
        return (*this)[index];
    }

    template<typename Row, auto... Members>
    Row MeshAttributes<Row, MeshAttributeMembers<Members...>>::read(std::size_t index) const {
        (void)at(index);
        Row result{};
        ((result.*Members = column<Members>()[index]), ...);
        return result;
    }

    template<typename Row, auto... Members>
    void MeshAttributes<Row, MeshAttributeMembers<Members...>>::write(std::size_t index, const Row& row) {
        (void)at(index);
        _columns.write(index, typename Columns::value_type{row.*Members...});
    }

    template<typename Row, auto... Members>
    template<bool IS_CONST>
    MeshAttributes<Row, MeshAttributeMembers<Members...>>::RowRef<IS_CONST>::RowRef(Parent& parent, std::size_t index) noexcept
        : _parent(&parent), _index(index) {}

    template<typename Row, auto... Members>
    template<bool IS_CONST>
    template<auto Member>
    decltype(auto) MeshAttributes<Row, MeshAttributeMembers<Members...>>::RowRef<IS_CONST>::get() const noexcept {
        return _parent->template column<Member>()[_index];
    }

    template<typename Row, auto... Members>
    template<bool IS_CONST>
    Row MeshAttributes<Row, MeshAttributeMembers<Members...>>::RowRef<IS_CONST>::read() const {
        return _parent->read(_index);
    }

    template<typename Row, auto... Members>
    template<bool IS_CONST>
    void MeshAttributes<Row, MeshAttributeMembers<Members...>>::RowRef<IS_CONST>::write(const Row& row) const
    requires(!IS_CONST)
    {
        _parent->write(_index, row);
    }
} // namespace AHO_NAMESPACE::x
