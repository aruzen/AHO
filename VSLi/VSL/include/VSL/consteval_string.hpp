#pragma once
#include "define.hpp"
#include <string>

namespace VSL_NAMESPACE::default_using {
    using character = char;
    using string = std::string;
    using string_view = std::string_view;
    using stringstream = std::stringstream;
}

namespace VSL_NAMESPACE {
    template<typename T>
    concept is_supported_charsets = std::is_same_v<char, T>
        || std::is_same_v<char8_t, T>
        || std::is_same_v<wchar_t, T>
        || std::is_same_v<char16_t, T>
        || std::is_same_v<char32_t, T>;
}

namespace VSL_NAMESPACE::helper {
    template<is_supported_charsets CharType, size_t Size>
    class constant_string_holder {
    public:
        using chcaracter = CharType;
        static constexpr size_t size = Size;

        consteval constant_string_holder(const CharType(&str)[Size]) {
            for (size_t i = 0; i < size; i++)
                value[i] = str[i];
        }

        CharType value[size];
    };

    template<is_supported_charsets CharType, char C, char8_t C8,
        wchar_t WC, char16_t C16, char32_t C32>
    consteval auto AdjustChar() {
        if constexpr (std::is_same_v<CharType, char>)
            return C;
        else if constexpr (std::is_same_v<CharType, char8_t>)
            return C8;
        else if constexpr (std::is_same_v<CharType, wchar_t>)
            return WC;
        else if constexpr (std::is_same_v<CharType, char16_t>)
            return C16;
        else if constexpr (std::is_same_v<CharType, char32_t>)
            return C32;
    }

    template<is_supported_charsets CharType, constant_string_holder C, constant_string_holder C8,
        constant_string_holder WC, constant_string_holder C16, constant_string_holder C32>
    consteval auto AdjustString() {
        if constexpr (std::is_same_v<CharType, char>)
            return C;
        else if constexpr (std::is_same_v<CharType, char8_t>)
            return C8;
        else if constexpr (std::is_same_v<CharType, wchar_t>)
            return WC;
        else if constexpr (std::is_same_v<CharType, char16_t>)
            return C16;
        else if constexpr (std::is_same_v<CharType, char32_t>)
            return C32;
    }

    class character_container {
        char char_holder;
        char8_t char8_t_holder;
        wchar_t wchar_t_holder;
        char16_t char16_t_holder;
        char32_t char32_t_holder;
    public:
        character_container(char char_holder,
            char8_t char8_t_holder,
            wchar_t wchar_t_holder,
            char16_t char16_t_holder,
            char32_t char32_t_holder) : char_holder(char_holder),
            char8_t_holder(char8_t_holder),
            wchar_t_holder(wchar_t_holder),
            char16_t_holder(char16_t_holder),
            char32_t_holder(char32_t_holder) {}

        template<is_supported_charsets CharType>
        CharType get() const {
            if constexpr (std::is_same_v<CharType, char>) { return char_holder; }
            if constexpr (std::is_same_v<CharType, char8_t>) { return char8_t_holder; }
            if constexpr (std::is_same_v<CharType, wchar_t>) { return wchar_t_holder; }
            if constexpr (std::is_same_v<CharType, char16_t>) { return char16_t_holder; }
            if constexpr (std::is_same_v<CharType, char32_t>) { return char32_t_holder; }
        }
    };

    class string_container {
        std::basic_string<char> char_holder;
        std::basic_string<char8_t> char8_t_holder;
        std::basic_string<wchar_t> wchar_t_holder;
        std::basic_string<char16_t> char16_t_holder;
        std::basic_string<char32_t> char32_t_holder;
    public:
        string_container(std::basic_string<char> char_holder,
            std::basic_string<char8_t> char8_t_holder,
            std::basic_string<wchar_t> wchar_t_holder,
            std::basic_string<char16_t> char16_t_holder,
            std::basic_string<char32_t> char32_t_holder) : char_holder(char_holder),
            char8_t_holder(char8_t_holder),
            wchar_t_holder(wchar_t_holder),
            char16_t_holder(char16_t_holder),
            char32_t_holder(char32_t_holder) {}

        template<is_supported_charsets CharType>
        std::basic_string<CharType> get() const {
            if constexpr (std::is_same_v<CharType, char>) { return char_holder; }
            if constexpr (std::is_same_v<CharType, char8_t>) { return char8_t_holder; }
            if constexpr (std::is_same_v<CharType, wchar_t>) { return wchar_t_holder; }
            if constexpr (std::is_same_v<CharType, char16_t>) { return char16_t_holder; }
            if constexpr (std::is_same_v<CharType, char32_t>) { return char32_t_holder; }
        }
    };
}

#define VSL_PREFIX(str) VSL_NAMESPACE::helper::AdjustString<character, \
                            str,\
                            ___VSL_JOIN_2(u8, str),\
                            ___VSL_JOIN_2(L, str),\
                            ___VSL_JOIN_2(u, str),\
                            ___VSL_JOIN_2(U, str)>().value

#define VSL_CHAR_PREFIX(str) VSL_NAMESPACE::helper::AdjustChar<character, \
                            str,\
                            ___VSL_JOIN_2(u8, str),\
                            ___VSL_JOIN_2(L, str),\
                            ___VSL_JOIN_2(u, str),\
                            ___VSL_JOIN_2(U, str)>()

#define VSL_STR_PREFIX(str) std::basic_string<character>(VSL_NAMESPACE::helper::AdjustString<character, \
                            str,\
                            ___VSL_JOIN_2(u8, str),\
                            ___VSL_JOIN_2(L, str),\
                            ___VSL_JOIN_2(u, str),\
                            ___VSL_JOIN_2(U, str)>().value)

#define VSL_T_PREFIX(type, str) VSL_NAMESPACE::helper::AdjustString<type, \
                            str,\
                            ___VSL_JOIN_2(u8, str),\
                            ___VSL_JOIN_2(L, str),\
                            ___VSL_JOIN_2(u, str),\
                            ___VSL_JOIN_2(U, str)>().value

#define VSL_T_CHAR_PREFIX(type, str) VSL_NAMESPACE::helper::AdjustChar<type, \
                            str,\
                            ___VSL_JOIN_2(u8, str),\
                            ___VSL_JOIN_2(L, str),\
                            ___VSL_JOIN_2(u, str),\
                            ___VSL_JOIN_2(U, str)>()

#define VSL_T_STR_PREFIX(type, str) std::basic_string<type>(VSL_NAMESPACE::helper::AdjustString<type, \
                            str,\
                            ___VSL_JOIN_2(u8, str),\
                            ___VSL_JOIN_2(L, str),\
                            ___VSL_JOIN_2(u, str),\
                            ___VSL_JOIN_2(U, str)>().value)

#define VSL_STRING_CONTAINER(str) VSL_NAMESPACE::helper::string_container(str, \
                            ___VSL_JOIN_2(u8, str),\
                            ___VSL_JOIN_2(L, str),\
                            ___VSL_JOIN_2(u, str),\
                            ___VSL_JOIN_2(U, str))

#define VSL_CHARACTER_CONTAINER(str) VSL_NAMESPACE::helper::character_container(str, \
                            ___VSL_JOIN_2(u8, str),\
                            ___VSL_JOIN_2(L, str),\
                            ___VSL_JOIN_2(u, str),\
                            ___VSL_JOIN_2(U, str))

#define VSL_DEFAULT_PREFIX(str)  ___VSL_JOIN_2(PARALLEL_PARADIGM_PREFIX, str)

#define VSL_EXPAND_TEMPLATE(expr1, expr2) ___VSL_JOIN_3(expr1, char, expr2)     \
                                         ___VSL_JOIN_3(expr1, char8_t, expr2)  \
                                         ___VSL_JOIN_3(expr1, wchar_t, expr2)  \
                                         ___VSL_JOIN_3(expr1, char16_t, expr2) \
                                         ___VSL_JOIN_3(expr1, char32_t, expr2)
