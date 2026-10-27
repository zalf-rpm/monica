// Binding of the Crop2ML package BiomaSurfacePartonSoilSWATC
// (amei/BiomaSurfacePartonSoilSWATC/): BioMA's Parton surface temperature with
// SWAT's soil temperature.
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// BiomaSurfacePartonSoilSWATC_monica_interface.cpp): layer thickness in m,
// bulk density kg m-3 -> t m-3, the profile depth, lag coefficient 0.8, the
// climate's annual average temperature. The volumetric water content is the
// soil moisture minus the wilting point, as the adapter has it.
package soil_temperature

import bps "../../../amei/BiomaSurfacePartonSoilSWATC"

Bioma_Parton_Swat :: struct {
	c:            bps.Surface_Parton_Soil_Swatc_Component,
	s, s1:        bps.Surface_Parton_Soil_Swatc_State, // no ModelUnit reads s1
	r:            bps.Surface_Parton_Soil_Swatc_Rate,
	a:            bps.Surface_Parton_Soil_Swatc_Auxiliary,
	ex:           bps.Surface_Parton_Soil_Swatc_Exogenous,
	no_of_layers: int,
}

make_bioma_parton_swat :: proc(allocator: Allocator) -> Module {
	b := new(Bioma_Parton_Swat, allocator)
	b.c = bps.make_surface_parton_soil_swatc_component(allocator)
	return b
}

destroy_bioma_parton_swat :: proc(b: ^Bioma_Parton_Swat, allocator := context.allocator) {
	context.allocator = allocator
	bps.destroy_surface_parton_soil_swatc_component(&b.c)
	bps.destroy_surface_parton_soil_swatc_state(&b.s)
	bps.destroy_surface_parton_soil_swatc_state(&b.s1)
	bps.destroy_surface_parton_soil_swatc_rate(&b.r)
	bps.destroy_surface_parton_soil_swatc_auxiliary(&b.a)
	bps.destroy_surface_parton_soil_swatc_exogenous(&b.ex)
	free(b)
}

bioma_parton_swat_init :: proc(b: ^Bioma_Parton_Swat, in_: ^Input, allocator := context.allocator) {
	context.allocator = allocator
	n := len(in_.layer_thickness)
	b.no_of_layers = n
	c := &b.c
	bd := make([]f64, n, context.temp_allocator)
	depth := 0.0
	for i in 0 ..< n {
		bd[i] = in_.bulk_density[i] / 1000.0 // kg m-3 -> t m-3
		depth += in_.layer_thickness[i]
	}
	bps.surface_parton_soil_swatc_component_set_layer_thickness(c, in_.layer_thickness, allocator)
	bps.surface_parton_soil_swatc_component_set_bulk_density(c, bd, allocator)
	bps.surface_parton_soil_swatc_component_set_soil_profile_depth(c, depth)
	bps.surface_parton_soil_swatc_component_set_lag_coefficient(c, 0.8)
	bps.surface_parton_soil_swatc_component_set_air_temperature_annual_average(c, in_.tav)
	resize(&b.ex.volumetric_water_content, n)
	bioma_parton_swat_exogenous(b, in_)
	bps.surface_parton_soil_swatc_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

bioma_parton_swat_step :: proc(b: ^Bioma_Parton_Swat, in_: ^Input, out: ^Output, allocator := context.allocator) {
	bioma_parton_swat_exogenous(b, in_)
	bps.surface_parton_soil_swatc_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.soil_surface_temperature = b.a.surface_soil_temperature
	copy(out.layer_temperature, b.s.soil_temperature_by_layers[:b.no_of_layers])
}

@(private = "file")
bioma_parton_swat_exogenous :: proc(b: ^Bioma_Parton_Swat, in_: ^Input) {
	b.ex.air_temperature_minimum = in_.tmin
	b.ex.air_temperature_maximum = in_.tmax
	b.ex.global_solar_radiation = in_.globrad
	b.ex.day_length = in_.day_length
	b.ex.above_ground_biomass = in_.above_ground_biomass
	for &w, i in b.ex.volumetric_water_content {
		w = in_.soil_moisture[i] - in_.permanent_wilting_point[i]
	}
}

// The table row (soil_temperature.odin's `modules`)
bioma_parton_swat_entry :: Module_Entry {
	name    = "BiomaSurfacePartonSoilSWATC",
	make    = make_bioma_parton_swat,
	init    = proc(m: Module, in_: ^Input, a: Allocator) {bioma_parton_swat_init(m.(^Bioma_Parton_Swat), in_, a)},
	step    = proc(m: Module, in_: ^Input, out: ^Output, a: Allocator) {bioma_parton_swat_step(m.(^Bioma_Parton_Swat), in_, out, a)},
	destroy = proc(m: Module, a: Allocator) {destroy_bioma_parton_swat(m.(^Bioma_Parton_Swat), a)},
}
