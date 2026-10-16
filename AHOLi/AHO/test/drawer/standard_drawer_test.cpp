#include <AHO/drawer/standard_drawer.hpp>
#include <AHO/drawer/draw_phase.hpp>
#if defined(AHO_LINK_STANDARD_DRAWER)
int main() {
    aho::Triangle<float, vsl::D2> triangle(aho::d2::PointF(0, 0), aho::d2::PointF(1, 0), aho::d2::PointF(0, 1));
    aho::draw(triangle, aho::RGBA(1, 0, 0, 1));
}
#elif defined(AHO_CONTRACT_STANDARD_DRAWER_TUPLE)
void verify() {
    aho::Triangle<float, vsl::D2> triangle;
    aho::RGBA color(1, 0, 0, 1);
    aho::DrawStream<int> stream;
    stream << aho::pack(triangle, color);
}
#else
static_assert(std::same_as<decltype(&aho::draw), void(*)(const aho::Triangle<float, vsl::D2>&, const aho::RGBA&)>);
#endif
