// Binding of the Crop2ML package Monica_Reference_Evapotranspiration
// (amei/Monica_Reference_Evapotranspiration/): just the reference ET (FAO-56
// Penman-Monteith) of MONICA's own evapotranspiration, for the AMEI
// evapotranspiration exercise.
//
// As Monica_AMEI_potential_Evapotranspiration (monica_amei_pot_et.odin), the
// net radiation and the stomata resistance are inputs, not calculated:
// standalone from the input file, as a driver MONICA's own - from the ET0
// MONICA uses that day, the crop module's with a crop, soil moisture's
// without (core/evapotranspiration_impl.odin). The package takes the
// saturation deficit, calculated here from the two vapour pressures as
// MONICA does (core/et0.odin).
//
// Only a driver: it doesn't give the evaporation, transpiration and soil
// water MONICA needs from its evapotranspiration (`integrated = false`).
package evapotranspiration

import c2m "../crop2ml"
import ret "../../../amei/Monica_Reference_Evapotranspiration"

Monica_Ref_Et :: struct {
	c:     ret.Amei_Ref_Et_Component,
	s, s1: ret.Amei_Ref_Et_State, // empty: the model has no state
	r:     ret.Amei_Ref_Et_Rate,
	a:     ret.Amei_Ref_Et_Auxiliary,
	ex:    ret.Amei_Ref_Et_Exogenous,
}

make_monica_ref_et :: proc(allocator: Allocator) -> Module {
	b := new(Monica_Ref_Et, allocator)
	b.c = ret.make_amei_ref_et_component(allocator)
	return b
}

destroy_monica_ref_et :: proc(b: ^Monica_Ref_Et, allocator := context.allocator) {
	context.allocator = allocator
	ret.destroy_amei_ref_et_component(&b.c)
	ret.destroy_amei_ref_et_state(&b.s)
	ret.destroy_amei_ref_et_state(&b.s1)
	ret.destroy_amei_ref_et_rate(&b.r)
	ret.destroy_amei_ref_et_auxiliary(&b.a)
	ret.destroy_amei_ref_et_exogenous(&b.ex)
	free(b)
}

// The model has no init; its only parameter is the height.
monica_ref_et_init :: proc(b: ^Monica_Ref_Et, in_: ^Input, allocator := context.allocator) {
	ret.amei_ref_et_component_set_height_nn(&b.c, in_.height_nn)
}

monica_ref_et_step :: proc(b: ^Monica_Ref_Et, in_: ^Input, out: ^Output, allocator := context.allocator) {
	ex := &b.ex
	ex.mean_air_temperature = in_.tavg
	ex.wind_speed = in_.wind
	ex.wind_speed_height = in_.wind_speed_height
	a := &b.a
	a.stomata_resistance = in_.stomata_resistance
	a.net_radiation = in_.net_radiation
	a.saturation_vapor_pressure_deficit = in_.saturated_vapor_pressure - in_.vapor_pressure
	c2m.copy_state(&b.s1, &b.s, allocator) // the previous day's state, for `_t1` inputs
	ret.amei_ref_et_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.reference_evapotranspiration = a.reference_evapotranspiration
	out.saturation_vapor_pressure_deficit = a.saturation_vapor_pressure_deficit
}

// The table row (evapotranspiration.odin's `modules`)
monica_ref_et_entry :: Module_Entry {
	name       = "Monica_Reference_Evapotranspiration",
	make       = make_monica_ref_et,
	init       = proc(m: Module, in_: ^Input, a: Allocator) {monica_ref_et_init(m.(^Monica_Ref_Et), in_, a)},
	step       = proc(m: Module, in_: ^Input, out: ^Output, a: Allocator) {monica_ref_et_step(m.(^Monica_Ref_Et), in_, out, a)},
	destroy    = proc(m: Module, a: Allocator) {destroy_monica_ref_et(m.(^Monica_Ref_Et), a)},
	integrated = false,
	inputs     = {
		"tavg",
		"wind",
		"wind_speed_height",
		"vapor_pressure",
		"saturated_vapor_pressure",
		"stomata_resistance",
		"net_radiation",
	},
	outputs    = {"reference_evapotranspiration", "saturation_vapor_pressure_deficit"},
}
