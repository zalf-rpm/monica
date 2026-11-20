package core

// N uptake (plan-component-interfaces.md F4; architecture.md section 5): per
// layer by mass flow and diffusion, N fixation, the crop's N contents and
// concentrations. State: Crop_N_Uptake (embedded in Crop_Module); the
// per-layer uptake goes to soil transport through Crop_To_Soil.

import tl "../../support/tools"
import p "../params"
import libc "core:c/libc"
import "../../support/cr_math"

// N uptake: per layer, fixation, N contents (fc_crop_n_uptake).
Crop_N_Uptake :: struct {
	n_uptake_from_layer:                 [dynamic]f64 `unit:"kgN m-2" unit_guess:"high"`, //! old PE
	fixed_n:                             f64 `unit:"kgN ha-1"`,
	n_concentration_aboveground_biomass: f64 `unit:"kgN kgDM-1"`, //! old GEHOB
	n_concentration_root:                f64 `unit:"kgN kgDM-1"`, //! old WUGEH
	total_biomass_n_content:             f64 `unit:"kgN ha-1"`, //! old PESUM
	total_n_input:                       f64 `unit:"kgN ha-1" unit_guess:"high"`,
	total_n_uptake:                      f64 `unit:"kgN ha-1"`, //! old SUMPE
	sum_total_n_uptake:                  f64 `unit:"kgN ha-1"`, //! summation of all calculated NUptake; needed for sensitivity analysis
	n_content_deficit:                   f64 `unit:"kgN ha-1" unit_guess:"low"`,
}

// What n_uptake_step reads on a day.
N_Uptake_In :: struct {
	groundwater_table_layer_idx:             int `unit:"layer#" src:"soil"`,
	soil_layers:                             []Soil_Layer `src:"soil"`, // NO3, moisture
	transpiration:                           []f64 `unit:"mm" src:"Water_Uptake"`, // per layer, today's
	crop_n_demand:                           f64 `unit:"kgN m-2" src:"Growth"`,
	aboveground_biomass:                     f64 `unit:"kgDM ha-1" src:"Growth"`,
	aboveground_biomass_old:                 f64 `unit:"kgDM ha-1" src:"Growth"`,
	belowground_biomass:                     f64 `unit:"kgDM ha-1" src:"Growth"`,
	belowground_biomass_old:                 f64 `unit:"kgDM ha-1" src:"Growth"`,
	n_concentration_aboveground_biomass_old: f64 `unit:"kgN kgDM-1" src:"N_Status"`,
	root_biomass:                            f64 `unit:"kgDM ha-1" src:"Root_System"`,
	root_biomass_old:                        f64 `unit:"kgDM ha-1" src:"Root_System"`,
	root_density:                            []f64 `unit:"m m-3" src:"Root_System"`,
	root_diameter:                           []f64 `unit:"m" src:"Root_System"`,
	rooting_zone:                            int `unit:"layer#" src:"Root_System"`,
	developmental_stage:                     int `unit:"" src:"Phenology"`,
	final_developmental_stage:               int `unit:"" src:"Phenology"`,
	time_step:                               f64 `unit:"d"`,
	tortuosity:                              f64 `unit:"1" src:"crop module parameters"`,
	minimum_available_n:                     f64 `unit:"kgN m-2" src:"crop module parameters"`,
	minimum_n_concentration_root:            f64 `unit:"kgN kgDM-1" src:"crop module parameters"`,
	max_crop_n_demand:                       f64 `unit:"kgN ha-1" src:"crop module parameters"`,
}

// Fills N_Uptake_In from the crop and its soil column (crop_module_step and
// the test drivers).
n_uptake_in :: proc(cm: ^Crop_Module) -> N_Uptake_In {
	return {
		groundwater_table_layer_idx = cm.soil_column.groundwater_table_layer_idx,
		soil_layers = cm.soil_column.layers[:],
		transpiration = cm.transpiration[:],
		crop_n_demand = cm.crop_n_demand,
		aboveground_biomass = cm.aboveground_biomass,
		aboveground_biomass_old = cm.aboveground_biomass_old,
		belowground_biomass = cm.belowground_biomass,
		belowground_biomass_old = cm.belowground_biomass_old,
		n_concentration_aboveground_biomass_old = cm.n_concentration_aboveground_biomass_old,
		root_biomass = cm.root_biomass,
		root_biomass_old = cm.root_biomass_old,
		root_density = cm.root_density[:],
		root_diameter = cm.root_diameter[:],
		rooting_zone = cm.rooting_zone,
		developmental_stage = cm.developmental_stage,
		final_developmental_stage = cm.final_developmental_stage,
		time_step = cm.time_step,
		tortuosity = cm.mod_params.tortuosity,
		minimum_available_n = cm.mod_params.minimum_available_n,
		minimum_n_concentration_root = cm.mod_params.minimum_n_concentration_root,
		max_crop_n_demand = cm.mod_params.max_crop_n_demand,
	}
}

// C++: void monica::cropmodule::fcCropNUptake(CropModule*, size_t
// vc_GroundwaterTable, double, double)
//
// The C++'s trailing two parameters are unused in its body (their names are
// commented out there); they are not in N_Uptake_In.
n_uptake_step :: proc(n: ^Crop_N_Uptake, ps: ^p.Crop_Parameters, input: N_Uptake_In) {
	tortuosity := input.tortuosity

	nols := len(input.soil_layers)
	layer_thickness := input.soil_layers[0].layer_thickness_m

	convective_n_uptake := 0.0 // old TRNSUM
	diffusive_n_uptake := 0.0 // old SUMDIFF
	convective_n_uptake_from_layer := make([dynamic]f64, nols, context.temp_allocator) // old MASS

	diffusion_coeff := make([dynamic]f64, nols, context.temp_allocator) // old D
	diffusive_n_uptake_from_layer := make([dynamic]f64, nols, context.temp_allocator) // old DIFF
	convective_n_uptake_1 := 0.0 // old MASSUM
	diffusive_n_uptake_1 := 0.0 // old DIFFSUM
	minimum_available_n := input.minimum_available_n // kg m-3
	minimum_n_concentration_root := input.minimum_n_concentration_root // kg kg-1
	max_crop_n_demand := input.max_crop_n_demand

	n.total_n_uptake = 0.0
	n.total_n_input = 0.0
	n.fixed_n = 0.0
	for &v in n.n_uptake_from_layer {
		v = 0.0
	}

	PI :: 3.14159265358979323

	// if the plant has matured, no N uptake occurs!
	if input.developmental_stage < input.final_developmental_stage {
		for i_layer := 0; i_layer < min(input.rooting_zone, input.groundwater_table_layer_idx); i_layer += 1 {
			soil_mineral_n_content_i := input.soil_layers[i_layer].soil_no3 // [kg m-3]

			// Convective N uptake per layer
			// ([mm -> m]) * ([kg m-3] / old WG [m3 m-3]) -> [kg m-2]
			convective_n_uptake_from_layer[i_layer] =
				(input.transpiration[i_layer] / 1000.0) *
				(soil_mineral_n_content_i / input.soil_layers[i_layer].soil_moisture_m3) *
				input.time_step

			convective_n_uptake += convective_n_uptake_from_layer[i_layer] // [kg m-2]

			diffusion_coeff[i_layer] =
				0.000214 *
				(tortuosity * cr_math.exp(input.soil_layers[i_layer].soil_moisture_m3 * 10)) /
				input.soil_layers[i_layer].soil_moisture_m3 // [m2 d-1]

			// ([m2 d-1] * [m3 m-3] * [m] * ([kg m-3])=[m3 m-3])=[m m-3] -> [kg m-2]
			diffusive_n_uptake_from_layer[i_layer] =
				(diffusion_coeff[i_layer] *
					input.soil_layers[i_layer].soil_moisture_m3 *
					2.0 *
					PI *
					input.root_diameter[i_layer] *
					(soil_mineral_n_content_i /
								1000.0 /
								input.soil_layers[i_layer].soil_moisture_m3 -
							0.000014) *
					libc.sqrt(PI * input.root_density[i_layer])) *
				input.root_density[i_layer] *
				1000.0 *
				input.time_step

			if diffusive_n_uptake_from_layer[i_layer] < 0.0 {
				diffusive_n_uptake_from_layer[i_layer] = 0
			}

			diffusive_n_uptake += diffusive_n_uptake_from_layer[i_layer] // [kg m-2]
		}

		for i_layer := 0; i_layer < min(input.rooting_zone, input.groundwater_table_layer_idx); i_layer += 1 {
			if input.crop_n_demand > 0.0 {
				if convective_n_uptake >= input.crop_n_demand {
					// convective N uptake is sufficient
					n.n_uptake_from_layer[i_layer] =
						input.crop_n_demand *
						convective_n_uptake_from_layer[i_layer] /
						convective_n_uptake
				} else {
					// N demand is not covered
					if (input.crop_n_demand - convective_n_uptake) < diffusive_n_uptake {
						n.n_uptake_from_layer[i_layer] =
							convective_n_uptake_from_layer[i_layer] +
							((input.crop_n_demand - convective_n_uptake) *
									diffusive_n_uptake_from_layer[i_layer] /
									diffusive_n_uptake)
					} else {
						n.n_uptake_from_layer[i_layer] =
							convective_n_uptake_from_layer[i_layer] +
							diffusive_n_uptake_from_layer[i_layer]
					}
				}

				convective_n_uptake_1 += convective_n_uptake_from_layer[i_layer]
				diffusive_n_uptake_1 += diffusive_n_uptake_from_layer[i_layer]

				soil_mineral_n_content_i := input.soil_layers[i_layer].soil_no3 // [kg m-3]
				if n.n_uptake_from_layer[i_layer] >
				   ((soil_mineral_n_content_i * layer_thickness) - minimum_available_n) {
					n.n_uptake_from_layer[i_layer] =
						(soil_mineral_n_content_i * layer_thickness) - minimum_available_n
				}

				if n.n_uptake_from_layer[i_layer] > (max_crop_n_demand / 10000.0 * 0.75) {
					n.n_uptake_from_layer[i_layer] = max_crop_n_demand / 10000.0 * 0.75
				}

				if n.n_uptake_from_layer[i_layer] < 0.0 {
					n.n_uptake_from_layer[i_layer] = 0.0
				}
			} else {
				n.n_uptake_from_layer[i_layer] = 0.0
			}

			n.total_n_uptake += n.n_uptake_from_layer[i_layer] * 10000.0 // [kg m-2] -> [kg ha-1]
		}

		// Biological N Fixation - part of the deficit which can be covered
		// by biological N fixation.
		n.fixed_n =
			ps.species_params.part_biological_n_fixation *
			input.crop_n_demand *
			10000.0 // [kg N ha-1]

		if ((input.crop_n_demand * 10000.0) - n.total_n_uptake) < n.fixed_n {
			n.total_n_input = input.crop_n_demand * 10000.0
			n.fixed_n = (input.crop_n_demand * 10000.0) - n.total_n_uptake
		} else {
			n.total_n_input = n.total_n_uptake + n.fixed_n
		}
	}
	// vc_ConvectiveNUptake_1/vc_DiffusiveNUptake_1: computed but unused
	// elsewhere in the C++ too - genuine dead stores, kept for fidelity.
	_ = convective_n_uptake_1
	_ = diffusive_n_uptake_1

	n.sum_total_n_uptake += n.total_n_uptake
	n.total_biomass_n_content += n.total_n_input

	if input.root_biomass > input.root_biomass_old {
		// root has been growing
		n.n_concentration_root =
			((input.root_biomass_old * n.n_concentration_root) +
				((input.root_biomass - input.root_biomass_old) /
						(input.aboveground_biomass -
								input.aboveground_biomass_old +
								input.belowground_biomass -
								input.belowground_biomass_old +
								input.root_biomass -
								input.root_biomass_old) *
						n.total_n_input)) /
			input.root_biomass

		n.n_concentration_root = tl.bound(
			minimum_n_concentration_root,
			n.n_concentration_root,
			ps.species_params.stage_max_root_n_concentration[input.developmental_stage],
		)
	}

	n.n_concentration_aboveground_biomass =
		(n.total_biomass_n_content - (input.root_biomass * n.n_concentration_root)) /
		(input.aboveground_biomass +
				(input.belowground_biomass / ps.cultivar_params.residue_n_ratio))

	if (n.n_concentration_aboveground_biomass * input.aboveground_biomass) <
	   (input.n_concentration_aboveground_biomass_old * input.aboveground_biomass_old) {
		temp_n_concentration_aboveground_biomass :=
			input.n_concentration_aboveground_biomass_old *
			input.aboveground_biomass_old /
			input.aboveground_biomass

		temp_n_concentration_root :=
			(n.total_biomass_n_content -
				(n.n_concentration_aboveground_biomass * input.aboveground_biomass) -
				(n.n_concentration_aboveground_biomass *
						input.belowground_biomass /
						ps.cultivar_params.residue_n_ratio)) /
			input.root_biomass

		if temp_n_concentration_root >= minimum_n_concentration_root {
			n.n_concentration_aboveground_biomass = temp_n_concentration_aboveground_biomass
			n.n_concentration_root = temp_n_concentration_root
		}
	}
}

// Transplanting (force_transplant_state): the species' N concentrations, to
// prevent severe immediate starvation stress in new seedlings, and the N
// content of the transplant.
n_uptake_transplant :: proc(
	n: ^Crop_N_Uptake,
	ps: ^p.Crop_Parameters,
	aboveground_biomass, root_biomass: f64,
) {
	n.n_concentration_aboveground_biomass = ps.species_params.n_concentration_aboveground_biomass
	n.n_concentration_root = ps.species_params.n_concentration_root
	n.total_biomass_n_content =
		(aboveground_biomass * n.n_concentration_aboveground_biomass) +
		(root_biomass * n.n_concentration_root)
}

// Cutting (apply_cutting): the N content loses the cut aboveground biomass'
// share of the aboveground N.
n_uptake_after_cut :: proc(
	n: ^Crop_N_Uptake,
	aboveground_biomass, old_aboveground_biomass, old_aboveground_n_content: f64,
) {
	if old_aboveground_biomass > 0.0 {
		n.total_biomass_n_content -=
			(1 - aboveground_biomass / old_aboveground_biomass) * old_aboveground_n_content
	}
}
