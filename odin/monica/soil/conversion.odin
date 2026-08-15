// Port of src/soil/conversion.{h,cpp}.
//
// humusClass2corg, bulkDensityClass2rawDensity, sandAndClay2lambda,
// KA5texture2sand and KA5texture2clay were already ported in phase 1b (needed
// by create-env-from-json-config's reference patterns). Phase 3 adds the last
// function: sandAndClay2KA5texture (the reverse direction).
package soil

import "core:strings"
import tl "../../support/tools"

// C++: EResult<double> Soil::humusClass2corg(int humusClass)
humus_class_2_corg :: proc(humus_class: int, allocator := context.allocator) -> tl.EResult(f64) {
	res: tl.EResult(f64)
	res.allocator = allocator
	switch humus_class {
	case 0:
		res.result = 0.0
		return res
	case 1:
		res.result = 0.5 / 1.72
		return res
	case 2:
		res.result = 1.5 / 1.72
		return res
	case 3:
		res.result = 3.0 / 1.72
		return res
	case 4:
		res.result = 6.0 / 1.72
		return res
	case 5:
		res.result = 11.5 / 2.0
		return res
	case 6:
		res.result = 17.5 / 2.0
		return res
	case 7:
		res.result = 30.0 / 2.0
		return res
	}
	res.result = 0.0
	tl.append_errorf(&res, "Soil::humusClass2corg: Unknown humus class: %d!", humus_class)
	return res
}

// C++: EResult<double> Soil::bulkDensityClass2rawDensity(int bulkDensityClass, double clay)
//
// NOTE(c++-quirk): on an unknown class the C++ records an error but still falls
// through and computes the result with x == 0.0, rather than returning early.
// Reproduced.
bulk_density_class_2_raw_density :: proc(
	bulk_density_class: int,
	clay: f64,
	allocator := context.allocator,
) -> tl.EResult(f64) {
	res: tl.EResult(f64)
	res.allocator = allocator
	x := 0.0

	switch bulk_density_class {
	case 1:
		x = 1.3
	case 2:
		x = 1.5
	case 3:
		x = 1.7
	case 4:
		x = 1.9
	case 5:
		x = 2.1
	case:
		tl.append_errorf(
			&res,
			"Soil::bulkDensityClass2rawDensity: Unknown bulk density class: %d!",
			bulk_density_class,
		)
	}

	// * 1000 = conversion from g cm-3 -> kg m-3
	res.result = (x - (0.9 * clay)) * 1000.0
	return res
}

// C++: double Soil::sandAndClay2lambda(double sand, double clay)
sand_and_clay_2_lambda :: proc(sand, clay: f64) -> f64 {
	lambda := (2.0 * (sand * sand * 0.575)) + (clay * 0.1) + ((1.0 - sand - clay) * 0.35)
	return lambda
}

@(private)
ka5_sand_lookup :: proc(t: string) -> (f64, bool) {
	switch t {
	case "FS":
		return 0.84, true
	case "FSMS":
		return 0.86, true
	case "FSGS":
		return 0.88, true
	case "GS":
		return 0.93, true
	case "MSGS":
		return 0.96, true
	case "MSFS":
		return 0.93, true
	case "MS":
		return 0.96, true
	case "SS":
		return 0.93, true
	case "SL2":
		return 0.76, true
	case "SL3":
		return 0.65, true
	case "SL4":
		return 0.60, true
	case "SLU":
		return 0.43, true
	case "ST2":
		return 0.84, true
	case "ST3":
		return 0.71, true
	case "SU2":
		return 0.80, true
	case "SU3":
		return 0.63, true
	case "SU4":
		return 0.56, true
	case "LS2":
		return 0.34, true
	case "LS3":
		return 0.44, true
	case "LS4":
		return 0.56, true
	case "LT2":
		return 0.30, true
	case "LT3":
		return 0.20, true
	case "LTS":
		return 0.42, true
	case "LU":
		return 0.19, true
	case "UU":
		return 0.10, true
	case "ULS":
		return 0.30, true
	case "US":
		return 0.31, true
	case "UT2":
		return 0.13, true
	case "UT3":
		return 0.11, true
	case "UT4":
		return 0.09, true
	case "UTL":
		return 0.19, true
	case "TT":
		return 0.17, true
	case "TL":
		return 0.17, true
	case "TU2":
		return 0.12, true
	case "TU3":
		return 0.10, true
	case "TS3":
		return 0.52, true
	case "TS2":
		return 0.37, true
	case "TS4":
		return 0.62, true
	case "TU4":
		return 0.05, true
	case "L":
		return 0.35, true
	case "S":
		return 0.93, true
	case "U":
		return 0.10, true
	case "T":
		return 0.17, true
	case "HZ1":
		return 0.30, true
	case "HZ2":
		return 0.30, true
	case "HZ3":
		return 0.30, true
	case "HH":
		return 0.15, true
	case "HN":
		return 0.15, true
	}
	return 0, false
}

@(private)
ka5_clay_lookup :: proc(t: string) -> (f64, bool) {
	switch t {
	case "FS":
		return 0.02, true
	case "FSMS":
		return 0.02, true
	case "FSGS":
		return 0.02, true
	case "GS":
		return 0.02, true
	case "MSGS":
		return 0.02, true
	case "MSFS":
		return 0.02, true
	case "MS":
		return 0.02, true
	case "SS":
		return 0.02, true
	case "SL2":
		return 0.06, true
	case "SL3":
		return 0.10, true
	case "SL4":
		return 0.14, true
	case "SLU":
		return 0.12, true
	case "ST2":
		return 0.11, true
	case "ST3":
		return 0.21, true
	case "SU2":
		return 0.02, true
	case "SU3":
		return 0.04, true
	case "SU4":
		return 0.04, true
	case "LS2":
		return 0.21, true
	case "LS3":
		return 0.21, true
	case "LS4":
		return 0.21, true
	case "LT2":
		return 0.30, true
	case "LT3":
		return 0.40, true
	case "LTS":
		return 0.35, true
	case "LU":
		return 0.23, true
	case "UU":
		return 0.04, true
	case "ULS":
		return 0.12, true
	case "US":
		return 0.04, true
	case "UT2":
		return 0.10, true
	case "UT3":
		return 0.14, true
	case "UT4":
		return 0.21, true
	case "UTL":
		return 0.23, true
	case "TT":
		return 0.82, true
	case "TL":
		return 0.55, true
	case "TU2":
		return 0.55, true
	case "TU3":
		return 0.37, true
	case "TS3":
		return 0.40, true
	case "TS2":
		return 0.55, true
	case "TS4":
		return 0.30, true
	case "TU4":
		return 0.30, true
	case "L":
		return 0.31, true
	case "S":
		return 0.02, true
	case "U":
		return 0.04, true
	case "T":
		return 0.82, true
	case "HZ1":
		return 0.15, true
	case "HZ2":
		return 0.15, true
	case "HZ3":
		return 0.15, true
	case "HH":
		return 0.1, true
	case "HN":
		return 0.1, true
	}
	return 0, false
}

// C++: EResult<double> Soil::KA5texture2sand(string soilType)
//
// Unknown types yield 0.66 plus an error.
ka5_texture_2_sand :: proc(soil_type: string, allocator := context.allocator) -> tl.EResult(f64) {
	res: tl.EResult(f64)
	res.allocator = allocator
	upper := strings.to_upper(soil_type, context.temp_allocator)
	if v, ok := ka5_sand_lookup(upper); ok {
		res.result = v
		return res
	}
	res.result = 0.66
	tl.append_errorf(&res, "Soil::KA5texture2sand Unknown soil type: %s!", upper)
	return res
}

// C++: EResult<double> Soil::KA5texture2clay(string soilType)
//
// Unknown types yield 0.0 plus an error.
ka5_texture_2_clay :: proc(soil_type: string, allocator := context.allocator) -> tl.EResult(f64) {
	res: tl.EResult(f64)
	res.allocator = allocator
	upper := strings.to_upper(soil_type, context.temp_allocator)
	if v, ok := ka5_clay_lookup(upper); ok {
		res.result = v
		return res
	}
	res.result = 0.0
	tl.append_errorf(&res, "Soil::KA5texture2clay Unknown soil type: %s!", upper)
	return res
}

// C++: string percentSandAndClayToKA5Texture(uint8_t sand, uint8_t clay) - a
// package-local (non-exported) helper in the C++ translation unit.
//
// NOTE: the C++ if-chain guards every branch with `silt >= 0`, which is always
// true for the unsigned silt - dropped here as a no-op. `silt` itself wraps on
// underflow (sand+clay > 100) exactly like the C++ uint8_t subtraction does;
// not guarded against, matching the source.
@(private)
percent_sand_and_clay_to_ka5_texture :: proc(sand, clay: u8) -> string {
	silt := u8(100) - sand - clay
	switch {
	case silt < 10 && clay < 5:
		return "SS"
	case silt < 10 && clay >= 5 && clay < 17:
		return "ST2"
	case silt < 15 && clay >= 17 && clay < 25:
		return "ST3"
	case silt >= 10 && silt < 25 && clay < 5:
		return "SU2"
	case silt >= 25 && silt < 40 && clay < 8:
		return "SU3"
	case silt >= 40 && silt < 50 && clay < 8:
		return "SU4"
	case silt >= 10 && silt < 25 && clay >= 5 && clay < 8:
		return "SL2"
	case silt >= 10 && silt < 40 && clay >= 8 && clay < 12:
		return "SL3"
	case silt >= 10 && silt < 40 && clay >= 12 && clay < 17:
		return "SL4"
	case silt >= 40 && silt < 50 && clay >= 8 && clay < 17:
		return "SLU"
	case silt >= 40 && silt < 50 && clay >= 17 && clay < 25:
		return "LS2"
	case silt >= 30 && silt < 40 && clay >= 17 && clay < 25:
		return "LS3"
	case silt >= 15 && silt < 30 && clay >= 17 && clay < 25:
		return "LS4"
	case silt >= 30 && silt < 50 && clay >= 25 && clay < 35:
		return "LT2"
	case silt >= 30 && silt < 50 && clay >= 35 && clay < 45:
		return "LT3"
	case silt >= 15 && silt < 30 && clay >= 25 && clay < 45:
		return "LTS"
	case silt >= 50 && silt < 65 && clay >= 17 && clay < 30:
		return "LU"
	case silt >= 50 && silt < 65 && clay >= 8 && clay < 17:
		return "ULS"
	case silt >= 50 && silt < 80 && clay < 8:
		return "US"
	case silt >= 80 && clay < 8:
		return "UU"
	case silt >= 65 && clay >= 8 && clay < 12:
		return "UT2"
	case silt >= 65 && clay >= 12 && clay < 17:
		return "UT3"
	case silt >= 65 && clay >= 17 && clay < 25:
		return "UT4"
	case silt < 15 && clay >= 45 && clay < 65:
		return "TS2"
	case silt < 15 && clay >= 35 && clay < 45:
		return "TS3"
	case silt < 15 && clay >= 25 && clay < 35:
		return "TS4"
	case silt >= 15 && silt < 30 && clay >= 45 && clay < 65:
		return "TL"
	case silt >= 50 && silt < 65 && clay >= 30 && clay < 45:
		return "TU3"
	case silt >= 30 && clay >= 45 && clay < 65:
		return "TU2"
	case silt >= 65 && clay >= 25:
		return "TU4"
	case clay >= 65:
		return "TT"
	}
	return ""
}

// C++: string Soil::sandAndClay2KA5texture(double sand, double clay) - sand and
// clay are fractions [0-1]
sand_and_clay_2_ka5_texture :: proc(sand, clay: f64) -> string {
	return percent_sand_and_clay_to_ka5_texture(u8(int(sand * 100.0)), u8(int(clay * 100.0)))
}
