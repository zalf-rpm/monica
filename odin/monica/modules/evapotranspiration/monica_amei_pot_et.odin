// Binding of the Crop2ML package Monica_AMEI_potential_Evapotranspiration
// (amei/Monica_AMEI_potential_Evapotranspiration/): the reference ET (FAO-56
// Penman-Monteith) and the potential ET of MONICA's own evapotranspiration,
// for the first phase of the AMEI evapotranspiration exercise.
//
// The vapour pressures, the net radiation and the stomata resistance are
// inputs, not calculated: standalone from the input file, as a driver
// MONICA's own - from the ET0 MONICA uses that day, the crop module's with a
// crop, soil moisture's without (core/evapotranspiration_impl.odin). With a
// crop the potential ET is the crop's remaining ET, without one ET0 * Kc, as
// in MONICA.
//
// Only a driver: it doesn't give the evaporation, transpiration and soil
// water MONICA needs from its evapotranspiration (`integrated = false`).
package evapotranspiration

import pet "../../../amei/Monica_AMEI_potential_Evapotranspiration"

Monica_Amei_Pot_Et :: struct {
	c:     pet.Amei_Pot_Et_Component,
	s, s1: pet.Amei_Pot_Et_State, // empty: the model has no state
	r:     pet.Amei_Pot_Et_Rate,
	a:     pet.Amei_Pot_Et_Auxiliary,
	ex:    pet.Amei_Pot_Et_Exogenous,
}

make_monica_amei_pot_et :: proc(allocator: Allocator) -> Module {
	b := new(Monica_Amei_Pot_Et, allocator)
	b.c = pet.make_amei_pot_et_component(allocator)
	return b
}

destroy_monica_amei_pot_et :: proc(b: ^Monica_Amei_Pot_Et, allocator := context.allocator) {
	context.allocator = allocator
	pet.destroy_amei_pot_et_component(&b.c)
	pet.destroy_amei_pot_et_state(&b.s)
	pet.destroy_amei_pot_et_state(&b.s1)
	pet.destroy_amei_pot_et_rate(&b.r)
	pet.destroy_amei_pot_et_auxiliary(&b.a)
	pet.destroy_amei_pot_et_exogenous(&b.ex)
	free(b)
}

// The model has no init; its only parameter besides the ET cap (6.5 mm, as
// MONICA's) is the height.
monica_amei_pot_et_init :: proc(b: ^Monica_Amei_Pot_Et, in_: ^Input, allocator := context.allocator) {
	pet.amei_pot_et_component_set_height_nn(&b.c, in_.height_nn)
}

monica_amei_pot_et_step :: proc(b: ^Monica_Amei_Pot_Et, in_: ^Input, out: ^Output, allocator := context.allocator) {
	ex := &b.ex
	ex.mean_air_temperature = in_.tavg
	ex.wind_speed = in_.wind
	ex.wind_speed_height = in_.wind_speed_height
	ex.kc_factor = in_.kc_factor
	ex.developmental_stage = in_.developmental_stage
	ex.crop_remaining_evapotranspiration = in_.crop_remaining_evapotranspiration
	a := &b.a
	a.vapor_pressure = in_.vapor_pressure
	a.saturated_vapor_pressure = in_.saturated_vapor_pressure
	a.stomata_resistance = in_.stomata_resistance
	a.net_radiation = in_.net_radiation
	pet.amei_pot_et_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.reference_evapotranspiration = a.reference_evapotranspiration
	out.potential_evapotranspiration = a.potential_evapotranspiration
	out.saturation_vapor_pressure_deficit = a.saturation_vapor_pressure_deficit
}

// The table row (evapotranspiration.odin's `modules`)
monica_amei_pot_et_entry :: Module_Entry {
	name       = "Monica_AMEI_potential_Evapotranspiration",
	make       = make_monica_amei_pot_et,
	init       = proc(m: Module, in_: ^Input, a: Allocator) {monica_amei_pot_et_init(m.(^Monica_Amei_Pot_Et), in_, a)},
	step       = proc(m: Module, in_: ^Input, out: ^Output, a: Allocator) {monica_amei_pot_et_step(m.(^Monica_Amei_Pot_Et), in_, out, a)},
	destroy    = proc(m: Module, a: Allocator) {destroy_monica_amei_pot_et(m.(^Monica_Amei_Pot_Et), a)},
	integrated = false,
	inputs     = {
		"tavg",
		"wind",
		"wind_speed_height",
		"kc_factor",
		"developmental_stage",
		"crop_remaining_evapotranspiration",
		"vapor_pressure",
		"saturated_vapor_pressure",
		"stomata_resistance",
		"net_radiation",
	},
	outputs    = {"reference_evapotranspiration", "potential_evapotranspiration", "saturation_vapor_pressure_deficit"},
}
