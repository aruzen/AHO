#pragma once
#include "define.hpp"
#include <string>
#include <sstream>
#include <type_traits>

namespace VSL_NAMESPACE::default_using {
    using Character = char;
    using String = std::string;
    using StringView = std::string_view;
    using Stringstream = std::stringstream;
} // namespace VSL_NAMESPACE::default_using

namespace VSL_NAMESPACE {
    template<typename T>
    concept is_supported_charsets = std::is_same_v<char, T> || std::is_same_v<char8_t, T> || std::is_same_v<wchar_t, T> || std::is_same_v<char16_t, T> || std::is_same_v<char32_t, T>;
}

namespace VSL_NAMESPACE::helper {
    template<is_supported_charsets CharType, size_t size>
    class ConstantStringHolder {
    public:
        using Chcaracter = CharType;
        static constexpr size_t SIZE = size;

        consteval ConstantStringHolder(const CharType (&str)[size]) {
            for (size_t i = 0; i < SIZE; i++)
                value[i] = str[i];
        }

        CharType value[SIZE];
    };

    template<is_supported_charsets CharType, char c, char8_t c8,
             wchar_t wc, char16_t c16, char32_t c32>
    consteval auto adjust_char();

    template<is_supported_charsets CharType, ConstantStringHolder c, ConstantStringHolder c8,
             ConstantStringHolder wc, ConstantStringHolder c16, ConstantStringHolder c32>
    consteval auto adjust_string();

    class CharacterContainer {
        char _char_holder;
        char8_t _char8_t_holder;
        wchar_t _wchar_t_holder;
        char16_t _char16_t_holder;
        char32_t _char32_t_holder;

    public:
        CharacterContainer(char char_holder,
                           char8_t char8_t_holder,
                           wchar_t wchar_t_holder,
                           char16_t char16_t_holder,
                           char32_t char32_t_holder) : _char_holder(char_holder),
                                                       _char8_t_holder(char8_t_holder),
                                                       _wchar_t_holder(wchar_t_holder),
                                                       _char16_t_holder(char16_t_holder),
                                                       _char32_t_holder(char32_t_holder) {}

        template<is_supported_charsets CharType>
        CharType get() const;
    };

    class StringContainer {
        std::basic_string<char> _char_holder;
        std::basic_string<char8_t> _char8_t_holder;
        std::basic_string<wchar_t> _wchar_t_holder;
        std::basic_string<char16_t> _char16_t_holder;
        std::basic_string<char32_t> _char32_t_holder;

    public:
        StringContainer(std::basic_string<char> char_holder,
                        std::basic_string<char8_t> char8_t_holder,
                        std::basic_string<wchar_t> wchar_t_holder,
                        std::basic_string<char16_t> char16_t_holder,
                        std::basic_string<char32_t> char32_t_holder) : _char_holder(char_holder),
                                                                       _char8_t_holder(char8_t_holder),
                                                                       _wchar_t_holder(wchar_t_holder),
                                                                       _char16_t_holder(char16_t_holder),
                                                                       _char32_t_holder(char32_t_holder) {}

        template<is_supported_charsets CharType>
        std::basic_string<CharType> get() const;
    };

    template<is_supported_charsets CharType, char c, char8_t c8,
             wchar_t wc, char16_t c16, char32_t c32>
    consteval auto adjust_char() {
        if constexpr (std::is_same_v<CharType, char>)
            return c;
        else if constexpr (std::is_same_v<CharType, char8_t>)
            return c8;
        else if constexpr (std::is_same_v<CharType, wchar_t>)
            return wc;
        else if constexpr (std::is_same_v<CharType, char16_t>)
            return c16;
        else if constexpr (std::is_same_v<CharType, char32_t>)
            return c32;
    }

    template<is_supported_charsets CharType, ConstantStringHolder c, ConstantStringHolder c8,
             ConstantStringHolder wc, ConstantStringHolder c16, ConstantStringHolder c32>
    consteval auto adjust_string() {
        if constexpr (std::is_same_v<CharType, char>)
            return c;
        else if constexpr (std::is_same_v<CharType, char8_t>)
            return c8;
        else if constexpr (std::is_same_v<CharType, wchar_t>)
            return wc;
        else if constexpr (std::is_same_v<CharType, char16_t>)
            return c16;
        else if constexpr (std::is_same_v<CharType, char32_t>)
            return c32;
    }

    template<is_supported_charsets CharType>
    CharType CharacterContainer::get() const {
        if constexpr (std::is_same_v<CharType, char>) {
            return _char_holder;
        }
        if constexpr (std::is_same_v<CharType, char8_t>) {
            return _char8_t_holder;
        }
        if constexpr (std::is_same_v<CharType, wchar_t>) {
            return _wchar_t_holder;
        }
        if constexpr (std::is_same_v<CharType, char16_t>) {
            return _char16_t_holder;
        }
        if constexpr (std::is_same_v<CharType, char32_t>) {
            return _char32_t_holder;
        }
    }

    template<is_supported_charsets CharType>
    std::basic_string<CharType> StringContainer::get() const {
        if constexpr (std::is_same_v<CharType, char>) {
            return _char_holder;
        }
        if constexpr (std::is_same_v<CharType, char8_t>) {
            return _char8_t_holder;
        }
        if constexpr (std::is_same_v<CharType, wchar_t>) {
            return _wchar_t_holder;
        }
        if constexpr (std::is_same_v<CharType, char16_t>) {
            return _char16_t_holder;
        }
        if constexpr (std::is_same_v<CharType, char32_t>) {
            return _char32_t_holder;
        }
    }
} // namespace VSL_NAMESPACE::helper

#define VSL_PREFIX(str) VSL_NAMESPACE::helper::adjust_string<Character,               \
                                                             str,                     \
                                                             ___VSL_JOIN_2(u8, str),  \
                                                             ___VSL_JOIN_2(L, str),   \
                                                             ___VSL_JOIN_2(u, str),   \
                                                             ___VSL_JOIN_2(U, str)>() \
                            .value

#define VSL_CHAR_PREFIX(str) VSL_NAMESPACE::helper::adjust_char<Character,              \
                                                                str,                    \
                                                                ___VSL_JOIN_2(u8, str), \
                                                                ___VSL_JOIN_2(L, str),  \
                                                                ___VSL_JOIN_2(u, str),  \
                                                                ___VSL_JOIN_2(U, str)>()

#define VSL_STR_PREFIX(str) std::basic_string<Character>(VSL_NAMESPACE::helper::adjust_string<Character,               \
                                                                                              str,                     \
                                                                                              ___VSL_JOIN_2(u8, str),  \
                                                                                              ___VSL_JOIN_2(L, str),   \
                                                                                              ___VSL_JOIN_2(u, str),   \
                                                                                              ___VSL_JOIN_2(U, str)>() \
                                                             .value)

#define VSL_T_PREFIX(type, str) VSL_NAMESPACE::helper::adjust_string<type,                    \
                                                                     str,                     \
                                                                     ___VSL_JOIN_2(u8, str),  \
                                                                     ___VSL_JOIN_2(L, str),   \
                                                                     ___VSL_JOIN_2(u, str),   \
                                                                     ___VSL_JOIN_2(U, str)>() \
                                    .value

#define VSL_T_CHAR_PREFIX(type, str) VSL_NAMESPACE::helper::adjust_char<type,                   \
                                                                        str,                    \
                                                                        ___VSL_JOIN_2(u8, str), \
                                                                        ___VSL_JOIN_2(L, str),  \
                                                                        ___VSL_JOIN_2(u, str),  \
                                                                        ___VSL_JOIN_2(U, str)>()

#define VSL_T_STR_PREFIX(type, str) std::basic_string<type>(VSL_NAMESPACE::helper::adjust_string<type,                    \
                                                                                                 str,                     \
                                                                                                 ___VSL_JOIN_2(u8, str),  \
                                                                                                 ___VSL_JOIN_2(L, str),   \
                                                                                                 ___VSL_JOIN_2(u, str),   \
                                                                                                 ___VSL_JOIN_2(U, str)>() \
                                                                .value)

#define VSL_STRING_CONTAINER(str) VSL_NAMESPACE::helper::StringContainer(str,                    \
                                                                         ___VSL_JOIN_2(u8, str), \
                                                                         ___VSL_JOIN_2(L, str),  \
                                                                         ___VSL_JOIN_2(u, str),  \
                                                                         ___VSL_JOIN_2(U, str))

#define VSL_CHARACTER_CONTAINER(str) VSL_NAMESPACE::helper::CharacterContainer(str,                    \
                                                                               ___VSL_JOIN_2(u8, str), \
                                                                               ___VSL_JOIN_2(L, str),  \
                                                                               ___VSL_JOIN_2(u, str),  \
                                                                               ___VSL_JOIN_2(U, str))

#define VSL_DEFAULT_PREFIX(str) ___VSL_JOIN_2(PARALLEL_PARADIGM_PREFIX, str)

#define VSL_EXPAND_TEMPLATE(expr1, expr2) \
    ___VSL_JOIN_3(expr1, char, expr2)     \
    ___VSL_JOIN_3(expr1, char8_t, expr2)  \
    ___VSL_JOIN_3(expr1, wchar_t, expr2)  \
    ___VSL_JOIN_3(expr1, char16_t, expr2) \
    ___VSL_JOIN_3(expr1, char32_t, expr2)
