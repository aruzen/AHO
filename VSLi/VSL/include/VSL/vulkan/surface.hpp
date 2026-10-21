#pragma once
#include "../define.hpp"
#include "pv.hpp"
#include "../utils/string.hpp"

#include "../window.hpp"
#include "vulkan.hpp"

namespace VSL_NAMESPACE {
    struct Surface : public VSL_NAMESPACE::PureWindow::Plugin {
        Surface(VSL_NAMESPACE::PureWindow* data, VSL_NAMESPACE::VulkanAccessor instance);

        std::shared_ptr<VSL_NAMESPACE::impl::SurfaceImpl> data_;
    };
} // namespace VSL_NAMESPACE
