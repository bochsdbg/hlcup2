#include "HttpParser.hpp"

#include <iostream>

namespace hlcup {
namespace {

// clang-format off
%%{
    machine http_parser;

    unsigned_number = [0-9] @{ tmp.u32_val = fc - '0'; } ( [0-9] @{ tmp.u32_val = tmp.u32_val * 10 + (fc - '0'); })**;

    entity_id = unsigned_number %{ req.query.basic.entity_id = tmp.u32_val; };

    vse_slozhno = "%D0%B2%D1%81%D1%91+%D1%81%D0%BB%D0%BE%D0%B6%D0%BD%D0%BE";
    zanyaty     = "%D0%B7%D0%B0%D0%BD%D1%8F%D1%82%D1%8B";
    svobodny    = "%D1%81%D0%B2%D0%BE%D0%B1%D0%BE%D0%B4%D0%BD%D1%8B";

    crlf = "\r\n";

    action start_string {
        tmp.u32_val = offset;
    }

    string_val = (([^%&+ ] @{ out_buf[offset++] = fc; }) |
      ( '+' @{ out_buf[offset++] = ' '; }) |
      ( "%" ([0-9] @{ out_buf[offset] = (fc - '0') << 4;} |
             [a-f] @{ out_buf[offset] = (fc - 'a' + 10) << 4;} |
             [A-F] @{ out_buf[offset] = (fc - 'A' + 10) << 4;} )
            ([0-9] @{ out_buf[offset++] |= (fc - '0');} |
             [a-f] @{ out_buf[offset++] |= (fc - 'a' + 10);} |
             [A-F] @{ out_buf[offset++] |= (fc - 'A' + 10);} )
      ))**;

    basic_param = (
        ("request_id=" unsigned_number %{ req.req_id = tmp.u32_val; })
                | ("sex=f" %{ req.mask |= B::kSex; req.query.basic.sex = Sex::kFemale; })
                | ("sex=m" %{ req.mask |= B::kSex; req.query.basic.sex = Sex::kMale; })
                | ("status=" vse_slozhno %{ req.mask |= B::kStatus; req.query.basic.status = Status::kComplicated; })
                | ("status=" zanyaty %{ req.mask |= B::kStatus; req.query.basic.status = Status::kOccupied; })
                | ("status=" svobodny %{ req.mask |= B::kStatus; req.query.basic.status = Status::kFree; })
                | ("country=" string_val >start_string %{ req.mask |= B::kCountry; req.query.basic.country.set_by_offset(tmp.u32_val, offset); })
                | ("city=" string_val >start_string %{ req.mask |= B::kCity; req.query.basic.city.set_by_offset(tmp.u32_val, offset); })
                | ("interests=" string_val >start_string %{ req.mask |= B::kInterests; req.query.basic.interests.set_by_offset(tmp.u32_val, offset); })
                | ("likes=" unsigned_number %{ req.mask |= B::kLikes; req.query.basic.likes = tmp.u32_val; })
                | ("joined=" unsigned_number %{ req.mask |= B::kJoined; req.query.basic.joined = tmp.u32_val; })
                | ("birth=" unsigned_number %{ req.mask |= B::kBirth; req.query.basic.birth = tmp.u32_val; })
                | ("limit=" unsigned_number %{ req.query.basic.limit = tmp.u32_val; })
    );

    filter_param = (
    "request_id=" unsigned_number %{ req.req_id = tmp.u32_val; }
    );

    basic_params = basic_param ("&" basic_param)**;

    filter_params = filter_param ("&" filter_param)**;

    proto = "HTTP/1.1";

    skip_line := [^\n]** '\n' @{ fgoto headers; };

    headers := (
    (/content-length/i space** ':' space** unsigned_number %{ content_length = tmp.u32_val; } crlf) |
    (crlf @{ fbreak; })
    )** @{ fgoto skip_line; };

    request_line = [\r\n ]** (
        ("GET /accounts/" entity_id "/suggest/?" basic_params " " proto crlf %{ req.type = R::Type::kSuggest; } ) |
        ("GET /accounts/" entity_id "/recommend/?" basic_params " " proto crlf %{ req.type = R::Type::kRecommend;  } ) |
        ("GET /accounts/filter/?" filter_params " " proto crlf  %{ req.type = R::Type::kFilter;  } ) |
        ("GET /accounts/group/?" basic_params " " proto crlf %{ req.type = R::Type::kGroup;  } )
    ) @!{ req.type = R::Type::kInvalid; fgoto skip_line; };

    main := ( request_line >{ out_buf = req.string_data; offset = 0; } @{ fgoto headers; } );

    write data;
}%%

}  // namespace

    // clang-format on

    void HttpParser::reset() {
        // clang-format off
    %% write init;
        // clang-format on
    }

    ssize_t HttpParser::parse(const char *p, const char *pe, Request &req) {
        using R = Request;
        using F = R::Filter;
        using B = R::Basic;

        const char *eof = nullptr;

        // clang-format off
        %% write exec;
        // clang-format on

        return 0;
    }

    }  // namespace hlcup
