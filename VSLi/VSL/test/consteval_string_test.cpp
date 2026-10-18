#include <VSL/consteval_string.hpp>
// The public stream alias must be complete before any other headers are included.
static_assert(sizeof(vsl::default_using::stringstream) > 0);
#include <gtest/gtest.h>

namespace {
static_assert(vsl::is_supported_charsets<char> && vsl::is_supported_charsets<char8_t>);
static_assert(vsl::is_supported_charsets<wchar_t> && vsl::is_supported_charsets<char16_t>);
static_assert(vsl::is_supported_charsets<char32_t> && !vsl::is_supported_charsets<int>);
constexpr vsl::helper::constant_string_holder holder("AHO");
static_assert(holder.size == 4 && holder.value[3] == '\0');
static_assert(VSL_T_CHAR_PREFIX(char, 'a') == 'a');
static_assert(VSL_T_CHAR_PREFIX(char8_t, 'a') == u8'a');
static_assert(VSL_T_CHAR_PREFIX(wchar_t, 'a') == L'a');
static_assert(VSL_T_CHAR_PREFIX(char16_t, 'a') == u'a');
static_assert(VSL_T_CHAR_PREFIX(char32_t, 'a') == U'a');
TEST(ConstantStringTest, AdjustStringPreservesCharactersAndTerminator) {
    EXPECT_EQ(VSL_T_STR_PREFIX(char, "hello"), "hello");
    EXPECT_TRUE(VSL_T_STR_PREFIX(char8_t, "hello") == u8"hello");
    EXPECT_EQ(VSL_T_STR_PREFIX(wchar_t, "hello"), L"hello");
    EXPECT_EQ(VSL_T_STR_PREFIX(char16_t, "hello"), u"hello");
    EXPECT_EQ(VSL_T_STR_PREFIX(char32_t, "hello"), U"hello");
}
TEST(ConstantStringTest, StringContainerReturnsEachEncoding) {
    const auto value = VSL_STRING_CONTAINER("AHO");
    EXPECT_EQ(value.get<char>(), "AHO");
    EXPECT_TRUE(value.get<char8_t>() == u8"AHO");
    EXPECT_EQ(value.get<wchar_t>(), L"AHO");
    EXPECT_EQ(value.get<char16_t>(), u"AHO");
    EXPECT_EQ(value.get<char32_t>(), U"AHO");
}
TEST(ConstantStringTest, CharacterContainerReturnsEachEncoding) {
    const auto value = VSL_CHARACTER_CONTAINER('z');
    EXPECT_EQ(value.get<char>(), 'z');
    EXPECT_EQ(value.get<char8_t>(), u8'z');
    EXPECT_EQ(value.get<wchar_t>(), L'z');
    EXPECT_EQ(value.get<char16_t>(), u'z');
    EXPECT_EQ(value.get<char32_t>(), U'z');
}
TEST(ConstantStringTest, DefaultStringStreamSupportsFormattedOutput) {
    vsl::default_using::stringstream stream;
    stream << "value=" << 7 << '/' << 2.5;
    EXPECT_EQ(stream.str(), "value=7/2.5");
    static_assert(std::same_as<decltype(stream.str()), vsl::default_using::string>);
}
TEST(ConstantStringTest, DefaultCharacterMacrosUseSelectedCharacterType) {
    using character = char16_t;
    EXPECT_EQ((VSL_CHAR_PREFIX('x')), u'x');
    EXPECT_EQ(VSL_STR_PREFIX("text"), u"text");
    EXPECT_EQ(std::u16string(VSL_PREFIX("text")), u"text");
}
} // namespace
