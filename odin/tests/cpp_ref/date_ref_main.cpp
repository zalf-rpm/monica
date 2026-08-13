/* Differential-test driver for the Odin Date port.
 *
 * Dumps Tools::Date behaviour over a multi-year sweep plus a set of edge cases
 * as CSV on stdout. The Odin side (odin/tests/date_ref/) emits the same CSV and
 * the two are diffed byte-for-byte. This is the Phase 0 oracle for
 * odin/support/date.
 *
 * Build (from the repo root, in a VS developer prompt):
 *   cl /EHsc /std:c++17 /nologo /Fe:build\date_ref.exe ^
 *      odin\tests\cpp_ref\date_ref_main.cpp mas_cpp_misc\tools\date.cpp ^
 *      mas_cpp_misc\tools\algorithms.cpp /I mas_cpp_misc /I mas_cpp_misc\tools
 *
 * See odin/tests/cpp_ref/run.sh.
 */

#include <cstdio>
#include <string>

#include "tools/date.h"

using namespace Tools;

static void row(const char *kind, const std::string &key, const Date &d) {
  // isValid is printed first so an invalid date's other fields are still
  // compared (they are all 0 by construction).
  printf("%s;%s;%d;%d;%d;%d;%d;%d;%d;%s;%s\n", kind, key.c_str(), d.isValid() ? 1 : 0,
         int(d.day()), int(d.month()), d.year(), d.isRelativeDate() ? 1 : 0,
         d.useLeapYears() ? 1 : 0, int(d.julianDay()), d.toIsoDateString().c_str(),
         d.toString().c_str());
}

int main() {
  // unbuffered, so that if a case aborts (see the month-13 notes below) the
  // output up to that point survives and pinpoints the culprit
  setvbuf(stdout, nullptr, _IONBF, 0);

  printf("kind;key;valid;day;month;year;rel;leap;jd;iso;str\n");

  // --- 1. day-by-day sweep across leap and non-leap years -------------------
  {
    Date d(1, 1, 1990);
    Date start(1, 1, 1990);
    for (int i = 0; i < 6 * 365 + 2; i++) {
      char key[64];
      snprintf(key, sizeof(key), "%d", i);
      row("sweep", key, d);
      // relations that must also match
      printf("sweepx;%s;%d;%d;%d;%d\n", key, start.numberOfDaysTo(d), d - start,
             int(d.daysInMonth()), d.isLeapYear() ? 1 : 0);
      d = d + 1;
    }
  }

  // --- 2. arithmetic at various offsets ------------------------------------
  {
    const int offsets[] = {0, 1, 5, 10, 20, 30, 100, 300, 400, 1000, 3000};
    const Date bases[] = {Date(25, 2, 2008), Date(5, 3, 2008),  Date(1, 1, 2000),
                          Date(31, 12, 1999), Date(28, 2, 1900), Date(1, 3, 2100)};
    for (const auto &b : bases) {
      for (int o : offsets) {
        char key[64];
        snprintf(key, sizeof(key), "%s+%d", b.toIsoDateString().c_str(), o);
        row("add", key, b + o);
        snprintf(key, sizeof(key), "%s-%d", b.toIsoDateString().c_str(), o);
        row("sub", key, b - o);
      }
    }
  }

  // --- 3. construction edge cases ------------------------------------------
  {
    for (int y : {1900, 1999, 2000, 2007, 2008, 2100}) {
      // NOTE: m must stay <= 12. The Date constructor calls daysInMonth(month),
      // which is `_daysInMonth->at(month)` on a 13-element vector, so month == 13
      // throws std::out_of_range and aborts the process (verified empirically).
      // The Odin port guards this and returns 0; since the C++ terminates, no
      // correct program can observe the difference.
      for (int m = 0; m <= 12; m++) {
        for (int dd : {0, 1, 28, 29, 30, 31, 32}) {
          char key[64];
          snprintf(key, sizeof(key), "%d-%d-%d", y, m, dd);
          row("ctor", key, Date(uint8_t(dd), uint8_t(m), uint16_t(y), false, false, true));
          row("ctorV", key, Date(uint8_t(dd), uint8_t(m), uint16_t(y), false, true, true));
          row("ctorNL", key, Date(uint8_t(dd), uint8_t(m), uint16_t(y), false, false, false));
        }
      }
    }
  }

  // --- 4. relative dates and toAbsoluteDate --------------------------------
  {
    for (int y : {0, 1, 2}) {
      for (int m : {1, 2, 9, 12}) {
        for (int dd : {1, 23, 28, 29}) {
          char key[64];
          snprintf(key, sizeof(key), "%d-%d-%d", y, m, dd);
          Date r = Date::relativeDate(uint8_t(dd), uint8_t(m), uint16_t(y));
          row("rel", key, r);
          row("relAbs1991", key, r.toAbsoluteDate(1991));
          row("relAbs2008", key, r.toAbsoluteDate(2008));
          row("relAbsIgn", key, r.toAbsoluteDate(2008, true));
        }
      }
    }
  }

  // --- 5. ISO parsing -------------------------------------------------------
  {
    // NOTE: no month-13 string here ("1991-13-01") - it reaches
    // daysInMonth(13) and aborts, same as the ctor loop above.
    const char *isos[] = {"1991-09-22", "1992-07-30", "0000-09-23", "0001-08-25",
                          "2008-02-29", "2007-02-29", "1900-02-29", "2000-02-29",
                          "1991-09",    "",           "1991-00-01", "0099-12-31",
                          "0100-01-01"};
    for (const char *s : isos) {
      row("iso", s, Date::fromIsoDateString(s));
      row("isoCtor", s, Date(std::string(s)));
    }
  }

  // --- 6. julianDate / dayInYear -------------------------------------------
  {
    for (int y : {1991, 2007, 2008, 2000, 1900}) {
      for (int jd = 1; jd <= 366; jd++) {
        char key[64];
        snprintf(key, sizeof(key), "%d/%d", y, jd);
        row("jd", key, Date::julianDate(uint16_t(jd), uint16_t(y)));
      }
    }
  }

  // --- 7. with*/setters, incl. the stale days-in-month quirk ----------------
  {
    Date base(29, 2, 2008);
    row("withYear2007", "", base.withYear(2007));
    row("withYear2012", "", base.withYear(2012));
    row("withAdded-1", "", base.withAddedYears(-1));
    row("withAdded+4", "", base.withAddedYears(4));
    row("withDay1", "", base.withDay(1));
    row("withDay31", "", base.withDay(31));
    row("withDay31V", "", base.withDay(31, true));
    row("withMonth1", "", base.withMonth(1));
    row("withMonth13V", "", base.withMonth(13, true));
    row("startOfYear", "", base.startOfYear());
    row("endOfYear", "", base.endOfYear());
    row("startOfMonth", "", base.startOfMonth());
    row("endOfMonth", "", base.endOfMonth());
    // daysInMonth after setYear must show the stale-table quirk
    Date s2007 = base.withYear(2007);
    printf("quirk;setYearDim;%d;%d\n", int(s2007.daysInMonth(2)), s2007.isLeapYear() ? 1 : 0);
    Date noLeap(1, 1, 2008, false, false, false);
    printf("quirk;ctorNoLeapDim;%d\n", int(noLeap.daysInMonth(2)));
    Date defd;
    printf("quirk;defaultDim;%d;%d;%d\n", int(defd.daysInMonth(2)), defd.useLeapYears() ? 1 : 0,
           defd.isValid() ? 1 : 0);
  }

  // --- 8. comparisons -------------------------------------------------------
  {
    const Date ds[] = {Date(1, 1, 2001),  Date(2, 1, 2001),  Date(31, 12, 2000),
                       Date(15, 6, 2001), Date(10, 7, 2001), Date(1, 1, 2000),
                       Date(29, 2, 2008), Date(1, 3, 2008)};
    for (const auto &a : ds) {
      for (const auto &b : ds) {
        printf("cmp;%s|%s;%d;%d;%d;%d;%d;%d;%d\n", a.toIsoDateString().c_str(),
               b.toIsoDateString().c_str(), a < b, a == b, a != b, a <= b, a > b, a >= b,
               a.numberOfDaysTo(b));
      }
    }
  }

  return 0;
}
