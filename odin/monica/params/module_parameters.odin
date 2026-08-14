// Port of the module-parameter structs from src/core/monica-parameters.{h,cpp}:
// SoilMoistureModuleParameters, SoilTemperatureModuleParameters,
// SoilTransportModuleParameters, SticsParameters and
// SoilOrganicModuleParameters, with their merge/to_json.
//
// These are the structs fed by monica-parameters/general/*.json. Field names are
// kept exactly as in the C++ (CONVENTIONS §2); only the procedures are
// snake_cased. Cap'n Proto serialize/deserialize is not ported.
package params

import "base:runtime"
import jx "../../support/jsonx"
import tl "../../support/tools"

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
	pm_SaturatedHydraulicConductivity:     f64,
	pm_SurfaceRoughness:                   f64,
	pm_GroundwaterDischarge:               f64,
	pm_HydraulicConductivityRedux:         f64,
	pm_SnowAccumulationTresholdTemperature: f64,
	pm_KcFactor:                           f64,
	pm_TemperatureLimitForLiquidWater:     f64,
	pm_CorrectionSnow:                     f64,
	pm_CorrectionRain:                     f64,
	pm_SnowMaxAdditionalDensity:           f64,
	pm_NewSnowDensityMin:                  f64,
	pm_SnowRetentionCapacityMin:           f64,
	pm_RefreezeParameter1:                 f64,
	pm_RefreezeParameter2:                 f64,
	pm_RefreezeTemperature:                f64,
	pm_SnowMeltTemperature:                f64,
	pm_SnowPacking:                        f64,
	pm_SnowRetentionCapacityMax:           f64,
	pm_EvaporationZeta:                    f64,
	pm_XSACriticalSoilMoisture:            f64,
	pm_MaximumEvaporationImpactDepth:      f64,
	pm_MaxPercolationRate:                 f64,
	pm_MoistureInitValue:                  f64,
}
// all C++ defaults are 0.0, so the Odin zero value matches

// C++: Errors soilmoisturemoduleparameters::merge(SoilMoistureModuleParameters*, Json)
soil_moisture_module_parameters_merge :: proc(
	smp: ^Soil_Moisture_Module_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(smp, j, soil_moisture_module_parameters_merge)

	jx.set_double_value(&smp.pm_SaturatedHydraulicConductivity, j, "SaturatedHydraulicConductivity")
	jx.set_double_value(&smp.pm_SurfaceRoughness, j, "SurfaceRoughness")
	jx.set_double_value(&smp.pm_GroundwaterDischarge, j, "GroundwaterDischarge")
	jx.set_double_value(&smp.pm_HydraulicConductivityRedux, j, "HydraulicConductivityRedux")
	jx.set_double_value(
		&smp.pm_SnowAccumulationTresholdTemperature,
		j,
		"SnowAccumulationTresholdTemperature",
	)
	jx.set_double_value(&smp.pm_KcFactor, j, "KcFactor")
	jx.set_double_value(&smp.pm_TemperatureLimitForLiquidWater, j, "TemperatureLimitForLiquidWater")
	jx.set_double_value(&smp.pm_CorrectionSnow, j, "CorrectionSnow")
	jx.set_double_value(&smp.pm_CorrectionRain, j, "CorrectionRain")
	jx.set_double_value(&smp.pm_SnowMaxAdditionalDensity, j, "SnowMaxAdditionalDensity")
	jx.set_double_value(&smp.pm_NewSnowDensityMin, j, "NewSnowDensityMin")
	jx.set_double_value(&smp.pm_SnowRetentionCapacityMin, j, "SnowRetentionCapacityMin")
	jx.set_double_value(&smp.pm_RefreezeParameter1, j, "RefreezeParameter1")
	jx.set_double_value(&smp.pm_RefreezeParameter2, j, "RefreezeParameter2")
	jx.set_double_value(&smp.pm_RefreezeTemperature, j, "RefreezeTemperature")
	jx.set_double_value(&smp.pm_SnowMeltTemperature, j, "SnowMeltTemperature")
	jx.set_double_value(&smp.pm_SnowPacking, j, "SnowPacking")
	jx.set_double_value(&smp.pm_SnowRetentionCapacityMax, j, "SnowRetentionCapacityMax")
	jx.set_double_value(&smp.pm_EvaporationZeta, j, "EvaporationZeta")
	jx.set_double_value(&smp.pm_XSACriticalSoilMoisture, j, "XSACriticalSoilMoisture")
	jx.set_double_value(&smp.pm_MaximumEvaporationImpactDepth, j, "MaximumEvaporationImpactDepth")
	jx.set_double_value(&smp.pm_MaxPercolationRate, j, "MaxPercolationRate")
	jx.set_double_value(&smp.pm_MoistureInitValue, j, "MoistureInitValue")

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
		{"SaturatedHydraulicConductivity", jx.f(smp.pm_SaturatedHydraulicConductivity)},
		{"SurfaceRoughness", jx.f(smp.pm_SurfaceRoughness)},
		{"GroundwaterDischarge", jx.f(smp.pm_GroundwaterDischarge)},
		{"HydraulicConductivityRedux", jx.f(smp.pm_HydraulicConductivityRedux)},
		{"SnowAccumulationTresholdTemperature", jx.f(smp.pm_SnowAccumulationTresholdTemperature)},
		{"KcFactor", jx.f(smp.pm_KcFactor)},
		{"TemperatureLimitForLiquidWater", jx.f(smp.pm_TemperatureLimitForLiquidWater)},
		{"CorrectionSnow", jx.f(smp.pm_CorrectionSnow)},
		{"CorrectionRain", jx.f(smp.pm_CorrectionRain)},
		{"SnowMaxAdditionalDensity", jx.f(smp.pm_SnowMaxAdditionalDensity)},
		{"NewSnowDensityMin", jx.f(smp.pm_NewSnowDensityMin)},
		{"SnowRetentionCapacityMin", jx.f(smp.pm_SnowRetentionCapacityMin)},
		{"RefreezeParameter1", jx.f(smp.pm_RefreezeParameter1)},
		{"RefreezeParameter2", jx.f(smp.pm_RefreezeParameter2)},
		{"RefreezeTemperature", jx.f(smp.pm_RefreezeTemperature)},
		{"SnowMeltTemperature", jx.f(smp.pm_SnowMeltTemperature)},
		{"SnowPacking", jx.f(smp.pm_SnowPacking)},
		{"SnowRetentionCapacityMax", jx.f(smp.pm_SnowRetentionCapacityMax)},
		{"EvaporationZeta", jx.f(smp.pm_EvaporationZeta)},
		{"XSACriticalSoilMoisture", jx.f(smp.pm_XSACriticalSoilMoisture)},
		{"MaximumEvaporationImpactDepth", jx.f(smp.pm_MaximumEvaporationImpactDepth)},
		{"MaxPercolationRate", jx.f(smp.pm_MaxPercolationRate)},
		{"MoistureInitValue", jx.f(smp.pm_MoistureInitValue)},
	)
}

// ---------------------------------------------------------------------------
// SoilTemperatureModuleParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SoilTemperatureModuleParameters
Soil_Temperature_Module_Parameters :: struct {
	pt_NTau:                     f64,
	pt_InitialSurfaceTemperature: f64,
	pt_BaseTemperature:          f64,
	pt_QuartzRawDensity:         f64,
	pt_DensityAir:               f64,
	pt_DensityWater:             f64,
	pt_DensityHumus:             f64,
	pt_SpecificHeatCapacityAir:  f64,
	pt_SpecificHeatCapacityQuartz: f64,
	pt_SpecificHeatCapacityWater: f64,
	pt_SpecificHeatCapacityHumus: f64,
	pt_SoilAlbedo:               f64,
	pt_SoilMoisture:             f64, // C++ default 0.25
}

// C++ default: pt_SoilMoisture{0.25}; everything else 0.0
make_soil_temperature_module_parameters :: proc() -> Soil_Temperature_Module_Parameters {
	return Soil_Temperature_Module_Parameters{pt_SoilMoisture = 0.25}
}

// C++: Errors soiltemperaturemoduleparameters::merge(...)
soil_temperature_module_parameters_merge :: proc(
	stp: ^Soil_Temperature_Module_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(stp, j, soil_temperature_module_parameters_merge)

	jx.set_double_value(&stp.pt_NTau, j, "NTau")
	jx.set_double_value(&stp.pt_InitialSurfaceTemperature, j, "InitialSurfaceTemperature")
	jx.set_double_value(&stp.pt_BaseTemperature, j, "BaseTemperature")
	jx.set_double_value(&stp.pt_QuartzRawDensity, j, "QuartzRawDensity")
	jx.set_double_value(&stp.pt_DensityAir, j, "DensityAir")
	jx.set_double_value(&stp.pt_DensityWater, j, "DensityWater")
	jx.set_double_value(&stp.pt_DensityHumus, j, "DensityHumus")
	jx.set_double_value(&stp.pt_SpecificHeatCapacityAir, j, "SpecificHeatCapacityAir")
	jx.set_double_value(&stp.pt_SpecificHeatCapacityQuartz, j, "SpecificHeatCapacityQuartz")
	jx.set_double_value(&stp.pt_SpecificHeatCapacityWater, j, "SpecificHeatCapacityWater")
	jx.set_double_value(&stp.pt_SpecificHeatCapacityHumus, j, "SpecificHeatCapacityHumus")
	jx.set_double_value(&stp.pt_SoilAlbedo, j, "SoilAlbedo")
	jx.set_double_value(&stp.pt_SoilMoisture, j, "SoilMoisture")

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
		{"NTau", jx.f(stp.pt_NTau)},
		{"InitialSurfaceTemperature", jx.f(stp.pt_InitialSurfaceTemperature)},
		{"BaseTemperature", jx.f(stp.pt_BaseTemperature)},
		{"QuartzRawDensity", jx.f(stp.pt_QuartzRawDensity)},
		{"DensityAir", jx.f(stp.pt_DensityAir)},
		{"DensityWater", jx.f(stp.pt_DensityWater)},
		{"DensityHumus", jx.f(stp.pt_DensityHumus)},
		{"SpecificHeatCapacityAir", jx.f(stp.pt_SpecificHeatCapacityAir)},
		{"SpecificHeatCapacityQuartz", jx.f(stp.pt_SpecificHeatCapacityQuartz)},
		{"SpecificHeatCapacityWater", jx.f(stp.pt_SpecificHeatCapacityWater)},
		{"SpecificHeatCapacityHumus", jx.f(stp.pt_SpecificHeatCapacityHumus)},
		{"SoilAlbedo", jx.f(stp.pt_SoilAlbedo)},
		{"SoilMoisture", jx.f(stp.pt_SoilMoisture)},
	)
}

// ---------------------------------------------------------------------------
// SoilTransportModuleParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SoilTransportModuleParameters
Soil_Transport_Module_Parameters :: struct {
	pq_DispersionLength:             f64,
	pq_AD:                           f64,
	pq_DiffusionCoefficientStandard: f64,
	pq_NDeposition:                  f64,
}

// C++: Errors soiltransportmoduleparameters::merge(...)
soil_transport_module_parameters_merge :: proc(
	stp: ^Soil_Transport_Module_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(stp, j, soil_transport_module_parameters_merge)

	jx.set_double_value(&stp.pq_DispersionLength, j, "DispersionLength")
	jx.set_double_value(&stp.pq_AD, j, "AD")
	jx.set_double_value(&stp.pq_DiffusionCoefficientStandard, j, "DiffusionCoefficientStandard")
	jx.set_double_value(&stp.pq_NDeposition, j, "NDeposition")

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
		{"DispersionLength", jx.f(stp.pq_DispersionLength)},
		{"AD", jx.f(stp.pq_AD)},
		{"DiffusionCoefficientStandard", jx.f(stp.pq_DiffusionCoefficientStandard)},
		{"NDeposition", jx.f(stp.pq_NDeposition)},
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
	po_SOM_SlowDecCoeffStandard:          f64,
	po_SOM_FastDecCoeffStandard:          f64,
	po_SMB_SlowMaintRateStandard:         f64,
	po_SMB_FastMaintRateStandard:         f64,
	po_SMB_SlowDeathRateStandard:         f64,
	po_SMB_FastDeathRateStandard:         f64,
	po_SMB_UtilizationEfficiency:         f64,
	po_SOM_SlowUtilizationEfficiency:     f64,
	po_SOM_FastUtilizationEfficiency:     f64,
	po_AOM_SlowUtilizationEfficiency:     f64,
	po_AOM_FastUtilizationEfficiency:     f64,
	po_AOM_FastMaxC_to_N:                 f64,
	po_PartSOM_Fast_to_SOM_Slow:          f64,
	po_PartSMB_Slow_to_SOM_Fast:          f64,
	po_PartSMB_Fast_to_SOM_Fast:          f64,
	po_PartSOM_to_SMB_Slow:               f64,
	po_PartSOM_to_SMB_Fast:               f64,
	po_CN_Ratio_SMB:                      f64,
	po_LimitClayEffect:                   f64,
	po_QTenFactor:                        f64,
	po_TempDecOptimal:                    f64,
	po_MoistureDecOptimal:                f64,
	po_AmmoniaOxidationRateCoeffStandard: f64,
	po_NitriteOxidationRateCoeffStandard: f64,
	po_TransportRateCoeff:                f64,
	po_SpecAnaerobDenitrification:        f64,
	po_ImmobilisationRateCoeffNO3:        f64,
	po_ImmobilisationRateCoeffNH4:        f64,
	po_Denit1:                            f64,
	po_Denit2:                            f64,
	po_Denit3:                            f64,
	po_HydrolysisKM:                      f64,
	po_ActivationEnergy:                  f64,
	po_HydrolysisP1:                      f64,
	po_HydrolysisP2:                      f64,
	po_AtmosphericResistance:             f64,
	po_N2OProductionRate:                 f64,
	po_Inhibitor_NH3:                     f64,
	ps_MaxMineralisationDepth:            f64,
	__enable_kaiteew_TempOnDecompostion__:  bool,
	__enable_kaiteew_MoistOnDecompostion__: bool,
	__enable_kaiteew_ClayOnDecompostion__:  bool,
	sticsParams:                          Stics_Parameters,
}

// The C++ in-class initialisers
make_soil_organic_module_parameters :: proc() -> Soil_Organic_Module_Parameters {
	return Soil_Organic_Module_Parameters {
		po_SOM_SlowDecCoeffStandard = 4.30e-5,
		po_SOM_FastDecCoeffStandard = 1.40e-4,
		po_SMB_SlowMaintRateStandard = 1.00e-3,
		po_SMB_FastMaintRateStandard = 1.00e-2,
		po_SMB_SlowDeathRateStandard = 1.00e-3,
		po_SMB_FastDeathRateStandard = 1.00e-2,
		po_SMB_UtilizationEfficiency = 0.60,
		po_SOM_SlowUtilizationEfficiency = 0.40,
		po_SOM_FastUtilizationEfficiency = 0.50,
		po_AOM_SlowUtilizationEfficiency = 0.40,
		po_AOM_FastUtilizationEfficiency = 0.10,
		po_AOM_FastMaxC_to_N = 1000.0,
		po_PartSOM_Fast_to_SOM_Slow = 0.30,
		po_PartSMB_Slow_to_SOM_Fast = 0.60,
		po_PartSMB_Fast_to_SOM_Fast = 0.60,
		po_PartSOM_to_SMB_Slow = 0.0150,
		po_PartSOM_to_SMB_Fast = 0.0002,
		po_CN_Ratio_SMB = 6.70,
		po_LimitClayEffect = 0.25,
		po_QTenFactor = 2.9,
		po_TempDecOptimal = 38,
		po_MoistureDecOptimal = 0.45,
		po_AmmoniaOxidationRateCoeffStandard = 1.0e-1,
		po_NitriteOxidationRateCoeffStandard = 9.0e-1,
		po_TransportRateCoeff = 0.1,
		po_SpecAnaerobDenitrification = 0.1,
		po_ImmobilisationRateCoeffNO3 = 0.5,
		po_ImmobilisationRateCoeffNH4 = 0.5,
		po_Denit1 = 0.2,
		po_Denit2 = 0.8,
		po_Denit3 = 0.9,
		po_HydrolysisKM = 0.00334,
		po_ActivationEnergy = 41000.0,
		po_HydrolysisP1 = 4.259e-12,
		po_HydrolysisP2 = 1.408e-12,
		po_AtmosphericResistance = 0.0025,
		po_N2OProductionRate = 0.5,
		po_Inhibitor_NH3 = 1.0,
		ps_MaxMineralisationDepth = 0.4,
		__enable_kaiteew_TempOnDecompostion__ = true,
		__enable_kaiteew_MoistOnDecompostion__ = true,
		__enable_kaiteew_ClayOnDecompostion__ = true,
		sticsParams = make_stics_parameters(),
	}
}

// C++: Errors soilorganicmoduleparameters::merge(SoilOrganicModuleParameters*, Json)
soil_organic_module_parameters_merge :: proc(
	sop: ^Soil_Organic_Module_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(sop, j, soil_organic_module_parameters_merge)

	jx.set_double_value(&sop.po_SOM_SlowDecCoeffStandard, j, "SOM_SlowDecCoeffStandard")
	jx.set_double_value(&sop.po_SOM_FastDecCoeffStandard, j, "SOM_FastDecCoeffStandard")
	jx.set_double_value(&sop.po_SMB_SlowMaintRateStandard, j, "SMB_SlowMaintRateStandard")
	jx.set_double_value(&sop.po_SMB_FastMaintRateStandard, j, "SMB_FastMaintRateStandard")
	jx.set_double_value(&sop.po_SMB_SlowDeathRateStandard, j, "SMB_SlowDeathRateStandard")
	jx.set_double_value(&sop.po_SMB_FastDeathRateStandard, j, "SMB_FastDeathRateStandard")
	jx.set_double_value(&sop.po_SMB_UtilizationEfficiency, j, "SMB_UtilizationEfficiency")
	jx.set_double_value(&sop.po_SOM_SlowUtilizationEfficiency, j, "SOM_SlowUtilizationEfficiency")
	jx.set_double_value(&sop.po_SOM_FastUtilizationEfficiency, j, "SOM_FastUtilizationEfficiency")
	jx.set_double_value(&sop.po_AOM_SlowUtilizationEfficiency, j, "AOM_SlowUtilizationEfficiency")
	jx.set_double_value(&sop.po_AOM_FastUtilizationEfficiency, j, "AOM_FastUtilizationEfficiency")
	jx.set_double_value(&sop.po_AOM_FastMaxC_to_N, j, "AOM_FastMaxC_to_N")
	jx.set_double_value(&sop.po_PartSOM_Fast_to_SOM_Slow, j, "PartSOM_Fast_to_SOM_Slow")
	jx.set_double_value(&sop.po_PartSMB_Slow_to_SOM_Fast, j, "PartSMB_Slow_to_SOM_Fast")
	jx.set_double_value(&sop.po_PartSMB_Fast_to_SOM_Fast, j, "PartSMB_Fast_to_SOM_Fast")
	jx.set_double_value(&sop.po_PartSOM_to_SMB_Slow, j, "PartSOM_to_SMB_Slow")
	jx.set_double_value(&sop.po_PartSOM_to_SMB_Fast, j, "PartSOM_to_SMB_Fast")
	jx.set_double_value(&sop.po_CN_Ratio_SMB, j, "CN_Ratio_SMB")
	jx.set_double_value(&sop.po_LimitClayEffect, j, "LimitClayEffect")
	jx.set_double_value(&sop.po_QTenFactor, j, "QTenFactor")
	jx.set_double_value(&sop.po_TempDecOptimal, j, "TempDecOptimal")
	jx.set_double_value(&sop.po_MoistureDecOptimal, j, "MoistureDecOptimal")
	jx.set_double_value(
		&sop.po_AmmoniaOxidationRateCoeffStandard,
		j,
		"AmmoniaOxidationRateCoeffStandard",
	)
	jx.set_double_value(
		&sop.po_NitriteOxidationRateCoeffStandard,
		j,
		"NitriteOxidationRateCoeffStandard",
	)
	jx.set_double_value(&sop.po_TransportRateCoeff, j, "TransportRateCoeff")
	jx.set_double_value(&sop.po_SpecAnaerobDenitrification, j, "SpecAnaerobDenitrification")
	jx.set_double_value(&sop.po_ImmobilisationRateCoeffNO3, j, "ImmobilisationRateCoeffNO3")
	jx.set_double_value(&sop.po_ImmobilisationRateCoeffNH4, j, "ImmobilisationRateCoeffNH4")
	jx.set_double_value(&sop.po_Denit1, j, "Denit1")
	jx.set_double_value(&sop.po_Denit2, j, "Denit2")
	jx.set_double_value(&sop.po_Denit3, j, "Denit3")
	jx.set_double_value(&sop.po_HydrolysisKM, j, "HydrolysisKM")
	jx.set_double_value(&sop.po_ActivationEnergy, j, "ActivationEnergy")
	jx.set_double_value(&sop.po_HydrolysisP1, j, "HydrolysisP1")
	jx.set_double_value(&sop.po_HydrolysisP2, j, "HydrolysisP2")
	jx.set_double_value(&sop.po_AtmosphericResistance, j, "AtmosphericResistance")
	jx.set_double_value(&sop.po_N2OProductionRate, j, "N2OProductionRate")
	jx.set_double_value(&sop.po_Inhibitor_NH3, j, "Inhibitor_NH3")
	jx.set_double_value(&sop.ps_MaxMineralisationDepth, j, "MaxMineralisationDepth")

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
		e := stics_parameters_merge(&sop.sticsParams, jx.get(j, "stics"))
		tl.append_errors(&res, e)
	}

	return res
}

// C++: json11::Json soilorganicmoduleparameters::to_json(const SoilOrganicModuleParameters*)
//
// NOTE(c++-quirk): to_json is lossy - it emits neither the three
// __enable_kaiteew_*__ flags nor the nested sticsParams, so a to_json/merge
// round trip does not preserve them. Reproduced.
soil_organic_module_parameters_to_json :: proc(
	sop: ^Soil_Organic_Module_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SoilOrganicModuleParameters")},
		{"SOM_SlowDecCoeffStandard", jx.vu(sop.po_SOM_SlowDecCoeffStandard, "d-1", a)},
		{"SOM_FastDecCoeffStandard", jx.vu(sop.po_SOM_FastDecCoeffStandard, "d-1", a)},
		{"SMB_SlowMaintRateStandard", jx.vu(sop.po_SMB_SlowMaintRateStandard, "d-1", a)},
		{"SMB_FastMaintRateStandard", jx.vu(sop.po_SMB_FastMaintRateStandard, "d-1", a)},
		{"SMB_SlowDeathRateStandard", jx.vu(sop.po_SMB_SlowDeathRateStandard, "d-1", a)},
		{"SMB_FastDeathRateStandard", jx.vu(sop.po_SMB_FastDeathRateStandard, "d-1", a)},
		{"SMB_UtilizationEfficiency", jx.vu(sop.po_SMB_UtilizationEfficiency, "d-1", a)},
		{"SOM_SlowUtilizationEfficiency", jx.vu(sop.po_SOM_SlowUtilizationEfficiency, "", a)},
		{"SOM_FastUtilizationEfficiency", jx.vu(sop.po_SOM_FastUtilizationEfficiency, "", a)},
		{"AOM_SlowUtilizationEfficiency", jx.vu(sop.po_AOM_SlowUtilizationEfficiency, "", a)},
		{"AOM_FastUtilizationEfficiency", jx.vu(sop.po_AOM_FastUtilizationEfficiency, "", a)},
		{"AOM_FastMaxC_to_N", jx.vu(sop.po_AOM_FastMaxC_to_N, "", a)},
		{"PartSOM_Fast_to_SOM_Slow", jx.vu(sop.po_PartSOM_Fast_to_SOM_Slow, "", a)},
		{"PartSMB_Slow_to_SOM_Fast", jx.vu(sop.po_PartSMB_Slow_to_SOM_Fast, "", a)},
		{"PartSMB_Fast_to_SOM_Fast", jx.vu(sop.po_PartSMB_Fast_to_SOM_Fast, "", a)},
		{"PartSOM_to_SMB_Slow", jx.vu(sop.po_PartSOM_to_SMB_Slow, "", a)},
		{"PartSOM_to_SMB_Fast", jx.vu(sop.po_PartSOM_to_SMB_Fast, "", a)},
		{"CN_Ratio_SMB", jx.vu(sop.po_CN_Ratio_SMB, "", a)},
		{"LimitClayEffect", jx.vu(sop.po_LimitClayEffect, "kg kg-1", a)},
		{"QTenFactor", jx.vu(sop.po_QTenFactor, "", a)},
		{"TempDecOptimal", jx.vu(sop.po_TempDecOptimal, "°C", a)},
		{"MoistureDecOptimal", jx.vu(sop.po_MoistureDecOptimal, "%", a)},
		{
			"AmmoniaOxidationRateCoeffStandard",
			jx.vu(sop.po_AmmoniaOxidationRateCoeffStandard, "d-1", a),
		},
		{
			"NitriteOxidationRateCoeffStandard",
			jx.vu(sop.po_NitriteOxidationRateCoeffStandard, "d-1", a),
		},
		{"TransportRateCoeff", jx.vu(sop.po_TransportRateCoeff, "d-1", a)},
		{
			"SpecAnaerobDenitrification",
			jx.vu(sop.po_SpecAnaerobDenitrification, "g gas-N g CO2-C-1", a),
		},
		{"ImmobilisationRateCoeffNO3", jx.vu(sop.po_ImmobilisationRateCoeffNO3, "d-1", a)},
		{"ImmobilisationRateCoeffNH4", jx.vu(sop.po_ImmobilisationRateCoeffNH4, "d-1", a)},
		{"Denit1", jx.vu(sop.po_Denit1, "", a)},
		{"Denit2", jx.vu(sop.po_Denit2, "", a)},
		{"Denit3", jx.vu(sop.po_Denit3, "", a)},
		{"HydrolysisKM", jx.vu(sop.po_HydrolysisKM, "", a)},
		{"ActivationEnergy", jx.vu(sop.po_ActivationEnergy, "", a)},
		{"HydrolysisP1", jx.vu(sop.po_HydrolysisP1, "", a)},
		{"HydrolysisP2", jx.vu(sop.po_HydrolysisP2, "", a)},
		{"AtmosphericResistance", jx.vu(sop.po_AtmosphericResistance, "s m-1", a)},
		{"N2OProductionRate", jx.vu(sop.po_N2OProductionRate, "d-1", a)},
		{"Inhibitor_NH3", jx.vu(sop.po_Inhibitor_NH3, "kg N m-3", a)},
		{"MaxMineralisationDepth", jx.f(sop.ps_MaxMineralisationDepth)},
	)
}
