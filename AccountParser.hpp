#pragma once

#include <cassert>
#include <string>

#include "Account.hpp"
#include "common.hpp"

#include <boost/spirit/include/qi_numeric.hpp>
#include <boost/spirit/include/qi_parse.hpp>

#include "core/SmallString.hpp"

namespace hlcup {

struct AccountParser {
    static inline void write_utf8(unsigned codepoint, char *&str) {
        if (codepoint < 0x80) {
            *str++ = static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            *str++ = static_cast<char>(0xC0 | (codepoint >> 6));
            *str++ = static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            *str++ = static_cast<char>(0xE0 | (codepoint >> 12));
            *str++ = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            *str++ = static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            assert(codepoint < 0x200000);
            *str++ = static_cast<char>(0xF0 | (codepoint >> 18));
            *str++ = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            *str++ = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            *str++ = static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }

    static inline bool read_hex(const char *&p, unsigned &u) {
        unsigned v = 0;
        unsigned i = 4;
        while (i--) {
            u8 c = static_cast<u8>(*p++);
            if (c >= '0' && c <= '9') {
                c -= '0';
            } else if (c >= 'a' && c <= 'f') {
                c = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                c = c - 'A' + 10;
            } else {
                return false;
            }
            v = (v << 4) + c;
        }

        u = v;
        return true;
    }

    inline bool parse_timestamp(const char *&p, const char *pe, Timestamp &val) { return boost::spirit::qi::parse(p, pe, boost::spirit::int_, val); }

    inline bool parse_uint(const char *&p, const char *pe, unsigned &val) { return boost::spirit::qi::parse(p, pe, boost::spirit::uint_, val); }

    bool parse_string(const char *&p, const char *pe, char *buf, u32 &offset, StringRef &ref) {
        static u8 escape_replacements[256] = {
            0,   1,   2,   3,   4,   5,   6,    7,   8,   9,   10,   11,  12,   13,  14,  15,  16,  17,  18,  19,  20,   21,  22,  23,  24,   25,
            26,  27,  28,  29,  30,  31,  32,   33,  34,  35,  36,   37,  38,   39,  40,  41,  42,  43,  44,  45,  46,   47,  48,  49,  50,   51,
            52,  53,  54,  55,  56,  57,  58,   59,  60,  61,  62,   63,  64,   65,  66,  67,  68,  69,  70,  71,  72,   73,  74,  75,  76,   77,
            78,  79,  80,  81,  82,  83,  84,   85,  86,  87,  88,   89,  90,   91,  92,  93,  94,  95,  96,  97,  '\b', 99,  100, 101, '\f', 103,
            104, 105, 106, 107, 108, 109, '\n', 111, 112, 113, '\r', 115, '\t', 117, 118, 119, 120, 121, 122, 123, 124,  125, 126, 127, 128,  129,
            130, 131, 132, 133, 134, 135, 136,  137, 138, 139, 140,  141, 142,  143, 144, 145, 146, 147, 148, 149, 150,  151, 152, 153, 154,  155,
            156, 157, 158, 159, 160, 161, 162,  163, 164, 165, 166,  167, 168,  169, 170, 171, 172, 173, 174, 175, 176,  177, 178, 179, 180,  181,
            182, 183, 184, 185, 186, 187, 188,  189, 190, 191, 192,  193, 194,  195, 196, 197, 198, 199, 200, 201, 202,  203, 204, 205, 206,  207,
            208, 209, 210, 211, 212, 213, 214,  215, 216, 217, 218,  219, 220,  221, 222, 223, 224, 225, 226, 227, 228,  229, 230, 231, 232,  233,
            234, 235, 236, 237, 238, 239, 240,  241, 242, 243, 244,  245, 246,  247, 248, 249, 250, 251, 252, 253, 254,  255};

        char *data_end = &buf[offset];

        while (HLCUP_LIKELY(p < pe)) {
            switch (*p) {
            case '"':
                ref.offset = offset;
                ref.size   = static_cast<u32>(data_end - buf) - offset;
                offset     = static_cast<u32>(data_end - buf);
                ++p;
                return true;

            case '\\': {
                ++p;
                if (HLCUP_UNLIKELY(p >= pe)) { return false; }

                u8 ch = static_cast<u8>(*p);
                if (ch == 'u') {
                    unsigned u = 0;
                    ++p;
                    assert(pe - p >= 4);
                    //                    printf("%s        ", p);
                    if (!read_hex(p, u)) { return false; }
                    //                    printf("%s\n", p);
                    write_utf8(u, data_end);
                } else {
                    *data_end++ = static_cast<char>(escape_replacements[ch]);
                    ++p;
                }
                break;
            }
            default: *data_end++ = *p; ++p;
            }
        }

        return false;
    }

    bool parse(const char *&p, const char *pe, Account &acc);
};

}  // namespace hlcup
