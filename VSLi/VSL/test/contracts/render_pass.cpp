#if VSL_TYPED_CASE == 0 && !defined(VSL_RENDER_PASS_FACADE)
#include <VSL/operation/render_pass_description.hpp>

void accept_description(vsl::RenderPassDescription& description);
void accept_description(vsl::RenderPassDescription& description) {
    description.add_color({.format = vsl::data_format::UNSIGNED_NORMALIZED8_RGBA});
    description.validate();
}
#else
#include <VSL/vulkan/render_pass.hpp>
#include <utility>

void accept_pass(const vsl::RenderPassAccessor& pass);
void accept_pass(const vsl::RenderPassAccessor& pass) {
    (void)pass.description();
    (void)pass.owner_ref();
#if VSL_TYPED_CASE == 2
    (void)std::move(pass).description();
#elif VSL_TYPED_CASE == 3
    (void)vsl::RenderPassAccessor{}.description();
#endif
}
#endif
