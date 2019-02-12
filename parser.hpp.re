#pragma once

#include <cstdio>
#include <string>

struct A {
    std::string aaa;
    std::string bbb;
};

const char *lex(const char *p, const char *pe, A *a) {
    printf("start\n");
    const char *YYMARKER;
    int         s = 0, c = 0;
    char        yych;

    std::string *buf;

    /*!types:re2c*/

    /*!stags:re2c format = 'const char *@@;';  */
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = p;
        re2c:define:YYLIMIT = pe;

        re2c:define:YYGETSTATE = "s";
        re2c:define:YYGETSTATE:naked = 1;
        re2c:define:YYSETSTATE = "s = @@;";
        re2c:define:YYSETSTATE:naked = 1;

        re2c:define:YYGETCONDITION = "c";
        re2c:define:YYGETCONDITION:naked = 1;
        re2c:define:YYSETCONDITION = "c = @@;";
        re2c:define:YYSETCONDITION:naked = 1;

        re2c:define:YYFILL = "void(@@);";
        re2c:define:YYFILL:naked = 1;

        ws = [ \t\n\r];

        <init> ws* "{" { goto yyc_inobject; }
        <init> * { printf("err\n"); return p; }

        <inobject> "aaa" ws* ":" ws* { buf = &a->aaa;  goto yyc_string; }
        <inobject> "bbb" ws* ":" ws* { buf = &a->bbb;  goto yyc_string; }
        <inobject> "}" { return 0; }
        <inobject> ws+ { goto yyc_inobject; }
        <inobject> * { printf("err: inobject \n"); return p; }

        <string> ["] { goto yyc_string_body; }
        <string> * { printf("err: string \n"); return p; }

        <string_body> ["] { buf = nullptr; goto yyc_inobject; }
        <string_body> * { *buf += yych; goto yyc_string_body; }
    */
}
