// Tranche 2 of the src/core/monica-parameters.{h,cpp} port: the structs
// CentralParameterProvider holds, plus the small ones they nest.
//
// MineralFertilizerParameters, NMinApplicationParameters, IrrigationParameters,
// AutomaticIrrigationParameters, MeasuredGroundwaterTableInformation,
// SiteParameters, SimulationParameters, CropModuleParameters,
// EnvironmentParameters, CentralParameterProvider.
package params

import "core:slice"
import "core:strconv"
import "core:strings"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "../soil"

// ---------------------------------------------------------------------------
// MineralFertilizerParameters
// ---------------------------------------------------------------------------

// C++: struct monica::MineralFertilizerParameters
Mineral_Fertilizer_Parameters :: struct {
	id:          string,
	name:        string,
	vo_Carbamid: f64, // [%]
	vo_NH4:      f64, // [%]
	vo_NO3:      f64, // [%]
}

// C++: Errors mineralfertilizerparameters::merge(...)
mineral_fertilizer_parameters_merge :: proc(
	fp: ^Mineral_Fertilizer_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(fp, j, mineral_fertilizer_parameters_merge)

	jx.set_string_value(&fp.id, j, "id")
	jx.set_string_value(&fp.name, j, "name")
	jx.set_double_value(&fp.vo_Carbamid, j, "Carbamid")
	jx.set_double_value(&fp.vo_NH4, j, "NH4")
	jx.set_double_value(&fp.vo_NO3, j, "NO3")

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
		{"Carbamid", jx.f(fp.vo_Carbamid)},
		{"NH4", jx.f(fp.vo_NH4)},
		{"NO3", jx.f(fp.vo_NO3)},
	)
}

// ---------------------------------------------------------------------------
// NMinApplicationParameters
// ---------------------------------------------------------------------------

// C++: struct monica::NMinApplicationParameters
NMin_Application_Parameters :: struct {
	min:         f64,
	max:         f64,
	delayInDays: int,
}

// C++: Errors nminapplicationparameters::merge(...)
nmin_application_parameters_merge :: proc(
	nap: ^NMin_Application_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(nap, j, nmin_application_parameters_merge)

	jx.set_double_value(&nap.min, j, "min")
	jx.set_double_value(&nap.max, j, "max")
	jx.set_int_value(&nap.delayInDays, j, "delayInDays")

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
		{"delayInDays", jx.i(nap.delayInDays)},
	)
}

// ---------------------------------------------------------------------------
// IrrigationParameters
// ---------------------------------------------------------------------------

// C++: struct monica::IrrigationParameters
Irrigation_Parameters :: struct {
	nitrateConcentration: f64, // [mg dm-3]
	sulfateConcentration: f64, // [mg dm-3]
	isDripIrrigation:     bool,
	fw:                   f64, // fraction of wetted soil surface [0-1]
}

// C++ default: fw{1.0}
make_irrigation_parameters :: proc() -> Irrigation_Parameters {
	return Irrigation_Parameters{fw = 1.0}
}

// C++: Errors irrigationparameters::merge(...)
irrigation_parameters_merge :: proc(ip: ^Irrigation_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(ip, j, irrigation_parameters_merge)

	jx.set_double_value(&ip.nitrateConcentration, j, "nitrateConcentration")
	jx.set_double_value(&ip.sulfateConcentration, j, "sulfateConcentration")
	jx.set_bool_value(&ip.isDripIrrigation, j, "isDripIrrigation")
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
		{"nitrateConcentration", jx.vu(ip.nitrateConcentration, "mg dm-3", a)},
		{"sulfateConcentration", jx.vu(ip.sulfateConcentration, "mg dm-3", a)},
		{"isDripIrrigation", jx.b(ip.isDripIrrigation)},
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
	using base:                    Irrigation_Parameters,
	startDate:                     d.Date,
	endDate:                       d.Date,
	amount:                        f64,
	percentNFC:                    f64,
	threshold:                     f64,
	criticalMoistureDepthM:        f64,
	minDaysBetweenIrrigationEvents: int,
}

// C++ defaults: amount{-1}, percentNFC{-1}, threshold{-1}, criticalMoistureDepthM{0.3}
make_automatic_irrigation_parameters :: proc() -> Automatic_Irrigation_Parameters {
	return Automatic_Irrigation_Parameters {
		base = make_irrigation_parameters(),
		amount = -1.0,
		percentNFC = -1.0,
		threshold = -1.0,
		criticalMoistureDepthM = 0.3,
	}
}

// C++: Errors automaticirrigationparameters::merge(...)
//
// NOTE(c++-quirk): endDate is read from the key "stopDate", not "endDate"; and
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

	jx.set_iso_date_value(&aip.startDate, j, "startDate")
	jx.set_iso_date_value(&aip.endDate, j, "stopDate")
	jx.set_double_value(&aip.amount, j, "amount")
	jx.set_double_value(&aip.percentNFC, j, "set_to_%nFC")
	jx.set_double_value(&aip.threshold, j, "threshold", jx.transform_if_percent(j, "threshold"))
	jx.set_double_value(&aip.threshold, j, "trigger_if_nFC_below_%", .PERCENT)
	jx.set_double_value(
		&aip.criticalMoistureDepthM,
		j,
		"calc_nFC_until_depth_m",
		jx.transform_if_not_meters(j, "calc_nFC_until_depth_m"),
	)
	jx.set_int_value(&aip.minDaysBetweenIrrigationEvents, j, "minDaysBetweenIrrigationEvents")

	return res
}

// C++: json11::Json automaticirrigationparameters::to_json(...)
//
// NOTE(c++-quirk): endDate is not emitted at all, so a to_json/merge round trip
// loses it. Reproduced.
automatic_irrigation_parameters_to_json :: proc(
	aip: ^Automatic_Irrigation_Parameters,
	a: Allocator,
) -> jx.Value {
	iso := d.to_iso_date_string(aip.startDate, "", a)
	o := jx.obj(
		a,
		{"type", jx.sl("AutomaticIrrigationParameters")},
		{"startDate", jx.s(iso, a)},
		{"irrigationParameters", irrigation_parameters_to_json(&aip.base, a)},
		{"trigger_if_nFC_below_%", jx.vu(aip.threshold * 100.0, "%", a)},
		{"calc_nFC_until_depth_m", jx.vu(aip.criticalMoistureDepthM, "m", a)},
		{"minDaysBetweenIrrigationEvents", jx.vu_int(aip.minDaysBetweenIrrigationEvents, "d", a)},
	)
	oo := o.(jx.Object)
	if aip.amount > 0 {
		jx.obj_set(&oo, "amount", jx.vu(aip.amount, "mm", a), a)
	} else {
		jx.obj_set(&oo, "set_to_%nFC", jx.vu(aip.percentNFC, "%", a), a)
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
	groundwaterInformationAvailable: bool,
	groundwaterInfo:                 map[string]f64, // ISO date -> depth
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

	jx.set_bool_value(&gwi.groundwaterInformationAvailable, j, "groundwaterInformationAvailable")

	if jx.has_object_shape(j, "groundwaterInfo") {
		if gwi.groundwaterInfo == nil {
			gwi.groundwaterInfo = make(map[string]f64, a)
		}
		for k, v in jx.object_items(jx.get(j, "groundwaterInfo")) {
			// the C++ round-trips through Date::fromIsoDateString; normalise the
			// same way so a non-canonical key produces the same output key
			dd := d.from_iso_date_string(k)
			key := d.to_iso_date_string(dd, "", a)
			gwi.groundwaterInfo[key] = jx.number_value(v)
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
	for k, v in gwi.groundwaterInfo {
		gi[strings.clone(k, a)] = jx.f(v)
	}
	return jx.obj(
		a,
		{"type", jx.sl("MeasuredGroundwaterTableInformation")},
		{"groundwaterInformationAvailable", jx.b(gwi.groundwaterInformationAvailable)},
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
	if gwi.groundwaterInformationAvailable && len(gwi.groundwaterInfo) > 0 {
		key := d.to_iso_date_string(gwDate, "", allocator)
		if v, ok := gwi.groundwaterInfo[key]; ok {
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
// data member here (plan-odin.md prep 2); pwpFcSatFunction selects the method by
// name at the point of use, via soil.pwp_fc_sat_method_from_name.
Site_Parameters :: struct {
	vs_Latitude:                         f64, // ZALF latitude
	vs_Slope:                            f64, // [m m-1]
	vs_HeightNN:                         f64, // [m]
	vs_GroundwaterDepth:                 f64, // [m]
	vs_Soil_CN_Ratio:                    f64,
	vs_DrainageCoeff:                    f64,
	vq_NDeposition:                      f64, // [kg N ha-1 y-1]
	vs_MaxEffectiveRootingDepth:         f64, // [m]
	vs_ImpenetrableLayerDepth:           f64, // [m]
	vs_SoilSpecificHumusBalanceCorrection: f64, // humus equivalents
	bareSoilKcFactor:                    f64,
	numberOfLayers:                      int,
	layerThickness:                      f64,
	vs_SoilParameters:                   [dynamic]soil.Soil_Parameters,
	initSoilProfileSpec:                 jx.Value, // the raw SoilProfileParameters array
	pwpFcSatFunction:                    string,
}

// The C++ in-class initialisers
make_site_parameters :: proc() -> Site_Parameters {
	return Site_Parameters {
		vs_Latitude = 52.5,
		vs_Slope = 0.01,
		vs_HeightNN = 50.0,
		vs_GroundwaterDepth = 70.0,
		vs_Soil_CN_Ratio = 10.0,
		vs_DrainageCoeff = 1.0,
		vq_NDeposition = 30.0,
		vs_MaxEffectiveRootingDepth = 2.0,
		vs_ImpenetrableLayerDepth = -1,
		vs_SoilSpecificHumusBalanceCorrection = 0.0,
		bareSoilKcFactor = 0.4,
		numberOfLayers = 20,
		layerThickness = 0.1,
		pwpFcSatFunction = "Wessolek2009",
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

	jx.set_double_value(&sp.vs_Latitude, j, "Latitude")
	jx.set_double_value(&sp.vs_Slope, j, "Slope")
	jx.set_double_value(&sp.vs_HeightNN, j, "HeightNN")
	jx.set_double_value(&sp.vs_GroundwaterDepth, j, "GroundwaterDepth")
	jx.set_double_value(&sp.vs_Soil_CN_Ratio, j, "Soil_CN_Ratio")
	jx.set_double_value(&sp.vs_DrainageCoeff, j, "DrainageCoeff")
	jx.set_double_value(&sp.vq_NDeposition, j, "NDeposition")
	jx.set_double_value(&sp.vs_MaxEffectiveRootingDepth, j, "MaxEffectiveRootingDepth")
	jx.set_double_value(&sp.vs_ImpenetrableLayerDepth, j, "ImpenetrableLayerDepth")
	jx.set_double_value(
		&sp.vs_SoilSpecificHumusBalanceCorrection,
		j,
		"SoilSpecificHumusBalanceCorrection",
	)
	jx.set_double_value(&sp.bareSoilKcFactor, j, "Bare_soil_KC_factor")
	jx.set_string_value(&sp.pwpFcSatFunction, j, "pwpFcSatFunction")

	jx.set_int_value(&sp.numberOfLayers, j, "NumberOfLayers")
	jx.set_double_value(&sp.layerThickness, j, "LayerThickness")

	// C++: std::function selectedSetPwpFcSatFunction = noSetPwpFcSat; if (find in
	// calculateAndSetPwpFcSatFunctions) ... else warn
	method, found := soil.pwp_fc_sat_method_from_name(sp.pwpFcSatFunction)
	if !found {
		tl.append_warningf(&res, "Couldn't find pwpFcSatFunction: %s", sp.pwpFcSatFunction)
	}

	if jx.is_array(jx.get(j, "SoilProfileParameters")) {
		sp.initSoilProfileSpec = jx.get(j, "SoilProfileParameters")
		r := soil.create_equal_sized_soil_pms(
			method,
			path_to_soil_dir,
			jx.array_items(sp.initSoilProfileSpec),
			sp.layerThickness,
			sp.numberOfLayers,
			allocator,
		)
		if tl.success(r.errs) {
			sp.vs_SoilParameters = r.result
			if len(sp.vs_SoilParameters) == 0 {
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
			jx.arr(a, jx.f(sp.vs_Latitude), jx.sl(""), jx.sl("latitude in decimal degrees")),
		},
		{"Slope", jx.vu(sp.vs_Slope, "m m-1", a)},
		{
			"HeightNN",
			jx.arr(a, jx.f(sp.vs_HeightNN), jx.sl("m"), jx.sl("height above sea level")),
		},
		{"GroundwaterDepth", jx.vu(sp.vs_GroundwaterDepth, "m", a)},
		{"Soil_CN_Ratio", jx.f(sp.vs_Soil_CN_Ratio)},
		{"DrainageCoeff", jx.f(sp.vs_DrainageCoeff)},
		{"NDeposition", jx.vu(sp.vq_NDeposition, "kg N ha-1 y-1", a)},
		{"MaxEffectiveRootingDepth", jx.vu(sp.vs_MaxEffectiveRootingDepth, "m", a)},
		{"ImpenetrableLayerDepth", jx.vu(sp.vs_ImpenetrableLayerDepth, "m", a)},
		{
			"SoilSpecificHumusBalanceCorrection",
			jx.vu(sp.vs_SoilSpecificHumusBalanceCorrection, "humus equivalents", a),
		},
		{"Bare_soil_KC_factor", jx.f(sp.bareSoilKcFactor)},
	)
	oo := o.(jx.Object)
	soil_profile_params := make(jx.Array, 0, len(sp.vs_SoilParameters), a)
	for &sp_item in sp.vs_SoilParameters {
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
	startDate:                          d.Date,
	endDate:                            d.Date,
	pc_NitrogenResponseOn:              bool,
	pc_WaterDeficitResponseOn:          bool,
	pc_EmergenceFloodingControlOn:      bool,
	pc_EmergenceMoistureControlOn:      bool,
	pc_FrostKillOn:                     bool,
	p_UseAutomaticIrrigation:           bool,
	p_AutoIrrigationParams:             Automatic_Irrigation_Parameters,
	p_UseNMinMineralFertilisingMethod:  bool,
	p_NMinFertiliserPartition:          Mineral_Fertilizer_Parameters,
	p_NMinUserParams:                   NMin_Application_Parameters,
	p_UseSecondaryYields:               bool,
	p_UseAutomaticHarvestTrigger:       bool,
	p_NumberOfLayers:                   int,
	p_LayerThickness:                   f64,
	p_StartPVIndex:                     int,
	p_JulianDayAutomaticFertilising:    int,
	serializeMonicaStateAtEnd:          bool,
	serializeMonicaStateAtEndToJson:    bool,
	pathToSerializationAtEndFile:       string,
	loadSerializedMonicaStateAtStart:   bool,
	deserializedMonicaStateFromJson:    bool,
	pathToLoadSerializationFile:        string,
	noOfPreviousDaysSerializedClimateData: u64,
	dualKcMethod:                       bool, // FAO-56 Dual Kc evaporation partitioning
}

// The C++ in-class initialisers
make_simulation_parameters :: proc() -> Simulation_Parameters {
	return Simulation_Parameters {
		pc_NitrogenResponseOn = true,
		pc_WaterDeficitResponseOn = true,
		pc_EmergenceFloodingControlOn = true,
		pc_EmergenceMoistureControlOn = true,
		pc_FrostKillOn = true,
		p_AutoIrrigationParams = make_automatic_irrigation_parameters(),
		p_UseSecondaryYields = true,
		p_NumberOfLayers = 20,
		p_LayerThickness = 0.1,
	}
}

// C++: Errors simulationparameters::merge(SimulationParameters*, Json)
simulation_parameters_merge :: proc(sp: ^Simulation_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(sp, j, simulation_parameters_merge)

	jx.set_iso_date_value(&sp.startDate, j, "startDate")
	jx.set_iso_date_value(&sp.endDate, j, "endDate")

	jx.set_bool_value(&sp.pc_NitrogenResponseOn, j, "NitrogenResponseOn")
	jx.set_bool_value(&sp.pc_WaterDeficitResponseOn, j, "WaterDeficitResponseOn")
	jx.set_bool_value(&sp.pc_EmergenceFloodingControlOn, j, "EmergenceFloodingControlOn")
	jx.set_bool_value(&sp.pc_EmergenceMoistureControlOn, j, "EmergenceMoistureControlOn")
	jx.set_bool_value(&sp.pc_FrostKillOn, j, "FrostKillOn")

	jx.set_bool_value(&sp.p_UseAutomaticIrrigation, j, "UseAutomaticIrrigation")
	// the C++ discards this merge's errors
	_ = automatic_irrigation_parameters_merge(
		&sp.p_AutoIrrigationParams,
		jx.get(j, "AutoIrrigationParams"),
	)

	jx.set_bool_value(&sp.p_UseNMinMineralFertilisingMethod, j, "UseNMinMineralFertilisingMethod")
	_ = mineral_fertilizer_parameters_merge(
		&sp.p_NMinFertiliserPartition,
		jx.get(j, "NMinFertiliserPartition"),
	)
	_ = nmin_application_parameters_merge(&sp.p_NMinUserParams, jx.get(j, "NMinUserParams"))
	jx.set_int_value(&sp.p_JulianDayAutomaticFertilising, j, "JulianDayAutomaticFertilising")

	jx.set_bool_value(&sp.p_UseSecondaryYields, j, "UseSecondaryYields")
	jx.set_bool_value(&sp.p_UseAutomaticHarvestTrigger, j, "UseAutomaticHarvestTrigger")
	jx.set_int_value(&sp.p_NumberOfLayers, j, "NumberOfLayers")
	jx.set_double_value(&sp.p_LayerThickness, j, "LayerThickness")

	jx.set_int_value(&sp.p_StartPVIndex, j, "StartPVIndex")

	ser_state := jx.get(j, "serializedMonicaState")
	if jx.is_object(ser_state) && len(jx.object_items(ser_state)) > 0 {
		load_state := jx.get(ser_state, "load")
		if jx.is_object(load_state) {
			jx.set_bool_value(&sp.loadSerializedMonicaStateAtStart, load_state, "atStart")
			jx.set_bool_value(&sp.deserializedMonicaStateFromJson, load_state, "fromJson")
			jx.set_string_value(&sp.pathToLoadSerializationFile, load_state, "path")
		}
		save_state := jx.get(ser_state, "save")
		if jx.is_object(save_state) {
			jx.set_bool_value(&sp.serializeMonicaStateAtEnd, save_state, "atEnd")
			jx.set_bool_value(&sp.serializeMonicaStateAtEndToJson, save_state, "toJson")
			jx.set_string_value(&sp.pathToSerializationAtEndFile, save_state, "path")
			sp.noOfPreviousDaysSerializedClimateData = u64(
				max(0, jx.int_value(save_state, "noOfPreviousDaysSerializedClimateData")),
			)
		}
	}

	// FAO-56 Dual Kc: "evapotranspiration-method": "FAO-56-Dual" activates the
	// Dual Kc pathway; any other value (or an absent key) keeps single-Kc.
	if jx.string_value_of(jx.get(j, "evapotranspiration-method")) == "FAO-56-Dual" {
		sp.dualKcMethod = true
	}

	return res
}

// C++: json11::Json simulationparameters::to_json(const SimulationParameters*)
simulation_parameters_to_json :: proc(sp: ^Simulation_Parameters, a: Allocator) -> jx.Value {
	start_iso := d.to_iso_date_string(sp.startDate, "", a)
	end_iso := d.to_iso_date_string(sp.endDate, "", a)

	load_o := jx.obj(
		a,
		{"atStart", jx.b(sp.loadSerializedMonicaStateAtStart)},
		{"fromJson", jx.b(sp.deserializedMonicaStateFromJson)},
		{"path", jx.s(sp.pathToLoadSerializationFile, a)},
	)
	save_o := jx.obj(
		a,
		{"atEnd", jx.b(sp.serializeMonicaStateAtEnd)},
		{"toJson", jx.b(sp.serializeMonicaStateAtEndToJson)},
		{"path", jx.s(sp.pathToSerializationAtEndFile, a)},
		{
			"noOfPreviousDaysSerializedClimateData",
			jx.i(int(sp.noOfPreviousDaysSerializedClimateData)),
		},
	)
	ser_o := jx.obj(a, {"load", load_o}, {"save", save_o})

	return jx.obj(
		a,
		{"type", jx.sl("SimulationParameters")},
		{"startDate", jx.s(start_iso, a)},
		{"endDate", jx.s(end_iso, a)},
		{"NitrogenResponseOn", jx.b(sp.pc_NitrogenResponseOn)},
		{"WaterDeficitResponseOn", jx.b(sp.pc_WaterDeficitResponseOn)},
		{"EmergenceFloodingControlOn", jx.b(sp.pc_EmergenceFloodingControlOn)},
		{"EmergenceMoistureControlOn", jx.b(sp.pc_EmergenceMoistureControlOn)},
		{"FrostKillOn", jx.b(sp.pc_FrostKillOn)},
		{"UseAutomaticIrrigation", jx.b(sp.p_UseAutomaticIrrigation)},
		{
			"AutoIrrigationParams",
			automatic_irrigation_parameters_to_json(&sp.p_AutoIrrigationParams, a),
		},
		{"UseNMinMineralFertilisingMethod", jx.b(sp.p_UseNMinMineralFertilisingMethod)},
		{
			"NMinFertiliserPartition",
			mineral_fertilizer_parameters_to_json(&sp.p_NMinFertiliserPartition, a),
		},
		{"NMinUserParams", nmin_application_parameters_to_json(&sp.p_NMinUserParams, a)},
		{"JulianDayAutomaticFertilising", jx.i(sp.p_JulianDayAutomaticFertilising)},
		{"UseSecondaryYields", jx.b(sp.p_UseSecondaryYields)},
		{"UseAutomaticHarvestTrigger", jx.b(sp.p_UseAutomaticHarvestTrigger)},
		{"NumberOfLayers", jx.i(sp.p_NumberOfLayers)},
		{"LayerThickness", jx.f(sp.p_LayerThickness)},
		{"StartPVIndex", jx.i(sp.p_StartPVIndex)},
		{"serializeMonicaStateAtEnd", jx.b(sp.serializeMonicaStateAtEnd)},
		{"serializedMonicaState", ser_o},
		{
			"evapotranspiration-method",
			jx.sl(sp.dualKcMethod ? "FAO-56-Dual" : "Penman-Monteith"),
		},
	)
}

// ---------------------------------------------------------------------------
// CropModuleParameters
// ---------------------------------------------------------------------------

// C++: struct monica::CropModuleParameters
Crop_Module_Parameters :: struct {
	pc_CanopyReflectionCoefficient:      f64,
	pc_ReferenceMaxAssimilationRate:     f64,
	pc_ReferenceLeafAreaIndex:           f64,
	pc_MaintenanceRespirationParameter1: f64,
	pc_MaintenanceRespirationParameter2: f64,
	pc_MinimumNConcentrationRoot:        f64,
	pc_MinimumAvailableN:                f64, // [kg m-2]
	pc_ReferenceAlbedo:                  f64,
	pc_StomataConductanceAlpha:          f64,
	pc_SaturationBeta:                   f64,
	pc_GrowthRespirationRedux:           f64,
	pc_MaxCropNDemand:                   f64,
	pc_GrowthRespirationParameter1:      f64,
	pc_GrowthRespirationParameter2:      f64,
	pc_Tortuosity:                       f64, // old AD
	pc_AdjustRootDepthForSoilProps:      bool,
	pc_TimeUnderAnoxiaThreshold:         [dynamic]int,
	__enable_Phenology_WangEngelTemperatureResponse__:      bool,
	__enable_Photosynthesis_WangEngelTemperatureResponse__: bool,
	__enable_hourly_FvCB_photosynthesis__:                  bool,
	__enable_T_response_leaf_expansion__:                   bool,
	__disable_daily_root_biomass_to_soil__:                 bool,
	__enable_vernalisation_factor_fix__:                    bool,
	__enable_PASW_root_penetration__:                       bool,
	isIntercropping:                     bool,
	sequentialWaterUse:                  bool,
	twoWaySync:                          bool,
	pc_intercropping_k_s:                f64,
	pc_intercropping_k_t:                f64,
	pc_intercropping_phRedux:            f64,
	pc_intercropping_dvs_phr:            f64,
	pc_intercropping_autoPhRedux:        bool,
	pc_intercropping_reader_sr:          string,
	pc_intercropping_writer_sr:          string,
}

// C++ in-class initialisers, incl. pc_TimeUnderAnoxiaThreshold{4,4,4,4,4,4,4}
make_crop_module_parameters :: proc(a: Allocator) -> Crop_Module_Parameters {
	cmp := Crop_Module_Parameters {
		pc_AdjustRootDepthForSoilProps = true,
		twoWaySync                     = true,
		pc_intercropping_phRedux       = 0.5,
		pc_intercropping_dvs_phr       = 5.791262,
		pc_intercropping_autoPhRedux   = true,
	}
	cmp.pc_TimeUnderAnoxiaThreshold = make([dynamic]int, 0, 7, a)
	for _ in 0 ..< 7 {
		append(&cmp.pc_TimeUnderAnoxiaThreshold, 4)
	}
	return cmp
}

// C++: Errors cropmoduleparameters::merge(CropModuleParameters*, Json)
crop_module_parameters_merge :: proc(cmp: ^Crop_Module_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(cmp, j, crop_module_parameters_merge)

	jx.set_double_value(&cmp.pc_CanopyReflectionCoefficient, j, "CanopyReflectionCoefficient")
	jx.set_double_value(&cmp.pc_ReferenceMaxAssimilationRate, j, "ReferenceMaxAssimilationRate")
	jx.set_double_value(&cmp.pc_ReferenceLeafAreaIndex, j, "ReferenceLeafAreaIndex")
	jx.set_double_value(
		&cmp.pc_MaintenanceRespirationParameter1,
		j,
		"MaintenanceRespirationParameter1",
	)
	jx.set_double_value(
		&cmp.pc_MaintenanceRespirationParameter2,
		j,
		"MaintenanceRespirationParameter2",
	)
	jx.set_double_value(&cmp.pc_MinimumNConcentrationRoot, j, "MinimumNConcentrationRoot")
	jx.set_double_value(&cmp.pc_MinimumAvailableN, j, "MinimumAvailableN")
	jx.set_double_value(&cmp.pc_ReferenceAlbedo, j, "ReferenceAlbedo")
	jx.set_double_value(&cmp.pc_StomataConductanceAlpha, j, "StomataConductanceAlpha")
	jx.set_double_value(&cmp.pc_SaturationBeta, j, "SaturationBeta")
	jx.set_double_value(&cmp.pc_GrowthRespirationRedux, j, "GrowthRespirationRedux")
	jx.set_double_value(&cmp.pc_MaxCropNDemand, j, "MaxCropNDemand")
	jx.set_double_value(&cmp.pc_GrowthRespirationParameter1, j, "GrowthRespirationParameter1")
	jx.set_double_value(&cmp.pc_GrowthRespirationParameter2, j, "GrowthRespirationParameter2")
	jx.set_double_value(&cmp.pc_Tortuosity, j, "Tortuosity")
	jx.set_bool_value(&cmp.pc_AdjustRootDepthForSoilProps, j, "AdjustRootDepthForSoilProps")

	// a bare number fills every element; otherwise it is read as a vector
	if jx.is_number(jx.get(j, "TimeUnderAnoxiaThreshold")) {
		v := int(jx.number_value(jx.get(j, "TimeUnderAnoxiaThreshold")))
		for i in 0 ..< len(cmp.pc_TimeUnderAnoxiaThreshold) {
			cmp.pc_TimeUnderAnoxiaThreshold[i] = v
		}
	} else if jx.is_array(jx.get(j, "TimeUnderAnoxiaThreshold")) {
		nv := jx.int_vector_d(jx.get(j, "TimeUnderAnoxiaThreshold"), nil, 0, context.allocator)
		clear(&cmp.pc_TimeUnderAnoxiaThreshold)
		for x in nv {
			append(&cmp.pc_TimeUnderAnoxiaThreshold, x)
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
	jx.set_bool_value(
		&cmp.__enable_PASW_root_penetration__,
		j,
		"__enable_PASW_root_penetration__",
	)

	ic := jx.get(j, "intercropping")
	jx.set_bool_value(&cmp.isIntercropping, ic, "is_intercropping")
	jx.set_bool_value(&cmp.sequentialWaterUse, ic, "sequential_water_use")
	jx.set_bool_value(&cmp.twoWaySync, ic, "two_way_sync")
	jx.set_double_value(&cmp.pc_intercropping_k_s, ic, "k_s")
	jx.set_double_value(&cmp.pc_intercropping_k_t, ic, "k_t")
	jx.set_double_value(&cmp.pc_intercropping_phRedux, ic, "PHredux")
	jx.set_double_value(&cmp.pc_intercropping_dvs_phr, ic, "DVS_PHr")
	jx.set_bool_value(&cmp.pc_intercropping_autoPhRedux, ic, "auto_PHredux")
	jx.set_string_value(&cmp.pc_intercropping_reader_sr, ic, "reader_sr")
	jx.set_string_value(&cmp.pc_intercropping_writer_sr, ic, "writer_sr")

	return res
}

// C++: json11::Json cropmoduleparameters::to_json(const CropModuleParameters*)
//
// NOTE(c++-quirk): to_json omits __enable_PASW_root_penetration__ and the whole
// intercropping block, so a to_json/merge round trip drops them. Reproduced.
crop_module_parameters_to_json :: proc(cmp: ^Crop_Module_Parameters, a: Allocator) -> jx.Value {
	anoxia := make(jx.Array, 0, len(cmp.pc_TimeUnderAnoxiaThreshold), a)
	for v in cmp.pc_TimeUnderAnoxiaThreshold {
		append(&anoxia, jx.i(v))
	}

	return jx.obj(
		a,
		{"type", jx.sl("CropModuleParameters")},
		{"CanopyReflectionCoefficient", jx.f(cmp.pc_CanopyReflectionCoefficient)},
		{"ReferenceMaxAssimilationRate", jx.f(cmp.pc_ReferenceMaxAssimilationRate)},
		{"ReferenceLeafAreaIndex", jx.f(cmp.pc_ReferenceLeafAreaIndex)},
		{"MaintenanceRespirationParameter1", jx.f(cmp.pc_MaintenanceRespirationParameter1)},
		{"MaintenanceRespirationParameter2", jx.f(cmp.pc_MaintenanceRespirationParameter2)},
		{"MinimumNConcentrationRoot", jx.f(cmp.pc_MinimumNConcentrationRoot)},
		{"MinimumAvailableN", jx.f(cmp.pc_MinimumAvailableN)},
		{"ReferenceAlbedo", jx.f(cmp.pc_ReferenceAlbedo)},
		{"StomataConductanceAlpha", jx.f(cmp.pc_StomataConductanceAlpha)},
		{"SaturationBeta", jx.f(cmp.pc_SaturationBeta)},
		{"GrowthRespirationRedux", jx.f(cmp.pc_GrowthRespirationRedux)},
		{"MaxCropNDemand", jx.f(cmp.pc_MaxCropNDemand)},
		{"GrowthRespirationParameter1", jx.f(cmp.pc_GrowthRespirationParameter1)},
		{"GrowthRespirationParameter2", jx.f(cmp.pc_GrowthRespirationParameter2)},
		{"Tortuosity", jx.f(cmp.pc_Tortuosity)},
		{"AdjustRootDepthForSoilProps", jx.b(cmp.pc_AdjustRootDepthForSoilProps)},
		{"TimeUnderAnoxiaThreshold", jx.Value(anoxia)},
		{
			"__enable_Phenology_WangEngelTemperatureResponse__",
			jx.b(cmp.__enable_Phenology_WangEngelTemperatureResponse__),
		},
		{
			"__enable_Photosynthesis_WangEngelTemperatureResponse__",
			jx.b(cmp.__enable_Photosynthesis_WangEngelTemperatureResponse__),
		},
		{
			"__enable_hourly_FvCB_photosynthesis__",
			jx.b(cmp.__enable_hourly_FvCB_photosynthesis__),
		},
		{
			"__enable_T_response_leaf_expansion__",
			jx.b(cmp.__enable_T_response_leaf_expansion__),
		},
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
	p_Albedo:                 f64,
	rcp:                      RCP,
	p_AtmosphericCO2:         f64,
	p_AtmosphericCO2s:        map[int]f64,
	p_AtmosphericO3:          f64,
	p_AtmosphericO3s:         map[int]f64,
	p_WindSpeedHeight:        f64,
	p_LeachingDepth:          f64,
	p_timeStep:               f64,
	p_MaxGroundwaterDepth:    f64,
	p_MinGroundwaterDepth:    f64,
	p_MinGroundwaterDepthMonth: int,
}

// The C++ in-class initialisers
make_environment_parameters :: proc() -> Environment_Parameters {
	return Environment_Parameters {
		p_Albedo = 0.23,
		rcp = .RCP85,
		p_WindSpeedHeight = 2.0,
		p_MaxGroundwaterDepth = 18.0,
		p_MinGroundwaterDepth = 20.0,
		p_MinGroundwaterDepthMonth = 3,
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

	jx.set_double_value(&ep.p_Albedo, j, "Albedo")

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

	jx.set_double_value(&ep.p_AtmosphericCO2, j, "AtmosphericCO2")
	if jx.is_object(jx.get(j, "AtmosphericCO2s")) {
		clear(&ep.p_AtmosphericCO2s)
		if ep.p_AtmosphericCO2s == nil {
			ep.p_AtmosphericCO2s = make(map[int]f64, a)
		}
		for k, v in jx.object_items(jx.get(j, "AtmosphericCO2s")) {
			if year, ok := strconv.parse_int(k); ok {
				ep.p_AtmosphericCO2s[year] = jx.number_value(v)
			}
		}
	}
	jx.set_double_value(&ep.p_AtmosphericO3, j, "AtmosphericO3")
	if jx.is_object(jx.get(j, "AtmosphericO3s")) {
		clear(&ep.p_AtmosphericO3s)
		if ep.p_AtmosphericO3s == nil {
			ep.p_AtmosphericO3s = make(map[int]f64, a)
		}
		for k, v in jx.object_items(jx.get(j, "AtmosphericO3s")) {
			if year, ok := strconv.parse_int(k); ok {
				ep.p_AtmosphericO3s[year] = jx.number_value(v)
			}
		}
	}
	jx.set_double_value(&ep.p_WindSpeedHeight, j, "WindSpeedHeight")
	jx.set_double_value(&ep.p_LeachingDepth, j, "LeachingDepth")
	jx.set_double_value(&ep.p_timeStep, j, "timeStep")
	jx.set_double_value(&ep.p_MaxGroundwaterDepth, j, "MaxGroundwaterDepth")
	jx.set_double_value(&ep.p_MinGroundwaterDepth, j, "MinGroundwaterDepth")
	jx.set_int_value(&ep.p_MinGroundwaterDepthMonth, j, "MinGroundwaterDepthMonth")

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
		{"Albedo", jx.f(ep.p_Albedo)},
		{"rcp", jx.sl(rcp_2_str(ep.rcp))},
		{"AtmosphericCO2", jx.f(ep.p_AtmosphericCO2)},
		{"AtmosphericCO2s", year_map_to_json(ep.p_AtmosphericCO2s, a)},
		{"AtmosphericO3", jx.f(ep.p_AtmosphericO3)},
		{"AtmosphericO3s", year_map_to_json(ep.p_AtmosphericO3s, a)},
		{"WindSpeedHeight", jx.f(ep.p_WindSpeedHeight)},
		{"LeachingDepth", jx.f(ep.p_LeachingDepth)},
		{"timeStep", jx.f(ep.p_timeStep)},
		{"MaxGroundwaterDepth", jx.f(ep.p_MaxGroundwaterDepth)},
		{"MinGroundwaterDepth", jx.f(ep.p_MinGroundwaterDepth)},
		{"MinGroundwaterDepthMonth", jx.i(ep.p_MinGroundwaterDepthMonth)},
	)
}

// ---------------------------------------------------------------------------
// CentralParameterProvider
// ---------------------------------------------------------------------------

// C++: struct monica::CentralParameterProvider
Central_Parameter_Provider :: struct {
	userCropParameters:            Crop_Module_Parameters,
	userEnvironmentParameters:     Environment_Parameters,
	userSoilMoistureParameters:    Soil_Moisture_Module_Parameters,
	userSoilTemperatureParameters: Soil_Temperature_Module_Parameters,
	userSoilTransportParameters:   Soil_Transport_Module_Parameters,
	userSoilOrganicParameters:     Soil_Organic_Module_Parameters,
	simulationParameters:          Simulation_Parameters,
	siteParameters:                Site_Parameters,
	groundwaterInformation:        Measured_Groundwater_Table_Information,
}

make_central_parameter_provider :: proc(a: Allocator) -> Central_Parameter_Provider {
	return Central_Parameter_Provider {
		userCropParameters = make_crop_module_parameters(a),
		userEnvironmentParameters = make_environment_parameters(),
		userSoilTemperatureParameters = make_soil_temperature_module_parameters(),
		userSoilOrganicParameters = make_soil_organic_module_parameters(),
		simulationParameters = make_simulation_parameters(),
		siteParameters = make_site_parameters(),
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
		crop_module_parameters_merge(&cpp.userCropParameters, jx.get(j, "userCropParameters")),
	)
	tl.append_errors(
		&res,
		environment_parameters_merge(
			&cpp.userEnvironmentParameters,
			jx.get(j, "userEnvironmentParameters"),
			a,
		),
	)
	tl.append_errors(
		&res,
		soil_moisture_module_parameters_merge(
			&cpp.userSoilMoistureParameters,
			jx.get(j, "userSoilMoistureParameters"),
		),
	)
	tl.append_errors(
		&res,
		soil_temperature_module_parameters_merge(
			&cpp.userSoilTemperatureParameters,
			jx.get(j, "userSoilTemperatureParameters"),
		),
	)
	tl.append_errors(
		&res,
		soil_transport_module_parameters_merge(
			&cpp.userSoilTransportParameters,
			jx.get(j, "userSoilTransportParameters"),
		),
	)
	tl.append_errors(
		&res,
		soil_organic_module_parameters_merge(
			&cpp.userSoilOrganicParameters,
			jx.get(j, "userSoilOrganicParameters"),
		),
	)
	tl.append_errors(
		&res,
		simulation_parameters_merge(&cpp.simulationParameters, jx.get(j, "simulationParameters")),
	)
	tl.append_errors(
		&res,
		site_parameters_merge(
			&cpp.siteParameters,
			jx.get(j, "siteParameters"),
			path_to_soil_dir,
			a,
		),
	)

	if !jx.is_null(jx.get(j, "groundwaterInformation")) {
		tl.append_errors(
			&res,
			measured_groundwater_table_information_merge(
				&cpp.groundwaterInformation,
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
		{"userCropParameters", crop_module_parameters_to_json(&cpp.userCropParameters, a)},
		{
			"userEnvironmentParameters",
			environment_parameters_to_json(&cpp.userEnvironmentParameters, a),
		},
		{
			"userSoilMoistureParameters",
			soil_moisture_module_parameters_to_json(&cpp.userSoilMoistureParameters, a),
		},
		{
			"userSoilTemperatureParameters",
			soil_temperature_module_parameters_to_json(&cpp.userSoilTemperatureParameters, a),
		},
		{
			"userSoilTransportParameters",
			soil_transport_module_parameters_to_json(&cpp.userSoilTransportParameters, a),
		},
		{
			"userSoilOrganicParameters",
			soil_organic_module_parameters_to_json(&cpp.userSoilOrganicParameters, a),
		},
		{"simulationParameters", simulation_parameters_to_json(&cpp.simulationParameters, a)},
		{"siteParameters", site_parameters_to_json(&cpp.siteParameters, a)},
	)
}

_ :: slice
