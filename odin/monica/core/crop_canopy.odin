package core

// The canopy component (plan-component-interfaces.md D3; architecture.md
// sections 5 and 12): size, leaf area, soil coverage, Kc and the FAO-56
// dual-Kc Kcb curve. Its state is Crop_Canopy (embedded in Crop_Module), its
// day's inputs are Canopy_Kc_In and Canopy_Growth_In, its parameters the
// crop's. Photosynthesis, water uptake and Crop_To_Soil read its state.
//
// A new canopy model would implement this interface - the same inputs, the
// same state - and so be swappable against these procedures.

import p "../params"
import "../../support/cr_math"

// Canopy: size, leaf area, soil coverage, Kc and the FAO-56 dual-Kc Kcb curve (fc_crop_size,
// fc_crop_green_area, fc_soil_coverage, fc_kc_factor, the dual-Kc block in the step).
Crop_Canopy :: struct {
	crop_height:      f64 `unit:"m"`,
	crop_diameter:    f64,
	leaf_area_index:  f64 `unit:"m2 m-2"`, //! old LAI
	green_area_index: f64,
	soil_coverage:    f64,
	kc_factor:        f64 `unit:"1"`, //! old FKc
	// vs_soil_specific_max_rooting_depth:      f64,
	kcb_factor:       f64 `unit:"1"`, // Current daily Kcb (output of GDD-based 4-phase interpolation)
	kcb_ini:          f64, // Initial/germination phase Kcb (flat, Phase 1)
	kcb_mid:          f64, // Mid-season plateau Kcb (Phase 3)
	kcb_end:          f64, // End of late-season Kcb target (Phase 4)
}

// What canopy_kc_factor and canopy_kcb_factor read on a day.
Canopy_Kc_In :: struct {
	developmental_stage:     int `unit:"" src:"Phenology"`,
	current_temperature_sum: []f64 `unit:"°C d" src:"Phenology"`, // per stage
	bare_soil_kc_factor:     f64 `unit:"1" src:"site"`,
}

// C++: the Kc part of step() and fcKcFactor(const CropModule*, double
// d_StageTemperatureSum, double d_CurrentTemperatureSum, double
// d_StageKcFactor, double d_EarlierStageKcFactor). fcKcFactor's branch for
// stage 0 (interpolating from the species' initial Kc) is not reachable from
// step(), which uses the site's bare-soil Kc at stage 0; it is left out.
canopy_kc_factor :: proc(c: ^Crop_Canopy, ps: ^p.Crop_Parameters, input: Canopy_Kc_In) {
	stage := input.developmental_stage
	if stage == 0 {
		c.kc_factor = input.bare_soil_kc_factor // @todo Claas: muss hier etwas Genaueres hin, siehe FAO?
		return
	}
	stage_temperature_sum := ps.cultivar_params.stage_temperature_sum[stage]
	current_temperature_sum := input.current_temperature_sum[stage]
	stage_kc_factor := ps.cultivar_params.stage_kc_factor[stage]
	earlier_stage_kc_factor := ps.cultivar_params.stage_kc_factor[stage - 1]

	relative_development := 0.0
	if stage_temperature_sum > 0.0 {
		relative_development = min(current_temperature_sum / stage_temperature_sum, 1.0) // old relint
	}
	// Interpolating the Kc Factors
	c.kc_factor =
		earlier_stage_kc_factor + ((stage_kc_factor - earlier_stage_kc_factor) * relative_development)
}

// FAO-56 Dual Kc: GDD-based 4-phase trapezoidal Kcb curve (replaces static
// per-stage arrays). Phase 1 (flat initial) | Phase 2 (linear ascent) |
// Phase 3 (mid-season) | Phase 4 (descent). Not a cropmodule:: function in
// C++ - inline in step() itself there. Leaves kcb_factor as it is for a
// cultivar without stage Kc factors.
canopy_kcb_factor :: proc(c: ^Crop_Canopy, ps: ^p.Crop_Parameters, input: Canopy_Kc_In) {
	stage_kc_factor := ps.cultivar_params.stage_kc_factor
	stage_temperature_sum := ps.cultivar_params.stage_temperature_sum
	n_stages := len(stage_kc_factor)
	if n_stages > 0 {
		// Compute total elapsed GDD as sum of all completed stages plus
		// current stage progress
		elapsed_gdd := 0.0
		for s := 0; s < n_stages; s += 1 {
			elapsed_gdd += input.current_temperature_sum[s]
		}

		// Identify mid-season start: first stage where Kc equals the maximum
		max_kc := stage_kc_factor[0]
		for v in stage_kc_factor {
			if v > max_kc {
				max_kc = v
			}
		}
		mid_stage_start := n_stages - 1
		for i := 1; i < n_stages; i += 1 {
			if stage_kc_factor[i] >= max_kc - 1e-6 {
				mid_stage_start = i
				break
			}
		}
		// Identify late-season start: first stage after plateau where Kc drops
		late_stage_start := n_stages - 1
		for i := mid_stage_start + 1; i < n_stages; i += 1 {
			if stage_kc_factor[i] < max_kc - 1e-6 {
				late_stage_start = i
				break
			}
		}

		// GDD boundaries
		gdd_phase1_end := stage_temperature_sum[0] // end of germination/initial phase
		gdd_to_mid := 0.0
		for i := 0; i < mid_stage_start; i += 1 {
			gdd_to_mid += stage_temperature_sum[i]
		}
		gdd_late_start := 0.0
		for i := 0; i < late_stage_start; i += 1 {
			gdd_late_start += stage_temperature_sum[i]
		}
		gdd_late_total := 0.0
		for i := late_stage_start; i < n_stages; i += 1 {
			gdd_late_total += stage_temperature_sum[i]
		}

		if elapsed_gdd <= gdd_phase1_end {
			// Phase 1: flat initial
			c.kcb_factor = c.kcb_ini
		} else if input.developmental_stage < mid_stage_start {
			// Phase 2: linear development ascent
			denom := gdd_to_mid - gdd_phase1_end
			frac := denom > 0.0 ? min(1.0, (elapsed_gdd - gdd_phase1_end) / denom) : 1.0
			c.kcb_factor = c.kcb_ini + frac * (c.kcb_mid - c.kcb_ini)
		} else if input.developmental_stage < late_stage_start {
			// Phase 3: mid-season plateau
			c.kcb_factor = c.kcb_mid
		} else {
			// Phase 4: late-season linear descent
			gdd_since_late := elapsed_gdd - gdd_late_start
			frac := gdd_late_total > 0.0 ? min(1.0, gdd_since_late / gdd_late_total) : 1.0
			c.kcb_factor = c.kcb_mid + frac * (c.kcb_end - c.kcb_mid)
		}
		c.kcb_factor = max(0.0, c.kcb_factor)
	}
}

// What canopy_growth reads on a day.
Canopy_Growth_In :: struct {
	developmental_stage:              int `unit:"" src:"Phenology"`,
	mean_air_temperature:             f64 `unit:"°C" src:"weather"`,
	// the cultivar's, reduced when the other crop is taller (intercropping)
	max_crop_height:                  f64 `unit:"m"`,
	current_total_temperature_sum:    f64 `unit:"°C d" src:"Phenology"`,
	current_temperature_sum:          []f64 `unit:"°C d" src:"Phenology"`, // per stage
	leaf_growth_increment:            f64 `unit:"kgDM ha-1" src:"Growth" lag:"1d"`,
	leaf_senescence_increment:        f64 `unit:"kgDM ha-1" src:"Growth" lag:"1d"`,
	enable_t_response_leaf_expansion: bool `unit:"" src:"crop module parameters"`,
	time_step:                        f64 `unit:"d"`,
}

// Crop size, green area and soil coverage, in the C++ order (fcCropSize,
// fcCropGreenArea, fcSoilCoverage).
canopy_growth :: proc(c: ^Crop_Canopy, ps: ^p.Crop_Parameters, input: Canopy_Growth_In) {
	canopy_size(c, ps, input)
	canopy_green_area(c, ps, input)
	// C++: double monica::cropmodule::fcSoilCoverage(const CropModule*)
	c.soil_coverage = 1.0 - cr_math.exp(-0.5 * c.leaf_area_index)
}

// C++: void monica::cropmodule::fcCropSize(CropModule*, double maxCropHeight)
@(private)
canopy_size :: proc(c: ^Crop_Canopy, ps: ^p.Crop_Parameters, input: Canopy_Growth_In) {
	stage_temperature_sum := ps.cultivar_params.stage_temperature_sum

	total_temperature_sum_for_height := 0.0
	for stage := 1; f64(stage) < ps.species_params.stage_at_max_height + 1; stage += 1 {
		total_temperature_sum_for_height += stage_temperature_sum[stage]
	}
	relative_total_development_for_height := min(
		input.current_total_temperature_sum / total_temperature_sum_for_height,
		1.0,
	)
	if relative_total_development_for_height > 0.0 {
		c.crop_height =
			input.max_crop_height /
			(1.0 +
					cr_math.exp(
						-ps.cultivar_params.crop_height_p1 *
						(relative_total_development_for_height -
								ps.cultivar_params.crop_height_p2),
					))
	} else {
		c.crop_height = 0.0
	}

	total_temperature_sum_for_diameter := 0.0
	for stage := 1; f64(stage) < ps.species_params.stage_at_max_diameter + 1; stage += 1 {
		total_temperature_sum_for_diameter += stage_temperature_sum[stage]
	}
	relative_total_development_for_diameter := min(
		input.current_total_temperature_sum / total_temperature_sum_for_diameter,
		1.0,
	)
	if relative_total_development_for_diameter > 0.0 {
		c.crop_diameter =
			ps.species_params.max_crop_diameter * relative_total_development_for_diameter
	} else {
		c.crop_diameter = 0.0
	}
}

// C++: void monica::cropmodule::fcCropGreenArea(CropModule*, double
// vw_MeanAirTemperature, double d_LeafBiomassIncrement, double
// d_LeafBiomassDecrement, double d_SpecificLeafAreaStart, double
// d_SpecificLeafAreaEnd, double d_SpecificLeafAreaEarly, double
// d_StageTemperatureSum, double d_CurrentTemperatureSum)
@(private)
canopy_green_area :: proc(c: ^Crop_Canopy, ps: ^p.Crop_Parameters, input: Canopy_Growth_In) {
	stage := input.developmental_stage
	mean_air_temperature := input.mean_air_temperature
	leaf_biomass_increment := input.leaf_growth_increment
	leaf_biomass_decrement := input.leaf_senescence_increment
	specific_leaf_area_start := ps.cultivar_params.specific_leaf_area[stage - 1]
	specific_leaf_area_end := ps.cultivar_params.specific_leaf_area[stage]
	specific_leaf_area_early := ps.cultivar_params.specific_leaf_area[1]
	stage_temperature_sum := ps.cultivar_params.stage_temperature_sum[stage]
	current_temperature_sum := input.current_temperature_sum[stage]

	temp_response_expansion := 1.0
	if input.enable_t_response_leaf_expansion {
		// Stage switch T response leaf exp (wheat = 2, maize = -1 (deactivated))
		if stage + 1 <= ps.species_params.transition_stage_leaf_exp {
			// Early stages leaf expansion T response
			reference_temp_response_expansion :=
				223.9 * cr_math.exp(-5.03 * cr_math.exp(-0.0653 * ps.cultivar_params.early_ref_leaf_exp))
			temp_response_expansion = min(
				223.9 *
				cr_math.exp(-5.03 * cr_math.exp(-0.0653 * mean_air_temperature)) /
				reference_temp_response_expansion,
				1.3,
			)
		} else {
			// leaf expansion T response
			reference_temp_response_expansion :=
				37.7 * cr_math.exp(-7.23 * cr_math.exp(-0.1462 * ps.cultivar_params.ref_leaf_exp))
			temp_response_expansion = min(
				37.7 *
				cr_math.exp(-7.23 * cr_math.exp(-0.1462 * mean_air_temperature)) /
				reference_temp_response_expansion,
				1.3,
			)
		}
	}

	c.leaf_area_index +=
		(leaf_biomass_increment *
			temp_response_expansion *
			(specific_leaf_area_start +
					(current_temperature_sum /
							stage_temperature_sum *
							(specific_leaf_area_end - specific_leaf_area_start))) *
			input.time_step) -
		(leaf_biomass_decrement * specific_leaf_area_early * input.time_step) // [ha ha-1]

	if c.leaf_area_index <= 0.0 {
		c.leaf_area_index = 0.001
	}
	PI :: 3.14159265358979323
	c.green_area_index =
		c.leaf_area_index + (c.crop_height * PI * c.crop_diameter * f64(ps.species_params.plant_density)) // [m2 m-2]
}

// Transplanting (force_transplant_state): the transplant's leaf area.
canopy_transplant :: proc(c: ^Crop_Canopy, leaf_area_index: f64) {
	c.leaf_area_index = leaf_area_index
}
