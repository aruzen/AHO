#include <VSL/utils/typed_frame_resources.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    struct Frame {
        int value;
    };
    using Frames = t::TypedFrameResources<Frame>;
#if VSL_TYPED_CASE == 0
    static_assert(!std::is_copy_constructible_v<Frames>);
    static_assert(std::is_nothrow_move_constructible_v<Frames>);
    static_assert(std::same_as<decltype(std::declval<Frames&>().get(std::declval<const vsl::ComputePhase&>())), const Frame&>);
    void accepted(vsl::CommandManager manager, const vsl::ComputePhase& phase) {
        Frames frames(manager, [](size_t) { return Frame{}; });
        frames.update(phase, [](Frame& frame) { frame.value = 7; });
        const Frame& current = frames.get(phase);
        (void)current;
    }
#elif VSL_TYPED_CASE == 1
    void rejected(Frames& frames) {
        frames.get(0);
    }
#elif VSL_TYPED_CASE == 2
    void rejected(vsl::CommandManager manager) {
        Frames frames(manager, [](size_t) { return 0; });
    }
#elif VSL_TYPED_CASE == 3
    void rejected(Frames& frames, const vsl::ComputePhase& phase) {
        frames.update(phase, [](int&) {});
    }
#elif VSL_TYPED_CASE == 4
    void rejected(Frames& frames, const vsl::ComputePhase& phase) {
        frames.update(phase, [](Frame&) { return 7; });
    }
#elif VSL_TYPED_CASE == 5
    void rejected(Frames& frames, const vsl::ComputePhase& phase) {
        frames.get(phase).value = 7;
    }
#elif VSL_TYPED_CASE == 6
    void rejected(Frames& frames) {
        Frames copy(frames);
    }
#endif
} // namespace
