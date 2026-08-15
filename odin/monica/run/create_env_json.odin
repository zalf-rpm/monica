// Port of createEnvJsonFromJsonObjects from
// src/run/create-env-from-json-config.cpp.
package run

import "core:strings"
import clim "../../support/climate"
import jx "../../support/jsonx"
import tl "../../support/tools"

// C++: Json monica::createEnvJsonFromJsonObjects(map<string, Json> params)
//
// `params` carries the three parsed documents under the keys "crop", "site"
// and "sim".
create_env_json_from_json_objects :: proc(
	crop_in, site_in, sim_in: jx.Value,
	allocator := context.allocator,
) -> jx.Value {
	// C++: for(auto& j : cropSiteSim) if(j.is_null()) return {};
	if jx.is_null(crop_in) || jx.is_null(site_in) || jx.is_null(sim_in) {
		return jx.Value{}
	}

	path_to_parameters := jx.string_value_of(jx.get(sim_in, "include-file-base-path"))

	// C++: auto addBasePath = [&](Json& j, const string& basePath)
	//
	// NOTE(c++-quirk): the lambda takes a `basePath` parameter but uses the
	// captured `pathToParameters` instead. Same value at every call site, so the
	// behaviour is unaffected; reproduced for comparability.
	add_base_path :: proc(j: jx.Value, path_to_parameters: string, allocator: Allocator) -> jx.Value {
		if jx.is_object(j) && jx.is_string(jx.get(j, "include-file-base-path")) {
			return j
		}
		m := make(jx.Object, 0, allocator)
		for k, v in jx.object_items(j) {
			m[strings.clone(k, allocator)] = v
		}
		m[strings.clone("include-file-base-path", allocator)] = jx.Value(
			jx.String(strings.clone(path_to_parameters, allocator)),
		)
		return jx.Value(m)
	}

	// collect all errors in all files and don't stop as early as possible
	errors: tl.Errors
	defer tl.errors_destroy(&errors)

	resolve :: proc(
		j_in: jx.Value,
		path_to_parameters: string,
		errors: ^tl.Errors,
		allocator: Allocator,
	) -> (
		jx.Value,
		bool,
	) {
		j := add_base_path(j_in, path_to_parameters, allocator)
		r := find_and_replace_references(j, j, allocator)
		if tl.success(r.errs) {
			return r.result, true
		}
		tl.append_errors(errors, r.errs)
		return jx.Value{}, false
	}

	cropj, ok1 := resolve(crop_in, path_to_parameters, &errors, allocator)
	sitej, ok2 := resolve(site_in, path_to_parameters, &errors, allocator)
	simj, ok3 := resolve(sim_in, path_to_parameters, &errors, allocator)

	if !ok1 || !ok2 || !ok3 {
		tl.print_possible_errors(errors)
		return jx.Value{}
	}

	set :: proc(o: ^jx.Object, key: string, v: jx.Value, allocator: Allocator) {
		o[strings.clone(key, allocator)] = v
	}
	str :: proc(s: string, allocator: Allocator) -> jx.Value {
		return jx.Value(jx.String(strings.clone(s, allocator)))
	}

	env := make(jx.Object, 0, allocator)
	set(&env, "type", str("Env", allocator), allocator)

	// store debug mode in env, take from sim.json, but prefer params map
	set(
		&env,
		"debugMode",
		jx.Value(jx.Boolean(jx.bool_value_of(jx.get(simj, "debug?")))),
		allocator,
	)

	cpp := make(jx.Object, 0, allocator)
	set(&cpp, "type", str("CentralParameterProvider", allocator), allocator)
	set(&cpp, "userCropParameters", jx.get(cropj, "CropParameters"), allocator)
	set(&cpp, "userEnvironmentParameters", jx.get(sitej, "EnvironmentParameters"), allocator)
	set(&cpp, "userSoilMoistureParameters", jx.get(sitej, "SoilMoistureParameters"), allocator)
	set(&cpp, "userSoilTemperatureParameters", jx.get(sitej, "SoilTemperatureParameters"), allocator)
	set(&cpp, "userSoilTransportParameters", jx.get(sitej, "SoilTransportParameters"), allocator)
	set(&cpp, "userSoilOrganicParameters", jx.get(sitej, "SoilOrganicParameters"), allocator)
	set(&cpp, "simulationParameters", simj, allocator)
	set(&cpp, "siteParameters", jx.get(sitej, "SiteParameters"), allocator)

	if !jx.is_null(jx.get(sitej, "groundwaterInformation")) {
		set(&cpp, "groundwaterInformation", jx.get(sitej, "groundwaterInformation"), allocator)
	}

	set(&env, "params", jx.Value(cpp), allocator)
	set(&env, "cropRotation", jx.get(cropj, "cropRotation"), allocator)
	if jx.is_array(jx.get(cropj, "cropRotation2")) {
		set(&env, "cropRotation2", jx.get(cropj, "cropRotation2"), allocator)
	}
	set(&env, "cropRotations", jx.get(cropj, "cropRotations"), allocator)
	if jx.is_array(jx.get(cropj, "cropRotations2")) {
		set(&env, "cropRotations2", jx.get(cropj, "cropRotations2"), allocator)
	}

	output := jx.get(simj, "output")
	set(&env, "events", jx.get(output, "events"), allocator)
	// NOTE(c++-quirk): the C++ tests `events` but assigns `events2`. Reproduced.
	if jx.is_array(jx.get(output, "events")) {
		set(&env, "events2", jx.get(output, "events2"), allocator)
	}

	inner := make(jx.Object, 0, allocator)
	set(
		&inner,
		"obj-outputs?",
		jx.Value(jx.Boolean(jx.bool_value_of(jx.get(output, "obj-outputs")))),
		allocator,
	)
	outputs := make(jx.Object, 0, allocator)
	set(&outputs, "output", jx.Value(inner), allocator)
	set(&env, "outputs", jx.Value(outputs), allocator)

	set(&env, "pathToClimateCSV", jx.get(simj, "climate.csv"), allocator)

	csvos := make(jx.Object, 0, allocator)
	for k, v in jx.object_items(jx.get(simj, "climate.csv-options")) {
		csvos[strings.clone(k, allocator)] = v
	}
	set(
		&csvos,
		"latitude",
		jx.Value(jx.Float(jx.double_value_key_d(jx.get(sitej, "SiteParameters"), "Latitude", 0.0))),
		allocator,
	)
	set(&env, "csvViaHeaderOptions", jx.Value(csvos), allocator)

	// C++: env["climateData"] = printPossibleErrors(readClimateDataFromCSVFile[s]
	//        ViaHeaders(simj["climate.csv"], env["csvViaHeaderOptions"]));
	//
	// `CSVViaHeaderOptions(json11::Json)` is an implicit constructor in the C++
	// (climate-file-io.h) that just calls merge(); built explicitly here.
	csv_opts := clim.make_csv_via_header_options()
	_ = clim.csv_via_header_options_merge(&csv_opts, jx.Value(csvos), allocator)

	climate_csv := jx.get(simj, "climate.csv")
	if jx.is_string(climate_csv) && len(jx.string_value_of(climate_csv)) > 0 {
		path := jx.string_value_of(climate_csv)
		if !strings.contains(path, "capnp://") {
			eda := clim.read_climate_data_from_csv_file_via_headers(
				path,
				csv_opts,
				true,
				allocator,
			)
			da := tl.print_possible_errors_r(eda, false)
			set(&env, "climateData", clim.data_accessor_to_json(&da, allocator), allocator)
		}
	} else if jx.is_array(climate_csv) && len(jx.array_items(climate_csv)) > 0 {
		paths := make([dynamic]string, 0, len(jx.array_items(climate_csv)), allocator)
		for v in jx.array_items(climate_csv) {
			append(&paths, jx.string_value_of(v))
		}
		eda := clim.read_climate_data_from_csv_files_via_headers(paths[:], csv_opts, allocator)
		da := tl.print_possible_errors_r(eda, false)
		set(&env, "climateData", clim.data_accessor_to_json(&da, allocator), allocator)
	}

	return jx.Value(env)
}
