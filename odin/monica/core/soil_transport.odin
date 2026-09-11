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

import libc "core:c/libc"
import p "../params"

// C++: struct monica::SoilTransport
Soil_Transport :: struct {
	soilColumn:             ^Soil_Column,
	modParams:              p.Soil_Transport_Module_Parameters,
	siteParams:             ^p.Site_Parameters,
	envParams:              ^p.Environment_Parameters,
	cropModParams:          ^p.Crop_Module_Parameters,

	convection:             [dynamic]f64,
	diffusion_coeff:        [dynamic]f64,
	dispersion:             [dynamic]f64,
	dispersion_coeff:       [dynamic]f64,
	leaching_at_boundary:   f64,
	vc_NUptakeFromLayer:    [dynamic]f64,
	pore_water_velocity:    [dynamic]f64,
	vs_SoilMineralNContent: [dynamic]f64, // never resized - dead field in the C++ too (only touched by dropped (de)serialize)
	soil_no3:               [dynamic]f64,
	soil_no3_aq:            [dynamic]f64,
	time_step:              f64,
	total_dispersion:       [dynamic]f64,
	percolation_rate:       [dynamic]f64,

	cropModule:             ^Crop_Module,
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
	st.soilColumn = soil_column
	st.siteParams = site_params
	st.modParams = mod_params
	st.envParams = env_params
	st.cropModParams = crop_mod_params
	st.time_step = 1.0 // C++ in-class initialiser: double time_step{1.0}

	scSize := len(soil_column.layers)
	resize(&st.convection, scSize)
	resize(&st.diffusion_coeff, scSize)
	resize(&st.dispersion, scSize)
	resize(&st.dispersion_coeff, scSize)
	for i in 0 ..< scSize {
		st.dispersion_coeff[i] = 1.0
	}
	resize(&st.vc_NUptakeFromLayer, scSize)
	resize(&st.pore_water_velocity, scSize)
	resize(&st.soil_no3, scSize)
	resize(&st.soil_no3_aq, scSize)
	resize(&st.total_dispersion, scSize)
	resize(&st.percolation_rate, scSize)

	return st
}

// C++: void monica::soiltransport::step(SoilTransport*)
soil_transport_step :: proc(st: ^Soil_Transport) {
	minTimeStepFactor := 1.0 // [t t-1]
	nols := len(st.soilColumn.layers)

	for i in 0 ..< nols {
		st.soil_no3[i] = st.soilColumn.layers[i].soil_no3

		st.vc_NUptakeFromLayer[i] = st.cropModule != nil ? st.cropModule.n_uptake_from_layer[i] : 0
		if i == nols-1 {
			st.percolation_rate[i] = st.soilColumn.vs_FluxAtLowerBoundary // [mm]
		} else {
			st.percolation_rate[i] = st.soilColumn.layers[i+1].soil_water_flux // [mm]
		}
		// Variable time step in case of high water fluxes to ensure stable numerics
		pri := st.percolation_rate[i]
		timeStepFactorCurrentLayer := minTimeStepFactor
		if -5.0 <= pri && pri <= 5.0 && minTimeStepFactor > 1.0 {
			timeStepFactorCurrentLayer = 1.0
		} else if (-10.0 <= pri && pri < -5.0) || (5.0 < pri && pri <= 10.0) {
			timeStepFactorCurrentLayer = 0.5
		} else if (-15.0 <= pri && pri < -10.0) || (10.0 < pri && pri <= 15.0) {
			timeStepFactorCurrentLayer = 0.25
		} else if pri < -15.0 || pri > 15.0 {
			timeStepFactorCurrentLayer = 0.125
		}

		minTimeStepFactor = min(minTimeStepFactor, timeStepFactorCurrentLayer)
	}

	n_deposition(st)
	n_uptake(st)

	// Nitrate transport is called according to the set time step
	st.leaching_at_boundary = 0.0
	for i_TimeStep := 0; f64(i_TimeStep) < (1.0 / minTimeStepFactor); i_TimeStep += 1 {
		n_transport(st, st.envParams.p_LeachingDepth, minTimeStepFactor)
	}

	for i in 0 ..< nols {
		st.soil_no3[i] = st.soil_no3_aq[i] * st.soilColumn.layers[i].soil_moisture_m3

		if st.soil_no3[i] < 0.0 {
			st.soil_no3[i] = 0.0
		}

		st.soilColumn.layers[i].soil_no3 = st.soil_no3[i]
	}
}

// C++: void monica::soiltransport::nDeposition(SoilTransport*)
//
// Kersebaum 1989. Daily N deposition, transformed from an annual value, added
// to the ammonium... (really nitrate: soil_no3) pool of the top soil layer.
n_deposition :: proc(st: ^Soil_Transport) {
	dailyNDeposition := st.siteParams.vq_NDeposition / 365.0

	st.soil_no3[0] += dailyNDeposition / (10000.0 * st.soilColumn.layers[0].layer_thickness)
}

// C++: void monica::soiltransport::nUptake(SoilTransport*)
//
// Kersebaum 1989.
n_uptake :: proc(st: ^Soil_Transport) {
	nols := len(st.soilColumn.layers)
	cropNUptake := 0.0
	for i in 0 ..< nols {
		lti := st.soilColumn.layers[i].layer_thickness
		smi := st.soilColumn.layers[i].soil_moisture_m3

		// Lower boundary for N exploitation per layer
		if st.vc_NUptakeFromLayer[i] > ((st.soil_no3[i] * lti) - st.cropModParams.pc_MinimumAvailableN) {
			st.vc_NUptakeFromLayer[i] = ((st.soil_no3[i] * lti) - st.cropModParams.pc_MinimumAvailableN)
		} // Crop N uptake from layer i [kg N m-2]

		if st.vc_NUptakeFromLayer[i] < 0 {
			st.vc_NUptakeFromLayer[i] = 0
		}

		cropNUptake += st.vc_NUptakeFromLayer[i]

		// Subtracting crop N uptake
		st.soil_no3[i] -= st.vc_NUptakeFromLayer[i] / lti

		// Calculation of solute NO3 concentration on the basis of the soil
		// moisture content before movement of current time step
		// (kg m soil-3 --> kg m solute-3)
		st.soil_no3_aq[i] = st.soil_no3[i] / smi
	}

	st.soilColumn.vq_CropNUptake = cropNUptake // [kg m-2]
}

// C++: void monica::soiltransport::nTransport(SoilTransport*, double, double)
//
// Kersebaum 1989.
n_transport :: proc(st: ^Soil_Transport, leachingDepth, timeStepFactor: f64) {
	diffusionCoeffStandard := st.modParams.pq_DiffusionCoefficientStandard // [m2 d-1]; old D0
	AD := st.modParams.pq_AD // Factor a in Kersebaum 1989 p.24 for Loess soils
	dispersionLength := st.modParams.pq_DispersionLength // [m]
	soilProfile := 0.0
	leachingDepthLayerIndex := 0
	nols := len(st.soilColumn.layers)
	soilMoistureGradient := make([dynamic]f64, nols, context.temp_allocator)

	for i in 0 ..< nols {
		soilProfile += st.soilColumn.layers[i].layer_thickness
		if (soilProfile - 0.001) < leachingDepth {
			leachingDepthLayerIndex = i
		}
	}

	// Calculation of convection for different cases of flux direction
	for i in 0 ..< nols {
		wf0 := st.soilColumn.layers[0].soil_water_flux
		lt := st.soilColumn.layers[i].layer_thickness
		NO3 := st.soil_no3_aq[i]

		if i == 0 {
			pr := st.percolation_rate[i] / 1000.0 * timeStepFactor // [mm t-1 --> m t-1]
			NO3_u := st.soil_no3_aq[i+1]

			if pr >= 0.0 && wf0 >= 0.0 {
				st.convection[i] = (NO3 * pr) / lt // old KONV = Konvektion Diss S. 23
			} else if pr >= 0 && wf0 < 0 {
				st.convection[i] = (NO3 * pr) / lt
			} else if pr < 0 && wf0 < 0 {
				st.convection[i] = (NO3_u * pr) / lt
			} else if pr < 0 && wf0 >= 0 {
				st.convection[i] = (NO3_u * pr) / lt
			}
		} else if i < nols-1 {
			// layer > 0 && < bottom
			pr_o := st.percolation_rate[i-1] / 1000.0 * timeStepFactor
			pr := st.percolation_rate[i] / 1000.0 * timeStepFactor
			NO3_u := st.soil_no3_aq[i+1]

			if pr >= 0.0 && pr_o >= 0.0 {
				NO3_o := st.soil_no3_aq[i-1]
				st.convection[i] = ((NO3 * pr) - (NO3_o * pr_o)) / lt
			} else if pr >= 0 && pr_o < 0 {
				st.convection[i] = ((NO3 * pr) - (NO3 * pr_o)) / lt
			} else if pr < 0 && pr_o < 0 {
				st.convection[i] = ((NO3_u * pr) - (NO3 * pr_o)) / lt
			} else if pr < 0 && pr_o >= 0 {
				NO3_o := st.soil_no3_aq[i-1]
				st.convection[i] = ((NO3_u * pr) - (NO3_o * pr_o)) / lt
			}
		} else {
			// bottom layer
			pr_o := st.percolation_rate[i-1] / 1000.0 * timeStepFactor
			pr := st.soilColumn.vs_FluxAtLowerBoundary / 1000.0 * timeStepFactor

			if pr >= 0.0 && pr_o >= 0.0 {
				NO3_o := st.soil_no3_aq[i-1]
				st.convection[i] = ((NO3 * pr) - (NO3_o * pr_o)) / lt
			} else if pr >= 0 && pr_o < 0 {
				st.convection[i] = ((NO3 * pr) - (NO3 * pr_o)) / lt
			} else if pr < 0 && pr_o < 0 {
				st.convection[i] = (-(NO3 * pr_o)) / lt
			} else if pr < 0 && pr_o >= 0 {
				NO3_o := st.soil_no3_aq[i-1]
				st.convection[i] = (-(NO3_o * pr_o)) / lt
			}
		}
	}

	// Calculation of dispersion depending on pore water velocity
	for i in 0 ..< nols {
		pri := st.percolation_rate[i] / 1000.0 * timeStepFactor
		pr0 := st.soilColumn.layers[0].soil_water_flux / 1000.0 * timeStepFactor
		lti := st.soilColumn.layers[i].layer_thickness
		NO3i := st.soil_no3_aq[i]
		fci := st.soilColumn.layers[i].field_capacity
		smi := st.soilColumn.layers[i].soil_moisture_m3

		if i == nols-1 {
			st.pore_water_velocity[i] = libc.fabs(pri / fci) // [m t-1]
			soilMoistureGradient[i] = smi // [m3 m-3]
		} else {
			fcip1 := st.soilColumn.layers[i+1].field_capacity
			smip1 := st.soilColumn.layers[i+1].soil_moisture_m3
			st.pore_water_velocity[i] = libc.fabs(pri / ((fci + fcip1) * 0.5)) // [m t-1]
			soilMoistureGradient[i] = (smi + smip1) * 0.5 // [m3 m-3]
		}

		st.diffusion_coeff[i] =
			diffusionCoeffStandard *
			(AD * libc.exp(soilMoistureGradient[i]*2.0*5.0) / soilMoistureGradient[i]) *
			timeStepFactor // [m2 t-1] * [t t-1]

		// Dispersion coefficient, old DB
		if i == 0 {
			st.dispersion_coeff[i] =
				soilMoistureGradient[i]*(st.diffusion_coeff[i]+dispersionLength*st.pore_water_velocity[i]) -
				(0.5 * lti * libc.fabs(pri)) +
				((0.5 * st.envParams.p_timeStep * timeStepFactor * libc.fabs((pri+pr0)/2.0)) *
						st.pore_water_velocity[i])
		} else {
			pr_o := st.percolation_rate[i-1] / 1000.0 * timeStepFactor // [m t-1]

			st.dispersion_coeff[i] =
				soilMoistureGradient[i]*(st.diffusion_coeff[i]+dispersionLength*st.pore_water_velocity[i]) -
				(0.5 * lti * libc.fabs(pri)) +
				((0.5 * st.envParams.p_timeStep * timeStepFactor * libc.fabs((pri+pr_o)/2.0)) *
						st.pore_water_velocity[i])
		}

		// old DISP = Gesamt-Dispersion (D in Diss S. 23)
		if i == 0 {
			NO3_u := st.soil_no3_aq[i+1]
			st.dispersion[i] = -st.dispersion_coeff[i] * (NO3i - NO3_u) / (lti * lti)
		} else if i < nols-1 {
			NO3_o := st.soil_no3_aq[i-1]
			NO3_u := st.soil_no3_aq[i+1]
			st.dispersion[i] =
				(st.dispersion_coeff[i-1] * (NO3_o - NO3i) / (lti * lti)) -
				(st.dispersion_coeff[i] * (NO3i - NO3_u) / (lti * lti))
		} else {
			NO3_o := st.soil_no3_aq[i-1]
			st.dispersion[i] = st.dispersion_coeff[i-1] * (NO3_o - NO3i) / (lti * lti)
		}
	}

	if st.percolation_rate[leachingDepthLayerIndex] > 0.0 {
		// leaching depth layer index = chosen leaching depth
		lt := st.soilColumn.layers[leachingDepthLayerIndex].layer_thickness
		NO3 := st.soil_no3_aq[leachingDepthLayerIndex]

		if leachingDepthLayerIndex < nols-1 {
			pr_u := st.percolation_rate[leachingDepthLayerIndex+1] / 1000.0 * timeStepFactor // [m t-1]
			NO3_u := st.soil_no3_aq[leachingDepthLayerIndex+1] // [kg m-3]
			// leaching_at_boundary: Summe fuer Auswaschung (Diff + Konv), old OUTSUM
			st.leaching_at_boundary +=
				((pr_u * NO3) / lt * 10000.0 * lt) +
				((st.dispersion_coeff[leachingDepthLayerIndex] * (NO3 - NO3_u)) / (lt * lt) * 10000.0 * lt) // [kg ha-1]
		} else {
			pr_u := st.soilColumn.vs_FluxAtLowerBoundary / 1000.0 * timeStepFactor // [m t-1]
			st.leaching_at_boundary += pr_u * NO3 / lt * 10000.0 * lt // [kg ha-1]
		}
	} else {
		pr_u := st.percolation_rate[leachingDepthLayerIndex] / 1000.0 * timeStepFactor
		lt := st.soilColumn.layers[leachingDepthLayerIndex].layer_thickness
		NO3 := st.soil_no3_aq[leachingDepthLayerIndex]

		if leachingDepthLayerIndex < nols-1 {
			NO3_u := st.soil_no3_aq[leachingDepthLayerIndex+1]
			st.leaching_at_boundary +=
				((pr_u * NO3_u) / (lt * 10000.0 * lt)) +
				st.dispersion_coeff[leachingDepthLayerIndex] * (NO3 - NO3_u) / ((lt * lt) * 10000.0 * lt) // [kg ha-1]
		}
	}

	st.leaching_at_boundary = max(0.0, st.leaching_at_boundary)

	// Update of NO3 concentration, including transformation back into [kg NO3-N m soil-3]
	for i in 0 ..< nols {
		smi := st.soilColumn.layers[i].soil_moisture_m3
		st.soil_no3_aq[i] += (st.dispersion[i] - st.convection[i]) / smi
	}
}

// C++: void monica::soiltransport::putCrop(SoilTransport*, CropModule*)
soil_transport_put_crop :: proc(st: ^Soil_Transport, cm: ^Crop_Module) {
	st.cropModule = cm
}

// C++: void monica::soiltransport::removeCrop(SoilTransport*)
soil_transport_remove_crop :: proc(st: ^Soil_Transport) {
	st.cropModule = nil
}
