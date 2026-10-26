// Binding of the Crop2ML package ApsimCampbell (amei/ApsimCampbell/), APSIM's
// implementation of Campbell's soil temperature.
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// ApsimCampbell_monica_interface.cpp): ps 2.63, pom 1.3, the soil
// constituents; thickness m -> mm, bulk density kg m-3 -> g cm-3, sand/silt/
// clay/organic carbon/stones as %; MONICA's soil moisture every day; wind;
// air pressure (1010 without one in the climate data); Eo and Eos both
// MONICA's potential evapotranspiration (ET0 * Kc; the adapter has a "!!!"
// questioning that), Es MONICA's actual evaporation. Its node 1 is the
// surface, nodes 2... MONICA's layers.
//
// NOTE(deviation) from the adapter, two of its bugs:
//   - the canopy height is MONICA's crop height in mm; the adapter passed
//     metres where the module expects mm
//   - the soil albedo (waterBalance_Salb) is MONICA's environment albedo; the
//     adapter left it at 0 outside its sensitivity build, which used the site's
package soil_temperature

import apsim "../../../amei/ApsimCampbell"

Apsim_Campbell :: struct {
	c:            apsim.Soiltemp_Component,
	s, s1:        apsim.Soiltemp_State, // no ModelUnit reads s1
	r:            apsim.Soiltemp_Rate,
	a:            apsim.Soiltemp_Auxiliary,
	ex:           apsim.Soiltemp_Exogenous,
	no_of_layers: int,
}

make_apsim_campbell :: proc(allocator := context.allocator) -> Module {
	b := new(Apsim_Campbell, allocator)
	b.c = apsim.make_soiltemp_component(allocator)
	return b
}

destroy_apsim_campbell :: proc(b: ^Apsim_Campbell, allocator := context.allocator) {
	context.allocator = allocator
	apsim.destroy_soiltemp_component(&b.c)
	apsim.destroy_soiltemp_state(&b.s)
	apsim.destroy_soiltemp_state(&b.s1)
	apsim.destroy_soiltemp_rate(&b.r)
	apsim.destroy_soiltemp_auxiliary(&b.a)
	apsim.destroy_soiltemp_exogenous(&b.ex)
	free(b)
}

apsim_campbell_init :: proc(b: ^Apsim_Campbell, in_: ^Input, allocator := context.allocator) {
	context.allocator = allocator
	n := len(in_.layer_thickness)
	b.no_of_layers = n
	c := &b.c
	apsim.soiltemp_component_set_ps(c, 2.63)
	apsim.soiltemp_component_set_pom(c, 1.3)
	apsim.soiltemp_component_set_soil_constituent_names(
		c,
		{"Rocks", "OrganicMatter", "Sand", "Silt", "Clay", "Water", "Ice", "Air"},
		allocator,
	)
	apsim.soiltemp_component_set_weather_latitude(c, in_.latitude)
	thickness := make([]f64, n, context.temp_allocator)
	bd := make([]f64, n, context.temp_allocator)
	for i in 0 ..< n {
		thickness[i] = in_.layer_thickness[i] * 1000.0 // m -> mm
		bd[i] = in_.bulk_density[i] / 1000.0 // kg m-3 -> g cm-3
	}
	apsim.soiltemp_component_set_physical_thickness(c, thickness, allocator)
	apsim.soiltemp_component_set_physical_bd(c, bd, allocator)

	ex := &b.ex
	for arr in ([]^[dynamic]f64 {
			&ex.physical_particle_size_sand,
			&ex.physical_particle_size_silt,
			&ex.physical_particle_size_clay,
			&ex.physical_rocks,
			&ex.organic_carbon,
			&ex.water_balance_sw,
		}) {
		resize(arr, n)
	}
	for i in 0 ..< n {
		ex.physical_particle_size_sand[i] = in_.sand[i] * 100.0 // %
		ex.physical_particle_size_silt[i] = in_.silt[i] * 100.0
		ex.physical_particle_size_clay[i] = in_.clay[i] * 100.0
		ex.physical_rocks[i] = in_.stones[i] * 100.0
		ex.organic_carbon[i] = in_.organic_carbon[i] * 100.0
	}
	apsim_campbell_exogenous(b, in_)
	apsim.soiltemp_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

apsim_campbell_step :: proc(b: ^Apsim_Campbell, in_: ^Input, out: ^Output, allocator := context.allocator) {
	apsim_campbell_exogenous(b, in_)
	apsim.soiltemp_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.soil_surface_temperature = b.s.ave_soil_temp[1]
	copy(out.layer_temperature, b.s.ave_soil_temp[2:2 + b.no_of_layers])
}

@(private = "file")
apsim_campbell_exogenous :: proc(b: ^Apsim_Campbell, in_: ^Input) {
	ex := &b.ex
	ex.weather_min_t = in_.tmin
	ex.weather_max_t = in_.tmax
	ex.weather_mean_t = in_.tavg
	ex.weather_radn = in_.globrad
	ex.clock_today_day_of_year = in_.julian_day
	ex.weather_wind = in_.wind
	ex.weather_air_pressure = in_.air_pressure
	potential_et := in_.et0 * in_.kc_factor
	ex.water_balance_eo = potential_et
	ex.water_balance_eos = potential_et
	ex.water_balance_es = in_.actual_evaporation
	ex.water_balance_salb = in_.albedo
	ex.weather_tav = in_.tav
	ex.weather_amp = in_.tamp
	ex.micro_climate_canopy_height = in_.crop_height * 1000.0 // m -> mm
	copy(ex.water_balance_sw[:], in_.soil_moisture)
}
