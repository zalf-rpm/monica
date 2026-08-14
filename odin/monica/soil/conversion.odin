// Partial port of src/soil/conversion.{h,cpp}.
//
// Only the procedures reachable from create-env-from-json-config's reference
// patterns are ported here, because phase 1b needs them. The rest of
// conversion.cpp (percentSandAndClayToKA5Texture, texture2lambda, ...) belongs
// to phase 3 - see plan-odin.md.
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
