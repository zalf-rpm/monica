// Port of the module-parameter structs from src/core/monica-parameters.{h,cpp}:
// SoilMoistureModuleParameters, SoilTemperatureModuleParameters,
// SoilTransportModuleParameters, SticsParameters and
// SoilOrganicModuleParameters, with their merge/to_json.
//
// These are the structs fed by monica-parameters/general/*.json. Field names are
// kept exactly as in the C++ (CONVENTIONS §2); only the procedures are
// snake_cased. Cap'n Proto serialize/deserialize is not ported.
package params

import jx "../../support/jsonx"
import tl "../../support/tools"
import "base:runtime"

Allocator :: runtime.Allocator

// C++: Errors Tools::defaultMerge(json11::Json j, const function<Errors(Json)>& merge)
//
// Parametric rather than a closure table: the C++ lambdas only ever capture the
// target pointer, which is passed explicitly here.
//
// NOTE(c++-quirk): the second branch ASSIGNS to res rather than appending, so a
// value carrying both "DEFAULT" and "=" reports only the "=" errors. Reproduced.
default_merge :: proc(
	target: ^$T,
	j: jx.Value,
	merge: proc(_: ^T, _: jx.Value) -> tl.Errors,
) -> tl.Errors {
	res: tl.Errors
	if jx.is_object(jx.get(j, "DEFAULT")) {
		res = merge(target, jx.get(j, "DEFAULT"))
	}
	if jx.is_object(jx.get(j, "=")) {
		res = merge(target, jx.get(j, "="))
	}
	return res
}

// ---------------------------------------------------------------------------
// SoilMoistureModuleParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SoilMoistureModuleParameters
//
// The C++ also carries a `std::function<double(string,size_t)> getCapillaryRiseRate`
// member; per plan-odin.md prep 2 that callback is not part of the data struct
// here - the capillary-rise lookup is wired up at the call site instead.
Soil_Moisture_Module_Parameters :: struct {
	saturated_hydraulic_conductivity:        f64,
	surface_roughness:                       f64,
	groundwater_discharge:                   f64,
	hydraulic_conductivity_redux:            f64,
	snow_accumulation_threshold_temperature: f64,
	kc_factor:                               f64,
	temperature_limit_for_liquid_water:      f64,
	correction_snow:                         f64,
	correction_rain:                         f64,
	snow_max_additional_density:             f64,
	new_snow_density_min:                    f64,
	snow_retention_capacity_min:             f64,
	refreeze_parameter1:                     f64,
	refreeze_parameter2:                     f64,
	refreeze_temperature:                    f64,
	snow_melt_temperature:                   f64,
	snow_packing:                            f64,
	snow_retention_capacity_max:             f64,
	evaporation_zeta:                        f64,
	xsa_critical_soil_moisture:              f64,
	maximum_evaporation_impact_depth:        f64,
	max_percolation_rate:                    f64,
	moisture_init_value:                     f64,
}
// all C++ defaults are 0.0, so the Odin zero value matches

// C++: Errors soilmoisturemoduleparameters::merge(SoilMoistureModuleParameters*, Json)
soil_moisture_module_parameters_merge :: proc(
	smp: ^Soil_Moisture_Module_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(smp, j, soil_moisture_module_parameters_merge)

	jx.set_double_value(
		&smp.saturated_hydraulic_conductivity,
		j,
		"SaturatedHydraulicConductivity",
	)
	jx.set_double_value(&smp.surface_roughness, j, "SurfaceRoughness")
	jx.set_double_value(&smp.groundwater_discharge, j, "GroundwaterDischarge")
	jx.set_double_value(&smp.hydraulic_conductivity_redux, j, "HydraulicConductivityRedux")
	jx.set_double_value(
		&smp.snow_accumulation_threshold_temperature,
		j,
		"SnowAccumulationTresholdTemperature",
	)
	jx.set_double_value(&smp.kc_factor, j, "KcFactor")
	jx.set_double_value(
		&smp.temperature_limit_for_liquid_water,
		j,
		"TemperatureLimitForLiquidWater",
	)
	jx.set_double_value(&smp.correction_snow, j, "CorrectionSnow")
	jx.set_double_value(&smp.correction_rain, j, "CorrectionRain")
	jx.set_double_value(&smp.snow_max_additional_density, j, "SnowMaxAdditionalDensity")
	jx.set_double_value(&smp.new_snow_density_min, j, "NewSnowDensityMin")
	jx.set_double_value(&smp.snow_retention_capacity_min, j, "SnowRetentionCapacityMin")
	jx.set_double_value(&smp.refreeze_parameter1, j, "RefreezeParameter1")
	jx.set_double_value(&smp.refreeze_parameter2, j, "RefreezeParameter2")
	jx.set_double_value(&smp.refreeze_temperature, j, "RefreezeTemperature")
	jx.set_double_value(&smp.snow_melt_temperature, j, "SnowMeltTemperature")
	jx.set_double_value(&smp.snow_packing, j, "SnowPacking")
	jx.set_double_value(&smp.snow_retention_capacity_max, j, "SnowRetentionCapacityMax")
	jx.set_double_value(&smp.evaporation_zeta, j, "EvaporationZeta")
	jx.set_double_value(&smp.xsa_critical_soil_moisture, j, "XSACriticalSoilMoisture")
	jx.set_double_value(&smp.maximum_evaporation_impact_depth, j, "MaximumEvaporationImpactDepth")
	jx.set_double_value(&smp.max_percolation_rate, j, "MaxPercolationRate")
	jx.set_double_value(&smp.moisture_init_value, j, "MoistureInitValue")

	return res
}

// C++: json11::Json soilmoisturemoduleparameters::to_json(const SoilMoistureModuleParameters*)
soil_moisture_module_parameters_to_json :: proc(
	smp: ^Soil_Moisture_Module_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SoilMoistureModuleParameters")},
		{"SaturatedHydraulicConductivity", jx.f(smp.saturated_hydraulic_conductivity)},
		{"SurfaceRoughness", jx.f(smp.surface_roughness)},
		{"GroundwaterDischarge", jx.f(smp.groundwater_discharge)},
		{"HydraulicConductivityRedux", jx.f(smp.hydraulic_conductivity_redux)},
		{"SnowAccumulationTresholdTemperature", jx.f(smp.snow_accumulation_threshold_temperature)},
		{"KcFactor", jx.f(smp.kc_factor)},
		{"TemperatureLimitForLiquidWater", jx.f(smp.temperature_limit_for_liquid_water)},
		{"CorrectionSnow", jx.f(smp.correction_snow)},
		{"CorrectionRain", jx.f(smp.correction_rain)},
		{"SnowMaxAdditionalDensity", jx.f(smp.snow_max_additional_density)},
		{"NewSnowDensityMin", jx.f(smp.new_snow_density_min)},
		{"SnowRetentionCapacityMin", jx.f(smp.snow_retention_capacity_min)},
		{"RefreezeParameter1", jx.f(smp.refreeze_parameter1)},
		{"RefreezeParameter2", jx.f(smp.refreeze_parameter2)},
		{"RefreezeTemperature", jx.f(smp.refreeze_temperature)},
		{"SnowMeltTemperature", jx.f(smp.snow_melt_temperature)},
		{"SnowPacking", jx.f(smp.snow_packing)},
		{"SnowRetentionCapacityMax", jx.f(smp.snow_retention_capacity_max)},
		{"EvaporationZeta", jx.f(smp.evaporation_zeta)},
		{"XSACriticalSoilMoisture", jx.f(smp.xsa_critical_soil_moisture)},
		{"MaximumEvaporationImpactDepth", jx.f(smp.maximum_evaporation_impact_depth)},
		{"MaxPercolationRate", jx.f(smp.max_percolation_rate)},
		{"MoistureInitValue", jx.f(smp.moisture_init_value)},
	)
}

// ---------------------------------------------------------------------------
// SoilTemperatureModuleParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SoilTemperatureModuleParameters
Soil_Temperature_Module_Parameters :: struct {
	n_tau:                         f64,
	initial_surface_temperature:   f64,
	base_temperature:              f64,
	quartz_raw_density:            f64,
	density_air:                   f64,
	density_water:                 f64,
	density_humus:                 f64,
	specific_heat_capacity_air:    f64,
	specific_heat_capacity_quartz: f64,
	specific_heat_capacity_water:  f64,
	specific_heat_capacity_humus:  f64,
	soil_albedo:                   f64,
	soil_moisture:                 f64, // C++ default 0.25
}

// C++ default: pt_SoilMoisture{0.25}; everything else 0.0
make_soil_temperature_module_parameters :: proc() -> Soil_Temperature_Module_Parameters {
	return Soil_Temperature_Module_Parameters{soil_moisture = 0.25}
}

// C++: Errors soiltemperaturemoduleparameters::merge(...)
soil_temperature_module_parameters_merge :: proc(
	stp: ^Soil_Temperature_Module_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(stp, j, soil_temperature_module_parameters_merge)

	jx.set_double_value(&stp.n_tau, j, "NTau")
	jx.set_double_value(&stp.initial_surface_temperature, j, "InitialSurfaceTemperature")
	jx.set_double_value(&stp.base_temperature, j, "BaseTemperature")
	jx.set_double_value(&stp.quartz_raw_density, j, "QuartzRawDensity")
	jx.set_double_value(&stp.density_air, j, "DensityAir")
	jx.set_double_value(&stp.density_water, j, "DensityWater")
	jx.set_double_value(&stp.density_humus, j, "DensityHumus")
	jx.set_double_value(&stp.specific_heat_capacity_air, j, "SpecificHeatCapacityAir")
	jx.set_double_value(&stp.specific_heat_capacity_quartz, j, "SpecificHeatCapacityQuartz")
	jx.set_double_value(&stp.specific_heat_capacity_water, j, "SpecificHeatCapacityWater")
	jx.set_double_value(&stp.specific_heat_capacity_humus, j, "SpecificHeatCapacityHumus")
	jx.set_double_value(&stp.soil_albedo, j, "SoilAlbedo")
	jx.set_double_value(&stp.soil_moisture, j, "SoilMoisture")

	return res
}

// C++: json11::Json soiltemperaturemoduleparameters::to_json(...)
soil_temperature_module_parameters_to_json :: proc(
	stp: ^Soil_Temperature_Module_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SoilTemperatureModuleParameters")},
		{"NTau", jx.f(stp.n_tau)},
		{"InitialSurfaceTemperature", jx.f(stp.initial_surface_temperature)},
		{"BaseTemperature", jx.f(stp.base_temperature)},
		{"QuartzRawDensity", jx.f(stp.quartz_raw_density)},
		{"DensityAir", jx.f(stp.density_air)},
		{"DensityWater", jx.f(stp.density_water)},
		{"DensityHumus", jx.f(stp.density_humus)},
		{"SpecificHeatCapacityAir", jx.f(stp.specific_heat_capacity_air)},
		{"SpecificHeatCapacityQuartz", jx.f(stp.specific_heat_capacity_quartz)},
		{"SpecificHeatCapacityWater", jx.f(stp.specific_heat_capacity_water)},
		{"SpecificHeatCapacityHumus", jx.f(stp.specific_heat_capacity_humus)},
		{"SoilAlbedo", jx.f(stp.soil_albedo)},
		{"SoilMoisture", jx.f(stp.soil_moisture)},
	)
}

// ---------------------------------------------------------------------------
// SoilTransportModuleParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SoilTransportModuleParameters
Soil_Transport_Module_Parameters :: struct {
	dispersion_length:              f64,
	ad:                             f64,
	diffusion_coefficient_standard: f64,
	n_deposition:                   f64,
}

// C++: Errors soiltransportmoduleparameters::merge(...)
soil_transport_module_parameters_merge :: proc(
	stp: ^Soil_Transport_Module_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(stp, j, soil_transport_module_parameters_merge)

	jx.set_double_value(&stp.dispersion_length, j, "DispersionLength")
	jx.set_double_value(&stp.ad, j, "AD")
	jx.set_double_value(&stp.diffusion_coefficient_standard, j, "DiffusionCoefficientStandard")
	jx.set_double_value(&stp.n_deposition, j, "NDeposition")

	return res
}

// C++: json11::Json soiltransportmoduleparameters::to_json(...)
soil_transport_module_parameters_to_json :: proc(
	stp: ^Soil_Transport_Module_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SoilTransportModuleParameters")},
		{"DispersionLength", jx.f(stp.dispersion_length)},
		{"AD", jx.f(stp.ad)},
		{"DiffusionCoefficientStandard", jx.f(stp.diffusion_coefficient_standard)},
		{"NDeposition", jx.f(stp.n_deposition)},
	)
}

// ---------------------------------------------------------------------------
// SticsParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SticsParameters
Stics_Parameters :: struct {
	use_n2o:                bool,
	use_nit:                bool,
	use_denit:              bool,
	code_vnit:              int,
	code_tnit:              int,
	code_rationit:          int,
	code_hourly_wfps_nit:   int,
	code_pdenit:            int,
	code_ratiodenit:        int,
	code_hourly_wfps_denit: int,
	hminn:                  f64,
	hoptn:                  f64,
	pHminnit:               f64,
	pHmaxnit:               f64,
	nh4_min:                f64, // [mg NH4-N/kg soil]
	pHminden:               f64,
	pHmaxden:               f64,
	wfpsc:                  f64,
	tdenitopt_gauss:        f64, // [°C]
	scale_tdenitopt:        f64, // [°C]
	Kd:                     f64, // [mg NO3-N/L]
	k_desat:                f64, // [1/day]
	fnx:                    f64, // [1/day]
	vnitmax:                f64, // [mg NH4-N/kg soil/day]
	Kamm:                   f64, // [mg NH4-N/L]
	tnitmin:                f64, // [°C]
	tnitopt:                f64, // [°C]
	tnitop2:                f64, // [°C]
	tnitmax:                f64, // [°C]
	tnitopt_gauss:          f64, // [°C]
	scale_tnitopt:          f64, // [°C]
	rationit:               f64,
	cmin_pdenit:            f64, // [% [0-100]]
	cmax_pdenit:            f64, // [% [0-100]]
	min_pdenit:             f64, // [mg N/Kg soil/day]
	max_pdenit:             f64, // [mg N/kg soil/day]
	ratiodenit:             f64,
	profdenit:              f64, // [cm]
	vpotdenit:              f64, // [kg N/ha/day]
}

// The C++ in-class initialisers
make_stics_parameters :: proc() -> Stics_Parameters {
	return Stics_Parameters {
		code_vnit = 1,
		code_tnit = 2,
		code_rationit = 2,
		code_hourly_wfps_nit = 2,
		code_pdenit = 1,
		code_ratiodenit = 2,
		code_hourly_wfps_denit = 2,
		hminn = 0.3,
		hoptn = 0.9,
		pHminnit = 4.0,
		pHmaxnit = 7.2,
		nh4_min = 1.0,
		pHminden = 7.2,
		pHmaxden = 9.2,
		wfpsc = 0.62,
		tdenitopt_gauss = 47,
		scale_tdenitopt = 25,
		Kd = 148,
		k_desat = 3.0,
		fnx = 0.8,
		vnitmax = 27.3,
		Kamm = 24,
		tnitmin = 5.0,
		tnitopt = 30.0,
		tnitop2 = 35.0,
		tnitmax = 58.0,
		tnitopt_gauss = 32.5,
		scale_tnitopt = 16.0,
		rationit = 0.0016,
		cmin_pdenit = 1.0,
		cmax_pdenit = 6.0,
		min_pdenit = 1.0,
		max_pdenit = 20.0,
		ratiodenit = 0.2,
		profdenit = 20,
		vpotdenit = 2.0,
	}
}

// C++: Errors sticsparameters::merge(SticsParameters*, Json)
stics_parameters_merge :: proc(sp: ^Stics_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(sp, j, stics_parameters_merge)

	jx.set_bool_value(&sp.use_n2o, j, "use_n2o")
	jx.set_bool_value(&sp.use_nit, j, "use_nit")
	jx.set_bool_value(&sp.use_denit, j, "use_denit")
	jx.set_int_value(&sp.code_vnit, j, "code_vnit")
	jx.set_int_value(&sp.code_tnit, j, "code_tnit")
	jx.set_int_value(&sp.code_rationit, j, "code_rationit")
	jx.set_int_value(&sp.code_hourly_wfps_nit, j, "code_hourly_wfps_nit")
	jx.set_int_value(&sp.code_pdenit, j, "code_pdenit")
	jx.set_int_value(&sp.code_ratiodenit, j, "code_ratiodenit")
	jx.set_int_value(&sp.code_hourly_wfps_denit, j, "code_hourly_wfps_denit")
	jx.set_double_value(&sp.hminn, j, "hminn")
	jx.set_double_value(&sp.hoptn, j, "hoptn")
	jx.set_double_value(&sp.pHminnit, j, "pHminnit")
	jx.set_double_value(&sp.pHmaxnit, j, "pHmaxnit")
	jx.set_double_value(&sp.nh4_min, j, "nh4_min")
	jx.set_double_value(&sp.pHminden, j, "pHminden")
	jx.set_double_value(&sp.pHmaxden, j, "pHmaxden")
	jx.set_double_value(&sp.wfpsc, j, "wfpsc")
	jx.set_double_value(&sp.tdenitopt_gauss, j, "tdenitopt_gauss")
	jx.set_double_value(&sp.scale_tdenitopt, j, "scale_tdenitopt")
	jx.set_double_value(&sp.Kd, j, "Kd")
	jx.set_double_value(&sp.k_desat, j, "k_desat")
	jx.set_double_value(&sp.fnx, j, "fnx")
	jx.set_double_value(&sp.vnitmax, j, "vnitmax")
	jx.set_double_value(&sp.Kamm, j, "Kamm")
	jx.set_double_value(&sp.tnitmin, j, "tnitmin")
	jx.set_double_value(&sp.tnitopt, j, "tnitopt")
	jx.set_double_value(&sp.tnitop2, j, "tnitop2")
	jx.set_double_value(&sp.tnitmax, j, "tnitmax")
	jx.set_double_value(&sp.tnitopt_gauss, j, "tnitopt_gauss")
	jx.set_double_value(&sp.scale_tnitopt, j, "scale_tnitopt")
	jx.set_double_value(&sp.rationit, j, "rationit")
	jx.set_double_value(&sp.cmin_pdenit, j, "cmin_pdenit")
	jx.set_double_value(&sp.cmax_pdenit, j, "cmax_pdenit")
	jx.set_double_value(&sp.min_pdenit, j, "min_pdenit")
	jx.set_double_value(&sp.max_pdenit, j, "max_pdenit")
	jx.set_double_value(&sp.ratiodenit, j, "ratiodenit")
	jx.set_double_value(&sp.profdenit, j, "profdenit")
	jx.set_double_value(&sp.vpotdenit, j, "vpotdenit")

	return res
}

// C++: json11::Json sticsparameters::to_json(const SticsParameters*)
//
// NOTE(c++-quirk): every numeric field is wrapped as a [value, ""] pair EXCEPT
// vpotdenit, which is emitted bare. Reproduced.
stics_parameters_to_json :: proc(sp: ^Stics_Parameters, a: Allocator) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SticsParameters")},
		{"use_n2o", jx.b(sp.use_n2o)},
		{"use_nit", jx.b(sp.use_nit)},
		{"use_denit", jx.b(sp.use_denit)},
		{"code_vnit", jx.vu_int(sp.code_vnit, "", a)},
		{"code_tnit", jx.vu_int(sp.code_tnit, "", a)},
		{"code_rationit", jx.vu_int(sp.code_rationit, "", a)},
		{"code_hourly_wfps_nit", jx.vu_int(sp.code_hourly_wfps_nit, "", a)},
		{"code_pdenit", jx.vu_int(sp.code_pdenit, "", a)},
		{"code_ratiodenit", jx.vu_int(sp.code_ratiodenit, "", a)},
		{"code_hourly_wfps_denit", jx.vu_int(sp.code_hourly_wfps_denit, "", a)},
		{"hminn", jx.vu(sp.hminn, "", a)},
		{"hoptn", jx.vu(sp.hoptn, "", a)},
		{"pHminnit", jx.vu(sp.pHminnit, "", a)},
		{"pHmaxnit", jx.vu(sp.pHmaxnit, "", a)},
		{"nh4_min", jx.vu(sp.nh4_min, "", a)},
		{"pHminden", jx.vu(sp.pHminden, "", a)},
		{"pHmaxden", jx.vu(sp.pHmaxden, "", a)},
		{"wfpsc", jx.vu(sp.wfpsc, "", a)},
		{"tdenitopt_gauss", jx.vu(sp.tdenitopt_gauss, "", a)},
		{"scale_tdenitopt", jx.vu(sp.scale_tdenitopt, "", a)},
		{"Kd", jx.vu(sp.Kd, "", a)},
		{"k_desat", jx.vu(sp.k_desat, "", a)},
		{"fnx", jx.vu(sp.fnx, "", a)},
		{"vnitmax", jx.vu(sp.vnitmax, "", a)},
		{"Kamm", jx.vu(sp.Kamm, "", a)},
		{"tnitmin", jx.vu(sp.tnitmin, "", a)},
		{"tnitopt", jx.vu(sp.tnitopt, "", a)},
		{"tnitop2", jx.vu(sp.tnitop2, "", a)},
		{"tnitmax", jx.vu(sp.tnitmax, "", a)},
		{"tnitopt_gauss", jx.vu(sp.tnitopt_gauss, "", a)},
		{"scale_tnitopt", jx.vu(sp.scale_tnitopt, "", a)},
		{"rationit", jx.vu(sp.rationit, "", a)},
		{"cmin_pdenit", jx.vu(sp.cmin_pdenit, "", a)},
		{"cmax_pdenit", jx.vu(sp.cmax_pdenit, "", a)},
		{"min_pdenit", jx.vu(sp.min_pdenit, "", a)},
		{"max_pdenit", jx.vu(sp.max_pdenit, "", a)},
		{"ratiodenit", jx.vu(sp.ratiodenit, "", a)},
		{"profdenit", jx.vu(sp.profdenit, "", a)},
		{"vpotdenit", jx.f(sp.vpotdenit)}, // the quirk: bare, not a [value, unit] pair
	)
}

// ---------------------------------------------------------------------------
// SoilOrganicModuleParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SoilOrganicModuleParameters
Soil_Organic_Module_Parameters :: struct {
	som_slow_dec_coeff_standard:            f64,
	som_fast_dec_coeff_standard:            f64,
	smb_slow_maint_rate_standard:           f64,
	smb_fast_maint_rate_standard:           f64,
	smb_slow_death_rate_standard:           f64,
	smb_fast_death_rate_standard:           f64,
	smb_utilization_efficiency:             f64,
	som_slow_utilization_efficiency:        f64,
	som_fast_utilization_efficiency:        f64,
	aom_slow_utilization_efficiency:        f64,
	aom_fast_utilization_efficiency:        f64,
	aom_fast_max_c_to_n:                    f64,
	part_som_fast_to_som_slow:              f64,
	part_smb_slow_to_som_fast:              f64,
	part_smb_fast_to_som_fast:              f64,
	part_som_to_smb_slow:                   f64,
	part_som_to_smb_fast:                   f64,
	cn_ratio_smb:                           f64,
	limit_clay_effect:                      f64,
	q_ten_factor:                           f64,
	temp_dec_optimal:                       f64,
	moisture_dec_optimal:                   f64,
	ammonia_oxidation_rate_coeff_standard:  f64,
	nitrite_oxidation_rate_coeff_standard:  f64,
	transport_rate_coeff:                   f64,
	spec_anaerob_denitrification:           f64,
	immobilisation_rate_coeff_no3:          f64,
	immobilisation_rate_coeff_nh4:          f64,
	denit1:                                 f64,
	denit2:                                 f64,
	denit3:                                 f64,
	hydrolysis_km:                          f64,
	activation_energy:                      f64,
	hydrolysis_p1:                          f64,
	hydrolysis_p2:                          f64,
	atmospheric_resistance:                 f64,
	n2o_production_rate:                    f64,
	inhibitor_nh3:                          f64,
	max_mineralisation_depth:               f64,
	__enable_kaiteew_TempOnDecompostion__:  bool,
	__enable_kaiteew_MoistOnDecompostion__: bool,
	__enable_kaiteew_ClayOnDecompostion__:  bool,
	stics_params:                           Stics_Parameters,
}

// The C++ in-class initialisers
make_soil_organic_module_parameters :: proc() -> Soil_Organic_Module_Parameters {
	return Soil_Organic_Module_Parameters {
		som_slow_dec_coeff_standard = 4.30e-5,
		som_fast_dec_coeff_standard = 1.40e-4,
		smb_slow_maint_rate_standard = 1.00e-3,
		smb_fast_maint_rate_standard = 1.00e-2,
		smb_slow_death_rate_standard = 1.00e-3,
		smb_fast_death_rate_standard = 1.00e-2,
		smb_utilization_efficiency = 0.60,
		som_slow_utilization_efficiency = 0.40,
		som_fast_utilization_efficiency = 0.50,
		aom_slow_utilization_efficiency = 0.40,
		aom_fast_utilization_efficiency = 0.10,
		aom_fast_max_c_to_n = 1000.0,
		part_som_fast_to_som_slow = 0.30,
		part_smb_slow_to_som_fast = 0.60,
		part_smb_fast_to_som_fast = 0.60,
		part_som_to_smb_slow = 0.0150,
		part_som_to_smb_fast = 0.0002,
		cn_ratio_smb = 6.70,
		limit_clay_effect = 0.25,
		q_ten_factor = 2.9,
		temp_dec_optimal = 38,
		moisture_dec_optimal = 0.45,
		ammonia_oxidation_rate_coeff_standard = 1.0e-1,
		nitrite_oxidation_rate_coeff_standard = 9.0e-1,
		transport_rate_coeff = 0.1,
		spec_anaerob_denitrification = 0.1,
		immobilisation_rate_coeff_no3 = 0.5,
		immobilisation_rate_coeff_nh4 = 0.5,
		denit1 = 0.2,
		denit2 = 0.8,
		denit3 = 0.9,
		hydrolysis_km = 0.00334,
		activation_energy = 41000.0,
		hydrolysis_p1 = 4.259e-12,
		hydrolysis_p2 = 1.408e-12,
		atmospheric_resistance = 0.0025,
		n2o_production_rate = 0.5,
		inhibitor_nh3 = 1.0,
		max_mineralisation_depth = 0.4,
		__enable_kaiteew_TempOnDecompostion__ = true,
		__enable_kaiteew_MoistOnDecompostion__ = true,
		__enable_kaiteew_ClayOnDecompostion__ = true,
		stics_params = make_stics_parameters(),
	}
}

// C++: Errors soilorganicmoduleparameters::merge(SoilOrganicModuleParameters*, Json)
soil_organic_module_parameters_merge :: proc(
	sop: ^Soil_Organic_Module_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(sop, j, soil_organic_module_parameters_merge)

	jx.set_double_value(&sop.som_slow_dec_coeff_standard, j, "SOM_SlowDecCoeffStandard")
	jx.set_double_value(&sop.som_fast_dec_coeff_standard, j, "SOM_FastDecCoeffStandard")
	jx.set_double_value(&sop.smb_slow_maint_rate_standard, j, "SMB_SlowMaintRateStandard")
	jx.set_double_value(&sop.smb_fast_maint_rate_standard, j, "SMB_FastMaintRateStandard")
	jx.set_double_value(&sop.smb_slow_death_rate_standard, j, "SMB_SlowDeathRateStandard")
	jx.set_double_value(&sop.smb_fast_death_rate_standard, j, "SMB_FastDeathRateStandard")
	jx.set_double_value(&sop.smb_utilization_efficiency, j, "SMB_UtilizationEfficiency")
	jx.set_double_value(&sop.som_slow_utilization_efficiency, j, "SOM_SlowUtilizationEfficiency")
	jx.set_double_value(&sop.som_fast_utilization_efficiency, j, "SOM_FastUtilizationEfficiency")
	jx.set_double_value(&sop.aom_slow_utilization_efficiency, j, "AOM_SlowUtilizationEfficiency")
	jx.set_double_value(&sop.aom_fast_utilization_efficiency, j, "AOM_FastUtilizationEfficiency")
	jx.set_double_value(&sop.aom_fast_max_c_to_n, j, "AOM_FastMaxC_to_N")
	jx.set_double_value(&sop.part_som_fast_to_som_slow, j, "PartSOM_Fast_to_SOM_Slow")
	jx.set_double_value(&sop.part_smb_slow_to_som_fast, j, "PartSMB_Slow_to_SOM_Fast")
	jx.set_double_value(&sop.part_smb_fast_to_som_fast, j, "PartSMB_Fast_to_SOM_Fast")
	jx.set_double_value(&sop.part_som_to_smb_slow, j, "PartSOM_to_SMB_Slow")
	jx.set_double_value(&sop.part_som_to_smb_fast, j, "PartSOM_to_SMB_Fast")
	jx.set_double_value(&sop.cn_ratio_smb, j, "CN_Ratio_SMB")
	jx.set_double_value(&sop.limit_clay_effect, j, "LimitClayEffect")
	jx.set_double_value(&sop.q_ten_factor, j, "QTenFactor")
	jx.set_double_value(&sop.temp_dec_optimal, j, "TempDecOptimal")
	jx.set_double_value(&sop.moisture_dec_optimal, j, "MoistureDecOptimal")
	jx.set_double_value(
		&sop.ammonia_oxidation_rate_coeff_standard,
		j,
		"AmmoniaOxidationRateCoeffStandard",
	)
	jx.set_double_value(
		&sop.nitrite_oxidation_rate_coeff_standard,
		j,
		"NitriteOxidationRateCoeffStandard",
	)
	jx.set_double_value(&sop.transport_rate_coeff, j, "TransportRateCoeff")
	jx.set_double_value(&sop.spec_anaerob_denitrification, j, "SpecAnaerobDenitrification")
	jx.set_double_value(&sop.immobilisation_rate_coeff_no3, j, "ImmobilisationRateCoeffNO3")
	jx.set_double_value(&sop.immobilisation_rate_coeff_nh4, j, "ImmobilisationRateCoeffNH4")
	jx.set_double_value(&sop.denit1, j, "Denit1")
	jx.set_double_value(&sop.denit2, j, "Denit2")
	jx.set_double_value(&sop.denit3, j, "Denit3")
	jx.set_double_value(&sop.hydrolysis_km, j, "HydrolysisKM")
	jx.set_double_value(&sop.activation_energy, j, "ActivationEnergy")
	jx.set_double_value(&sop.hydrolysis_p1, j, "HydrolysisP1")
	jx.set_double_value(&sop.hydrolysis_p2, j, "HydrolysisP2")
	jx.set_double_value(&sop.atmospheric_resistance, j, "AtmosphericResistance")
	jx.set_double_value(&sop.n2o_production_rate, j, "N2OProductionRate")
	jx.set_double_value(&sop.inhibitor_nh3, j, "Inhibitor_NH3")
	jx.set_double_value(&sop.max_mineralisation_depth, j, "MaxMineralisationDepth")

	jx.set_bool_value(
		&sop.__enable_kaiteew_TempOnDecompostion__,
		j,
		"__enable_kaiteew_TempOnDecompostion__",
	)
	jx.set_bool_value(
		&sop.__enable_kaiteew_MoistOnDecompostion__,
		j,
		"__enable_kaiteew_MoistOnDecompostion__",
	)
	jx.set_bool_value(
		&sop.__enable_kaiteew_ClayOnDecompostion__,
		j,
		"__enable_kaiteew_ClayOnDecompostion__",
	)

	if jx.is_object(jx.get(j, "stics")) {
		e := stics_parameters_merge(&sop.stics_params, jx.get(j, "stics"))
		tl.append_errors(&res, e)
	}

	return res
}

// C++: json11::Json soilorganicmoduleparameters::to_json(const SoilOrganicModuleParameters*)
//
// NOTE(c++-quirk): to_json is lossy - it emits neither the three
// __enable_kaiteew_*__ flags nor the nested stics_params, so a to_json/merge
// round trip does not preserve them. Reproduced.
soil_organic_module_parameters_to_json :: proc(
	sop: ^Soil_Organic_Module_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SoilOrganicModuleParameters")},
		{"SOM_SlowDecCoeffStandard", jx.vu(sop.som_slow_dec_coeff_standard, "d-1", a)},
		{"SOM_FastDecCoeffStandard", jx.vu(sop.som_fast_dec_coeff_standard, "d-1", a)},
		{"SMB_SlowMaintRateStandard", jx.vu(sop.smb_slow_maint_rate_standard, "d-1", a)},
		{"SMB_FastMaintRateStandard", jx.vu(sop.smb_fast_maint_rate_standard, "d-1", a)},
		{"SMB_SlowDeathRateStandard", jx.vu(sop.smb_slow_death_rate_standard, "d-1", a)},
		{"SMB_FastDeathRateStandard", jx.vu(sop.smb_fast_death_rate_standard, "d-1", a)},
		{"SMB_UtilizationEfficiency", jx.vu(sop.smb_utilization_efficiency, "d-1", a)},
		{"SOM_SlowUtilizationEfficiency", jx.vu(sop.som_slow_utilization_efficiency, "", a)},
		{"SOM_FastUtilizationEfficiency", jx.vu(sop.som_fast_utilization_efficiency, "", a)},
		{"AOM_SlowUtilizationEfficiency", jx.vu(sop.aom_slow_utilization_efficiency, "", a)},
		{"AOM_FastUtilizationEfficiency", jx.vu(sop.aom_fast_utilization_efficiency, "", a)},
		{"AOM_FastMaxC_to_N", jx.vu(sop.aom_fast_max_c_to_n, "", a)},
		{"PartSOM_Fast_to_SOM_Slow", jx.vu(sop.part_som_fast_to_som_slow, "", a)},
		{"PartSMB_Slow_to_SOM_Fast", jx.vu(sop.part_smb_slow_to_som_fast, "", a)},
		{"PartSMB_Fast_to_SOM_Fast", jx.vu(sop.part_smb_fast_to_som_fast, "", a)},
		{"PartSOM_to_SMB_Slow", jx.vu(sop.part_som_to_smb_slow, "", a)},
		{"PartSOM_to_SMB_Fast", jx.vu(sop.part_som_to_smb_fast, "", a)},
		{"CN_Ratio_SMB", jx.vu(sop.cn_ratio_smb, "", a)},
		{"LimitClayEffect", jx.vu(sop.limit_clay_effect, "kg kg-1", a)},
		{"QTenFactor", jx.vu(sop.q_ten_factor, "", a)},
		{"TempDecOptimal", jx.vu(sop.temp_dec_optimal, "°C", a)},
		{"MoistureDecOptimal", jx.vu(sop.moisture_dec_optimal, "%", a)},
		{
			"AmmoniaOxidationRateCoeffStandard",
			jx.vu(sop.ammonia_oxidation_rate_coeff_standard, "d-1", a),
		},
		{
			"NitriteOxidationRateCoeffStandard",
			jx.vu(sop.nitrite_oxidation_rate_coeff_standard, "d-1", a),
		},
		{"TransportRateCoeff", jx.vu(sop.transport_rate_coeff, "d-1", a)},
		{
			"SpecAnaerobDenitrification",
			jx.vu(sop.spec_anaerob_denitrification, "g gas-N g CO2-C-1", a),
		},
		{"ImmobilisationRateCoeffNO3", jx.vu(sop.immobilisation_rate_coeff_no3, "d-1", a)},
		{"ImmobilisationRateCoeffNH4", jx.vu(sop.immobilisation_rate_coeff_nh4, "d-1", a)},
		{"Denit1", jx.vu(sop.denit1, "", a)},
		{"Denit2", jx.vu(sop.denit2, "", a)},
		{"Denit3", jx.vu(sop.denit3, "", a)},
		{"HydrolysisKM", jx.vu(sop.hydrolysis_km, "", a)},
		{"ActivationEnergy", jx.vu(sop.activation_energy, "", a)},
		{"HydrolysisP1", jx.vu(sop.hydrolysis_p1, "", a)},
		{"HydrolysisP2", jx.vu(sop.hydrolysis_p2, "", a)},
		{"AtmosphericResistance", jx.vu(sop.atmospheric_resistance, "s m-1", a)},
		{"N2OProductionRate", jx.vu(sop.n2o_production_rate, "d-1", a)},
		{"Inhibitor_NH3", jx.vu(sop.inhibitor_nh3, "kg N m-3", a)},
		{"MaxMineralisationDepth", jx.f(sop.max_mineralisation_depth)},
	)
}
