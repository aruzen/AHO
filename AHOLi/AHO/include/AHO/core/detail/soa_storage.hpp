#pragma once
#include "../../define.hpp"
#include <saitekikuukan.hpp>
#include <expected>
#include <stdexcept>

namespace AHO_NAMESPACE::detail {
    // IDとイベントを使わない所有列の失敗を、AHOの例外契約へ合わせる。
    inline void check_soa_result(std::expected<void, SAITEKIKUUKAN_NAMESPACE::error_code> result) {
        if (result)
            return;
        if (result.error() == SAITEKIKUUKAN_NAMESPACE::error_code::buffer_over)
            throw std::length_error("SoA storage capacity exceeded");
        throw std::logic_error("Unexpected SoA storage failure");
    }
} // namespace AHO_NAMESPACE::detail
