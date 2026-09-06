// Port of the three JSON-table readers in src/soil/soil.cpp: CapillaryRiseRates
// (public), and the two anonymous-namespace (file-local in the C++) readers
// readPrincipalSoilCharacteristicData / readSoilCharacteristicModifier.
//
// The C++ tries a `.sercapnp` (binary Cap'n Proto) file first, falling back to
// `.json` (decoded via capnp::JsonCodec) only if that's missing. Per
// plan-odin.md phase 3 scope, only the `.json` path is ported - `.sercapnp` is
// ignored entirely, matching the rest of this port's Cap'n Proto quarantine.
//
// Each reader caches its parsed table in a function-local static, matching the
// C++'s `static bool initialized` + `static map<...> m` pattern (the C++ also
// takes a mutex; not reproduced, this CLI is single-threaded - the RPC/server
// mains that made the mutex necessary are out of scope, see plan-odin.md §7).
package soil

import "core:strings"
import jx "../../support/jsonx"
import tl "../../support/tools"

// C++: class Soil::CapillaryRiseRates
Capillary_Rise_Rates :: struct {
	rates: map[string]map[int]f64, // uppercased soil type -> distance -> rate
}

// C++: void CapillaryRiseRates::addRate(const string&, size_t, double)
capillary_rise_rates_add_rate :: proc(
	cr: ^Capillary_Rise_Rates,
	soil_type: string,
	distance: int,
	value: f64,
	allocator := context.allocator,
) {
	if cr.rates == nil {
		cr.rates = make(map[string]map[int]f64, allocator)
	}
	if soil_type not_in cr.rates {
		cr.rates[soil_type] = make(map[int]f64, allocator)
	}
	inner := cr.rates[soil_type]
	inner[distance] = value
	cr.rates[soil_type] = inner
}

// C++: double CapillaryRiseRates::getRate(const string&, size_t) const
//
// Falls back to the 3- then 2-character prefix of soilType if the full name
// isn't found, then to 0.0.
capillary_rise_rates_get_rate :: proc(
	cr: ^Capillary_Rise_Rates,
	soil_type: string,
	distance: int,
) -> f64 {
	inner, ok := cr.rates[soil_type]
	if !ok && len(soil_type) >= 3 {
		inner, ok = cr.rates[soil_type[:3]]
	}
	if !ok && len(soil_type) >= 2 {
		inner, ok = cr.rates[soil_type[:2]]
	}
	if !ok {
		return 0.0
	}
	v, vok := inner[distance]
	return vok ? v : 0.0
}

@(private)
g_capillary_rise_rates: Capillary_Rise_Rates
@(private)
g_capillary_rise_rates_initialized: bool

// C++: const CapillaryRiseRates& Soil::readCapillaryRiseRates()
read_capillary_rise_rates :: proc(allocator := context.allocator) -> ^Capillary_Rise_Rates {
	if !g_capillary_rise_rates_initialized {
		soil_dir := tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", allocator)
		path := strings.concatenate({soil_dir, "CapillaryRiseRates.json"}, allocator)
		r := jx.read_and_parse_json_file(path, allocator)
		if tl.success(r.errs) {
			for item in jx.array_items(jx.get(r.result, "list")) {
				soil_type := strings.to_upper(jx.string_value(item, "soilType"), allocator)
				distance := jx.int_value(item, "distance")
				rate := jx.double_value(item, "rate")
				capillary_rise_rates_add_rate(
					&g_capillary_rise_rates,
					soil_type,
					distance,
					rate,
					allocator,
				)
			}
		}
		g_capillary_rise_rates_initialized = true
	}
	return &g_capillary_rise_rates
}

// C++: (anonymous namespace) struct RPSCDRes
//
// `unset` defaults to `false` here vs the C++'s `bool unset{true}`; harmless,
// nothing ever reads this field (not fcSatPwpFromKA5textureClass, not its
// callers).
Rpscd_Res :: struct {
	sat: f64, // [m3 m-3]
	fc:  f64, // [m3 m-3]
	pwp: f64, // [m3 m-3]
}

@(private)
g_soil_characteristic_data: map[string]map[int]Rpscd_Res
@(private)
g_soil_characteristic_data_initialized: bool
@(private)
g_soil_characteristic_data_file_errors: tl.Errors

// C++: (anonymous namespace) EResult<RPSCDRes> readPrincipalSoilCharacteristicData(
//        const string& pathToSoilDir, const string& soilType, double rawDensity)
read_principal_soil_characteristic_data :: proc(
	path_to_soil_dir: string,
	soil_type: string,
	raw_density: f64,
	allocator := context.allocator,
) -> tl.EResult(Rpscd_Res) {
	if !g_soil_characteristic_data_initialized {
		// Process-lifetime, like the C++ function-local static this stands in for -
		// NOT `allocator`, which for a server is one request's arena. See
		// tools.process_cache_allocator.
		cache_allocator := tl.process_cache_allocator()
		path := strings.concatenate({path_to_soil_dir, "SoilCharacteristicData.json"}, cache_allocator)
		r := jx.read_and_parse_json_file(path, cache_allocator)
		if tl.success(r.errs) {
			if g_soil_characteristic_data == nil {
				g_soil_characteristic_data = make(map[string]map[int]Rpscd_Res, cache_allocator)
			}
			for item in jx.array_items(jx.get(r.result, "list")) {
				ac := jx.double_value(item, "airCapacity")
				fc := jx.double_value(item, "fieldCapacity")
				nfc := jx.double_value(item, "nFieldCapacity")
				res := Rpscd_Res {
					sat = ac + fc,
					fc  = fc,
					pwp = fc - nfc,
				}
				key := strings.to_upper(jx.string_value(item, "soilType"), cache_allocator)
				if key not_in g_soil_characteristic_data {
					g_soil_characteristic_data[key] = make(map[int]Rpscd_Res, cache_allocator)
				}
				inner := g_soil_characteristic_data[key]
				inner[int(jx.double_value(item, "soilRawDensity") / 100.0)] = res
				g_soil_characteristic_data[key] = inner
			}
		} else {
			tl.append_errors(&g_soil_characteristic_data_file_errors, r.errs)
		}
		g_soil_characteristic_data_initialized = true
	}

	res: tl.EResult(Rpscd_Res)
	res.allocator = allocator

	inner, ok := g_soil_characteristic_data[soil_type]
	if ok {
		rd10 := int(raw_density * 10)
		delta := rd10 < 15 ? 2 : -2

		v, vok := inner[rd10]
		for !vok && rd10 >= 11 && rd10 <= 19 {
			rd10 += delta
			v, vok = inner[rd10]
		}
		if vok {
			res.result = v
		} else {
			tl.append_errorf(
				&res,
				"Couldn't find soil characteristic data for soil type %s and raw density %g",
				soil_type,
				raw_density,
			)
		}
		return res
	}

	tl.append_errors(&res, g_soil_characteristic_data_file_errors)
	return res
}

@(private)
g_soil_characteristic_modifier: map[string]map[int]Rpscd_Res
@(private)
g_soil_characteristic_modifier_initialized: bool
@(private)
g_soil_characteristic_modifier_file_errors: tl.Errors

// C++: (anonymous namespace) EResult<RPSCDRes> readSoilCharacteristicModifier(
//        const string& pathToSoilDir, const string& soilType, double organicMatter)
read_soil_characteristic_modifier :: proc(
	path_to_soil_dir: string,
	soil_type: string,
	organic_matter: f64,
	allocator := context.allocator,
) -> tl.EResult(Rpscd_Res) {
	if !g_soil_characteristic_modifier_initialized {
		// Process-lifetime - see read_principal_soil_characteristic_data above.
		cache_allocator := tl.process_cache_allocator()
		path := strings.concatenate(
			{path_to_soil_dir, "SoilCharacteristicModifier.json"},
			cache_allocator,
		)
		r := jx.read_and_parse_json_file(path, cache_allocator)
		if tl.success(r.errs) {
			if g_soil_characteristic_modifier == nil {
				g_soil_characteristic_modifier = make(map[string]map[int]Rpscd_Res, cache_allocator)
			}
			for item in jx.array_items(jx.get(r.result, "list")) {
				ac := jx.double_value(item, "airCapacity")
				fc := jx.double_value(item, "fieldCapacity")
				nfc := jx.double_value(item, "nFieldCapacity")
				res := Rpscd_Res {
					sat = ac + fc,
					fc  = fc,
					pwp = fc - nfc,
				}
				key := strings.to_upper(jx.string_value(item, "soilType"), cache_allocator)
				if key not_in g_soil_characteristic_modifier {
					g_soil_characteristic_modifier[key] = make(map[int]Rpscd_Res, cache_allocator)
				}
				inner := g_soil_characteristic_modifier[key]
				inner[int(jx.double_value(item, "organicMatter") * 10)] = res
				g_soil_characteristic_modifier[key] = inner
			}
		} else {
			tl.append_errors(&g_soil_characteristic_modifier_file_errors, r.errs)
		}
		g_soil_characteristic_modifier_initialized = true
	}

	res: tl.EResult(Rpscd_Res)
	res.allocator = allocator

	inner, ok := g_soil_characteristic_modifier[strings.to_upper(soil_type, allocator)]
	if ok {
		v, vok := inner[int(organic_matter * 10)]
		if vok {
			res.result = v
		} else {
			tl.append_errorf(
				&res,
				"Couldn't find soil characteristic data for soil type %s and organic matter %g",
				soil_type,
				organic_matter,
			)
		}
		return res
	}

	tl.append_errors(&res, g_soil_characteristic_modifier_file_errors)
	return res
}
