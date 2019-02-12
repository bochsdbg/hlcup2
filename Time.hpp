#pragma once

#include <limits>

namespace hlcup {

struct Time {
    static const long long kLeapoch     = (946684800LL + 86400 * (31 + 29));
    static const long      kDaysPer400Y = (365 * 400 + 97);
    static const long      kDaysPer100Y = (365 * 100 + 24);
    static const long      kDaysPer4Y   = (365 * 4 + 1);

    Time(long long t) {
        long long         days, secs, years;
        int               remdays, remsecs, remyears;
        int               qc_cycles, c_cycles, q_cycles;
        int               months;
        int               wday, yday, leap;
        static const char days_in_month[] = {31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 29};

        secs    = t - kLeapoch;
        days    = secs / 86400;
        remsecs = secs % 86400;
        if (remsecs < 0) {
            remsecs += 86400;
            days--;
        }

        wday = (3 + days) % 7;
        if (wday < 0) wday += 7;

        qc_cycles = static_cast<int>(days / kDaysPer400Y);
        remdays   = days % kDaysPer400Y;
        if (remdays < 0) {
            remdays += kDaysPer400Y;
            qc_cycles--;
        }

        c_cycles = remdays / kDaysPer100Y;
        if (c_cycles == 4) c_cycles--;
        remdays -= c_cycles * kDaysPer100Y;

        q_cycles = remdays / kDaysPer4Y;
        if (q_cycles == 25) q_cycles--;
        remdays -= q_cycles * kDaysPer4Y;

        remyears = remdays / 365;
        if (remyears == 4) remyears--;
        remdays -= remyears * 365;

        leap = !remyears && (q_cycles || !c_cycles);
        yday = remdays + 31 + 28 + leap;
        if (yday >= 365 + leap) yday -= 365 + leap;

        years = remyears + 4 * q_cycles + 100 * c_cycles + 400LL * qc_cycles;

        for (months = 0; days_in_month[months] <= remdays; months++) remdays -= days_in_month[months];

        if (months >= 10) {
            months -= 12;
            years++;
        }

        year = static_cast<int>(years + 100);
        mon  = months + 2;
        mday = remdays + 1;
        wday = static_cast<int>(wday);
        yday = static_cast<int>(yday);

        hour = remsecs / 3600;
        min  = remsecs / 60 % 60;
        sec  = remsecs % 60;
    }

    int year, mon, mday, wday, yday, hour, min, sec;
};

}  // namespace hlcup
