package core

// O3 uptake and damage as a component (plan-component-interfaces.md E2): cut
// out of fc_crop_photosynthesis. State: Crop_O3 (embedded in Crop_Module).
// Stepped hourly from inside photosynthesis' FvCB loop: the damage it
// computes lowers vcmax in the next hour. The process itself is
// o3_impact.odin's port of the C++ O3 module.

// O3 impact (computed in fc_crop_photosynthesis, o3_impact.odin).
Crop_O3 :: struct {
	o3_short_term_damage:  f64 `unit:"0-1"`,
	o3_long_term_damage:   f64 `unit:"0-1"`,
	o3_senescence:         f64,
	o3_sum_uptake:         f64 `unit:"µmol m-2"`,
	o3_w_stomatal_closure: f64 `unit:"0-1"`,
}

// What o3_hourly_step reads in an hour.
O3_Hourly_In :: struct {
	hour:                         int `unit:"h"`,
	// this hour's leaf fractions (LAI, stomatal conductance)
	sunlit:                       Fvcb_Leaf_Fraction `src:"Photosynthesis"`,
	shaded:                       Fvcb_Leaf_Fraction `src:"Photosynthesis"`,
	rooting_depth:                int `unit:"layer#" src:"Root_System" lag:"1d"`,
	soil_layers:                  []Soil_Layer `src:"soil"`, // FC, PWP, soil moisture in the rooted zone
	reference_et0:                f64 `unit:"mm" src:"Et0" lag:"1d"`,
	atmospheric_o3_concentration: f64 `unit:"ppb" src:"weather"`,
	relative_total_development:   f64 `unit:"0-1" src:"Phenology"`,
	temperature_sum_to_flowering: f64 `unit:"°C d" src:"Phenology"`,
	total_temperature_sum:        f64 `unit:"°C d" src:"Phenology"`,
	water_deficit_response_on:    bool `unit:"" src:"simulation parameters"`,
}

o3_hourly_step :: proc(o: ^Crop_O3, input: O3_Hourly_In) {
	// hourly O3 uptake and damage
	o3_par := make_o3_impact_params()
	o3_par.gamma3 = 0.05 // TODO: calibrate and add to crop params
	o3_par.gamma1 = 0.025 // TODO: calibrate and add to crop params

	root_depth := input.rooting_depth
	if root_depth >= 1 { 	// the crop has emerged
		fc := 0.0
		wp := 0.0
		swc := 0.0
		for i := 0; i < root_depth; i += 1 {
			fc += input.soil_layers[i].field_capacity
			wp += input.soil_layers[i].permanent_wilting_point
			swc += input.soil_layers[i].soil_moisture_m3
		}

		// weighted average gs and conversion from unit ground area
		// to unit leaf area
		lai_sun_weight := input.sunlit.lai / (input.sunlit.lai + input.shaded.lai)
		lai_sh_weight := 1 - lai_sun_weight
		avg_leaf_gs := lai_sh_weight * input.shaded.gs / input.shaded.lai
		if input.sunlit.lai > 0 {
			avg_leaf_gs += lai_sun_weight * input.sunlit.gs / input.sunlit.lai
		}

		o3_in: O3_Impact_In
		o3_in.fc = fc / f64(root_depth + 1) // field capacity, m3 m-3, avg in the rooted zone
		o3_in.wp = wp / f64(root_depth + 1) // wilting point, m3 m-3
		o3_in.swc = swc / f64(root_depth + 1) // soil water content, m3 m-3
		o3_in.et0 = input.reference_et0
		o3_in.o3a = input.atmospheric_o3_concentration
		o3_in.gs = avg_leaf_gs
		o3_in.h = input.hour
		o3_in.reldev = input.relative_total_development
		o3_in.gdd_flo = input.temperature_sum_to_flowering
		o3_in.gdd_mat = input.total_temperature_sum
		o3_in.f_o3s_d_prev = o.o3_short_term_damage
		o3_in.sum_o3_up = o.o3_sum_uptake

		o3_res := o3_impact_hourly(o3_in, o3_par, input.water_deficit_response_on)

		o.o3_short_term_damage = o3_res.f_o3s_d
		o.o3_long_term_damage = o3_res.f_o3l
		o.o3_senescence = o3_res.f_ls
		o.o3_sum_uptake += o3_res.hourly_o3_up
		o.o3_w_stomatal_closure = o3_res.ws_st_clos
	}
}
