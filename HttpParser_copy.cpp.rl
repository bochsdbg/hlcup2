#include "HttpParser.hpp"

#include <iostream>

namespace hlcup {

void HttpParser::parse(char *&p, const char *pe, Request &req) {
    using R = Request;
    using F = R::Filter;
    using B = R::Basic;

    char *   marker;
    int      condition = -1;
    unsigned yyaccept;
    char     yych;

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

re2c:yyfill:enable = 0;
//            re2c:define:YYFILL = "void(@@);";
//            re2c:define:YYFILL:naked = 1;

vse_slozhno = "%D0%B2%D1%81%D1%91+%D1%81%D0%BB%D0%BE%D0%B6%D0%BD%D0%BE";
zanyaty     = "%D0%B7%D0%B0%D0%BD%D1%8F%D1%82%D1%8B";
svobodny    = "%D1%81%D0%B2%D0%BE%D0%B1%D0%BE%D0%B4%D0%BD%D1%8B";

<>  {}

<init>  "GET /accounts/filter/?"    { req.type = Request::Type::kFilter; goto yyc_filter_param; }
<init>  "GET /accounts/group/?"     { req.type = Request::Type::kGroup; goto yyc_basic_param; }
<init>  "GET /accounts/"  { goto yyc_get_accounts; }
<init>  "POST /accounts/new/?" { req.type = Request::Type::kAccountsNew; printf("accounts/new\n"); return; }
<init>  "POST /accounts/" { printf("accounts/<id>\n"); return; }
<init>  "POST /accounts/likes/?" {}

<get_accounts> "" {}

<filter_param> "sex_eq=f" { req.setSex(F::kSexEq, Sex::kFemale); goto yyc_filter_param_next; }
<filter_param> "sex_eq=m" { req.setSex(F::kSexEq, Sex::kMale);   goto yyc_filter_param_next; }
<filter_param> "status_eq=" vse_slozhno  { req.setStatus(F::kStatusEq, Status::kComplicated); goto yyc_filter_param_next; }
<filter_param> "status_eq=" zanyaty      { req.setStatus(F::kStatusEq, Status::kOccupied); goto yyc_filter_param_next; }
<filter_param> "status_eq=" svobodny     { req.setStatus(F::kStatusEq, Status::kFree); goto yyc_filter_param_next; }
<filter_param> "status_neq=" vse_slozhno { req.setStatus(F::kStatusNeq, Status::kComplicated); goto yyc_filter_param_next; }
<filter_param> "status_neq=" zanyaty     { req.setStatus(F::kStatusNeq, Status::kOccupied); goto yyc_filter_param_next; }
<filter_param> "status_neq=" svobodny    { req.setStatus(F::kStatusNeq, Status::kFree); goto yyc_filter_param_next; }
<filter_param> "" {}

<filter_param_next> "" {}

<basic_param> "" {}

    */
}

}  // namespace hlcup
