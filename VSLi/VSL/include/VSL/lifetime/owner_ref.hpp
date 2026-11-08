#pragma once
#include "../define.hpp"
#include <memory>
#include <type_traits>
#include <utility>

namespace VSL_NAMESPACE::lifetime {
    class DependencyNode;
    namespace detail {
        struct OwnerAccess;
    }
    // ownerと、そのowner内にある依存nodeを一体にする。shared_ptr入力はopaqueな移行入口。
    template<typename T>
    class OwnedRef {
        template<typename>
        friend class OwnedRef;
        friend class DependencyNode;
        friend struct detail::OwnerAccess;
        std::shared_ptr<T> _value;
        const DependencyNode* _node = nullptr;
        OwnedRef(std::shared_ptr<T> value, const DependencyNode* node) : _value(std::move(value)), _node(node) {}

    public:
        OwnedRef() = default;
        OwnedRef(std::nullptr_t) {}
        OwnedRef(const OwnedRef&) = default;
        OwnedRef(OwnedRef&& other) noexcept : _value(std::move(other._value)), _node(std::exchange(other._node, nullptr)) {}
        template<typename U>
        requires(std::is_convertible_v<U*, T*>)
        OwnedRef(std::shared_ptr<U> value) : _value(std::move(value)) {}
        template<typename U>
        requires(std::is_convertible_v<U*, T*>)
        OwnedRef(const OwnedRef<U>& other) : _value(other._value), _node(other._node) {}
        template<typename U>
        requires(std::is_convertible_v<U*, T*>)
        OwnedRef(OwnedRef<U>&& other) noexcept : _value(std::move(other._value)), _node(std::exchange(other._node, nullptr)) {}
        template<typename U>
        OwnedRef(const OwnedRef<U>& owner, T* pointer) : _value(owner._value, pointer), _node(owner._node) {}
        template<typename U>
        OwnedRef(const std::shared_ptr<U>& owner, T* pointer) : _value(owner, pointer) {}
        OwnedRef& operator=(OwnedRef replacement) noexcept;
        void swap(OwnedRef& other) noexcept;
        void reset() noexcept;
        T* get() const noexcept { return _value.get(); }
        T* operator->() const noexcept { return get(); }
        std::add_lvalue_reference_t<T> operator*() const
        requires(!std::is_void_v<T>)
        { return *_value; }
        explicit operator bool() const noexcept { return bool(_value); }
        long use_count() const noexcept { return _value.use_count(); }
        bool managed() const noexcept { return _node && use_count(); }
        // provenanceを持たない既存APIへ渡す明示的な移行境界。
        std::shared_ptr<T> share() const { return _value; }
        template<typename U>
        OwnedRef<U> alias(U* pointer) const { return {_value, pointer, _node}; }
        template<typename U>
        bool owner_before(const OwnedRef<U>& other) const noexcept { return _value.owner_before(other._value); }
        template<typename U>
        bool owner_before(const std::shared_ptr<U>& other) const noexcept { return _value.owner_before(other); }
        bool operator==(const OwnedRef& other) const noexcept { return get() == other.get(); }
        template<typename U>
        bool operator==(const std::shared_ptr<U>& other) const noexcept { return get() == other.get(); }

    private:
        template<typename U>
        OwnedRef(const std::shared_ptr<U>& owner, T* pointer, const DependencyNode* node) : _value(owner, pointer), _node(node) {}
    };
    using OwnerRef = OwnedRef<void>;

    template<typename T>
    OwnedRef<T>& OwnedRef<T>::operator=(OwnedRef replacement) noexcept {
        swap(replacement);
        return *this;
    }
    template<typename T>
    void OwnedRef<T>::swap(OwnedRef& other) noexcept {
        _value.swap(other._value);
        std::swap(_node, other._node);
    }
    template<typename T>
    void OwnedRef<T>::reset() noexcept {
        OwnedRef empty;
        swap(empty);
    }
} // namespace VSL_NAMESPACE::lifetime
