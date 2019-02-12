#pragma once

#include <cstring>
#include <string_view>

namespace hlcup {

template <size_t MaxLength = 255, typename CharT = char>
struct SmallString {
    using Char = CharT;
    static const constexpr size_t kCapacity = MaxLength;

    constexpr SmallString() : size(0) {}

    template <size_t N>
    constexpr SmallString(const char (&str)[N]) : size(N - 1) {
        static_assert (N - 1 < kCapacity, "Source string length exceeds capacity");
        ::memcpy(data, str, N - 1);
    }

    const Char *const_begin() const { return data; }
    const Char *const_end() const { return data + size; }
    Char *begin() { return data; }
    Char *end() { return data + size; }

    std::string_view toStringView() const {
        return std::string_view(data, size);
    }

    Char data[kCapacity];
    size_t size = 0;
};

}
