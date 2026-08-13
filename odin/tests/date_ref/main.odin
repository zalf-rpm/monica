// Odin side of the Phase 0 differential test for support/date.
//
// Must emit byte-identical output to odin/tests/cpp_ref/date_ref_main.cpp.
// Run odin/tests/cpp_ref/run.sh to build both and diff them.
package date_ref

import "core:fmt"
import d "../../support/date"

b :: proc(v: bool) -> int {return v ? 1 : 0}

row :: proc(kind: string, key: string, dt: d.Date) {
	iso := d.to_iso_date_string(dt, "", context.temp_allocator)
	str := d.to_string(dt, ".", false, context.temp_allocator)
	fmt.printf(
		"%s;%s;%d;%d;%d;%d;%d;%d;%d;%s;%s\n",
		kind,
		key,
		b(d.is_valid(dt)),
		int(d.day(dt)),
		int(d.month(dt)),
		d.year(dt),
		b(d.is_relative_date(dt)),
		b(d.use_leap_years(dt)),
		int(d.julian_day(dt)),
		iso,
		str,
	)
}

main :: proc() {
	fmt.printf("kind;key;valid;day;month;year;rel;leap;jd;iso;str\n")

	// --- 1. day-by-day sweep across leap and non-leap years -------------------
	{
		dt := d.make_date(1, 1, 1990)
		start := d.make_date(1, 1, 1990)
		for i in 0 ..< (6 * 365 + 2) {
			key := fmt.tprintf("%d", i)
			row("sweep", key, dt)
			fmt.printf(
				"sweepx;%s;%d;%d;%d;%d\n",
				key,
				d.number_of_days_to(start, dt),
				d.diff(dt, start),
				int(d.days_in_month(dt)),
				b(d.is_leap_year(dt)),
			)
			dt = d.add(dt, 1)
			free_all(context.temp_allocator)
		}
	}

	// --- 2. arithmetic at various offsets ------------------------------------
	{
		offsets := []int{0, 1, 5, 10, 20, 30, 100, 300, 400, 1000, 3000}
		bases := []d.Date {
			d.make_date(25, 2, 2008),
			d.make_date(5, 3, 2008),
			d.make_date(1, 1, 2000),
			d.make_date(31, 12, 1999),
			d.make_date(28, 2, 1900),
			d.make_date(1, 3, 2100),
		}
		for base in bases {
			for o in offsets {
				biso := d.to_iso_date_string(base, "", context.temp_allocator)
				row("add", fmt.tprintf("%s+%d", biso, o), d.add(base, u64(o)))
				row("sub", fmt.tprintf("%s-%d", biso, o), d.sub(base, u64(o)))
				free_all(context.temp_allocator)
			}
		}
	}

	// --- 3. construction edge cases ------------------------------------------
	{
		years := []int{1900, 1999, 2000, 2007, 2008, 2100}
		days := []int{0, 1, 28, 29, 30, 31, 32}
		for y in years {
			// m stays <= 12: month 13 aborts the C++ (see the driver's note)
			for m in 0 ..= 12 {
				for dd in days {
					key := fmt.tprintf("%d-%d-%d", y, m, dd)
					row("ctor", key, d.make_date(u8(dd), u8(m), u16(y), false, false, true))
					row("ctorV", key, d.make_date(u8(dd), u8(m), u16(y), false, true, true))
					row("ctorNL", key, d.make_date(u8(dd), u8(m), u16(y), false, false, false))
					free_all(context.temp_allocator)
				}
			}
		}
	}

	// --- 4. relative dates and toAbsoluteDate --------------------------------
	{
		rel_years := []int{0, 1, 2}
		rel_months := []int{1, 2, 9, 12}
		rel_days := []int{1, 23, 28, 29}
		for y in rel_years {
			for m in rel_months {
				for dd in rel_days {
					key := fmt.tprintf("%d-%d-%d", y, m, dd)
					r := d.relative_date(u8(dd), u8(m), u16(y))
					row("rel", key, r)
					row("relAbs1991", key, d.to_absolute_date(r, 1991))
					row("relAbs2008", key, d.to_absolute_date(r, 2008))
					row("relAbsIgn", key, d.to_absolute_date(r, 2008, true))
					free_all(context.temp_allocator)
				}
			}
		}
	}

	// --- 5. ISO parsing -------------------------------------------------------
	{
		isos := []string {
			"1991-09-22",
			"1992-07-30",
			"0000-09-23",
			"0001-08-25",
			"2008-02-29",
			"2007-02-29",
			"1900-02-29",
			"2000-02-29",
			"1991-09",
			"",
			"1991-00-01",
			"0099-12-31",
			"0100-01-01",
		}
		for s in isos {
			row("iso", s, d.from_iso_date_string(s))
			row("isoCtor", s, d.make_date_from_iso_string(s))
			free_all(context.temp_allocator)
		}
	}

	// --- 6. julianDate / dayInYear -------------------------------------------
	{
		jd_years := []int{1991, 2007, 2008, 2000, 1900}
		for y in jd_years {
			for jd in 1 ..= 366 {
				key := fmt.tprintf("%d/%d", y, jd)
				row("jd", key, d.julian_date(u16(jd), u16(y)))
				free_all(context.temp_allocator)
			}
		}
	}

	// --- 7. with*/setters, incl. the stale days-in-month quirk ----------------
	{
		base := d.make_date(29, 2, 2008)
		row("withYear2007", "", d.with_year(base, 2007))
		row("withYear2012", "", d.with_year(base, 2012))
		row("withAdded-1", "", d.with_added_years(base, -1))
		row("withAdded+4", "", d.with_added_years(base, 4))
		row("withDay1", "", d.with_day(base, 1))
		row("withDay31", "", d.with_day(base, 31))
		row("withDay31V", "", d.with_day(base, 31, true))
		row("withMonth1", "", d.with_month(base, 1))
		row("withMonth13V", "", d.with_month(base, 13, true))
		row("startOfYear", "", d.start_of_year(base))
		row("endOfYear", "", d.end_of_year(base))
		row("startOfMonth", "", d.start_of_month(base))
		row("endOfMonth", "", d.end_of_month(base))

		s2007 := d.with_year(base, 2007)
		fmt.printf("quirk;setYearDim;%d;%d\n", int(d.days_in_month(s2007, 2)), b(d.is_leap_year(s2007)))
		no_leap := d.make_date(1, 1, 2008, false, false, false)
		fmt.printf("quirk;ctorNoLeapDim;%d\n", int(d.days_in_month(no_leap, 2)))
		defd: d.Date
		fmt.printf(
			"quirk;defaultDim;%d;%d;%d\n",
			int(d.days_in_month(defd, 2)),
			b(d.use_leap_years(defd)),
			b(d.is_valid(defd)),
		)
		free_all(context.temp_allocator)
	}

	// --- 8. comparisons -------------------------------------------------------
	{
		ds := []d.Date {
			d.make_date(1, 1, 2001),
			d.make_date(2, 1, 2001),
			d.make_date(31, 12, 2000),
			d.make_date(15, 6, 2001),
			d.make_date(10, 7, 2001),
			d.make_date(1, 1, 2000),
			d.make_date(29, 2, 2008),
			d.make_date(1, 3, 2008),
		}
		for a in ds {
			for bb in ds {
				aiso := d.to_iso_date_string(a, "", context.temp_allocator)
				biso := d.to_iso_date_string(bb, "", context.temp_allocator)
				fmt.printf(
					"cmp;%s|%s;%d;%d;%d;%d;%d;%d;%d\n",
					aiso,
					biso,
					b(d.lt(a, bb)),
					b(d.eq(a, bb)),
					b(d.ne(a, bb)),
					b(d.le(a, bb)),
					b(d.gt(a, bb)),
					b(d.ge(a, bb)),
					d.number_of_days_to(a, bb),
				)
				free_all(context.temp_allocator)
			}
		}
	}
}
