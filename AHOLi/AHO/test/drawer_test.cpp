#include <AHO/drawer/draw_phase.hpp>
#include "support/commands.hpp"

#if defined(AHO_LINK_DRAWER_COMMAND)
int main() {
    aho::DrawStream<aho_test::RecordingStream> stream;
    std::shared_ptr<vsl::command::__Command> command;
    stream << command;
}
#elif defined(AHO_LINK_DRAWER_DELEGATE)
int main() {
    aho::DrawStream<aho_test::RecordingStream> stream;
    std::shared_ptr<vsl::command::__DelegateCommand> command;
    stream << command;
}
#else
#include <gtest/gtest.h>
namespace drawer_payload {
struct Value { int* calls; aho::DrawPhase** received; };
void draw(Value value, aho::DrawPhase* phase) { ++*value.calls; *value.received = phase; }
}
namespace {
TEST(DrawPackTest, PackKeepsLvalueReferencesAndForwardsRvalueTypes) {
    int number = 3;
    std::string text = "hello";
    auto packed = aho::pack(number, text);
    EXPECT_EQ(&std::get<0>(packed), &number); EXPECT_EQ(&std::get<1>(packed), &text);
    std::get<0>(packed) = 7; EXPECT_EQ(number, 7);
    static_assert(noexcept(aho::pack(number, text)));
    static_assert(std::same_as<decltype(aho::pack(std::declval<std::string>())), std::tuple<std::string&&>>);
}
TEST(DrawStreamTest, CommandsAndDelegatesAreForwardedAndReturnOriginalStream) {
    aho::DrawStream<aho_test::RecordingStream> stream;
    int commands = 0;
    EXPECT_EQ(&(stream << aho_test::RecordingCommand(&commands)), &stream);
    EXPECT_EQ(&(stream << aho_test::RecordingDelegate(&commands)), &stream);
    EXPECT_EQ(stream.phase_stream.calls, 2);
    EXPECT_EQ(commands, 0) << "DrawStream must forward commands rather than invoking them itself.";
}
TEST(DrawStreamTest, TupleDispatchCallsAdlDrawWithCurrentPhase) {
    const auto previous = aho::THREAD_LOCAL_DRAW_PHASE;
    struct Restore { aho::DrawPhase* value; ~Restore() { aho::THREAD_LOCAL_DRAW_PHASE = value; } } restore{previous};
    aho::THREAD_LOCAL_DRAW_PHASE = nullptr;
    int calls = 0;
    aho::DrawPhase* received = previous;
    drawer_payload::Value value{&calls, &received};
    aho::DrawStream<aho_test::RecordingStream> stream;
    EXPECT_EQ(&(stream << aho::pack(value)), &stream);
    EXPECT_EQ(calls, 1); EXPECT_EQ(received, nullptr);
}
}
#endif
