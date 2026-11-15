package core

// The root system (plan-component-interfaces.md E1; architecture.md section
// 5): rooting depth and zone, root biomass, length, density and diameter.
// Cut out of the C++ fcCropDryMatter, which computed it after growth. State:
// Crop_Root_System (embedded in Crop_Module).

import p "../params"
import libc "core:c/libc"
import "../../support/cr_math"

// Root system state: rooting depth and zone, root biomass, density, diameter and length
// (root_system_step).
Crop_Root_System :: struct {
	rooting_depth:                      int `unit:"layer#"`, // C++ size_t, //! old WURZ
	rooting_depth_m:                    f64,
	rooting_zone:                       int `unit:"layer#"`, // C++ size_t
	max_rooting_depth:                  f64, //! old WURM
	// vs_soil_mineral_n_content:               [dynamic]f64, //! old C1
	soil_specific_max_rooting_depth:    f64, //! old WURZMAX [m]
	root_biomass:                       f64, //! old WUMAS
	root_biomass_old:                   f64, //! old WUMALT
	root_density:                       [dynamic]f64, //! old WUDICH
	root_diameter:                      [dynamic]f64, //! old WRAD
	total_root_length:                  f64, //! old WULAEN
	current_total_temperature_sum_root: f64,
}

// What root_system_step reads on a day.
Root_System_In :: struct {
	root_biomass:                       f64 `unit:"kgDM ha-1" src:"Growth"`, // today's root organ biomass
	dead_root_biomass:                  f64 `unit:"kgDM ha-1" src:"Growth"`, // today's
	n_concentration_root:               f64 `unit:"kgN kgDM-1" src:"N_Uptake" lag:"1d"`,
	// yesterday's transpiration deficit, clamped to >= 0 (crop_module_step)
	water_stress:                       f64 `unit:"0-1" src:"Water_Uptake" lag:"1d"`,
	developmental_stage:                int `unit:"" src:"Phenology"`,
	no_of_dev_stages:                   int `unit:"" src:"crop parameters"`,
	mean_air_temperature:               f64 `unit:"°C" src:"weather"`,
	soil_layers:                        []Soil_Layer `src:"soil"`,
	number_of_organic_layers:           int `unit:"" src:"soil"`,
	impenetrable_layer_depth:           f64 `unit:"m" src:"site"`,
	max_effective_rooting_depth:        f64 `unit:"m" src:"site"`,
	enable_pasw_root_penetration:       bool `unit:"" src:"crop module parameters"`,
	disable_daily_root_biomass_to_soil: bool `unit:"" src:"crop module parameters"`,
}

// The second part of the C++ fcCropDryMatter. Returns the day's dead root
// biomass per organic layer (empty if none or disabled), which goes to soil
// organic - in the C++ through the addOrganicMatter callback, right here.
root_system_step :: proc(
	r: ^Crop_Root_System,
	ps: ^p.Crop_Parameters,
	input: Root_System_In,
	allocator := context.allocator,
) -> (
	dead_roots: map[int]f64,
) {
	species_ps := &ps.species_params
	cultivar_ps := &ps.cultivar_params
	aboveground_organ := species_ps.aboveground_organ
	nols := len(input.soil_layers)
	layer_thickness := input.soil_layers[0].layer_thickness_m

	r.root_biomass_old = r.root_biomass
	r.root_biomass = input.root_biomass
	// vc_RootNIncrement: computed but unused elsewhere in the C++ too - a
	// genuine dead store, kept for fidelity.
	root_n_increment := 0.0 // old WUMM
	if r.root_biomass < r.root_biomass_old {
		root_n_increment = (r.root_biomass_old - r.root_biomass) * input.n_concentration_root
	} else {
		root_n_increment = 0
	}
	_ = root_n_increment

	layer_index_below_rooting_depth := min(r.rooting_depth, nols - 1)
	available_water_percentage := 0.0
	if input.enable_pasw_root_penetration {
		// In case of drought stress the root will grow deeper
		available_water :=
			input.soil_layers[layer_index_below_rooting_depth].field_capacity -
			input.soil_layers[layer_index_below_rooting_depth].permanent_wilting_point
		available_water_percentage =
			(input.soil_layers[layer_index_below_rooting_depth].soil_moisture_m3 -
				input.soil_layers[layer_index_below_rooting_depth].permanent_wilting_point) /
			available_water
		if available_water_percentage < 0.0 {
			available_water_percentage = 0.0
		}
	} else {
		if input.water_stress <
			   (0.95 * cultivar_ps.drought_stress_threshold[input.developmental_stage]) &&
		   cultivar_ps.crop_specific_max_rooting_depth >= 0.8 &&
		   r.rooting_depth_m > 0.95 * r.max_rooting_depth &&
		   input.developmental_stage < input.no_of_dev_stages - 1 { 	// only if crop-specific max rooting depth is deeper than 80cm
			r.max_rooting_depth += 0.005
		}
	}

	if r.max_rooting_depth > (f64(nols - 1) * layer_thickness) {
		r.max_rooting_depth = f64(nols - 1) * layer_thickness
	}

	// restrict root growth to everything above the impenetrable layer
	if input.impenetrable_layer_depth > 0 {
		r.max_rooting_depth = min(r.max_rooting_depth, input.impenetrable_layer_depth)
	}

	// Pedersen et al. (2010): Modelling diverse root density dynamics and
	// deep nitrogen uptake - a simple approach. Plant & Soil 326, 493-510.

	// Determining temperature sum for root growth
	maximum_temperature_root_growth := species_ps.minimum_temperature_root_growth + 20.0
	daily_temperature_root := 0.0
	if input.mean_air_temperature >= maximum_temperature_root_growth {
		daily_temperature_root =
			maximum_temperature_root_growth - species_ps.minimum_temperature_root_growth
	} else {
		daily_temperature_root = input.mean_air_temperature - species_ps.minimum_temperature_root_growth
	}
	if daily_temperature_root < 0.0 {
		daily_temperature_root = 0.0
	}
	r.current_total_temperature_sum_root += daily_temperature_root

	// Determining root penetration rate according to soil clay content [m degC-1 d-1]
	root_penetration_rate := 0.0
	if input.soil_layers[layer_index_below_rooting_depth].soil_clay_content <= 0.02 {
		root_penetration_rate = 0.5 * species_ps.root_penetration_rate
	} else if input.soil_layers[layer_index_below_rooting_depth].soil_clay_content <= 0.08 {
		root_penetration_rate =
			((1.0 / 3.0) +
				(0.5 / 0.06 * input.soil_layers[layer_index_below_rooting_depth].soil_clay_content)) *
			species_ps.root_penetration_rate
	} else {
		root_penetration_rate = species_ps.root_penetration_rate
	}
	if input.enable_pasw_root_penetration {
		if available_water_percentage <= 0.25 {
			root_penetration_rate =
				min(1.0, 4 * available_water_percentage) * root_penetration_rate
		} else {
			root_penetration_rate = species_ps.root_penetration_rate
		}
	}

	// Calculating rooting depth [m]
	if r.current_total_temperature_sum_root <= species_ps.root_growth_lag {
		r.rooting_depth_m = species_ps.initial_rooting_depth
	} else {
		r.rooting_depth_m += daily_temperature_root * root_penetration_rate
	}

	if r.rooting_depth_m <= species_ps.initial_rooting_depth {
		r.rooting_depth_m = species_ps.initial_rooting_depth
	}
	if r.rooting_depth_m > r.max_rooting_depth {
		r.rooting_depth_m = r.max_rooting_depth
	}
	if r.rooting_depth_m > input.max_effective_rooting_depth {
		r.rooting_depth_m = input.max_effective_rooting_depth
	}

	r.rooting_depth = min(int(libc.round(r.rooting_depth_m / layer_thickness)), nols) // layer no
	r.rooting_zone = min(int(libc.round(1.3 * r.rooting_depth_m / layer_thickness)), nols) // layer no

	r.total_root_length = r.root_biomass * species_ps.specific_root_length // [m m-2]

	// Calculating a root density distribution factor []
	root_density_factor, root_density_factor_sum := root_density_factor_and_sum(
		r.rooting_depth,
		r.rooting_zone,
		species_ps.root_form_factor,
		input.soil_layers,
		allocator,
	)

	// calculate the distribution of dead root biomass (for later addition
	// into AOM pools in soil-organic)
	if !input.disable_daily_root_biomass_to_soil {
		dead_roots = dead_roots_per_layer(
			input.dead_root_biomass,
			root_density_factor_sum,
			root_density_factor,
			r.rooting_zone,
			input.number_of_organic_layers,
			allocator,
		)
	}

	// Calculating root density per layer from total root length and a
	// relative root density distribution factor
	for i_layer := 0; i_layer < r.rooting_zone; i_layer += 1 {
		r.root_density[i_layer] =
			(root_density_factor[i_layer] / root_density_factor_sum) * r.total_root_length // [m m-3]
	}

	for i_layer := 0; i_layer < r.rooting_zone; i_layer += 1 {
		// Root diameter [m]
		if aboveground_organ[3] {
			r.root_diameter[i_layer] = 0.0002 - (f64(i_layer + 1) * 0.00001)
		} else {
			r.root_diameter[i_layer] = 0.0001
		}
	}

	return
}

// The root density distribution factor per layer and its sum over the
// rooting zone (C++ calcRootDensityFactorAndSum, on values).
root_density_factor_and_sum :: proc(
	rooting_depth, rooting_zone: int,
	root_form_factor: f64,
	soil_layers: []Soil_Layer,
	allocator := context.allocator,
) -> (
	[dynamic]f64,
	f64,
) {
	nols := len(soil_layers)
	layer_thickness := soil_layers[0].layer_thickness_m

	// Calculating a root density distribution factor []
	root_density_factor := make([dynamic]f64, nols, allocator)
	for i_layer := 0; i_layer < nols; i_layer += 1 {
		if i_layer < rooting_depth {
			root_density_factor[i_layer] = cr_math.exp(
				-root_form_factor * (f64(i_layer) * layer_thickness),
			)
		} else if i_layer < rooting_zone {
			root_density_factor[i_layer] =
				cr_math.exp(
					-root_form_factor *
					(f64(i_layer) * layer_thickness),
				) *
				(1.0 -
						(f64(i_layer - rooting_depth) /
								f64(rooting_zone - rooting_depth)))
		} else {
			root_density_factor[i_layer] = 0.0
		}
	}

	// Summing up all factors to scale to a relative factor between [0;1]
	root_density_factor_sum := 0.0
	for i_layer := 0; i_layer < rooting_zone; i_layer += 1 {
		root_density_factor_sum += root_density_factor[i_layer]
	}

	return root_density_factor, root_density_factor_sum
}

// The dead root biomass per organic layer (the distribution part of the C++
// fcMoveDeadRootBiomassToSoil); only amounts > 0.0001 kg ha-1.
dead_roots_per_layer :: proc(
	dead_root_biomass, root_density_factor_sum: f64,
	root_density_factor: [dynamic]f64,
	rooting_zone, number_of_organic_layers: int,
	allocator := context.allocator,
) -> map[int]f64 {
	nools := number_of_organic_layers

	layer2_dead_root_biomass_at_layer := make(map[int]f64, allocator)
	for i := 0; i < rooting_zone; i += 1 {
		dead_root_biomass_at_layer := root_density_factor[i] / root_density_factor_sum * dead_root_biomass
		// just add organic matter if > 0.0001
		if int(dead_root_biomass_at_layer * 10000) > 0 {
			idx := i < nools ? i : nools - 1
			layer2_dead_root_biomass_at_layer[idx] += dead_root_biomass_at_layer
		}
	}

	return layer2_dead_root_biomass_at_layer
}

// Transplanting (force_transplant_state): the root biomass, the species'
// initial rooting depth and zone, and the total root length.
root_system_transplant :: proc(
	r: ^Crop_Root_System,
	ps: ^p.Crop_Parameters,
	root_mass: f64,
	soil_layers: []Soil_Layer,
) {
	r.root_biomass = root_mass

	// Settle rooting zone and layers based on standard species parameters
	nols := len(soil_layers)
	layer_thickness := soil_layers[0].layer_thickness_m
	r.rooting_depth_m = ps.species_params.initial_rooting_depth
	r.rooting_depth = min(int(libc.round(r.rooting_depth_m / layer_thickness)), nols)
	r.rooting_zone = min(int(libc.round(1.3 * r.rooting_depth_m / layer_thickness)), nols)

	// Force total root length based on physical constants
	r.total_root_length =
		(r.root_biomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * 3.14159265358979323)
}
