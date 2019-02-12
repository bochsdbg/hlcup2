#include "AccountParser.hpp"

#define YYDEBUG(state, ch) \
    { printf("debug: %d, %c\n", state, ch); }

namespace hlcup {

bool AccountParser::parse(const char *&p, const char *pe, Account &acc) {
    char *      string_data = acc.string_data;
    u32         offset      = 0;
    const char *marker;
    int         condition = -1;
    char        yych;

    StringRef     tmp_ref;
    Account::Like tmp_like;

    /*!stags:re2c format = "const char *@@;"; */

    /*!re2c
        re2c:flags:bit-vectors = 1;
        re2c:flags:computed-gotos = 0;
        re2c:flags:lookahead = 0;

        re2c:indent:string = '    ';
        re2c:indent:top = 1;

        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = p;
        re2c:define:YYLIMIT = pe;
        re2c:define:YYMARKER = marker;
        re2c:variable:yych   = yych;

        re2c:define:YYGETSTATE = "-1";
        re2c:define:YYGETSTATE:naked = 1;
        re2c:define:YYSETSTATE = "";
        re2c:define:YYSETSTATE:naked = 1;

        re2c:define:YYGETCONDITION = "condition";
        re2c:define:YYGETCONDITION:naked = 1;
        re2c:define:YYSETCONDITION = "condition = @@;";
        re2c:define:YYSETCONDITION:naked = 1;

//            re2c:define:YYMTAGP = "mtagp";
//            re2c:define:YYMTAGN = "mtagn";

        re2c:yyfill:enable = 0;
//            re2c:define:YYFILL = "void(@@);";
//            re2c:define:YYFILL:naked = 1;

        ws = [ \t\n\r];
        quot = ["];
        kv_sep = quot ws* ":" ws*;
        vse_slozhno = "\\u0432\\u0441\\u0451 \\u0441\\u043b\\u043e\\u0436\\u043d\\u043e";
        zanyaty = "\\u0437\\u0430\\u043d\\u044f\\u0442\\u044b";
        svobodny = "\\u0441\\u0432\\u043e\\u0431\\u043e\\u0434\\u043d\\u044b";

        <init> ws* "{" ws* quot  { goto yyc_key; }

        <key> "fname"   kv_sep quot { if (!parse_string(p, pe, string_data, offset, acc.fname)) return false;   goto yyc_next_field; }
        <key> "sname"   kv_sep quot { if (!parse_string(p, pe, string_data, offset, acc.sname)) return false;   goto yyc_next_field; }
        <key> "email"   kv_sep quot { if (!parse_string(p, pe, string_data, offset, acc.email)) return false;   goto yyc_next_field; }
        <key> "phone"   kv_sep quot { if (!parse_string(p, pe, string_data, offset, acc.phone)) return false;   goto yyc_next_field; }
        <key> "city"    kv_sep quot { if (!parse_string(p, pe, string_data, offset, acc.city)) return false;    goto yyc_next_field; }
        <key> "country" kv_sep quot { if (!parse_string(p, pe, string_data, offset, acc.country)) return false; goto yyc_next_field; }

        <key> "status" kv_sep quot zanyaty quot { acc.status = Account::kOccupied; goto yyc_next_field; }
        <key> "status" kv_sep quot vse_slozhno quot { acc.status = Account::kComplicated; goto yyc_next_field; }
        <key> "status" kv_sep quot svobodny quot { acc.status = Account::kFree; goto yyc_next_field; }

        <key> "sex" kv_sep quot "f" quot { acc.sex = Account::kFemale; goto yyc_next_field; }
        <key> "sex" kv_sep quot "m" quot { acc.sex = Account::kMale; goto yyc_next_field; }

        <key> "joined" kv_sep { if (!parse_timestamp(p, pe, acc.joined)) return false; goto yyc_next_field; }
        <key> "id" kv_sep     { if (!parse_uint(p, pe, acc.id)) return false; goto yyc_next_field; }
        <key> "birth" kv_sep  { if (!parse_timestamp(p, pe, acc.birth)) return false; goto yyc_next_field; }

        <key> "interests" kv_sep "[" ws* { goto yyc_interests; }
        <key> "likes" kv_sep "[" ws* { goto yyc_likes; }

        <key> "premium" kv_sep "{" ws* quot { goto yyc_premium_key; }

        <premium_key> "start" kv_sep  { if (!parse_timestamp(p, pe, acc.premium.start)) return false; goto yyc_premium_next_field; }
        <premium_key> "finish" kv_sep { if (!parse_timestamp(p, pe, acc.premium.finish)) return false; goto yyc_premium_next_field; }

        <premium_next_field> ws* "," ws* quot { goto yyc_premium_key; }
        <premium_next_field> ws* "}" { goto yyc_next_field; }

        <next_field> ws* "," ws* quot { goto yyc_key; }
        <next_field> ws* "}" { return true; }

        <interests> "]" ws* { goto yyc_next_field; }
        <interests> quot {
            if (!parse_string(p, pe, string_data, offset, tmp_ref)) return false;
            acc.interests.push_back(tmp_ref.offset);
            goto yyc_interests_next;
        }

        <interests_next> ws* "," ws* { goto yyc_interests; }
        <interests_next> "]" { acc.interests.push_back(tmp_ref.offset + tmp_ref.size); goto yyc_next_field; }

        <likes> "{" ws* quot { goto yyc_likes_key; }
        <likes> "]" { goto yyc_next_field; }

        <likes_key> "id" kv_sep { if (!parse_uint(p, pe, tmp_like.to_id)) return false; goto yyc_likes_next_field; }
        <likes_key> "ts" kv_sep { if (!parse_timestamp(p, pe, tmp_like.ts)) return false; goto yyc_likes_next_field; }

        <likes_next_field> ws* "," ws* quot { goto yyc_likes_key; }
        <likes_next_field> ws* "}" ws* { if (!acc.addLike(tmp_like)) return false; goto yyc_likes_next_like; }

        <likes_next_like> "," ws* "{" ws* quot { goto yyc_likes_key; }
        <likes_next_like> "]" { goto yyc_next_field; }


        <*> * { printf("char: %c error\n", yych); p = pe; return false;  }

    */

    HLCUP_UNREACHABLE();
    return false;
}

}  // namespace hlcup
