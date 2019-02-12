#pragma once

#include "common.hpp"

namespace hlcup {

struct ParseUtils {
    static const constexpr u8 kUnhexTable[64] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    inline static u8 hexToInt(u8 ch) { return kUnhexTable[(ch - '0') & 0x3f]; }

    static ssize_t unescapeUrlValue(const u8 *&p, const u8 *pe, u8 *&out) {
        while (p != pe) {
            u8 ch = *p;

            if (ch == '&' || ch == ' ' || ch == '\0') {
                return 0;
            } else if (ch == '%') {
                ssize_t dp = pe - p;
                if (dp < 3) {
                    return 3 - dp;
                } else {
                    *out++ = u8(hexToInt(p[1]) << 4) | hexToInt(p[2]);
                    p += 3;
                }
            } else if (ch == '+') {
                *out++ = ' ';
                ++p;
            } else {
                *out++ = ch;
                ++p;
            }
        }
        return 1;
    }
};

}  // namespace hlcup
