#pragma once
#include "typed_resources.hpp"

namespace VSL_NAMESPACE::typed {
    struct DefaultResourceStoragePolicy {
        template<typename Binding, typename Allocation>
        using Buffer = TypedResourceBuffer<typename Binding::Payload, Binding::POINT.binding_type>;
        template<typename Binding, typename Allocation>
        Buffer<Binding, Allocation> make_buffer(LogicalDeviceAccessor device, size_t count, size_t element) const;
    };

    namespace detail {
        template<typename Policy, typename Binding, typename Allocation>
        consteval bool storage_policy_binding_matches();
        template<typename Schema, typename Policy>
        consteval bool bundle_storage_policy_matches();
        void validate_storage_policy_buffer(const DescriptorValue& value, size_t count, size_t expected, size_t payload_size);
    } // namespace detail

    template<typename Binding, typename Allocation>
    auto DefaultResourceStoragePolicy::make_buffer(LogicalDeviceAccessor device, size_t count, size_t) const -> Buffer<Binding, Allocation> {
        return Buffer<Binding, Allocation>(device, std::in_place, count);
    }

    namespace detail {
        template<typename Policy, typename Binding, typename Allocation>
        consteval bool storage_policy_binding_matches() {
            if constexpr (Allocation::EXTERNAL)
                return true;
            else if constexpr (!requires { typename Policy::template Buffer<Binding, Allocation>; })
                return false;
            else {
                using Buffer = typename Policy::template Buffer<Binding, Allocation>;
                if constexpr (!descriptor_matches<Binding, Buffer>())
                    return false;
                else
                    return std::move_constructible<Buffer> && requires(Policy& policy, LogicalDeviceAccessor device, Buffer& buffer, const Buffer& view, std::span<const typename Binding::Payload> values) {
                        { policy.template make_buffer<Binding, Allocation>(device, size_t{}, size_t{}) } -> std::same_as<Buffer>;
                        { view.size() } -> std::convertible_to<size_t>;
                        { view.descriptor() } -> std::same_as<DescriptorValue>;
                        { buffer.copy(values) } -> std::same_as<void>;
                    };
            }
        }
        template<typename Schema, typename Policy>
        consteval bool bundle_storage_policy_matches() {
            return []<typename... Allocations>(std::tuple<Allocations...>*) {
                return (storage_policy_binding_matches<Policy, typename Schema::template Binding<Allocations::SET, Allocations::BINDING>, Allocations>() && ...);
            }(static_cast<typename Schema::Allocations*>(nullptr));
        }
    } // namespace detail
} // namespace VSL_NAMESPACE::typed
