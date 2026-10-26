// Binding of the Crop2ML package BiomaSurfaceSWATSoilSWATC
// (amei/BiomaSurfaceSWATSoilSWATC/): SWAT's surface and soil temperature, in
// BioMA's implementation.
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// BiomaSurfaceSWATSoilSWATC_monica_interface.cpp): as
// BiomaSurfacePartonSoilSWATC (bioma_parton_swat.odin), plus MONICA's
// environment albedo and the snow's water equivalent; the above-ground
// biomass goes into the auxiliary struct, where this package has it.
package soil_temperature

import bss "../../../amei/BiomaSurfaceSWATSoilSWATC"

Bioma_Swat_Swat :: struct {
	c:            bss.Surface_Swat_Soil_Swatc_Component,
	s, s1:        bss.Surface_Swat_Soil_Swatc_State, // no ModelUnit reads s1
	r:            bss.Surface_Swat_Soil_Swatc_Rate,
	a:            bss.Surface_Swat_Soil_Swatc_Auxiliary,
	ex:           bss.Surface_Swat_Soil_Swatc_Exogenous,
	no_of_layers: int,
}

make_bioma_swat_swat :: proc(allocator := context.allocator) -> Module {
	b := new(Bioma_Swat_Swat, allocator)
	b.c = bss.make_surface_swat_soil_swatc_component(allocator)
	return b
}

destroy_bioma_swat_swat :: proc(b: ^Bioma_Swat_Swat, allocator := context.allocator) {
	context.allocator = allocator
	bss.destroy_surface_swat_soil_swatc_component(&b.c)
	bss.destroy_surface_swat_soil_swatc_state(&b.s)
	bss.destroy_surface_swat_soil_swatc_state(&b.s1)
	bss.destroy_surface_swat_soil_swatc_rate(&b.r)
	bss.destroy_surface_swat_soil_swatc_auxiliary(&b.a)
	bss.destroy_surface_swat_soil_swatc_exogenous(&b.ex)
	free(b)
}

bioma_swat_swat_init :: proc(b: ^Bioma_Swat_Swat, in_: ^Input, allocator := context.allocator) {
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
	bss.surface_swat_soil_swatc_component_set_layer_thickness(c, in_.layer_thickness, allocator)
	bss.surface_swat_soil_swatc_component_set_bulk_density(c, bd, allocator)
	bss.surface_swat_soil_swatc_component_set_soil_profile_depth(c, depth)
	bss.surface_swat_soil_swatc_component_set_lag_coefficient(c, 0.8)
	bss.surface_swat_soil_swatc_component_set_air_temperature_annual_average(c, in_.tav)
	resize(&b.ex.volumetric_water_content, n)
	bioma_swat_swat_exogenous(b, in_)
	bss.surface_swat_soil_swatc_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

bioma_swat_swat_step :: proc(b: ^Bioma_Swat_Swat, in_: ^Input, out: ^Output, allocator := context.allocator) {
	bioma_swat_swat_exogenous(b, in_)
	bss.surface_swat_soil_swatc_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.soil_surface_temperature = b.a.surface_soil_temperature
	copy(out.layer_temperature, b.s.soil_temperature_by_layers[:b.no_of_layers])
}

@(private = "file")
bioma_swat_swat_exogenous :: proc(b: ^Bioma_Swat_Swat, in_: ^Input) {
	b.ex.air_temperature_minimum = in_.tmin
	b.ex.air_temperature_maximum = in_.tmax
	b.ex.global_solar_radiation = in_.globrad
	b.ex.water_equivalent_of_snow_pack = in_.snow_water_equivalent
	b.ex.albedo = in_.albedo
	b.a.above_ground_biomass = in_.above_ground_biomass
	for &w, i in b.ex.volumetric_water_content {
		w = in_.soil_moisture[i] - in_.permanent_wilting_point[i]
	}
}
