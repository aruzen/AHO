#pragma once
#include <VSL/operation/buffer_access.hpp>

namespace VSL_NAMESPACE::sync {
    BufferCopyVisibility buffer_copy_visibility(bool host_read);
} // namespace VSL_NAMESPACE::sync
