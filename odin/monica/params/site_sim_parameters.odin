// Tranche 2 of the src/core/monica-parameters.{h,cpp} port: the structs
// CentralParameterProvider holds, plus the small ones they nest.
//
// MineralFertilizerParameters, NMinApplicationParameters, IrrigationParameters,
// AutomaticIrrigationParameters, MeasuredGroundwaterTableInformation,
// SiteParameters, SimulationParameters, CropModuleParameters,
// EnvironmentParameters, CentralParameterProvider.
package params

import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "../soil"
import "core:slice"
import "core:strconv"
import "core:strings"

// ---------------------------------------------------------------------------
// MineralFertilizerParameters
// ---------------------------------------------------------------------------

// C++: struct monica::MineralFertilizerParameters
Mineral_Fertilizer_Parameters :: struct {
	id:       string,
	name:     string,
	carbamid: f64, // [%]
	nh4:      f64, // [%]
	no3:      f64, // [%]
}

// C++: Errors mineralfertilizerparameters::merge(...)
mineral_fertilizer_parameters_merge :: proc(
	fp: ^Mineral_Fertilizer_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(fp, j, mineral_fertilizer_parameters_merge)

	jx.set_string_value(&fp.id, j, "id")
	jx.set_string_value(&fp.name, j, "name")
	jx.set_double_value(&fp.carbamid, j, "Carbamid")
	jx.set_double_value(&fp.nh4, j, "NH4")
	jx.set_double_value(&fp.no3, j, "NO3")

	return res
}

// C++: json11::Json mineralfertilizerparameters::to_json(...)
mineral_fertilizer_parameters_to_json :: proc(
	fp: ^Mineral_Fertilizer_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("MineralFertilizerParameters")},
		{"id", jx.s(fp.id, a)},
		{"name", jx.s(fp.name, a)},
		{"Carbamid", jx.f(fp.carbamid)},
		{"NH4", jx.f(fp.nh4)},
		{"NO3", jx.f(fp.no3)},
	)
}

// ---------------------------------------------------------------------------
// NMinApplicationParameters
// ---------------------------------------------------------------------------

// C++: struct monica::NMinApplicationParameters
NMin_Application_Parameters :: struct {
	min:           f64,
	max:           f64,
	delay_in_days: int,
}

// C++: Errors nminapplicationparameters::merge(...)
nmin_application_parameters_merge :: proc(
	nap: ^NMin_Application_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(nap, j, nmin_application_parameters_merge)

	jx.set_double_value(&nap.min, j, "min")
	jx.set_double_value(&nap.max, j, "max")
	jx.set_int_value(&nap.delay_in_days, j, "delayInDays")

	return res
}

// C++: json11::Json nminapplicationparameters::to_json(...)
nmin_application_parameters_to_json :: proc(
	nap: ^NMin_Application_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("NMinApplicationParameters")},
		{"min", jx.f(nap.min)},
		{"max", jx.f(nap.max)},
		{"delayInDays", jx.i(nap.delay_in_days)},
	)
}

// ---------------------------------------------------------------------------
// IrrigationParameters
// ---------------------------------------------------------------------------

// C++: struct monica::IrrigationParameters
Irrigation_Parameters :: struct {
	nitrate_concentration: f64, // [mg dm-3]
	sulfate_concentration: f64, // [mg dm-3]
	is_drip_irrigation:    bool,
	fw:                    f64, // fraction of wetted soil surface [0-1]
}

// C++ default: fw{1.0}
make_irrigation_parameters :: proc() -> Irrigation_Parameters {
	return Irrigation_Parameters{fw = 1.0}
}

// C++: Errors irrigationparameters::merge(...)
irrigation_parameters_merge :: proc(ip: ^Irrigation_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(ip, j, irrigation_parameters_merge)

	jx.set_double_value(&ip.nitrate_concentration, j, "nitrateConcentration")
	jx.set_double_value(&ip.sulfate_concentration, j, "sulfateConcentration")
	jx.set_bool_value(&ip.is_drip_irrigation, j, "isDripIrrigation")
	// note: fw is clamped and only assigned when the key is a bare number
	if jx.is_number(jx.get(j, "fw")) {
		ip.fw = max(0.0, min(1.0, jx.number_value(jx.get(j, "fw"))))
	}

	return res
}

// C++: json11::Json irrigationparameters::to_json(...)
irrigation_parameters_to_json :: proc(ip: ^Irrigation_Parameters, a: Allocator) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("IrrigationParameters")},
		{"nitrateConcentration", jx.vu(ip.nitrate_concentration, "mg dm-3", a)},
		{"sulfateConcentration", jx.vu(ip.sulfate_concentration, "mg dm-3", a)},
		{"isDripIrrigation", jx.b(ip.is_drip_irrigation)},
		{"fw", jx.f(ip.fw)},
	)
}

// ---------------------------------------------------------------------------
// AutomaticIrrigationParameters
// ---------------------------------------------------------------------------

// C++: struct monica::AutomaticIrrigationParameters : public IrrigationParameters
//
// `using base` reproduces public data inheritance: aip.fw etc. resolve directly.
Automatic_Irrigation_Parameters :: struct {
	using base:                         Irrigation_Parameters,
	start_date:                         d.Date,
	end_date:                           d.Date,
	amount:                             f64,
	percent_nfc:                        f64,
	threshold:                          f64,
	critical_moisture_depth_m:          f64,
	min_days_between_irrigation_events: int,
}

// C++ defaults: amount{-1}, percent_nfc{-1}, threshold{-1}, critical_moisture_depth_m{0.3}
make_automatic_irrigation_parameters :: proc() -> Automatic_Irrigation_Parameters {
	return Automatic_Irrigation_Parameters {
		base = make_irrigation_parameters(),
		amount = -1.0,
		percent_nfc = -1.0,
		threshold = -1.0,
		critical_moisture_depth_m = 0.3,
	}
}

// C++: Errors automaticirrigationparameters::merge(...)
//
// NOTE(c++-quirk): end_date is read from the key "stopDate", not "endDate"; and
// `threshold` is written twice - first from "threshold" (percent-transformed),
// then unconditionally from "trigger_if_nFC_below_%" divided by 100, so the
// latter wins whenever it is present. Both reproduced.
automatic_irrigation_parameters_merge :: proc(
	aip: ^Automatic_Irrigation_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(aip, j, automatic_irrigation_parameters_merge)

	e := irrigation_parameters_merge(&aip.base, jx.get(j, "irrigationParameters"))
	tl.append_errors(&res, e)

	jx.set_iso_date_value(&aip.start_date, j, "startDate")
	jx.set_iso_date_value(&aip.end_date, j, "stopDate")
	jx.set_double_value(&aip.amount, j, "amount")
	jx.set_double_value(&aip.percent_nfc, j, "set_to_%nFC")
	jx.set_double_value(&aip.threshold, j, "threshold", jx.transform_if_percent(j, "threshold"))
	jx.set_double_value(&aip.threshold, j, "trigger_if_nFC_below_%", .PERCENT)
	jx.set_double_value(
		&aip.critical_moisture_depth_m,
		j,
		"calc_nFC_until_depth_m",
		jx.transform_if_not_meters(j, "calc_nFC_until_depth_m"),
	)
	jx.set_int_value(&aip.min_days_between_irrigation_events, j, "minDaysBetweenIrrigationEvents")

	return res
}

// C++: json11::Json automaticirrigationparameters::to_json(...)
//
// NOTE(c++-quirk): end_date is not emitted at all, so a to_json/merge round trip
// loses it. Reproduced.
automatic_irrigation_parameters_to_json :: proc(
	aip: ^Automatic_Irrigation_Parameters,
	a: Allocator,
) -> jx.Value {
	iso := d.to_iso_date_string(aip.start_date, "", a)
	o := jx.obj(
		a,
		{"type", jx.sl("AutomaticIrrigationParameters")},
		{"startDate", jx.s(iso, a)},
		{"irrigationParameters", irrigation_parameters_to_json(&aip.base, a)},
		{"trigger_if_nFC_below_%", jx.vu(aip.threshold * 100.0, "%", a)},
		{"calc_nFC_until_depth_m", jx.vu(aip.critical_moisture_depth_m, "m", a)},
		{"minDaysBetweenIrrigationEvents", jx.vu_int(aip.min_days_between_irrigation_events, "d", a)},
	)
	oo := o.(jx.Object)
	if aip.amount > 0 {
		jx.obj_set(&oo, "amount", jx.vu(aip.amount, "mm", a), a)
	} else {
		jx.obj_set(&oo, "set_to_%nFC", jx.vu(aip.percent_nfc, "%", a), a)
	}
	return jx.Value(oo)
}

// ---------------------------------------------------------------------------
// MeasuredGroundwaterTableInformation
// ---------------------------------------------------------------------------

// C++: struct monica::MeasuredGroundwaterTableInformation
//
// The C++ keys the map by Tools::Date (a std::map, so ordered). Here the ISO
// string is the key and to_json sorts, which reproduces the ordering json11's
// std::map<string,Json> would have produced on output.
Measured_Groundwater_Table_Information :: struct {
	groundwater_information_available: bool,
	groundwater_info:                 map[string]f64, // ISO date -> depth
}

// C++: Errors measuredgroundwatertableinformation::merge(...)
//
// Note: no defaultMerge call here, unlike every other merge in this file.
measured_groundwater_table_information_merge :: proc(
	gwi: ^Measured_Groundwater_Table_Information,
	j: jx.Value,
	a: Allocator,
) -> tl.Errors {
	res: tl.Errors

	jx.set_bool_value(&gwi.groundwater_information_available, j, "groundwaterInformationAvailable")

	if jx.has_object_shape(j, "groundwaterInfo") {
		if gwi.groundwater_info == nil {
			gwi.groundwater_info = make(map[string]f64, a)
		}
		for k, v in jx.object_items(jx.get(j, "groundwaterInfo")) {
			// the C++ round-trips through Date::fromIsoDateString; normalise the
			// same way so a non-canonical key produces the same output key
			dd := d.from_iso_date_string(k)
			key := d.to_iso_date_string(dd, "", a)
			gwi.groundwater_info[key] = jx.number_value(v)
		}
	} else {
		dump := jx.dump(j, a)
		tl.append_errorf(&res, "Couldn't read 'groundwaterInfo' key from JSON object:\n%s", dump)
	}

	return res
}

// C++: json11::Json measuredgroundwatertableinformation::to_json(...)
measured_groundwater_table_information_to_json :: proc(
	gwi: ^Measured_Groundwater_Table_Information,
	a: Allocator,
) -> jx.Value {
	gi := make(jx.Object, 0, a)
	for k, v in gwi.groundwater_info {
		gi[strings.clone(k, a)] = jx.f(v)
	}
	return jx.obj(
		a,
		{"type", jx.sl("MeasuredGroundwaterTableInformation")},
		{"groundwaterInformationAvailable", jx.b(gwi.groundwater_information_available)},
		{"groundwaterInfo", jx.Value(gi)},
	)
}

// C++: std::pair<bool,double> measuredgroundwatertableinformation::
//        getGroundwaterInformation(const MeasuredGroundwaterTableInformation*, Tools::Date)
get_groundwater_information :: proc(
	gwi: ^Measured_Groundwater_Table_Information,
	gwDate: d.Date,
	allocator := context.allocator,
) -> (
	available: bool,
	depth: f64,
) {
	if gwi.groundwater_information_available && len(gwi.groundwater_info) > 0 {
		key := d.to_iso_date_string(gwDate, "", allocator)
		if v, ok := gwi.groundwater_info[key]; ok {
			return true, v
		}
	}
	return false, 0
}

// ---------------------------------------------------------------------------
// SiteParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SiteParameters
//
// The C++ `calculateAndSetPwpFcSatFunctions` map<string, std::function> is not a
// data member here (plan-odin.md prep 2); pwp_fc_sat_function selects the method by
// name at the point of use, via soil.pwp_fc_sat_method_from_name.
Site_Parameters :: struct {
	latitude:                                f64, // ZALF latitude
	slope:                                   f64, // [m m-1]
	height_nn:                               f64, // [m]
	groundwater_depth:                       f64, // [m]
	soil_cn_ratio:                           f64,
	drainage_coeff:                          f64,
	n_deposition:                            f64, // [kg N ha-1 y-1]
	max_effective_rooting_depth:             f64, // [m]
	impenetrable_layer_depth:                f64, // [m]
	soil_specific_humus_balance_correction:  f64, // humus equivalents
	bare_soil_kc_factor:                     f64,
	number_of_layers:                        int,
	layer_thickness:                         f64,
	soil_parameters:                         [dynamic]soil.Soil_Parameters,
	init_soil_profile_spec:                  jx.Value, // the raw SoilProfileParameters array
	pwp_fc_sat_function:                     string,
}

// The C++ in-class initialisers
make_site_parameters :: proc() -> Site_Parameters {
	return Site_Parameters {
		latitude = 52.5,
		slope = 0.01,
		height_nn = 50.0,
		groundwater_depth = 70.0,
		soil_cn_ratio = 10.0,
		drainage_coeff = 1.0,
		n_deposition = 30.0,
		max_effective_rooting_depth = 2.0,
		impenetrable_layer_depth = -1,
		soil_specific_humus_balance_correction = 0.0,
		bare_soil_kc_factor = 0.4,
		number_of_layers = 20,
		layer_thickness = 0.1,
		pwp_fc_sat_function = "Wessolek2009",
	}
}

// C++: Errors siteparameters::merge(SiteParameters*, Json)
//
// path_to_soil_dir replaces the C++'s calculateAndSetPwpFcSatFunctions map
// entry for "Wessolek2009" (see soil_parameters.odin's Pwp_Fc_Sat_Method doc
// comment) - it's threaded in as a parameter since this merge needs it, unlike
// the generic single-signature default_merge helper; the DEFAULT/= unwrap is
// therefore inlined here the same way environment_parameters_merge does.
site_parameters_merge :: proc(
	sp: ^Site_Parameters,
	j: jx.Value,
	path_to_soil_dir: string,
	allocator := context.allocator,
) -> tl.Errors {
	res: tl.Errors
	if jx.is_object(jx.get(j, "DEFAULT")) {
		res = site_parameters_merge(sp, jx.get(j, "DEFAULT"), path_to_soil_dir, allocator)
	}
	if jx.is_object(jx.get(j, "=")) {
		res = site_parameters_merge(sp, jx.get(j, "="), path_to_soil_dir, allocator)
	}

	jx.set_double_value(&sp.latitude, j, "Latitude")
	jx.set_double_value(&sp.slope, j, "Slope")
	jx.set_double_value(&sp.height_nn, j, "HeightNN")
	jx.set_double_value(&sp.groundwater_depth, j, "GroundwaterDepth")
	jx.set_double_value(&sp.soil_cn_ratio, j, "Soil_CN_Ratio")
	jx.set_double_value(&sp.drainage_coeff, j, "DrainageCoeff")
	jx.set_double_value(&sp.n_deposition, j, "NDeposition")
	jx.set_double_value(&sp.max_effective_rooting_depth, j, "MaxEffectiveRootingDepth")
	jx.set_double_value(&sp.impenetrable_layer_depth, j, "ImpenetrableLayerDepth")
	jx.set_double_value(
		&sp.soil_specific_humus_balance_correction,
		j,
		"SoilSpecificHumusBalanceCorrection",
	)
	jx.set_double_value(&sp.bare_soil_kc_factor, j, "Bare_soil_KC_factor")
	jx.set_string_value(&sp.pwp_fc_sat_function, j, "pwpFcSatFunction")

	jx.set_int_value(&sp.number_of_layers, j, "NumberOfLayers")
	jx.set_double_value(&sp.layer_thickness, j, "LayerThickness")

	// C++: std::function selectedSetPwpFcSatFunction = noSetPwpFcSat; if (find in
	// calculateAndSetPwpFcSatFunctions) ... else warn
	method, found := soil.pwp_fc_sat_method_from_name(sp.pwp_fc_sat_function)
	if !found {
		tl.append_warningf(&res, "Couldn't find pwpFcSatFunction: %s", sp.pwp_fc_sat_function)
	}

	if jx.is_array(jx.get(j, "SoilProfileParameters")) {
		sp.init_soil_profile_spec = jx.get(j, "SoilProfileParameters")
		r := soil.create_equal_sized_soil_pms(
			method,
			path_to_soil_dir,
			jx.array_items(sp.init_soil_profile_spec),
			sp.layer_thickness,
			sp.number_of_layers,
			allocator,
		)
		if tl.success(r.errs) {
			sp.soil_parameters = r.result
			if len(sp.soil_parameters) == 0 {
				tl.append_error(&res, "Soil profile is empty!")
			}
		} else {
			tl.append_errors(&res, r.errs)
		}
	} else if jx.is_string(jx.get(j, "SoilProfileParameters")) &&
	   !strings.has_prefix(jx.string_value_of(jx.get(j, "SoilProfileParameters")), "capnp") {
		tl.append_error(
			&res,
			"Couldn't read 'SoilProfileParameters' JSON array from JSON object:\n",
		)
	}

	return res
}

// C++: json11::Json siteparameters::to_json(const SiteParameters*)
site_parameters_to_json :: proc(sp: ^Site_Parameters, a: Allocator) -> jx.Value {
	o := jx.obj(
		a,
		{"type", jx.sl("SiteParameters")},
		{
			"Latitude",
			jx.arr(a, jx.f(sp.latitude), jx.sl(""), jx.sl("latitude in decimal degrees")),
		},
		{"Slope", jx.vu(sp.slope, "m m-1", a)},
		{"HeightNN", jx.arr(a, jx.f(sp.height_nn), jx.sl("m"), jx.sl("height above sea level"))},
		{"GroundwaterDepth", jx.vu(sp.groundwater_depth, "m", a)},
		{"Soil_CN_Ratio", jx.f(sp.soil_cn_ratio)},
		{"DrainageCoeff", jx.f(sp.drainage_coeff)},
		{"NDeposition", jx.vu(sp.n_deposition, "kg N ha-1 y-1", a)},
		{"MaxEffectiveRootingDepth", jx.vu(sp.max_effective_rooting_depth, "m", a)},
		{"ImpenetrableLayerDepth", jx.vu(sp.impenetrable_layer_depth, "m", a)},
		{
			"SoilSpecificHumusBalanceCorrection",
			jx.vu(sp.soil_specific_humus_balance_correction, "humus equivalents", a),
		},
		{"Bare_soil_KC_factor", jx.f(sp.bare_soil_kc_factor)},
	)
	oo := o.(jx.Object)
	soil_profile_params := make(jx.Array, 0, len(sp.soil_parameters), a)
	for &sp_item in sp.soil_parameters {
		append(&soil_profile_params, soil.soil_parameters_to_json(&sp_item, a))
	}
	jx.obj_set(&oo, "SoilProfileParameters", jx.Value(soil_profile_params), a)
	return jx.Value(oo)
}

// ---------------------------------------------------------------------------
// SimulationParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SimulationParameters
Simulation_Parameters :: struct {
	start_date:                                  d.Date,
	end_date:                                    d.Date,
	nitrogen_response_on:                        bool,
	water_deficit_response_on:                   bool,
	emergence_flooding_control_on:               bool,
	emergence_moisture_control_on:               bool,
	frost_kill_on:                               bool,
	use_automatic_irrigation:                    bool,
	auto_irrigation_params:                      Automatic_Irrigation_Parameters,
	use_n_min_mineral_fertilising_method:        bool,
	n_min_fertiliser_partition:                  Mineral_Fertilizer_Parameters,
	n_min_user_params:                           NMin_Application_Parameters,
	use_secondary_yields:                        bool,
	use_automatic_harvest_trigger:               bool,
	number_of_layers:                            int,
	layer_thickness:                             f64,
	start_pv_index:                              int,
	julian_day_automatic_fertilising:            int,
	serialize_monica_state_at_end:               bool,
	serialize_monica_state_at_end_to_json:       bool,
	path_to_serialization_at_end_file:           string,
	load_serialized_monica_state_at_start:       bool,
	deserialized_monica_state_from_json:         bool,
	path_to_load_serialization_file:             string,
	no_of_previous_days_serialized_climate_data: u64,
	dual_kc_method:                              bool, // FAO-56 Dual Kc evaporation partitioning
}

// The C++ in-class initialisers
make_simulation_parameters :: proc() -> Simulation_Parameters {
	return Simulation_Parameters {
		nitrogen_response_on = true,
		water_deficit_response_on = true,
		emergence_flooding_control_on = true,
		emergence_moisture_control_on = true,
		frost_kill_on = true,
		auto_irrigation_params = make_automatic_irrigation_parameters(),
		use_secondary_yields = true,
		number_of_layers = 20,
		layer_thickness = 0.1,
	}
}

// C++: Errors simulationparameters::merge(SimulationParameters*, Json)
simulation_parameters_merge :: proc(sp: ^Simulation_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(sp, j, simulation_parameters_merge)

	jx.set_iso_date_value(&sp.start_date, j, "startDate")
	jx.set_iso_date_value(&sp.end_date, j, "endDate")

	jx.set_bool_value(&sp.nitrogen_response_on, j, "NitrogenResponseOn")
	jx.set_bool_value(&sp.water_deficit_response_on, j, "WaterDeficitResponseOn")
	jx.set_bool_value(&sp.emergence_flooding_control_on, j, "EmergenceFloodingControlOn")
	jx.set_bool_value(&sp.emergence_moisture_control_on, j, "EmergenceMoistureControlOn")
	jx.set_bool_value(&sp.frost_kill_on, j, "FrostKillOn")

	jx.set_bool_value(&sp.use_automatic_irrigation, j, "UseAutomaticIrrigation")
	// the C++ discards this merge's errors
	_ = automatic_irrigation_parameters_merge(
		&sp.auto_irrigation_params,
		jx.get(j, "AutoIrrigationParams"),
	)

	jx.set_bool_value(&sp.use_n_min_mineral_fertilising_method, j, "UseNMinMineralFertilisingMethod")
	_ = mineral_fertilizer_parameters_merge(
		&sp.n_min_fertiliser_partition,
		jx.get(j, "NMinFertiliserPartition"),
	)
	_ = nmin_application_parameters_merge(&sp.n_min_user_params, jx.get(j, "NMinUserParams"))
	jx.set_int_value(&sp.julian_day_automatic_fertilising, j, "JulianDayAutomaticFertilising")

	jx.set_bool_value(&sp.use_secondary_yields, j, "UseSecondaryYields")
	jx.set_bool_value(&sp.use_automatic_harvest_trigger, j, "UseAutomaticHarvestTrigger")
	jx.set_int_value(&sp.number_of_layers, j, "NumberOfLayers")
	jx.set_double_value(&sp.layer_thickness, j, "LayerThickness")

	jx.set_int_value(&sp.start_pv_index, j, "StartPVIndex")

	ser_state := jx.get(j, "serializedMonicaState")
	if jx.is_object(ser_state) && len(jx.object_items(ser_state)) > 0 {
		load_state := jx.get(ser_state, "load")
		if jx.is_object(load_state) {
			jx.set_bool_value(&sp.load_serialized_monica_state_at_start, load_state, "atStart")
			jx.set_bool_value(&sp.deserialized_monica_state_from_json, load_state, "fromJson")
			jx.set_string_value(&sp.path_to_load_serialization_file, load_state, "path")
		}
		save_state := jx.get(ser_state, "save")
		if jx.is_object(save_state) {
			jx.set_bool_value(&sp.serialize_monica_state_at_end, save_state, "atEnd")
			jx.set_bool_value(&sp.serialize_monica_state_at_end_to_json, save_state, "toJson")
			jx.set_string_value(&sp.path_to_serialization_at_end_file, save_state, "path")
			sp.no_of_previous_days_serialized_climate_data = u64(
				max(0, jx.int_value(save_state, "noOfPreviousDaysSerializedClimateData")),
			)
		}
	}

	// FAO-56 Dual Kc: "evapotranspiration-method": "FAO-56-Dual" activates the
	// Dual Kc pathway; any other value (or an absent key) keeps single-Kc.
	if jx.string_value_of(jx.get(j, "evapotranspiration-method")) == "FAO-56-Dual" {
		sp.dual_kc_method = true
	}

	return res
}

// C++: json11::Json simulationparameters::to_json(const SimulationParameters*)
simulation_parameters_to_json :: proc(sp: ^Simulation_Parameters, a: Allocator) -> jx.Value {
	start_iso := d.to_iso_date_string(sp.start_date, "", a)
	end_iso := d.to_iso_date_string(sp.end_date, "", a)

	load_o := jx.obj(
		a,
		{"atStart", jx.b(sp.load_serialized_monica_state_at_start)},
		{"fromJson", jx.b(sp.deserialized_monica_state_from_json)},
		{"path", jx.s(sp.path_to_load_serialization_file, a)},
	)
	save_o := jx.obj(
		a,
		{"atEnd", jx.b(sp.serialize_monica_state_at_end)},
		{"toJson", jx.b(sp.serialize_monica_state_at_end_to_json)},
		{"path", jx.s(sp.path_to_serialization_at_end_file, a)},
		{
			"noOfPreviousDaysSerializedClimateData",
			jx.i(int(sp.no_of_previous_days_serialized_climate_data)),
		},
	)
	ser_o := jx.obj(a, {"load", load_o}, {"save", save_o})

	return jx.obj(
		a,
		{"type", jx.sl("SimulationParameters")},
		{"startDate", jx.s(start_iso, a)},
		{"endDate", jx.s(end_iso, a)},
		{"NitrogenResponseOn", jx.b(sp.nitrogen_response_on)},
		{"WaterDeficitResponseOn", jx.b(sp.water_deficit_response_on)},
		{"EmergenceFloodingControlOn", jx.b(sp.emergence_flooding_control_on)},
		{"EmergenceMoistureControlOn", jx.b(sp.emergence_moisture_control_on)},
		{"FrostKillOn", jx.b(sp.frost_kill_on)},
		{"UseAutomaticIrrigation", jx.b(sp.use_automatic_irrigation)},
		{
			"AutoIrrigationParams",
			automatic_irrigation_parameters_to_json(&sp.auto_irrigation_params, a),
		},
		{"UseNMinMineralFertilisingMethod", jx.b(sp.use_n_min_mineral_fertilising_method)},
		{
			"NMinFertiliserPartition",
			mineral_fertilizer_parameters_to_json(&sp.n_min_fertiliser_partition, a),
		},
		{"NMinUserParams", nmin_application_parameters_to_json(&sp.n_min_user_params, a)},
		{"JulianDayAutomaticFertilising", jx.i(sp.julian_day_automatic_fertilising)},
		{"UseSecondaryYields", jx.b(sp.use_secondary_yields)},
		{"UseAutomaticHarvestTrigger", jx.b(sp.use_automatic_harvest_trigger)},
		{"NumberOfLayers", jx.i(sp.number_of_layers)},
		{"LayerThickness", jx.f(sp.layer_thickness)},
		{"StartPVIndex", jx.i(sp.start_pv_index)},
		{"serializeMonicaStateAtEnd", jx.b(sp.serialize_monica_state_at_end)},
		{"serializedMonicaState", ser_o},
		{"evapotranspiration-method", jx.sl(sp.dual_kc_method ? "FAO-56-Dual" : "Penman-Monteith")},
	)
}

// ---------------------------------------------------------------------------
// CropModuleParameters
// ---------------------------------------------------------------------------

// C++: struct monica::CropModuleParameters
Crop_Module_Parameters :: struct {
	canopy_reflection_coefficient:                          f64,
	reference_max_assimilation_rate:                        f64,
	reference_leaf_area_index:                              f64,
	maintenance_respiration_parameter1:                     f64,
	maintenance_respiration_parameter2:                     f64,
	minimum_n_concentration_root:                           f64,
	minimum_available_n:                                    f64, // [kg m-2]
	reference_albedo:                                       f64,
	stomata_conductance_alpha:                              f64,
	saturation_beta:                                        f64,
	growth_respiration_redux:                               f64,
	max_crop_n_demand:                                      f64,
	growth_respiration_parameter1:                          f64,
	growth_respiration_parameter2:                          f64,
	tortuosity:                                             f64, // old AD
	adjust_root_depth_for_soil_props:                       bool,
	time_under_anoxia_threshold:                            [dynamic]int,
	__enable_Phenology_WangEngelTemperatureResponse__:      bool,
	__enable_Photosynthesis_WangEngelTemperatureResponse__: bool,
	__enable_hourly_FvCB_photosynthesis__:                  bool,
	__enable_T_response_leaf_expansion__:                   bool,
	__disable_daily_root_biomass_to_soil__:                 bool,
	__enable_vernalisation_factor_fix__:                    bool,
	__enable_PASW_root_penetration__:                       bool,
	is_intercropping:                                       bool,
	sequential_water_use:                                   bool,
	two_way_sync:                                           bool,
	intercropping_k_s:                                      f64,
	intercropping_k_t:                                      f64,
	intercropping_ph_redux:                                 f64,
	intercropping_dvs_phr:                                  f64,
	intercropping_auto_ph_redux:                            bool,
	intercropping_reader_sr:                                string,
	intercropping_writer_sr:                                string,
}

// C++ in-class initialisers, incl. time_under_anoxia_threshold{4,4,4,4,4,4,4}
make_crop_module_parameters :: proc(a: Allocator) -> Crop_Module_Parameters {
	cmp := Crop_Module_Parameters {
		adjust_root_depth_for_soil_props = true,
		two_way_sync                     = true,
		intercropping_ph_redux           = 0.5,
		intercropping_dvs_phr            = 5.791262,
		intercropping_auto_ph_redux      = true,
	}
	cmp.time_under_anoxia_threshold = make([dynamic]int, 0, 7, a)
	for _ in 0 ..< 7 {
		append(&cmp.time_under_anoxia_threshold, 4)
	}
	return cmp
}

// C++: Errors cropmoduleparameters::merge(CropModuleParameters*, Json)
crop_module_parameters_merge :: proc(cmp: ^Crop_Module_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(cmp, j, crop_module_parameters_merge)

	jx.set_double_value(&cmp.canopy_reflection_coefficient, j, "CanopyReflectionCoefficient")
	jx.set_double_value(&cmp.reference_max_assimilation_rate, j, "ReferenceMaxAssimilationRate")
	jx.set_double_value(&cmp.reference_leaf_area_index, j, "ReferenceLeafAreaIndex")
	jx.set_double_value(
		&cmp.maintenance_respiration_parameter1,
		j,
		"MaintenanceRespirationParameter1",
	)
	jx.set_double_value(
		&cmp.maintenance_respiration_parameter2,
		j,
		"MaintenanceRespirationParameter2",
	)
	jx.set_double_value(&cmp.minimum_n_concentration_root, j, "MinimumNConcentrationRoot")
	jx.set_double_value(&cmp.minimum_available_n, j, "MinimumAvailableN")
	jx.set_double_value(&cmp.reference_albedo, j, "ReferenceAlbedo")
	jx.set_double_value(&cmp.stomata_conductance_alpha, j, "StomataConductanceAlpha")
	jx.set_double_value(&cmp.saturation_beta, j, "SaturationBeta")
	jx.set_double_value(&cmp.growth_respiration_redux, j, "GrowthRespirationRedux")
	jx.set_double_value(&cmp.max_crop_n_demand, j, "MaxCropNDemand")
	jx.set_double_value(&cmp.growth_respiration_parameter1, j, "GrowthRespirationParameter1")
	jx.set_double_value(&cmp.growth_respiration_parameter2, j, "GrowthRespirationParameter2")
	jx.set_double_value(&cmp.tortuosity, j, "Tortuosity")
	jx.set_bool_value(&cmp.adjust_root_depth_for_soil_props, j, "AdjustRootDepthForSoilProps")

	// a bare number fills every element; otherwise it is read as a vector
	if jx.is_number(jx.get(j, "TimeUnderAnoxiaThreshold")) {
		v := int(jx.number_value(jx.get(j, "TimeUnderAnoxiaThreshold")))
		for i in 0 ..< len(cmp.time_under_anoxia_threshold) {
			cmp.time_under_anoxia_threshold[i] = v
		}
	} else if jx.is_array(jx.get(j, "TimeUnderAnoxiaThreshold")) {
		nv := jx.int_vector_d(jx.get(j, "TimeUnderAnoxiaThreshold"), nil, 0, context.allocator)
		clear(&cmp.time_under_anoxia_threshold)
		for x in nv {
			append(&cmp.time_under_anoxia_threshold, x)
		}
		delete(nv)
	}

	jx.set_bool_value(
		&cmp.__enable_Photosynthesis_WangEngelTemperatureResponse__,
		j,
		"__enable_Photosynthesis_WangEngelTemperatureResponse__",
	)
	jx.set_bool_value(
		&cmp.__enable_Phenology_WangEngelTemperatureResponse__,
		j,
		"__enable_Phenology_WangEngelTemperatureResponse__",
	)
	jx.set_bool_value(
		&cmp.__enable_hourly_FvCB_photosynthesis__,
		j,
		"__enable_hourly_FvCB_photosynthesis__",
	)
	jx.set_bool_value(
		&cmp.__enable_T_response_leaf_expansion__,
		j,
		"__enable_T_response_leaf_expansion__",
	)
	jx.set_bool_value(
		&cmp.__disable_daily_root_biomass_to_soil__,
		j,
		"__disable_daily_root_biomass_to_soil__",
	)
	jx.set_bool_value(
		&cmp.__enable_vernalisation_factor_fix__,
		j,
		"__enable_vernalisation_factor_fix__",
	)
	jx.set_bool_value(&cmp.__enable_PASW_root_penetration__, j, "__enable_PASW_root_penetration__")

	ic := jx.get(j, "intercropping")
	jx.set_bool_value(&cmp.is_intercropping, ic, "is_intercropping")
	jx.set_bool_value(&cmp.sequential_water_use, ic, "sequential_water_use")
	jx.set_bool_value(&cmp.two_way_sync, ic, "two_way_sync")
	jx.set_double_value(&cmp.intercropping_k_s, ic, "k_s")
	jx.set_double_value(&cmp.intercropping_k_t, ic, "k_t")
	jx.set_double_value(&cmp.intercropping_ph_redux, ic, "PHredux")
	jx.set_double_value(&cmp.intercropping_dvs_phr, ic, "DVS_PHr")
	jx.set_bool_value(&cmp.intercropping_auto_ph_redux, ic, "auto_PHredux")
	jx.set_string_value(&cmp.intercropping_reader_sr, ic, "reader_sr")
	jx.set_string_value(&cmp.intercropping_writer_sr, ic, "writer_sr")

	return res
}

// C++: json11::Json cropmoduleparameters::to_json(const CropModuleParameters*)
//
// NOTE(c++-quirk): to_json omits __enable_PASW_root_penetration__ and the whole
// intercropping block, so a to_json/merge round trip drops them. Reproduced.
crop_module_parameters_to_json :: proc(cmp: ^Crop_Module_Parameters, a: Allocator) -> jx.Value {
	anoxia := make(jx.Array, 0, len(cmp.time_under_anoxia_threshold), a)
	for v in cmp.time_under_anoxia_threshold {
		append(&anoxia, jx.i(v))
	}

	return jx.obj(
		a,
		{"type", jx.sl("CropModuleParameters")},
		{"CanopyReflectionCoefficient", jx.f(cmp.canopy_reflection_coefficient)},
		{"ReferenceMaxAssimilationRate", jx.f(cmp.reference_max_assimilation_rate)},
		{"ReferenceLeafAreaIndex", jx.f(cmp.reference_leaf_area_index)},
		{"MaintenanceRespirationParameter1", jx.f(cmp.maintenance_respiration_parameter1)},
		{"MaintenanceRespirationParameter2", jx.f(cmp.maintenance_respiration_parameter2)},
		{"MinimumNConcentrationRoot", jx.f(cmp.minimum_n_concentration_root)},
		{"MinimumAvailableN", jx.f(cmp.minimum_available_n)},
		{"ReferenceAlbedo", jx.f(cmp.reference_albedo)},
		{"StomataConductanceAlpha", jx.f(cmp.stomata_conductance_alpha)},
		{"SaturationBeta", jx.f(cmp.saturation_beta)},
		{"GrowthRespirationRedux", jx.f(cmp.growth_respiration_redux)},
		{"MaxCropNDemand", jx.f(cmp.max_crop_n_demand)},
		{"GrowthRespirationParameter1", jx.f(cmp.growth_respiration_parameter1)},
		{"GrowthRespirationParameter2", jx.f(cmp.growth_respiration_parameter2)},
		{"Tortuosity", jx.f(cmp.tortuosity)},
		{"AdjustRootDepthForSoilProps", jx.b(cmp.adjust_root_depth_for_soil_props)},
		{"TimeUnderAnoxiaThreshold", jx.Value(anoxia)},
		{
			"__enable_Phenology_WangEngelTemperatureResponse__",
			jx.b(cmp.__enable_Phenology_WangEngelTemperatureResponse__),
		},
		{
			"__enable_Photosynthesis_WangEngelTemperatureResponse__",
			jx.b(cmp.__enable_Photosynthesis_WangEngelTemperatureResponse__),
		},
		{"__enable_hourly_FvCB_photosynthesis__", jx.b(cmp.__enable_hourly_FvCB_photosynthesis__)},
		{"__enable_T_response_leaf_expansion__", jx.b(cmp.__enable_T_response_leaf_expansion__)},
		{
			"__disable_daily_root_biomass_to_soil__",
			jx.b(cmp.__disable_daily_root_biomass_to_soil__),
		},
		{"__enable_vernalisation_factor_fix__", jx.b(cmp.__enable_vernalisation_factor_fix__)},
	)
}

// ---------------------------------------------------------------------------
// EnvironmentParameters
// ---------------------------------------------------------------------------

// C++: mas::schema::climate::RCP, used as a plain enum (plan-odin.md prep 3)
RCP :: enum {
	RCP19,
	RCP26,
	RCP34,
	RCP45,
	RCP60,
	RCP70,
	RCP85,
}

// C++: struct monica::EnvironmentParameters
Environment_Parameters :: struct {
	albedo:                      f64,
	rcp:                         RCP,
	atmospheric_CO2:             f64,
	atmospheric_CO2s:            map[int]f64,
	atmospheric_O3:              f64,
	atmospheric_O3s:             map[int]f64,
	wind_speed_height_m:         f64,
	leaching_depth_m:            f64,
	time_step:                   f64,
	max_groundwater_depth_m:     f64,
	min_groundwater_depth_m:     f64,
	min_groundwater_depth_month: int,
}

// The C++ in-class initialisers
make_environment_parameters :: proc() -> Environment_Parameters {
	return Environment_Parameters {
		albedo = 0.23,
		rcp = .RCP85,
		wind_speed_height_m = 2.0,
		max_groundwater_depth_m = 18.0,
		min_groundwater_depth_m = 20.0,
		min_groundwater_depth_month = 3,
	}
}

// C++: the rcpNo2rcpEnum lambda inside environmentparameters::merge
@(private)
rcp_no_2_rcp_enum :: proc(rcp_no: int, res: ^tl.Errors) -> RCP {
	switch rcp_no {
	case 19:
		return .RCP19
	case 26:
		return .RCP26
	case 34:
		return .RCP34
	case 45:
		return .RCP45
	case 60:
		return .RCP60
	case 70:
		return .RCP70
	case 85:
		return .RCP85
	}
	tl.append_warningf(res, "RCP%d unknown. Default RCP 8.5 used.", rcp_no)
	return .RCP85
}

// C++: the rcp2str lambda inside environmentparameters::to_json
@(private)
rcp_2_str :: proc(rcp: RCP) -> string {
	switch rcp {
	case .RCP19:
		return "rcp19"
	case .RCP26:
		return "rcp26"
	case .RCP34:
		return "rcp34"
	case .RCP45:
		return "rcp45"
	case .RCP60:
		return "rcp60"
	case .RCP70:
		return "rcp70"
	case .RCP85:
		return "rcp85"
	}
	return "rcp85"
}

// C++: Errors environmentparameters::merge(EnvironmentParameters*, Json)
environment_parameters_merge :: proc(
	ep: ^Environment_Parameters,
	j: jx.Value,
	a: Allocator,
) -> tl.Errors {
	res: tl.Errors
	// defaultMerge, spelled out because this merge takes an extra allocator
	if jx.is_object(jx.get(j, "DEFAULT")) {
		res = environment_parameters_merge(ep, jx.get(j, "DEFAULT"), a)
	}
	if jx.is_object(jx.get(j, "=")) {
		res = environment_parameters_merge(ep, jx.get(j, "="), a)
	}

	jx.set_double_value(&ep.albedo, j, "Albedo")

	// rcp accepts "85" / "8.5" / "rcp85" / "rcp8.5" as strings, or a number
	// either as 8.5 or as 85. A non-numeric string throws in the C++ and is
	// caught, leaving rcp untouched; here parse failures do the same.
	rcp_j := jx.get(j, "rcp")
	if jx.is_string(rcp_j) {
		rcp_str := jx.string_value_of(rcp_j)
		switch len(rcp_str) {
		case 2:
			if v, ok := strconv.parse_int(rcp_str); ok {
				ep.rcp = rcp_no_2_rcp_enum(v, &res)
			} else {
				tl.append_warningf(&res, "%s unknown. Default RCP 8.5 used.", rcp_str)
			}
		case 3:
			if v, ok := strconv.parse_f64(rcp_str); ok {
				ep.rcp = rcp_no_2_rcp_enum(int(v * 10), &res)
			} else {
				tl.append_warningf(&res, "%s unknown. Default RCP 8.5 used.", rcp_str)
			}
		case 5:
			if v, ok := strconv.parse_int(rcp_str[3:]); ok {
				ep.rcp = rcp_no_2_rcp_enum(v, &res)
			} else {
				tl.append_warningf(&res, "%s unknown. Default RCP 8.5 used.", rcp_str)
			}
		case 6:
			if v, ok := strconv.parse_f64(rcp_str[3:]); ok {
				ep.rcp = rcp_no_2_rcp_enum(int(v * 10), &res)
			} else {
				tl.append_warningf(&res, "%s unknown. Default RCP 8.5 used.", rcp_str)
			}
		case:
			ep.rcp = .RCP85
		}
	} else if jx.is_number(rcp_j) {
		rcp_no := jx.number_value(rcp_j)
		if rcp_no < 10 {
			ep.rcp = rcp_no_2_rcp_enum(int(rcp_no * 10), &res)
		} else {
			ep.rcp = rcp_no_2_rcp_enum(int(rcp_no), &res)
		}
	}

	jx.set_double_value(&ep.atmospheric_CO2, j, "AtmosphericCO2")
	if jx.is_object(jx.get(j, "AtmosphericCO2s")) {
		clear(&ep.atmospheric_CO2s)
		if ep.atmospheric_CO2s == nil {
			ep.atmospheric_CO2s = make(map[int]f64, a)
		}
		for k, v in jx.object_items(jx.get(j, "AtmosphericCO2s")) {
			if year, ok := strconv.parse_int(k); ok {
				ep.atmospheric_CO2s[year] = jx.number_value(v)
			}
		}
	}
	jx.set_double_value(&ep.atmospheric_O3, j, "AtmosphericO3")
	if jx.is_object(jx.get(j, "AtmosphericO3s")) {
		clear(&ep.atmospheric_O3s)
		if ep.atmospheric_O3s == nil {
			ep.atmospheric_O3s = make(map[int]f64, a)
		}
		for k, v in jx.object_items(jx.get(j, "AtmosphericO3s")) {
			if year, ok := strconv.parse_int(k); ok {
				ep.atmospheric_O3s[year] = jx.number_value(v)
			}
		}
	}
	jx.set_double_value(&ep.wind_speed_height_m, j, "WindSpeedHeight")
	jx.set_double_value(&ep.leaching_depth_m, j, "LeachingDepth")
	jx.set_double_value(&ep.time_step, j, "timeStep")
	jx.set_double_value(&ep.max_groundwater_depth_m, j, "MaxGroundwaterDepth")
	jx.set_double_value(&ep.min_groundwater_depth_m, j, "MinGroundwaterDepth")
	jx.set_int_value(&ep.min_groundwater_depth_month, j, "MinGroundwaterDepthMonth")

	return res
}

// C++: json11::Json environmentparameters::to_json(const EnvironmentParameters*)
environment_parameters_to_json :: proc(ep: ^Environment_Parameters, a: Allocator) -> jx.Value {
	year_map_to_json :: proc(m: map[int]f64, a: Allocator) -> jx.Value {
		o := make(jx.Object, 0, a)
		for year, v in m {
			buf: [32]byte
			key := strconv.write_int(buf[:], i64(year), 10)
			o[strings.clone(key, a)] = jx.f(v)
		}
		return jx.Value(o)
	}

	return jx.obj(
		a,
		{"type", jx.sl("EnvironmentParameters")},
		{"Albedo", jx.f(ep.albedo)},
		{"rcp", jx.sl(rcp_2_str(ep.rcp))},
		{"AtmosphericCO2", jx.f(ep.atmospheric_CO2)},
		{"AtmosphericCO2s", year_map_to_json(ep.atmospheric_CO2s, a)},
		{"AtmosphericO3", jx.f(ep.atmospheric_O3)},
		{"AtmosphericO3s", year_map_to_json(ep.atmospheric_O3s, a)},
		{"WindSpeedHeight", jx.f(ep.wind_speed_height_m)},
		{"LeachingDepth", jx.f(ep.leaching_depth_m)},
		{"timeStep", jx.f(ep.time_step)},
		{"MaxGroundwaterDepth", jx.f(ep.max_groundwater_depth_m)},
		{"MinGroundwaterDepth", jx.f(ep.min_groundwater_depth_m)},
		{"MinGroundwaterDepthMonth", jx.i(ep.min_groundwater_depth_month)},
	)
}

// ---------------------------------------------------------------------------
// CentralParameterProvider
// ---------------------------------------------------------------------------

// C++: struct monica::CentralParameterProvider
Central_Parameter_Provider :: struct {
	crop_params:                 Crop_Module_Parameters,
	env_params:                  Environment_Parameters,
	soil_moisture_mod_params:    Soil_Moisture_Module_Parameters,
	soil_temperature_mod_params: Soil_Temperature_Module_Parameters,
	soil_transport_mod_params:   Soil_Transport_Module_Parameters,
	soil_organic_mod_params:     Soil_Organic_Module_Parameters,
	sim_params:                  Simulation_Parameters,
	site_params:                 Site_Parameters,
	groundwater_information:     Measured_Groundwater_Table_Information,
}

make_central_parameter_provider :: proc(a: Allocator) -> Central_Parameter_Provider {
	return Central_Parameter_Provider {
		crop_params = make_crop_module_parameters(a),
		env_params = make_environment_parameters(),
		soil_temperature_mod_params = make_soil_temperature_module_parameters(),
		soil_organic_mod_params = make_soil_organic_module_parameters(),
		sim_params = make_simulation_parameters(),
		site_params = make_site_parameters(),
	}
}

// C++: Errors centralparameterprovider::merge(CentralParameterProvider*, Json)
//
// path_to_soil_dir is threaded through to site_parameters_merge - see its doc
// comment. The C++ instead pre-populates SiteParameters.calculateAndSetPwpFcSatFunctions
// with a closure over this same path before merge ever runs (monica-run-main.cpp:239-249).
central_parameter_provider_merge :: proc(
	cpp: ^Central_Parameter_Provider,
	j: jx.Value,
	path_to_soil_dir: string,
	a: Allocator,
) -> tl.Errors {
	res: tl.Errors

	tl.append_errors(
		&res,
		crop_module_parameters_merge(&cpp.crop_params, jx.get(j, "userCropParameters")),
	)
	tl.append_errors(
		&res,
		environment_parameters_merge(&cpp.env_params, jx.get(j, "userEnvironmentParameters"), a),
	)
	tl.append_errors(
		&res,
		soil_moisture_module_parameters_merge(
			&cpp.soil_moisture_mod_params,
			jx.get(j, "userSoilMoistureParameters"),
		),
	)
	tl.append_errors(
		&res,
		soil_temperature_module_parameters_merge(
			&cpp.soil_temperature_mod_params,
			jx.get(j, "userSoilTemperatureParameters"),
		),
	)
	tl.append_errors(
		&res,
		soil_transport_module_parameters_merge(
			&cpp.soil_transport_mod_params,
			jx.get(j, "userSoilTransportParameters"),
		),
	)
	tl.append_errors(
		&res,
		soil_organic_module_parameters_merge(
			&cpp.soil_organic_mod_params,
			jx.get(j, "userSoilOrganicParameters"),
		),
	)
	tl.append_errors(
		&res,
		simulation_parameters_merge(&cpp.sim_params, jx.get(j, "simulationParameters")),
	)
	tl.append_errors(
		&res,
		site_parameters_merge(&cpp.site_params, jx.get(j, "siteParameters"), path_to_soil_dir, a),
	)

	if !jx.is_null(jx.get(j, "groundwaterInformation")) {
		tl.append_errors(
			&res,
			measured_groundwater_table_information_merge(
				&cpp.groundwater_information,
				jx.get(j, "groundwaterInformation"),
				a,
			),
		)
	}

	return res
}

// C++: json11::Json centralparameterprovider::to_json(const CentralParameterProvider*)
//
// NOTE(c++-quirk): groundwaterInformation is commented out in the C++ to_json,
// so it is not emitted. Reproduced.
central_parameter_provider_to_json :: proc(
	cpp: ^Central_Parameter_Provider,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("CentralParameterProvider")},
		{"userCropParameters", crop_module_parameters_to_json(&cpp.crop_params, a)},
		{"userEnvironmentParameters", environment_parameters_to_json(&cpp.env_params, a)},
		{
			"userSoilMoistureParameters",
			soil_moisture_module_parameters_to_json(&cpp.soil_moisture_mod_params, a),
		},
		{
			"userSoilTemperatureParameters",
			soil_temperature_module_parameters_to_json(&cpp.soil_temperature_mod_params, a),
		},
		{
			"userSoilTransportParameters",
			soil_transport_module_parameters_to_json(&cpp.soil_transport_mod_params, a),
		},
		{
			"userSoilOrganicParameters",
			soil_organic_module_parameters_to_json(&cpp.soil_organic_mod_params, a),
		},
		{"simulationParameters", simulation_parameters_to_json(&cpp.sim_params, a)},
		{"siteParameters", site_parameters_to_json(&cpp.site_params, a)},
	)
}

_ :: slice
