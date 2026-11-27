// MONICA's own reference evapotranspiration as an evapotranspiration module,
// for the AMEI evapotranspiration exercise (phase 1, the sensitivity analysis
// of the original module with standardized inputs): MONICA's FAO-56
// Penman-Monteith equation (fao56_penman_monteith, core/et0.odin - the one
// fao56_et0 uses) on intermediates that come in, like the Crop2ML package
// Monica_Reference_Evapotranspiration's binding (monica_ref_et.odin), so
// that monica-module can run both on the same input file.
//
// The intermediates: the vapour pressures, the net radiation, the stomata
// resistance, and - < 0: calculated as fao56_et0 does - the slope of the
// saturation vapour pressure curve (from tavg) and the psychrometric constant
// (from height_nn and λ). λ: Input.latent_heat_of_vaporization, <= 0 MONICA's
// 0.408 = 1/2.45. As a driver in MONICA all of them are MONICA's own ET0's.
//
// Only a driver: it doesn't give the evaporation, transpiration and soil
// water MONICA needs from its evapotranspiration (`integrated = false`).
package core

import etm "../modules/evapotranspiration"

MONICA_INTERNAL_REFERENCE_EVAPOTRANSPIRATION :: "Monica_Internal_Reference_Evapotranspiration"

// No state, no parameters besides Input's.
Internal_Ref_Et :: struct {
	unused: int,
}

internal_ref_et_step :: proc(in_: ^etm.Input, out: ^etm.Output) {
	terms := Et0_Terms {
		vapour_pressure                 = in_.vapor_pressure,
		saturated_vapour_pressure       = in_.saturated_vapor_pressure,
		net_radiation                   = in_.net_radiation,
		stomata_resistance              = in_.stomata_resistance,
		saturated_vapour_pressure_slope = in_.saturated_vapor_pressure_slope,
		psychrometric_constant          = in_.psychrometric_constant,
		latent_heat_of_vaporization     = in_.latent_heat_of_vaporization,
	}
	if terms.saturated_vapour_pressure_slope < 0 {
		terms.saturated_vapour_pressure_slope = fao56_saturated_vapour_pressure_slope(in_.tavg)
	}
	if terms.psychrometric_constant < 0 {
		terms.psychrometric_constant = fao56_psychrometric_constant(in_.height_nn, in_.latent_heat_of_vaporization)
	}
	out.reference_evapotranspiration = fao56_penman_monteith(
		terms,
		in_.tavg,
		fao56_wind_speed_2m(in_.wind, in_.wind_speed_height),
	)
	out.saturation_vapor_pressure_deficit = terms.saturated_vapour_pressure - terms.vapour_pressure
	out.net_radiation = terms.net_radiation
}

// The table row (modules/evapotranspiration's `modules`), registered in
// builtin_modules.odin.
internal_ref_et_entry :: etm.Module_Entry {
	name       = MONICA_INTERNAL_REFERENCE_EVAPOTRANSPIRATION,
	type       = Internal_Ref_Et,
	make       = proc(a: etm.Allocator) -> etm.Module {return etm.Module{data = new(Internal_Ref_Et, a), type = Internal_Ref_Et}},
	init       = proc(m: etm.Module, in_: ^etm.Input, a: etm.Allocator) {},
	step       = proc(m: etm.Module, in_: ^etm.Input, out: ^etm.Output, a: etm.Allocator) {internal_ref_et_step(in_, out)},
	destroy    = proc(m: etm.Module, a: etm.Allocator) {free(m.data, a)},
	integrated = false,
	inputs     = {
		"tavg",
		"wind",
		"wind_speed_height",
		"vapor_pressure",
		"saturated_vapor_pressure",
		"stomata_resistance",
		"net_radiation",
		"saturated_vapor_pressure_slope",
		"psychrometric_constant",
	},
	outputs    = {"reference_evapotranspiration", "saturation_vapor_pressure_deficit", "net_radiation"},
}
