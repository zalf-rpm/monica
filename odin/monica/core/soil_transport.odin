// Port of src/core/soiltransport.{h,cpp}: SoilTransport, make_soil_transport,
// and every soiltransport:: proc.
//
// No monica-back-pointer deviation needed here, unlike soil_temperature.odin/
// soil_moisture.odin: the C++ struct already takes siteParams/envParams/
// cropModParams as raw pointers passed directly into makeSoilTransport, not
// read through a MonicaModel&. cropModule points at the same phase-5
// Crop_Module stub soil_moisture.odin introduced (crop_module_stub.odin),
// extended with the one extra field this file reads (vc_NUptakeFromLayer).
package core

import p "../params"
import libc "core:c/libc"

// C++: struct monica::SoilTransport
Soil_Transport :: struct {
	soil_column:          ^Soil_Column,
	mod_params:           p.Soil_Transport_Module_Parameters,
	site_params:          ^p.Site_Parameters,
	env_params:           ^p.Environment_Parameters,
	crop_mod_params:      ^p.Crop_Module_Parameters,
	convection:           [dynamic]f64,
	diffusion_coeff:      [dynamic]f64,
	dispersion:           [dynamic]f64,
	dispersion_coeff:     [dynamic]f64,
	leaching_at_boundary: f64,
	n_uptake_from_layer:  [dynamic]f64,
	pore_water_velocity:  [dynamic]f64,
	// vs_SoilMineralNContent: [dynamic]f64, // never resized - dead field in the C++ too (only touched by dropped (de)serialize)
	// soil_no3:             [dynamic]f64,
	soil_no3_aq:          [dynamic]f64,
	time_step:            f64,
	total_dispersion:     [dynamic]f64,
	percolation_rate:     [dynamic]f64,
	crop_module:          ^Crop_Module,
}

// C++: kj::Own<SoilTransport> monica::makeSoilTransport(
//        SoilTransportModuleParameters, SoilColumn*, const SiteParameters*,
//        const EnvironmentParameters*, const CropModuleParameters*)
//
// Returns by value, matching make_soil_column/make_soil_temperature/
// make_soil_moisture's precedent.
make_soil_transport :: proc(
	mod_params: p.Soil_Transport_Module_Parameters,
	soil_column: ^Soil_Column,
	site_params: ^p.Site_Parameters,
	env_params: ^p.Environment_Parameters,
	crop_mod_params: ^p.Crop_Module_Parameters,
) -> Soil_Transport {
	st: Soil_Transport
	st.soil_column = soil_column
	st.site_params = site_params
	st.mod_params = mod_params
	st.env_params = env_params
	st.crop_mod_params = crop_mod_params
	st.time_step = 1.0 // C++ in-class initialiser: double time_step{1.0}

	scSize := len(soil_column.layers)
	resize(&st.convection, scSize)
	resize(&st.diffusion_coeff, scSize)
	resize(&st.dispersion, scSize)
	resize(&st.dispersion_coeff, scSize)
	for i in 0 ..< scSize {
		st.dispersion_coeff[i] = 1.0
	}
	resize(&st.n_uptake_from_layer, scSize)
	resize(&st.pore_water_velocity, scSize)
	// resize(&st.soil_no3, scSize)
	resize(&st.soil_no3_aq, scSize)
	resize(&st.total_dispersion, scSize)
	resize(&st.percolation_rate, scSize)

	return st
}

// C++: void monica::soiltransport::step(SoilTransport*)
soil_transport_step :: proc(st: ^Soil_Transport) {
	min_time_step_factor := 1.0 // [t t-1]
	nols := len(st.soil_column.layers)

	for i in 0 ..< nols {
		// st.soil_no3[i] = st.soil_column.layers[i].soil_no3

		st.n_uptake_from_layer[i] =
			st.crop_module != nil ? st.crop_module.n_uptake_from_layer[i] : 0
		if i == nols - 1 {
			st.percolation_rate[i] = st.soil_column.flux_at_lower_boundary // [mm]
		} else {
			st.percolation_rate[i] = st.soil_column.layers[i + 1].soil_water_flux // [mm]
		}
		// Variable time step in case of high water fluxes to ensure stable numerics
		pri := st.percolation_rate[i]
		time_step_factor_current_layer := min_time_step_factor
		if -5.0 <= pri && pri <= 5.0 && min_time_step_factor > 1.0 {
			time_step_factor_current_layer = 1.0
		} else if (-10.0 <= pri && pri < -5.0) || (5.0 < pri && pri <= 10.0) {
			time_step_factor_current_layer = 0.5
		} else if (-15.0 <= pri && pri < -10.0) || (10.0 < pri && pri <= 15.0) {
			time_step_factor_current_layer = 0.25
		} else if pri < -15.0 || pri > 15.0 {
			time_step_factor_current_layer = 0.125
		}

		min_time_step_factor = min(min_time_step_factor, time_step_factor_current_layer)
	}

	n_deposition(st)
	n_uptake(st)

	// Nitrate transport is called according to the set time step
	st.leaching_at_boundary = 0.0
	for ts := 0; f64(ts) < (1.0 / min_time_step_factor); ts += 1 {
		n_transport(st, st.env_params.leaching_depth_m, min_time_step_factor)
	}

	for i in 0 ..< nols {
		st.soil_column.layers[i].soil_no3 = max(
			0.0,
			st.soil_no3_aq[i] * st.soil_column.layers[i].soil_moisture_m3,
		)
	}
}

// C++: void monica::soiltransport::nDeposition(SoilTransport*)
//
// Kersebaum 1989. Daily N deposition, transformed from an annual value, added
// to the ammonium... (really nitrate: soil_no3) pool of the top soil layer.
n_deposition :: proc(st: ^Soil_Transport) {
	daily_N_deposition := st.site_params.n_deposition / 365.0

	st.soil_column.layers[0].soil_no3 +=
		daily_N_deposition / (10000.0 * st.soil_column.layers[0].layer_thickness_m)
}

// C++: void monica::soiltransport::nUptake(SoilTransport*)
//
// Kersebaum 1989.
n_uptake :: proc(st: ^Soil_Transport) {
	nols := len(st.soil_column.layers)
	crop_N_uptake := 0.0
	for i in 0 ..< nols {
		lti := st.soil_column.layers[i].layer_thickness_m
		smi := st.soil_column.layers[i].soil_moisture_m3
		soil_no3_i := &st.soil_column.layers[i].soil_no3

		// Lower boundary for N exploitation per layer
		if st.n_uptake_from_layer[i] >
		   ((soil_no3_i^ * lti) - st.crop_mod_params.minimum_available_n) {
			st.n_uptake_from_layer[i] =
				((soil_no3_i^ * lti) - st.crop_mod_params.minimum_available_n)
		} // Crop N uptake from layer i [kg N m-2]

		if st.n_uptake_from_layer[i] < 0 {
			st.n_uptake_from_layer[i] = 0
		}

		crop_N_uptake += st.n_uptake_from_layer[i]

		// Subtracting crop N uptake
		soil_no3_i^ -= st.n_uptake_from_layer[i] / lti

		// Calculation of solute NO3 concentration on the basis of the soil
		// moisture content before movement of current time step
		// (kg m soil-3 --> kg m solute-3)
		st.soil_no3_aq[i] = soil_no3_i^ / smi
	}

	st.soil_column.crop_N_uptake = crop_N_uptake // [kg m-2]
}

// C++: void monica::soiltransport::nTransport(SoilTransport*, double, double)
//
// Kersebaum 1989.
n_transport :: proc(st: ^Soil_Transport, leaching_depth_m, time_step_factor: f64) {
	diffusion_coeff_standard := st.mod_params.diffusion_coefficient_standard // [m2 d-1]; old D0
	ad := st.mod_params.ad // Factor a in Kersebaum 1989 p.24 for Loess soils
	dispersion_length := st.mod_params.dispersion_length // [m]
	soil_profile := 0.0
	leaching_depth_layer_idx := 0
	nols := len(st.soil_column.layers)
	soil_moisture_gradient := make([dynamic]f64, nols, context.temp_allocator)

	for i in 0 ..< nols {
		soil_profile += st.soil_column.layers[i].layer_thickness_m
		if (soil_profile - 0.001) < leaching_depth_m {
			leaching_depth_layer_idx = i
		}
	}

	// Calculation of convection for different cases of flux direction
	for i in 0 ..< nols {
		wf0 := st.soil_column.layers[0].soil_water_flux
		lt := st.soil_column.layers[i].layer_thickness_m
		no3 := st.soil_no3_aq[i]

		if i == 0 {
			pr := st.percolation_rate[i] / 1000.0 * time_step_factor // [mm t-1 --> m t-1]
			NO3_u := st.soil_no3_aq[i + 1]

			if pr >= 0.0 && wf0 >= 0.0 {
				st.convection[i] = (no3 * pr) / lt // old KONV = Konvektion Diss S. 23
			} else if pr >= 0 && wf0 < 0 {
				st.convection[i] = (no3 * pr) / lt
			} else if pr < 0 && wf0 < 0 {
				st.convection[i] = (NO3_u * pr) / lt
			} else if pr < 0 && wf0 >= 0 {
				st.convection[i] = (NO3_u * pr) / lt
			}
		} else if i < nols - 1 {
			// layer > 0 && < bottom
			pr_o := st.percolation_rate[i - 1] / 1000.0 * time_step_factor
			pr := st.percolation_rate[i] / 1000.0 * time_step_factor
			NO3_u := st.soil_no3_aq[i + 1]

			if pr >= 0.0 && pr_o >= 0.0 {
				NO3_o := st.soil_no3_aq[i - 1]
				st.convection[i] = ((no3 * pr) - (NO3_o * pr_o)) / lt
			} else if pr >= 0 && pr_o < 0 {
				st.convection[i] = ((no3 * pr) - (no3 * pr_o)) / lt
			} else if pr < 0 && pr_o < 0 {
				st.convection[i] = ((NO3_u * pr) - (no3 * pr_o)) / lt
			} else if pr < 0 && pr_o >= 0 {
				NO3_o := st.soil_no3_aq[i - 1]
				st.convection[i] = ((NO3_u * pr) - (NO3_o * pr_o)) / lt
			}
		} else {
			// bottom layer
			pr_o := st.percolation_rate[i - 1] / 1000.0 * time_step_factor
			pr := st.soil_column.flux_at_lower_boundary / 1000.0 * time_step_factor

			if pr >= 0.0 && pr_o >= 0.0 {
				NO3_o := st.soil_no3_aq[i - 1]
				st.convection[i] = ((no3 * pr) - (NO3_o * pr_o)) / lt
			} else if pr >= 0 && pr_o < 0 {
				st.convection[i] = ((no3 * pr) - (no3 * pr_o)) / lt
			} else if pr < 0 && pr_o < 0 {
				st.convection[i] = (-(no3 * pr_o)) / lt
			} else if pr < 0 && pr_o >= 0 {
				NO3_o := st.soil_no3_aq[i - 1]
				st.convection[i] = (-(NO3_o * pr_o)) / lt
			}
		}
	}

	// Calculation of dispersion depending on pore water velocity
	for i in 0 ..< nols {
		pri := st.percolation_rate[i] / 1000.0 * time_step_factor
		pr0 := st.soil_column.layers[0].soil_water_flux / 1000.0 * time_step_factor
		lti := st.soil_column.layers[i].layer_thickness_m
		NO3i := st.soil_no3_aq[i]
		fci := st.soil_column.layers[i].field_capacity
		smi := st.soil_column.layers[i].soil_moisture_m3

		if i == nols - 1 {
			st.pore_water_velocity[i] = libc.fabs(pri / fci) // [m t-1]
			soil_moisture_gradient[i] = smi // [m3 m-3]
		} else {
			fcip1 := st.soil_column.layers[i + 1].field_capacity
			smip1 := st.soil_column.layers[i + 1].soil_moisture_m3
			st.pore_water_velocity[i] = libc.fabs(pri / ((fci + fcip1) * 0.5)) // [m t-1]
			soil_moisture_gradient[i] = (smi + smip1) * 0.5 // [m3 m-3]
		}

		st.diffusion_coeff[i] =
			diffusion_coeff_standard *
			(ad * libc.exp(soil_moisture_gradient[i] * 2.0 * 5.0) / soil_moisture_gradient[i]) *
			time_step_factor // [m2 t-1] * [t t-1]

		// Dispersion coefficient, old DB
		if i == 0 {
			st.dispersion_coeff[i] =
				soil_moisture_gradient[i] *
					(st.diffusion_coeff[i] + dispersion_length * st.pore_water_velocity[i]) -
				(0.5 * lti * libc.fabs(pri)) +
				((0.5 *
							st.env_params.time_step *
							time_step_factor *
							libc.fabs((pri + pr0) / 2.0)) *
						st.pore_water_velocity[i])
		} else {
			pr_o := st.percolation_rate[i - 1] / 1000.0 * time_step_factor // [m t-1]

			st.dispersion_coeff[i] =
				soil_moisture_gradient[i] *
					(st.diffusion_coeff[i] + dispersion_length * st.pore_water_velocity[i]) -
				(0.5 * lti * libc.fabs(pri)) +
				((0.5 *
							st.env_params.time_step *
							time_step_factor *
							libc.fabs((pri + pr_o) / 2.0)) *
						st.pore_water_velocity[i])
		}

		// old DISP = Gesamt-Dispersion (D in Diss S. 23)
		if i == 0 {
			NO3_u := st.soil_no3_aq[i + 1]
			st.dispersion[i] = -st.dispersion_coeff[i] * (NO3i - NO3_u) / (lti * lti)
		} else if i < nols - 1 {
			NO3_o := st.soil_no3_aq[i - 1]
			NO3_u := st.soil_no3_aq[i + 1]
			st.dispersion[i] =
				(st.dispersion_coeff[i - 1] * (NO3_o - NO3i) / (lti * lti)) -
				(st.dispersion_coeff[i] * (NO3i - NO3_u) / (lti * lti))
		} else {
			NO3_o := st.soil_no3_aq[i - 1]
			st.dispersion[i] = st.dispersion_coeff[i - 1] * (NO3_o - NO3i) / (lti * lti)
		}
	}

	if st.percolation_rate[leaching_depth_layer_idx] > 0.0 {
		// leaching depth layer index = chosen leaching depth
		lt := st.soil_column.layers[leaching_depth_layer_idx].layer_thickness_m
		NO3 := st.soil_no3_aq[leaching_depth_layer_idx]

		if leaching_depth_layer_idx < nols - 1 {
			pr_u := st.percolation_rate[leaching_depth_layer_idx + 1] / 1000.0 * time_step_factor // [m t-1]
			NO3_u := st.soil_no3_aq[leaching_depth_layer_idx + 1] // [kg m-3]
			// leaching_at_boundary: Summe fuer Auswaschung (Diff + Konv), old OUTSUM
			st.leaching_at_boundary +=
				((pr_u * NO3) / lt * 10000.0 * lt) +
				((st.dispersion_coeff[leaching_depth_layer_idx] * (NO3 - NO3_u)) /
						(lt * lt) *
						10000.0 *
						lt) // [kg ha-1]
		} else {
			pr_u := st.soil_column.flux_at_lower_boundary / 1000.0 * time_step_factor // [m t-1]
			st.leaching_at_boundary += pr_u * NO3 / lt * 10000.0 * lt // [kg ha-1]
		}
	} else {
		pr_u := st.percolation_rate[leaching_depth_layer_idx] / 1000.0 * time_step_factor
		lt := st.soil_column.layers[leaching_depth_layer_idx].layer_thickness_m
		NO3 := st.soil_no3_aq[leaching_depth_layer_idx]

		if leaching_depth_layer_idx < nols - 1 {
			NO3_u := st.soil_no3_aq[leaching_depth_layer_idx + 1]
			st.leaching_at_boundary +=
				((pr_u * NO3_u) / (lt * 10000.0 * lt)) +
				st.dispersion_coeff[leaching_depth_layer_idx] *
					(NO3 - NO3_u) /
					((lt * lt) * 10000.0 * lt) // [kg ha-1]
		}
	}

	st.leaching_at_boundary = max(0.0, st.leaching_at_boundary)

	// Update of NO3 concentration, including transformation back into [kg NO3-N m soil-3]
	for i in 0 ..< nols {
		smi := st.soil_column.layers[i].soil_moisture_m3
		st.soil_no3_aq[i] += (st.dispersion[i] - st.convection[i]) / smi
	}
}
