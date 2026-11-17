package core

// Water uptake (plan-component-interfaces.md F3; architecture.md section 5):
// interception, potential and actual transpiration per layer, the water
// stress (transpiration deficit). State: Crop_Water_Uptake (embedded in
// Crop_Module); the per-layer transpiration goes to soil moisture through
// Crop_To_Soil.

import libc "core:c/libc"

// Water uptake: interception, potential and actual transpiration per layer, water stress
// (fc_crop_water_uptake).
Crop_Water_Uptake :: struct {
	interception_storage:            f64,
	net_precipitation:               f64,
	evaporated_from_intercept:       f64 `unit:"mm"`,
	remaining_evapotranspiration:    f64 `unit:"mm"`,
	potential_transpiration:         f64,
	actual_transpiration:            f64 `unit:"mm"`,
	transpiration:                   [dynamic]f64 `unit:"mm"`, //! old TP
	transpiration_redux:             [dynamic]f64, //! old TRRED
	root_effectivity:                [dynamic]f64, //! old WUEFF
	crop_water_uptake:               [dynamic]f64, //! old TP
	transpiration_deficit:           f64 `unit:"0-1"`, //! old TRREL
	potential_transpiration_deficit: f64 `unit:"0-1"`,
	actual_transpiration_deficit:    f64 `unit:"0-1"`,
	transpiration_reduced:           f64 `unit:"mm"`,
	accumulated_et_a:                f64,
	accumulated_transpiration:       f64,
}

// What water_uptake_step reads on a day.
Water_Uptake_In :: struct {
	gross_precipitation:           f64 `unit:"mm" src:"weather"`,
	groundwater_table_layer_idx:   int `unit:"layer#" src:"soil"`,
	soil_layers:                   []Soil_Layer `src:"soil"`,
	current_total_temperature_sum: f64 `unit:"°C d" src:"Phenology"`,
	total_temperature_sum:         f64 `unit:"°C d" src:"Phenology"`,
	developmental_stage:           int `unit:"" src:"Phenology"`,
	final_developmental_stage:     int `unit:"" src:"Phenology"`,
	crop_height:                   f64 `unit:"m" src:"Canopy"`,
	kc_factor:                     f64 `unit:"1" src:"Canopy"`,
	soil_coverage:                 f64 `unit:"0-1" src:"Canopy"`,
	oxygen_deficit:                f64 `unit:"0-1" src:"Stress"`,
	reference_evapotranspiration:  f64 `unit:"mm" src:"Et0"`, // today's
	root_density:                  []f64 `unit:"m m-3" src:"Root_System"`, // per layer
	rooting_depth:                 int `unit:"layer#" src:"Root_System"`,
	rooting_zone:                  int `unit:"layer#" src:"Root_System"`,
	transplant_efficiency:         f64 `unit:"0-1" src:"Management"`,
	water_deficit_response_on:     bool `unit:"" src:"simulation parameters"`,
	max_potential_evapotranspiration: f64 `unit:"mm d-1" src:"soil moisture parameters"`,
	max_effective_rooting_depth:   f64 `unit:"m" src:"site"`,
}

// Fills Water_Uptake_In from the crop and its soil column (crop_module_step
// and the test drivers).
water_uptake_in :: proc(cm: ^Crop_Module, gross_precipitation: f64) -> Water_Uptake_In {
	return {
		gross_precipitation = gross_precipitation,
		groundwater_table_layer_idx = cm.soil_column.groundwater_table_layer_idx,
		soil_layers = cm.soil_column.layers[:],
		current_total_temperature_sum = cm.current_total_temperature_sum,
		total_temperature_sum = cm.total_temperature_sum,
		developmental_stage = cm.developmental_stage,
		final_developmental_stage = cm.final_developmental_stage,
		crop_height = cm.crop_height,
		kc_factor = cm.kc_factor,
		soil_coverage = cm.soil_coverage,
		oxygen_deficit = cm.oxygen_deficit,
		reference_evapotranspiration = cm.reference_evapotranspiration,
		root_density = cm.root_density[:],
		rooting_depth = cm.rooting_depth,
		rooting_zone = cm.rooting_zone,
		transplant_efficiency = cm.transplant_efficiency,
		water_deficit_response_on = cm.sim_params.water_deficit_response_on,
		max_potential_evapotranspiration = cm.max_potential_evapotranspiration,
		max_effective_rooting_depth = cm.site_params.max_effective_rooting_depth,
	}
}

// C++: void monica::cropmodule::fcCropWaterUptake(CropModule*, size_t
// vc_GroundwaterTable, double vw_GrossPrecipitation, double, double)
//
// Water uptake by the crop: potential transpiration from potential
// evapotranspiration by soil cover fraction, reduced by water availability
// in the soil according to actual water contents, root distribution and root
// effectivity. The trailing two C++ parameters are unused in the C++ body
// too (their names are commented out there) - kept for signature fidelity
// with step()'s call site.
water_uptake_step :: proc(w: ^Crop_Water_Uptake, input: Water_Uptake_In) {
	groundwater_table_layer_idx := input.groundwater_table_layer_idx
	gross_precipitation := input.gross_precipitation
	current_total_temperature_sum := input.current_total_temperature_sum
	total_temperature_sum := input.total_temperature_sum
	max_effective_rooting_depth := input.max_effective_rooting_depth

	nols := len(input.soil_layers)
	layer_thickness := input.soil_layers[0].layer_thickness_m
	w.potential_transpiration_deficit = 0.0 // [mm]
	w.potential_transpiration = 0.0 // old TRAMAX [mm]
	potential_evapotranspiration := 0.0 // [mm]
	w.transpiration_reduced = 0.0 // old TDRED [mm]
	w.actual_transpiration = 0.0 // [mm]
	remaining_total_root_effectivity := 0.0 // old WEFFREST [m]
	crop_water_uptake_from_groundwater := 0.0 // old GAUF [mm]
	total_root_effectivity := 0.0 // old WEFF [m]
	w.actual_transpiration_deficit = 0.0 // old TREST [mm]
	interception := 0.0
	w.remaining_evapotranspiration = 0.0

	for i_layer := 0; i_layer < nols; i_layer += 1 {
		w.transpiration[i_layer] = 0.0 // old TP [mm]
		w.transpiration_redux[i_layer] = 0.0 // old TRRED []
		w.root_effectivity[i_layer] = 0.0 // old WUEFF [?]
	}

	// --- Interception ---
	interception_storage_old := w.interception_storage

	// Interception in [mm d-1]
	interception = (2.5 * input.crop_height * input.soil_coverage) - w.interception_storage
	if interception < 0 {
		interception = 0.0
	}
	// If no precipitation occurs, interception = 0
	if gross_precipitation <= 0 {
		interception = 0.0
	}

	// Calculating net precipitation and adding to surface water
	if gross_precipitation <= interception {
		interception = gross_precipitation
		w.net_precipitation = 0.0
	} else {
		w.net_precipitation = gross_precipitation - interception
	}

	// add intercepted precipitation to the virtual interception water storage
	w.interception_storage = interception_storage_old + interception

	// --- Transpiration ---
	potential_evapotranspiration = input.reference_evapotranspiration * input.kc_factor // [mm]

	// from HERMES (6.5 mm by default)
	if potential_evapotranspiration > input.max_potential_evapotranspiration {
		potential_evapotranspiration = input.max_potential_evapotranspiration
	}

	w.remaining_evapotranspiration = potential_evapotranspiration // [mm]

	// If crop holds intercepted water, first evaporation from crop surface
	if w.interception_storage > 0.0 {
		if w.remaining_evapotranspiration >= w.interception_storage {
			w.remaining_evapotranspiration -= w.interception_storage
			w.evaporated_from_intercept = w.interception_storage
			w.interception_storage = 0.0
		} else {
			w.interception_storage -= w.remaining_evapotranspiration
			w.evaporated_from_intercept = w.remaining_evapotranspiration
			w.remaining_evapotranspiration = 0.0
		}
	} else {
		w.evaporated_from_intercept = 0.0
	}

	// if the plant has matured, no transpiration occurs!
	if input.developmental_stage < input.final_developmental_stage {
		w.potential_transpiration = w.remaining_evapotranspiration * input.soil_coverage // [mm]

		for i_layer := 0; i_layer < input.rooting_zone; i_layer += 1 {
			available_water :=
				input.soil_layers[i_layer].field_capacity -
				input.soil_layers[i_layer].permanent_wilting_point
			available_water_percentage :=
				(input.soil_layers[i_layer].soil_moisture_m3 -
					input.soil_layers[i_layer].permanent_wilting_point) /
				available_water
			if available_water_percentage < 0.0 {
				available_water_percentage = 0.0
			}
			// MP: access point for drought optimisation (this could be
			// extended for waterlogging); an alternative approach for
			// compensatory effects is a soil water-dependent root
			// penetration rate.
			switch {
			case available_water_percentage < 0.15:
				w.transpiration_redux[i_layer] = available_water_percentage * 3.0 // []
				w.root_effectivity[i_layer] = 0.15 + 0.45 * available_water_percentage / 0.15 // [] MP: essentially *3
			case available_water_percentage < 0.3:
				w.transpiration_redux[i_layer] =
					0.45 + (0.25 * (available_water_percentage - 0.15) / 0.15)
				w.root_effectivity[i_layer] =
					0.6 + (0.2 * (available_water_percentage - 0.15) / 0.15)
			case available_water_percentage < 0.5:
				// MP: ab hier hat das fast keinen Effekt mehr
				w.transpiration_redux[i_layer] =
					0.7 + (0.275 * (available_water_percentage - 0.3) / 0.2)
				w.root_effectivity[i_layer] =
					0.8 + (0.2 * (available_water_percentage - 0.3) / 0.2)
			case available_water_percentage < 0.75:
				// MP: ab hier ist nur mehr die Transpiration betroffen
				w.transpiration_redux[i_layer] =
					0.975 + (0.025 * (available_water_percentage - 0.5) / 0.25)
				w.root_effectivity[i_layer] = 1.0
			case:
				w.transpiration_redux[i_layer] = 1.0
				w.root_effectivity[i_layer] = 1.0
			}
			if w.transpiration_redux[i_layer] < 0 {
				w.transpiration_redux[i_layer] = 0.0
			}
			if w.root_effectivity[i_layer] < 0 {
				w.root_effectivity[i_layer] = 0.0
			}
			if i_layer == groundwater_table_layer_idx { 	// old GRW
				w.root_effectivity[i_layer] = 0.5
			}
			if i_layer > groundwater_table_layer_idx { 	// old GRW
				w.root_effectivity[i_layer] = 0.0
			}
			if f64(i_layer + 1) * layer_thickness >= max_effective_rooting_depth {
				w.root_effectivity[i_layer] = 0.0
			}

			total_root_effectivity += w.root_effectivity[i_layer] * input.root_density[i_layer] // [m m-3]
			remaining_total_root_effectivity = total_root_effectivity
		}

		// [TRANSPLANT SHOCK] Water Uptake Limitation.
		if input.transplant_efficiency < 1.0 {
			total_root_effectivity *= input.transplant_efficiency
			remaining_total_root_effectivity = total_root_effectivity
		}

		for i_layer := 0; i_layer < nols; i_layer += 1 {
			if i_layer > min(input.rooting_zone, groundwater_table_layer_idx + 1) {
				w.transpiration[i_layer] = 0.0 // [mm]
			} else {
				if total_root_effectivity != 0.0 {
					w.transpiration[i_layer] =
						w.potential_transpiration *
						((w.root_effectivity[i_layer] * input.root_density[i_layer]) /
								total_root_effectivity) *
						input.oxygen_deficit
				} else {
					// MP: why is this not changing anything? (probably only
					// matters for too-dry conditions)
					w.transpiration[i_layer] = 0
				}
			}
		}

		for i_layer := 0;
		    i_layer < min(input.rooting_zone, groundwater_table_layer_idx + 1);
		    i_layer += 1 {
			remaining_total_root_effectivity -=
				w.root_effectivity[i_layer] * input.root_density[i_layer] // [m m-3]

			if remaining_total_root_effectivity <= 0.0 {
				remaining_total_root_effectivity = 0.00001
			}
			if ((w.transpiration[i_layer] / 1000.0) / layer_thickness) >
			   (input.soil_layers[i_layer].soil_moisture_m3 -
					   input.soil_layers[i_layer].permanent_wilting_point) {
				w.potential_transpiration_deficit =
					(((w.transpiration[i_layer] / 1000.0) / layer_thickness) -
						(input.soil_layers[i_layer].soil_moisture_m3 -
								input.soil_layers[i_layer].permanent_wilting_point)) *
					layer_thickness *
					1000.0 // [mm]
				if w.potential_transpiration_deficit < 0.0 {
					w.potential_transpiration_deficit = 0.0
				}
				if w.potential_transpiration_deficit > w.transpiration[i_layer] {
					w.potential_transpiration_deficit = w.transpiration[i_layer] // [mm]
				}
			} else {
				w.potential_transpiration_deficit = 0.0
			}
			w.transpiration_reduced =
				w.transpiration[i_layer] * (1.0 - w.transpiration_redux[i_layer])

			// MP: this is a key line for water stress response
			w.actual_transpiration_deficit = max(
				w.transpiration_reduced,
				w.potential_transpiration_deficit,
			) // [mm]
			if w.actual_transpiration_deficit > 0.0 {
				if i_layer < min(input.rooting_zone, groundwater_table_layer_idx + 1) {
					for i_layer2 := i_layer + 1;
					    i_layer2 < min(input.rooting_zone, groundwater_table_layer_idx + 1);
					    i_layer2 += 1 {
						w.transpiration[i_layer2] +=
							w.actual_transpiration_deficit *
							(w.root_effectivity[i_layer2] *
									input.root_density[i_layer2] /
									remaining_total_root_effectivity)
					}
				}
			}
			w.transpiration[i_layer] = w.transpiration[i_layer] - w.actual_transpiration_deficit
			if w.transpiration[i_layer] < 0.0 {
				w.transpiration[i_layer] = 0.0
			}
			w.actual_transpiration += w.transpiration[i_layer]
			if i_layer == groundwater_table_layer_idx {
				crop_water_uptake_from_groundwater =
					(w.transpiration[i_layer] / 1000.0) / layer_thickness // [m3 m-3]
			}
		}
		if w.potential_transpiration > 0 {
			w.transpiration_deficit = w.actual_transpiration / w.potential_transpiration
		} else {
			w.transpiration_deficit = 1.0
		}

		groundwater_distance := groundwater_table_layer_idx - input.rooting_depth
		if groundwater_distance <= 1 {
			w.transpiration_deficit = 1.0
		}
		if !input.water_deficit_response_on {
			w.transpiration_deficit = 1.0
		}
	}
	// vc_CropWaterUptakeFromGroundwater: computed but unused elsewhere in the
	// C++ too - a genuine dead store, kept for fidelity.
	_ = crop_water_uptake_from_groundwater
}
