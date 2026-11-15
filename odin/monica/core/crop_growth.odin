package core

// Growth and partitioning (plan-component-interfaces.md F2; architecture.md
// section 5): organ biomass, senescence, reallocation, the N demand. The first
// and last part of the C++ fcCropDryMatter (the root system in between is
// crop_root_system.odin). State: Crop_Growth (embedded in Crop_Module).

import p "../params"
import libc "core:c/libc"

// Growth and partitioning: organ biomass, senescence, reserves, N demand (first part of
// fc_crop_dry_matter).
Crop_Growth :: struct {
	storage_organ:                  int,
	organ_biomass:                  [dynamic]f64, //! old WORG
	organ_green_biomass:            [dynamic]f64,
	organ_dead_biomass:             [dynamic]f64, //! old WDORG
	organ_growth_increment:         [dynamic]f64 `unit:"kgDM ha-1"`, //! old GORG
	organ_senescence_increment:     [dynamic]f64, //! old DGORG
	aboveground_biomass:            f64 `unit:"kgDM ha-1"`, //! old OBMAS
	aboveground_biomass_old:        f64, //! old OBALT
	belowground_biomass:            f64,
	belowground_biomass_old:        f64,
	total_biomass:                  f64,
	reserve_assimilate_pool:        f64, //! old ASPOO
	net_photosynthesis:             f64 `unit:"kgCH2O ha-1"`, //! old GTW
	crop_n_demand:                  f64, //! old DTGESN
	max_n_uptake:                   f64, //! old MAXUP
	assimilate_part_coeffs_reduced: bool,
	dying_out:                      bool,
}

// What growth_step reads on a day.
Growth_In :: struct {
	assimilates:                 f64 `unit:"kgCH2O ha-1" src:"Photosynthesis"`,
	k_tkc:                       f64 `unit:"1" src:"Photosynthesis"`,
	crop_heat_redux:             f64 `unit:"0-1" src:"Stress"`,
	drought_impact_on_fertility: f64 `unit:"0-1" src:"Stress"`,
	crop_n_redux:                f64 `unit:"0-1" src:"N_Status"`,
	critical_n_concentration:    f64 `unit:"kgN kgDM-1" src:"N_Status"`,
	target_n_concentration:      f64 `unit:"kgN kgDM-1" src:"N_Status"`,
	developmental_stage:         int `unit:"" src:"Phenology"`,
	current_temperature_sum:     []f64 `unit:"°C d" src:"Phenology"`, // per stage
	n_concentration_root:        f64 `unit:"kgN kgDM-1" src:"N_Uptake" lag:"1d"`,
	// N uptake's; growth subtracts the dead roots' N and returns the new value
	total_biomass_n_content:     f64 `unit:"kgN ha-1" src:"N_Uptake"`,
	no_of_organs:                int `unit:"" src:"crop parameters"`,
	time_step:                   f64 `unit:"d"`,
	max_crop_n_demand:           f64 `unit:"kgN ha-1" src:"crop module parameters"`, // per day
}

// Fills Growth_In from the crop (crop_module_step and the test drivers).
growth_in :: proc(cm: ^Crop_Module, n_concentration_root: f64) -> Growth_In {
	return {
		assimilates = cm.assimilates,
		k_tkc = cm.k_tkc,
		crop_heat_redux = cm.crop_heat_redux,
		drought_impact_on_fertility = cm.drought_impact_on_fertility,
		crop_n_redux = cm.crop_n_redux,
		critical_n_concentration = cm.critical_n_concentration,
		target_n_concentration = cm.target_n_concentration,
		developmental_stage = cm.developmental_stage,
		current_temperature_sum = cm.current_temperature_sum[:],
		n_concentration_root = n_concentration_root,
		total_biomass_n_content = cm.total_biomass_n_content,
		no_of_organs = cm.no_of_organs,
		time_step = cm.time_step,
		max_crop_n_demand = cm.mod_params.max_crop_n_demand,
	}
}

// C++: void monica::cropmodule::fcCropDryMatter(CropModule*, double vw_MeanAirTemperature)
//
// Dry matter allocation within the crop: the result from crop photosynthesis
// is allocated to the different crop organs under consideration of stress
// factors (water, nitrogen, temperature), plus root growth (rooting depth,
// root density/diameter per layer) and handing daily dead root biomass off
// to soil organic matter via fcMoveDeadRootBiomassToSoil/add_organic_matter.
//
// Returns the day's dead root biomass (for the root system), the updated
// total biomass N content (N uptake's) and whether a perennial's growth cycle
// ended (phenology's) - the C++ writes the last two into the crop.
growth_step :: proc(
	g: ^Crop_Growth,
	ps: ^p.Crop_Parameters,
	input: Growth_In,
) -> (
	dead_root_biomass: f64,
	total_biomass_n_content: f64,
	growth_cycle_ended: bool,
) {
	species_ps := &ps.species_params
	cultivar_ps := &ps.cultivar_params
	aboveground_organ := species_ps.aboveground_organ
	assimilate_partitioning_coeff_arr := cultivar_ps.assimilate_partitioning_coeff
	assimilate_reallocation := species_ps.assimilate_reallocation
	organ_senescence_rate := cultivar_ps.organ_senescence_rate
	perennial := cultivar_ps.perennial
	stage_temperature_sum := cultivar_ps.stage_temperature_sum
	storage_organ := species_ps.storage_organ
	// N uptake's, updated here by the dead roots' N and returned
	total_biomass_n_content = input.total_biomass_n_content

	max_root_n_concentration := 0.0 // old WGM
	assimilate_partitioning_coeff_old := 0.0
	assimilate_partitioning_coeff := 0.0

	// Assuming that growth respiration takes 30% of total assimilation - from
	// AGROSIM algorithms (the HERMES alternative is commented out in the C++)
	g.net_photosynthesis = input.assimilates
	// TMP_Regulatory_factor: computed but never read again afterward in the
	// C++ either - a genuine dead store, kept for fidelity.
	tmp_regulatory_factor := species_ps.stage_mobil_from_storage_coeff[input.developmental_stage]
	if input.developmental_stage == 1 {
		tmp_regulatory_factor =
			species_ps.stage_mobil_from_storage_coeff[input.developmental_stage] * input.k_tkc
	}
	_ = tmp_regulatory_factor

	mobilization_from_storage :=
		g.organ_biomass[g.storage_organ] *
		species_ps.stage_mobil_from_storage_coeff[input.developmental_stage] *
		input.k_tkc

	g.reserve_assimilate_pool = 0.0

	g.aboveground_biomass_old = g.aboveground_biomass
	g.aboveground_biomass = 0.0
	g.belowground_biomass_old = g.belowground_biomass
	g.belowground_biomass = 0.0
	g.total_biomass = 0.0

	// Dry matter production - old NRKOM
	assimilate_partition_leaf := 0.05
	daily_dead_root_biomass_increment := 0.0
	for i_organ := 0; i_organ < input.no_of_organs; i_organ += 1 {
		// Prevent out-of-bounds array access on developmental stage indices
		// during early phases. If developmental_stage is 0, fall back to
		// index 0 as the previous stage index.
		prev_stage := input.developmental_stage > 0 ? input.developmental_stage - 1 : 0
		assimilate_partitioning_coeff_old = assimilate_partitioning_coeff_arr[prev_stage][i_organ]
		assimilate_partitioning_coeff =
			assimilate_partitioning_coeff_arr[input.developmental_stage][i_organ]

		// Identify storage organ and reduce assimilate flux in case of heat stress
		if storage_organ[i_organ] {
			assimilate_partitioning_coeff_old =
				assimilate_partitioning_coeff_old *
				input.crop_heat_redux *
				input.drought_impact_on_fertility
			assimilate_partitioning_coeff =
				assimilate_partitioning_coeff *
				input.crop_heat_redux *
				input.drought_impact_on_fertility
		}

		if (input.current_temperature_sum[input.developmental_stage] /
			   stage_temperature_sum[input.developmental_stage]) >
		   1 {
			// Pflanze ist ausgewachsen
			g.organ_growth_increment[i_organ] = 0.0
			g.organ_senescence_increment[i_organ] = 0.0
			if perennial {
				growth_cycle_ended = true
			}
		} else {
			// test if there is a positive balance of produced assimilates; if
			// net_photosynthesis is negative, the crop needs more for
			// maintenance than for building new biomass
			if g.net_photosynthesis < 0.0 {
				// reduce biomass from leaf and shoot because of negative assimilate
				if i_organ == Organ_Leaf {
					incr := assimilate_partition_leaf * g.net_photosynthesis
					if libc.fabs(incr) <= g.organ_biomass[i_organ] {
						g.organ_growth_increment[i_organ] = incr
					} else {
						// temporary hack because complex algorithm produces
						// questionable results - reduce only what is available
						g.organ_growth_increment[i_organ] = (-1) * g.organ_biomass[i_organ]
					}
				} else if i_organ == Organ_Shoot {
					incr := assimilate_partition_leaf * g.net_photosynthesis // should be negative
					if libc.fabs(incr) <= g.organ_biomass[i_organ] {
						g.organ_growth_increment[i_organ] = incr
					} else {
						g.organ_growth_increment[i_organ] = (-1) * g.organ_biomass[i_organ]
					}
				} else {
					// root or storage organ - do nothing in case of negative photosynthesis
					g.organ_growth_increment[i_organ] = 0
				}
			} else { 	// net_photosynthesis >= 0.0
				g.organ_growth_increment[i_organ] =
					g.net_photosynthesis *
					(assimilate_partitioning_coeff_old +
							((assimilate_partitioning_coeff - assimilate_partitioning_coeff_old) *
									(input.current_temperature_sum[input.developmental_stage] /
											stage_temperature_sum[input.developmental_stage]))) *
					input.crop_n_redux // [kg CH2O ha-1]

				use_mobilization_from_storage := true
				if use_mobilization_from_storage {
					if i_organ != g.storage_organ {
						g.organ_growth_increment[i_organ] +=
							mobilization_from_storage *
							(assimilate_partitioning_coeff_old +
									((assimilate_partitioning_coeff -
												assimilate_partitioning_coeff_old) *
											(input.current_temperature_sum[input.developmental_stage] /
													stage_temperature_sum[input.developmental_stage]))) *
							input.crop_n_redux
					} else {
						g.organ_growth_increment[i_organ] -=
							mobilization_from_storage * input.crop_n_redux
						g.organ_growth_increment[i_organ] +=
							mobilization_from_storage *
							(assimilate_partitioning_coeff_old +
									((assimilate_partitioning_coeff -
												assimilate_partitioning_coeff_old) *
											(input.current_temperature_sum[input.developmental_stage] /
													stage_temperature_sum[input.developmental_stage]))) *
							input.crop_n_redux
					}
				}
			}
			g.organ_senescence_increment[i_organ] =
				g.organ_green_biomass[i_organ] *
				(organ_senescence_rate[prev_stage][i_organ] +
						((organ_senescence_rate[input.developmental_stage][i_organ] -
									organ_senescence_rate[prev_stage][i_organ]) *
								(input.current_temperature_sum[input.developmental_stage] /
										stage_temperature_sum[input.developmental_stage]))) // [kg CH2O ha-1]
		}

		g.organ_biomass[i_organ] += g.organ_growth_increment[i_organ] * input.time_step // [kg CH2O ha-1]
		if i_organ == g.storage_organ {
			g.organ_dead_biomass[i_organ] += g.organ_senescence_increment[i_organ] * input.time_step
		} else {
			// root, shoot, leaf
			reallocation_rate :=
				assimilate_reallocation * g.organ_senescence_increment[i_organ] * input.time_step
			g.organ_biomass[g.storage_organ] += reallocation_rate
			daily_dead_biomass_increment := g.organ_senescence_increment[i_organ] - reallocation_rate

			if i_organ == Organ_Root {
				g.organ_biomass[Organ_Root] -= g.organ_senescence_increment[Organ_Root]
				total_biomass_n_content -= daily_dead_biomass_increment * input.n_concentration_root
				daily_dead_root_biomass_increment = daily_dead_biomass_increment
			} else {
				// shoot or leaf
				g.organ_biomass[i_organ] -= reallocation_rate
				g.organ_dead_biomass[i_organ] += daily_dead_biomass_increment // [kg CH2O ha-1]
			}
		}

		g.organ_green_biomass[i_organ] =
			g.organ_biomass[i_organ] - g.organ_dead_biomass[i_organ]
		if g.organ_green_biomass[i_organ] < 0.0 {
			g.organ_dead_biomass[i_organ] = g.organ_biomass[i_organ]
			g.organ_green_biomass[i_organ] = 0.0
		}

		if aboveground_organ[i_organ] {
			g.aboveground_biomass += g.organ_biomass[i_organ]
		} else if i_organ != Organ_Root {
			g.belowground_biomass += g.organ_biomass[i_organ]
		}

		g.total_biomass += g.organ_biomass[i_organ]
	}

	// old @todo: N redux noch ausgeschaltet
	g.reserve_assimilate_pool = 0.0

	if input.developmental_stage > 0 {
		max_root_n_concentration =
			species_ps.stage_max_root_n_concentration[input.developmental_stage - 1] -
			(species_ps.stage_max_root_n_concentration[input.developmental_stage - 1] -
					species_ps.stage_max_root_n_concentration[input.developmental_stage]) *
				input.current_temperature_sum[input.developmental_stage] /
				stage_temperature_sum[input.developmental_stage] // [kg kg-1]
	} else {
		max_root_n_concentration = species_ps.stage_max_root_n_concentration[input.developmental_stage]
	}

	g.crop_n_demand =
		((input.target_n_concentration * g.aboveground_biomass) +
			(g.organ_biomass[Organ_Root] * max_root_n_concentration) +
			(input.target_n_concentration * g.belowground_biomass / cultivar_ps.residue_n_ratio) -
			total_biomass_n_content) *
		input.time_step // [kg ha-1]

	// vc_NConcentrationOptimum: computed but unused elsewhere in the C++ too -
	// a genuine dead store, kept for fidelity.
	n_concentration_optimum :=
		((input.target_n_concentration -
					(input.target_n_concentration - input.critical_n_concentration) * 0.15) *
				g.aboveground_biomass +
			(input.target_n_concentration -
					(input.target_n_concentration - input.critical_n_concentration) * 0.15) *
				g.belowground_biomass /
				cultivar_ps.residue_n_ratio +
			(g.organ_biomass[Organ_Root] * max_root_n_concentration) -
			total_biomass_n_content) *
		input.time_step // [kg ha-1]
	_ = n_concentration_optimum

	if g.crop_n_demand > (input.max_crop_n_demand * input.time_step) {
		// Not more than 6kg N per day to be taken up.
		g.crop_n_demand = input.max_crop_n_demand * input.time_step
	}

	if g.crop_n_demand < 0 {
		g.crop_n_demand = 0.0
	}

	dead_root_biomass = daily_dead_root_biomass_increment
	return
}

// What growth_limit_n_demand reads.
Growth_N_Limit_In :: struct {
	total_root_length:             f64 `unit:"m m-2" src:"Root_System"`, // today's
	current_total_temperature_sum: f64 `unit:"°C d" src:"Phenology"`,
	total_temperature_sum:         f64 `unit:"°C d" src:"Phenology"`,
	time_step:                     f64 `unit:"d"`,
}

// The end of the C++ fcCropDryMatter: the maximum N uptake, and the N demand
// limited by the root length the root system computed before.
growth_limit_n_demand :: proc(g: ^Crop_Growth, ps: ^p.Crop_Parameters, input: Growth_N_Limit_In) {
	species_ps := &ps.species_params

	// Limiting the maximum N-uptake to 26-13*10^-14 mol/cm root/sec
	g.max_n_uptake =
		species_ps.max_n_uptake_param -
		(input.current_total_temperature_sum / input.total_temperature_sum) // [kg m root-1]

	if (g.crop_n_demand / 10000.0) > (input.total_root_length * g.max_n_uptake * input.time_step) {
		g.crop_n_demand = input.total_root_length * g.max_n_uptake * input.time_step // [kg m-2]
	} else {
		g.crop_n_demand = g.crop_n_demand / 10000.0 // [kg ha-1 -> kg m-2]
	}
}
