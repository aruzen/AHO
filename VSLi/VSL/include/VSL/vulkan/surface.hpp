#pragma once
#include "../define.hpp"
#include "pv.hpp"
#include "../utils/string.hpp"

#include "../window.hpp"
#include "vulkan.hpp"

namespace VSL_NAMESPACE {
    struct Surface : public VSL_NAMESPACE::PureWindow::Plugin {
        Surface(VSL_NAMESPACE::PureWindow* data, VSL_NAMESPACE::VulkanAccessor instance);
        // 移行用plugin登録。sourceのnative Surfaceとownerをそのまま使う。
        Surface(VSL_NAMESPACE::PureWindow* window, Surface source);
        explicit Surface(std::shared_ptr<impl::SurfaceImpl> data);
        lifetime::OwnerRef owner_ref() const;
        VulkanAccessor get_instance() const;
        lifetime::OSWindowOwner get_window_owner() const;

        std::shared_ptr<VSL_NAMESPACE::impl::SurfaceImpl> data_;
    };
} // namespace VSL_NAMESPACE
