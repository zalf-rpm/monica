// Port of the SoilParameters half of src/soil/soil.{h,cpp}: the struct itself,
// merge/to_json, the resolved-value getters, and createSoilPMs /
// createEqualSizedSoilPMs.
//
// C++'s SoilParameters.calculateAndSetPwpFcSat is a std::function<Errors(SoilParameters*)>,
// built by wrapping createEqualSizedSoilPMs's 2-arg
// function<Errors(SoilParameters*,int)> parameter in a closure that captures the
// per-layer repeat index `i`. Per plan-odin.md prep 2 (std::function removal),
// this is not ported as a stored closure field: Soil_Parameters has no
// calculateAndSetPwpFcSat field at all. Instead soil_parameters_merge takes the
// method selector, the soil-table directory and the layer number as explicit
// parameters and dispatches with apply_pwp_fc_sat_method (enum + switch, see
// soil_pwp_fc_sat.odin) - observably identical, no closures needed.
package soil

import "base:runtime"
import "core:strings"
import jx "../../support/jsonx"
import tl "../../support/tools"

Allocator :: runtime.Allocator

// C++: enum-ised selector for SiteParameters::calculateAndSetPwpFcSatFunctions,
// which in the C++ maps pwpFcSatFunction strings to functions registered once in
// main() (monica-run-main.cpp:240-249): "Wessolek2009" ->
// getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir),
// "VanGenuchten"/"VanGenuchtenVereecken" -> updateUnsetPwpFcSatFromVanGenuchtenVereecken,
// "VanGenuchtenToth" -> updateUnsetPwpFcSatFromVanGenuchtenToth, "Toth" ->
// updateUnsetPwpFcSatFromToth. An unregistered name (or NONE) falls back to
// noSetPwpFcSat, matching the C++'s `std::function selectedSetPwpFcSatFunction =
// noSetPwpFcSat;` default.
Pwp_Fc_Sat_Method :: enum {
	NONE,
	WESSOLEK2009,
	VAN_GENUCHTEN_VEREECKEN,
	VAN_GENUCHTEN_TOTH,
	TOTH,
}

// C++: the pwpFcSatFunction string -> map lookup in siteparameters::merge
// (monica-parameters.cpp:1117-1123).
pwp_fc_sat_method_from_name :: proc(name: string) -> (Pwp_Fc_Sat_Method, bool) {
	switch name {
	case "Wessolek2009":
		return .WESSOLEK2009, true
	case "VanGenuchten", "VanGenuchtenVereecken":
		return .VAN_GENUCHTEN_VEREECKEN, true
	case "VanGenuchtenToth":
		return .VAN_GENUCHTEN_TOTH, true
	case "Toth":
		return .TOTH, true
	}
	return .NONE, false
}

// C++: struct Soil::SoilParameters (data members only - see the package comment
// for calculateAndSetPwpFcSat)
Soil_Parameters :: struct {
	vs_SoilSandContent:       f64, // [kg kg-1]
	vs_SoilClayContent:       f64, // [kg kg-1] (Ton)
	vs_SoilpH:                f64,
	vs_SoilStoneContent:      f64, // [m3 m-3]
	vs_Lambda:                f64,
	vs_FieldCapacity:         f64, // [m3 m-3]
	vs_Saturation:            f64, // [m3 m-3]
	vs_PermanentWiltingPoint: f64, // [m3 m-3]
	vs_SoilTexture:           string,
	vs_SoilAmmonium:          f64, // [kg NH4-N m-3]
	vs_SoilNitrate:           f64, // [kg NO3-N m-3]
	vs_Soil_CN_Ratio:         f64,
	vs_SoilMoisturePercentFC: f64,

	thickness: f64, // layer thickness in m

	// Raw/override values; -1 means "unset" and the resolved value has to be
	// computed via the corresponding soil_xyz() proc below (e.g. from the other
	// value + clay content, or from organic carbon<->matter conversion). Read/
	// write the override directly if that's really what's needed, otherwise use
	// the resolved getter.
	_vs_SoilRawDensity:    f64, // [kg m-3]
	_vs_SoilBulkDensity:   f64, // [kg m-3]
	_vs_SoilOrganicCarbon: f64, // [kg kg-1]
	_vs_SoilOrganicMatter: f64, // [kg kg-1]
}

// C++ in-class initialisers
make_soil_parameters :: proc() -> Soil_Parameters {
	return Soil_Parameters {
		vs_SoilSandContent = -1.0,
		vs_SoilClayContent = -1.0,
		vs_SoilpH = 6.9,
		vs_Lambda = -1.0,
		vs_FieldCapacity = -1.0,
		vs_Saturation = -1.0,
		vs_PermanentWiltingPoint = -1.0,
		vs_SoilAmmonium = 0.0005,
		vs_SoilNitrate = 0.005,
		vs_Soil_CN_Ratio = 10.0,
		vs_SoilMoisturePercentFC = 100.0,
		_vs_SoilRawDensity = -1.0,
		_vs_SoilBulkDensity = -1.0,
		_vs_SoilOrganicCarbon = -1.0,
		_vs_SoilOrganicMatter = -1.0,
	}
}

// C++: Errors Soil::noSetPwpFcSat(SoilParameters*, int)
no_set_pwp_fc_sat :: proc(sp: ^Soil_Parameters) -> tl.Errors {
	errors: tl.Errors
	if sp.vs_FieldCapacity < 0 {
		tl.append_error(&errors, "Field capacity not set!")
	}
	if sp.vs_Saturation < 0 {
		tl.append_error(&errors, "Saturation not set!")
	}
	if sp.vs_PermanentWiltingPoint < 0 {
		tl.append_error(&errors, "Permanent wilting point not set!")
	}
	return errors
}

// C++: double soilparameters::soilSiltContent(const SoilParameters*) - (Schluff)
soil_silt_content :: proc(sp: ^Soil_Parameters) -> f64 {
	return 1.0 - sp.vs_SoilSandContent - sp.vs_SoilClayContent
}

// C++: double soilparameters::soilRawDensity(const SoilParameters*)
soil_raw_density :: proc(sp: ^Soil_Parameters) -> f64 {
	if sp._vs_SoilRawDensity < 0 {
		return ((sp._vs_SoilBulkDensity / 1000.0) - (0.009 * 100.0 * sp.vs_SoilClayContent)) * 1000.0
	}
	return sp._vs_SoilRawDensity
}

// C++: double soilparameters::soilBulkDensity(const SoilParameters*)
soil_bulk_density :: proc(sp: ^Soil_Parameters) -> f64 {
	if sp._vs_SoilBulkDensity < 0 {
		return ((sp._vs_SoilRawDensity / 1000.0) + (0.009 * 100.0 * sp.vs_SoilClayContent)) * 1000.0
	}
	return sp._vs_SoilBulkDensity
}

// C++: double soilparameters::soilOrganicCarbon(const SoilParameters*)
soil_organic_carbon :: proc(sp: ^Soil_Parameters) -> f64 {
	if sp._vs_SoilOrganicCarbon < 0 {
		return sp._vs_SoilOrganicMatter * PO_SOM_TO_C
	}
	return sp._vs_SoilOrganicCarbon
}

// C++: double soilparameters::soilOrganicMatter(const SoilParameters*)
soil_organic_matter :: proc(sp: ^Soil_Parameters) -> f64 {
	if sp._vs_SoilOrganicMatter < 0 {
		return sp._vs_SoilOrganicCarbon / PO_SOM_TO_C
	}
	return sp._vs_SoilOrganicMatter
}

// C++: bool soilparameters::isValid(const SoilParameters*)
//
// The C++ also logs each failure via debug() (a build-time-gated debug
// stream); not reproduced, it's not part of any oracle's output.
soil_parameters_is_valid :: proc(sp: ^Soil_Parameters) -> bool {
	is_valid := true
	if sp.vs_FieldCapacity < 0 {
		is_valid = false
	}
	if sp.vs_Saturation < 0 {
		is_valid = false
	}
	if sp.vs_PermanentWiltingPoint < 0 {
		is_valid = false
	}
	if sp.vs_SoilSandContent < 0 {
		is_valid = false
	}
	if sp.vs_SoilClayContent < 0 {
		is_valid = false
	}
	if sp.vs_SoilpH < 0 {
		is_valid = false
	}
	if sp.vs_SoilStoneContent < 0 {
		is_valid = false
	}
	if sp.vs_Saturation < 0 {
		is_valid = false
	}
	if sp.vs_PermanentWiltingPoint < 0 {
		is_valid = false
	}
	return is_valid
}

// C++: Errors soilparameters::merge(SoilParameters*, json11::Json)
//
// method/path_to_soil_dir/layer_no replace the C++'s stored
// calculateAndSetPwpFcSat closure - see the package comment.
soil_parameters_merge :: proc(
	sp: ^Soil_Parameters,
	j: jx.Value,
	method: Pwp_Fc_Sat_Method,
	path_to_soil_dir: string,
	layer_no: int,
	allocator := context.allocator,
) -> tl.Errors {
	es: tl.Errors

	jx.set_double_value(&sp.vs_SoilSandContent, j, "Sand", jx.transform_if_percent(j, "Sand"))
	jx.set_double_value(&sp.vs_SoilClayContent, j, "Clay", jx.transform_if_percent(j, "Clay"))
	jx.set_double_value(&sp.vs_SoilpH, j, "pH")
	jx.set_double_value(
		&sp.vs_SoilStoneContent,
		j,
		"Sceleton",
		jx.transform_if_percent(j, "Sceleton"),
	)
	jx.set_double_value(&sp.vs_Lambda, j, "Lambda")
	jx.set_double_value(
		&sp.vs_FieldCapacity,
		j,
		"FieldCapacity",
		jx.transform_if_percent(j, "FieldCapacity"),
	)
	jx.set_double_value(
		&sp.vs_Saturation,
		j,
		"PoreVolume",
		jx.transform_if_percent(j, "PoreVolume"),
	)
	jx.set_double_value(
		&sp.vs_PermanentWiltingPoint,
		j,
		"PermanentWiltingPoint",
		jx.transform_if_percent(j, "PermanentWiltingPoint"),
	)
	jx.set_string_value(&sp.vs_SoilTexture, j, "KA5TextureClass")
	jx.set_double_value(&sp.vs_SoilAmmonium, j, "SoilAmmonium")
	jx.set_double_value(&sp.vs_SoilNitrate, j, "SoilNitrate")
	jx.set_double_value(&sp.vs_Soil_CN_Ratio, j, "CN")
	jx.set_double_value(&sp.vs_SoilMoisturePercentFC, j, "SoilMoisturePercentFC")
	jx.set_double_value(&sp._vs_SoilRawDensity, j, "SoilRawDensity")
	jx.set_double_value(&sp._vs_SoilBulkDensity, j, "SoilBulkDensity")
	// unconditional /100, unlike the transformIfPercent-guarded fields above
	jx.set_double_value(&sp._vs_SoilOrganicCarbon, j, "SoilOrganicCarbon", .PERCENT)
	jx.set_double_value(
		&sp._vs_SoilOrganicMatter,
		j,
		"SoilOrganicMatter",
		jx.transform_if_percent(j, "SoilOrganicMatter"),
	)

	st := sp.vs_SoilTexture
	// use internally just uppercase chars
	sp.vs_SoilTexture = strings.to_upper(sp.vs_SoilTexture, allocator)

	if sp.vs_SoilSandContent < 0 && len(sp.vs_SoilTexture) > 0 {
		res := ka5_texture_2_sand(sp.vs_SoilTexture, allocator)
		if tl.success(res.errs) {
			sp.vs_SoilSandContent = res.result
		} else {
			tl.append_errors(&es, res.errs)
		}
	}

	if sp.vs_SoilClayContent < 0 && len(sp.vs_SoilTexture) > 0 {
		res := ka5_texture_2_clay(sp.vs_SoilTexture, allocator)
		if tl.success(res.errs) {
			sp.vs_SoilClayContent = res.result
		} else {
			tl.append_errors(&es, res.errs)
		}
	}

	if sp.vs_SoilClayContent >= 0 && sp.vs_SoilSandContent >= 0 && len(sp.vs_SoilTexture) == 0 {
		sp.vs_SoilTexture = sand_and_clay_2_ka5_texture(sp.vs_SoilSandContent, sp.vs_SoilClayContent)
	}

	// restrict sceleton to 80%, else FC, PWP and SAT could be calculated too low,
	// so that the water transport algorithm gets unstable
	if sp.vs_SoilStoneContent > 0 {
		sp.vs_SoilStoneContent = min(sp.vs_SoilStoneContent, 0.8)
	}

	tl.append_errors(&es, apply_pwp_fc_sat_method(method, sp, layer_no, path_to_soil_dir, allocator))

	// restrict FC, PWP and SAT else the water transport algorithm gets instable
	if sp.vs_FieldCapacity < 0.05 {
		tl.append_warningf(
			&es,
			"Field capacity is too low (%g%%). Is being set to 5%%.",
			sp.vs_FieldCapacity * 100,
		)
		sp.vs_FieldCapacity = 0.05
	}
	if sp.vs_PermanentWiltingPoint < 0.01 {
		tl.append_warningf(
			&es,
			"Permanent wilting point is too low (%g%%). Is being set to 1%%.",
			sp.vs_PermanentWiltingPoint * 100,
		)
		sp.vs_PermanentWiltingPoint = 0.01
	}
	if sp.vs_Saturation < 0.1 {
		tl.append_warningf(
			&es,
			"Saturation is too low (%g%%). Is being set to 10%%.",
			sp.vs_Saturation * 100,
		)
		sp.vs_Saturation = 0.1
	}

	if sp.vs_Lambda < 0 && sp.vs_SoilSandContent > 0 && sp.vs_SoilClayContent > 0 {
		sp.vs_Lambda = sand_and_clay_2_lambda(sp.vs_SoilSandContent, sp.vs_SoilClayContent)
	}

	if len(sp.vs_SoilTexture) > 0 {
		r := ka5_texture_2_sand(sp.vs_SoilTexture, allocator)
		if tl.failure(r.errs) {
			tl.append_errorf(&es, "KA5TextureClass (%s) is unknown.", st)
		}
	}
	if sp.vs_SoilClayContent < 0 || sp.vs_SoilClayContent > 1.0 {
		tl.append_errorf(&es, "Clay content (%g) is out of bounds [0, 1].", sp.vs_SoilClayContent)
	}
	if sp.vs_SoilpH < 0 || sp.vs_SoilpH > 14 {
		tl.append_errorf(&es, "pH value (%g) is out of bounds [0, 14].", sp.vs_SoilpH)
	}
	if sp.vs_SoilStoneContent < 0 || sp.vs_SoilStoneContent > 1.0 {
		tl.append_errorf(
			&es,
			"Sceleton (%g) is out of bounds [0, 1].",
			sp.vs_SoilStoneContent,
		)
	}
	if sp.vs_FieldCapacity < 0 || sp.vs_FieldCapacity > 1.0 {
		tl.append_errorf(&es, "FieldCapacity (%g) is out of bounds [0, 1].", sp.vs_FieldCapacity)
	}
	if sp.vs_Saturation < 0 || sp.vs_Saturation > 1.0 {
		tl.append_errorf(&es, "PoreVolume (%g) is out of bounds [0, 1].", sp.vs_Saturation)
	}
	if sp.vs_PermanentWiltingPoint < 0 || sp.vs_PermanentWiltingPoint > 1.0 {
		tl.append_errorf(
			&es,
			"PermanentWiltingPoint (%g) is out of bounds [0, 1].",
			sp.vs_PermanentWiltingPoint,
		)
	}
	if sp.vs_SoilMoisturePercentFC < 0 || sp.vs_SoilMoisturePercentFC > 100 {
		tl.append_errorf(
			&es,
			"SoilMoisturePercentFC (%g) is out of bounds [0, 100].",
			sp.vs_SoilMoisturePercentFC,
		)
	}
	if sp._vs_SoilBulkDensity < 0 && (sp._vs_SoilRawDensity < 0 || sp._vs_SoilRawDensity > 2000) {
		tl.append_warningf(
			&es,
			"SoilRawDensity (%g) is out of bounds [0, 2000].",
			sp._vs_SoilRawDensity,
		)
	}
	if sp._vs_SoilRawDensity < 0 && (sp._vs_SoilBulkDensity < 0 || sp._vs_SoilBulkDensity > 2000) {
		tl.append_warningf(
			&es,
			"SoilBulkDensity (%g) is out of bounds [0, 2000].",
			sp._vs_SoilBulkDensity,
		)
	}
	if sp._vs_SoilOrganicMatter < 0 &&
	   (sp._vs_SoilOrganicCarbon < 0 || sp._vs_SoilOrganicCarbon > 1.0) {
		tl.append_errorf(
			&es,
			"SoilOrganicCarbon content (%g) is out of bounds [0, 1].",
			sp._vs_SoilOrganicCarbon,
		)
	}
	if sp._vs_SoilOrganicCarbon < 0 &&
	   (sp._vs_SoilOrganicMatter < 0 || sp._vs_SoilOrganicMatter > 1.0) {
		tl.append_errorf(
			&es,
			"SoilOrganicMatter content (%g) is out of bounds [0, 1].",
			sp._vs_SoilOrganicMatter,
		)
	}

	return es
}

// C++: json11::Json soilparameters::to_json(const SoilParameters*)
soil_parameters_to_json :: proc(sp: ^Soil_Parameters, a: Allocator) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SoilParameters")},
		{"Sand", jx.vu(sp.vs_SoilSandContent, "% [0-1]", a)},
		{"Clay", jx.vu(sp.vs_SoilClayContent, "% [0-1]", a)},
		{"pH", jx.f(sp.vs_SoilpH)},
		{"Sceleton", jx.vu(sp.vs_SoilStoneContent, "vol% [0-1] (m3 m-3)", a)},
		{"Lambda", jx.f(sp.vs_Lambda)},
		{"FieldCapacity", jx.vu(sp.vs_FieldCapacity, "vol% [0-1] (m3 m-3)", a)},
		{"PoreVolume", jx.vu(sp.vs_Saturation, "vol% [0-1] (m3 m-3)", a)},
		{"PermanentWiltingPoint", jx.vu(sp.vs_PermanentWiltingPoint, "vol% [0-1] (m3 m-3)", a)},
		{"KA5TextureClass", jx.s(sp.vs_SoilTexture, a)},
		{"SoilAmmonium", jx.vu(sp.vs_SoilAmmonium, "kg NH4-N m-3", a)},
		{"SoilNitrate", jx.vu(sp.vs_SoilNitrate, "kg NO3-N m-3", a)},
		{"CN", jx.f(sp.vs_Soil_CN_Ratio)},
		{"SoilRawDensity", jx.vu(sp._vs_SoilRawDensity, "kg m-3", a)},
		{"SoilBulkDensity", jx.vu(sp._vs_SoilBulkDensity, "kg m-3", a)},
		{"SoilOrganicCarbon", jx.vu(sp._vs_SoilOrganicCarbon * 100.0, "mass% [0-100]", a)},
		{"SoilOrganicMatter", jx.vu(sp._vs_SoilOrganicMatter, "mass% [0-1]", a)},
		{"SoilMoisturePercentFC", jx.vu(sp.vs_SoilMoisturePercentFC, "% [0-100]", a)},
	)
}

// C++: EResult<SoilPMs> Soil::createEqualSizedSoilPMs(setPwpFcSat, jsonSoilPMs,
//        layerThickness, numberOfLayers)
create_equal_sized_soil_pms :: proc(
	method: Pwp_Fc_Sat_Method,
	path_to_soil_dir: string,
	json_soil_pms: []jx.Value,
	layer_thickness: f64 = 0.1,
	number_of_layers: int = 20,
	allocator := context.allocator,
) -> tl.EResult([dynamic]Soil_Parameters) {
	errors: tl.Errors
	soil_pms := make([dynamic]Soil_Parameters, 0, allocator)
	layer_count := 0

	for spi := 0; spi < len(json_soil_pms); spi += 1 {
		sp_json := json_soil_pms[spi]

		// repeat layers if there is an associated Thickness parameter
		repeat_layer := 1
		if !jx.is_null(jx.get(sp_json, "Thickness")) {
			transf := jx.transform_if_not_meters(sp_json, "Thickness")
			lt := jx.apply_transform(
				transf,
				jx.double_value_key_d(sp_json, "Thickness", layer_thickness),
			)
			no_of_monica_layers := int(tl.round(lt / layer_thickness))
			repeat_layer = min(max(1, no_of_monica_layers), number_of_layers - layer_count)
		}

		// simply repeat the last layer as often as necessary to fill the layers
		if spi + 1 == len(json_soil_pms) {
			repeat_layer = number_of_layers - layer_count
		}

		for i := 1; i <= repeat_layer; i += 1 {
			sps := make_soil_parameters()
			es := soil_parameters_merge(
				&sps,
				sp_json,
				method,
				path_to_soil_dir,
				layer_count + i,
				allocator,
			)
			append(&soil_pms, sps)
			if tl.failure(es) {
				tl.append_errorf(&errors, "Config-layer:%d Monica-layer:%d:", spi + 1, layer_count + i)
				tl.append_errors(&errors, es)
			}
		}

		layer_count += repeat_layer
	}

	res: tl.EResult([dynamic]Soil_Parameters)
	res.allocator = allocator
	res.result = soil_pms
	tl.append_errors(&res, errors)
	return res
}

// C++: EResult<SoilPMs> Soil::createSoilPMs(setPwpFcSat, jsonSoilPMs)
create_soil_pms :: proc(
	method: Pwp_Fc_Sat_Method,
	path_to_soil_dir: string,
	json_soil_pms: []jx.Value,
	allocator := context.allocator,
) -> tl.EResult([dynamic]Soil_Parameters) {
	errors: tl.Errors
	soil_pms := make([dynamic]Soil_Parameters, 0, allocator)

	for sp_json in json_soil_pms {
		sps := make_soil_parameters()
		es := soil_parameters_merge(&sps, sp_json, method, path_to_soil_dir, -1, allocator)
		transf := jx.transform_if_not_meters(sp_json, "Thickness")
		lt := jx.apply_transform(transf, jx.double_value_key_d(sp_json, "Thickness", 0.1))
		sps.thickness = lt
		append(&soil_pms, sps)
		if tl.failure(es) {
			tl.append_errors(&errors, es)
		}
	}

	res: tl.EResult([dynamic]Soil_Parameters)
	res.allocator = allocator
	res.result = soil_pms
	tl.append_errors(&res, errors)
	return res
}
