package core

// Phenology (plan-component-interfaces.md F1; architecture.md section 5):
// developmental stage, temperature sums, vernalisation, day length factor,
// dormancy and the crop's events. State: Crop_Phenology (embedded in
// Crop_Module).

import d "../../support/date"
import p "../params"
import "../../support/cr_math"

// Phenology: stage, temperature sums, vernalisation, day length factor, dormancy and the events
// (fc_crop_developmental_stage, fc_daylength_factor, fc_vernalisation_factor, set_stage, the step).
Crop_Phenology :: struct {
	developmental_stage:                     int, // C++ size_t, //! old INTWICK
	final_developmental_stage:               int, // C++ size_t
	current_temperature_sum:                 [dynamic]f64, //! old SUM
	current_total_temperature_sum:           f64 `unit:"°C d"`, //! old FP
	total_temperature_sum:                   f64,
	temperature_sum_to_flowering:            f64,
	relative_total_development:              f64 `unit:"0-1"`,
	daylength_factor:                        f64 `unit:"0-1"`, //! old DAYL
	vernalisation_factor:                    f64 `unit:"0-1"`, //! old FV
	vernalisation_days:                      f64,
	growth_cycle_ended:                      bool,
	perennial_crop_dormancy_period_end_date: d.Date,
	no_of_crop_steps:                        int,
	stem_elongation_event_fired:             bool,
	anthesis_day:                            int,
	maturity_day:                            int,
	maturity_reached:                        bool,
	time_step:                               f64, //! old dt
}

// What phenology_development reads on a day.
Phenology_In :: struct {
	mean_air_temperature:                   f64 `unit:"°C" src:"weather"`,
	current_date:                           d.Date,
	soil_temperature_top:                   f64 `unit:"°C" src:"soil"`, // layer 0
	soil_moisture_top:                      f64 `unit:"m3 m-3" src:"soil"`,
	field_capacity_top:                     f64 `unit:"m3 m-3" src:"soil"`,
	permanent_wilting_point_top:            f64 `unit:"m3 m-3" src:"soil"`,
	surface_water_storage:                  f64 `unit:"mm" src:"soil"`,
	water_stress:                           f64 `unit:"0-1" src:"Water_Uptake" lag:"1d"`,
	n_stress:                               f64 `unit:"0-1" src:"N_Status" lag:"1d"`,
	// computed after the stage, so yesterday's (Crop_Carry_Over; the C++ order)
	daylength_factor:                       f64 `unit:"0-1" src:"Phenology" lag:"1d"`,
	vernalisation_factor:                   f64 `unit:"0-1" src:"Phenology" lag:"1d"`,
	oxygen_deficit:                         f64 `unit:"0-1" src:"Stress"`,
	storage_organ:                          int `unit:"" src:"Growth"`,
	no_of_dev_stages:                       int `unit:"" src:"crop parameters"`,
	time_step:                              f64 `unit:"d"`,
	emergence_moisture_control_on:          bool `unit:"" src:"simulation parameters"`,
	emergence_flooding_control_on:          bool `unit:"" src:"simulation parameters"`,
	enable_wang_engel_temperature_response: bool `unit:"" src:"crop module parameters"`,
}

// Fills Phenology_In from the crop and its soil column (crop_module_step and
// the test drivers); the four stress/factor values are yesterday's.
phenology_in :: proc(
	cm: ^Crop_Module,
	mean_air_temperature: f64,
	current_date: d.Date,
	water_stress, n_stress, daylength_factor, vernalisation_factor: f64,
) -> Phenology_In {
	top := &cm.soil_column.layers[0]
	return {
		mean_air_temperature = mean_air_temperature,
		current_date = current_date,
		soil_temperature_top = top.soil_temperature,
		soil_moisture_top = top.soil_moisture_m3,
		field_capacity_top = top.field_capacity,
		permanent_wilting_point_top = top.permanent_wilting_point,
		surface_water_storage = cm.soil_column.surface_water_storage,
		water_stress = water_stress,
		n_stress = n_stress,
		daylength_factor = daylength_factor,
		vernalisation_factor = vernalisation_factor,
		oxygen_deficit = cm.oxygen_deficit,
		storage_organ = cm.storage_organ,
		no_of_dev_stages = cm.no_of_dev_stages,
		time_step = cm.time_step,
		emergence_moisture_control_on = cm.sim_params.emergence_moisture_control_on,
		emergence_flooding_control_on = cm.sim_params.emergence_flooding_control_on,
		enable_wang_engel_temperature_response = cm.mod_params.__enable_Phenology_WangEngelTemperatureResponse__,
	}
}

// The end of the dormancy, the first time a crop is stepped: temperature sums
// accumulate only after it (part of the C++ step()).
phenology_start_dormancy :: proc(ph: ^Crop_Phenology, ps: ^p.Crop_Parameters, current_date: d.Date) {
	if !d.is_valid(ph.perennial_crop_dormancy_period_end_date) {
		if ps.species_params.dormancy_end_doy == 0 {
			ph.perennial_crop_dormancy_period_end_date = current_date
		} else {
			ph.perennial_crop_dormancy_period_end_date = d.add(
				d.make_date(
					1,
					1,
					u16(d.year(current_date)),
					false,
					false,
					d.DEFAULT_USE_LEAP_YEARS,
				),
				u64(ps.species_params.dormancy_end_doy - 1),
			)
		}
	}
}

// C++: void monica::cropmodule::fcCropDevelopmentalStage(CropModule*, double
// meanAirTemperature, double soilMoisture_m3, double fieldCapacity, double
// permanentWiltingPoint, Tools::Date currentDate)
//
// Returns whether a perennial crop starts a new cycle - the caller then
// replaces the crop parameters (fcUpdateCropParametersForPerennial) and calls
// phenology_reset_perennial, the rest of the C++ reset, with the new ones -
// and whether the stage was irregular (the C++ sets the crop's error status).
phenology_development :: proc(
	ph: ^Crop_Phenology,
	ps: ^p.Crop_Parameters,
	input: Phenology_In,
) -> (
	reset_perennial: bool,
	irregular: bool,
) {
	mean_air_temperature := input.mean_air_temperature
	soil_moisture_m3 := input.soil_moisture_top
	field_capacity := input.field_capacity_top
	permanent_wilting_point := input.permanent_wilting_point_top
	current_date := input.current_date
	water_stress := input.water_stress
	n_stress := input.n_stress
	daylength_factor := input.daylength_factor
	vernalisation_factor := input.vernalisation_factor
	cultivar_ps := &ps.cultivar_params
	species_ps := &ps.species_params
	base_temperature := species_ps.base_temperature
	stage_temperature_sum := cultivar_ps.stage_temperature_sum

	if ph.developmental_stage == 0 {
		if cultivar_ps.perennial {
			if mean_air_temperature > base_temperature[ph.developmental_stage] {
				temp_incr :=
					(min(mean_air_temperature, cultivar_ps.optimum_temperature[ph.developmental_stage]) -
						base_temperature[ph.developmental_stage]) *
					vernalisation_factor *
					daylength_factor *
					input.time_step
				ph.current_temperature_sum[ph.developmental_stage] += temp_incr
				ph.current_total_temperature_sum += temp_incr
			}

			// @todo: shouldn't current_temperature_sum be reduces by the
			// excess temperature like further below?
			if ph.current_temperature_sum[ph.developmental_stage] >=
			   stage_temperature_sum[ph.developmental_stage] {
				if ph.developmental_stage < input.no_of_dev_stages - 1 {
					ph.developmental_stage += 1
				}
			}
		} else {
			soil_temperature := input.soil_temperature_top // MP: Bodentemperatur der ersten 10cm
			if soil_temperature > base_temperature[ph.developmental_stage] {
				emergence_condition := true
				// Germination only if soil water content in top layer exceeds
				// 20% of capillary water, but is not beyond field capacity
				if input.emergence_moisture_control_on {
					capillary_water := field_capacity - permanent_wilting_point
					emergence_condition =
						emergence_condition &&
						soil_moisture_m3 > ((0.2 * capillary_water) + permanent_wilting_point) &&
						soil_moisture_m3 <= field_capacity
				}
				// Germination only if no water is stored on the soil surface.
				if input.emergence_flooding_control_on {
					emergence_condition =
						emergence_condition && input.surface_water_storage < 0.001
				}

				if emergence_condition {
					ph.current_temperature_sum[ph.developmental_stage] +=
						(soil_temperature - base_temperature[ph.developmental_stage]) *
						input.time_step

					if ph.current_temperature_sum[ph.developmental_stage] >=
					   stage_temperature_sum[ph.developmental_stage] {
						stage_excess_temperature_sum :=
							ph.current_temperature_sum[ph.developmental_stage] -
							stage_temperature_sum[ph.developmental_stage]
						if ph.developmental_stage < input.no_of_dev_stages - 1 {
							ph.developmental_stage += 1
							ph.current_temperature_sum[ph.developmental_stage] +=
								stage_excess_temperature_sum
						}
					}
				}
			}
		}
	} else if ph.developmental_stage > 0 {
		// MP: wenn die Frucht aufgegangen ist, können N- und Wasser-Stress zum
		// Tragen kommen (nur während der Kornfüllungsphase --> schnelleres Abreifen)
		apc := cultivar_ps.assimilate_partitioning_coeff[ph.developmental_stage][input.storage_organ]

		// Development acceleration by N deficit in crop tissue
		development_acceleration_by_nitrogen_stress := 1.0 // old NPROG
		if species_ps.development_acceleration_by_nitrogen_stress == 1 && apc > 0.9 {
			development_acceleration_by_nitrogen_stress =
				1.0 + ((1.0 - n_stress) * (1.0 - n_stress))
		}

		// Development acceleration by water deficit
		development_acceleration_by_water_stress := 1.0 // old WPROG
		if water_stress < cultivar_ps.drought_stress_threshold[ph.developmental_stage] &&
		   apc > 0.9 {
			// Das heißt, das betrifft nur die Kornfüllungsphase (acp>0.9).
			if input.oxygen_deficit >= 1.0 {
				development_acceleration_by_water_stress =
					1.0 + ((1.0 - water_stress) * (1.0 - water_stress))
			}
		}

		// old DEVPROG
		development_acceleration_by_stress := max(
			development_acceleration_by_nitrogen_stress,
			development_acceleration_by_water_stress,
		)

		if input.enable_wang_engel_temperature_response {
			dev_t_response := max(
				0.0,
				wang_engel_temperature_response(
					mean_air_temperature,
					cultivar_ps.min_temp_dev_we,
					cultivar_ps.opt_temp_dev_we,
					cultivar_ps.max_temp_dev_we,
					1.0,
				), // MP: warum steht hier 1?
			)
			temp_incr :=
				dev_t_response *
				mean_air_temperature *
				vernalisation_factor *
				daylength_factor *
				development_acceleration_by_stress *
				input.time_step
			ph.current_temperature_sum[ph.developmental_stage] += temp_incr
			ph.current_total_temperature_sum += temp_incr
		} else {
			if mean_air_temperature > base_temperature[ph.developmental_stage] {
				temp_incr :=
					(min(mean_air_temperature, cultivar_ps.optimum_temperature[ph.developmental_stage]) -
						base_temperature[ph.developmental_stage]) *
					vernalisation_factor *
					daylength_factor *
					development_acceleration_by_stress *
					input.time_step
				ph.current_temperature_sum[ph.developmental_stage] += temp_incr // MP: effektive Temperatur wird aufsummiert
				ph.current_total_temperature_sum += temp_incr
			}
		}

		do_reset_perennial_crop :=
			cultivar_ps.perennial &&
			species_ps.dormancy_start_doy > 0 &&
			int(d.day_of_year(current_date)) >= species_ps.dormancy_start_doy
		if ph.current_temperature_sum[ph.developmental_stage] >=
		   stage_temperature_sum[ph.developmental_stage] {
			if ph.developmental_stage < input.no_of_dev_stages - 1 {
				stage_excess_temperature_sum :=
					ph.current_temperature_sum[ph.developmental_stage] -
					stage_temperature_sum[ph.developmental_stage]
				ph.developmental_stage += 1
				ph.current_temperature_sum[ph.developmental_stage] += stage_excess_temperature_sum
			} else if ph.developmental_stage == input.no_of_dev_stages - 1 { 	// MP: Frucht ist reif
				if cultivar_ps.perennial && ph.growth_cycle_ended {
					do_reset_perennial_crop = true
				}
			}
		}
		reset_perennial = do_reset_perennial_crop
	} else {
		irregular = true
	}
	return
}

// The rest of the C++ perennial reset in fcCropDevelopmentalStage, after the
// crop parameters were replaced: stage 0, temperature sums cleared, the next
// dormancy end.
phenology_reset_perennial :: proc(
	ph: ^Crop_Phenology,
	ps: ^p.Crop_Parameters,
	no_of_dev_stages: int,
	current_date: d.Date,
) {
	species_ps := &ps.species_params
	ph.developmental_stage = 0
	for stage := 0; stage < no_of_dev_stages; stage += 1 {
		ph.current_temperature_sum[stage] = 0.0
	}
	ph.current_total_temperature_sum = 0.0
	ph.growth_cycle_ended = false
	if species_ps.dormancy_end_doy == 0 {
		ph.perennial_crop_dormancy_period_end_date = current_date
	} else {
		year_delta := 0
		if int(d.day_of_year(current_date)) > species_ps.dormancy_end_doy {
			year_delta = 1
		}
		ph.perennial_crop_dormancy_period_end_date = d.add(
			d.make_date(
				1,
				1,
				u16(d.year(current_date) + year_delta),
				false,
				false,
				d.DEFAULT_USE_LEAP_YEARS,
			),
			u64(species_ps.dormancy_end_doy - 1),
		)
	}
}

// What phenology_events reads.
Phenology_Events_In :: struct {
	old_developmental_stage: int `unit:""`, // before today's development
	julian_day:              int `unit:"d"`,
	no_of_dev_stages:        int `unit:"" src:"crop parameters"`,
}

// The day's events, anthesis and maturity days, stem elongation (part of the
// C++ step()).
phenology_events :: proc(
	ph: ^Crop_Phenology,
	ps: ^p.Crop_Parameters,
	input: Phenology_Events_In,
) -> (
	events: Crop_Events,
) {
	old_developmental_stage := input.old_developmental_stage
	if old_developmental_stage == 0 && ph.developmental_stage == 1 {
		events.emergence = true
	} else if is_anthesis_stage_change(input.no_of_dev_stages, old_developmental_stage, ph.developmental_stage) {
		ph.anthesis_day = input.julian_day
		events.anthesis = true
	} else if is_maturity_stage_change(input.no_of_dev_stages, old_developmental_stage, ph.developmental_stage) {
		ph.maturity_day = input.julian_day
		ph.maturity_reached = true
		events.maturity = true
	}

	if !ph.stem_elongation_event_fired &&
	   ph.current_total_temperature_sum >=
		   ps.cultivar_params.stage_temperature_sum[2] * 0.25 + ps.cultivar_params.stage_temperature_sum[1] {
		events.cereal_stem_elongation = true
		ph.stem_elongation_event_fired = true
	}

	// stage event on stage change or right after sowing
	if old_developmental_stage != ph.developmental_stage || ph.no_of_crop_steps == 0 {
		events.stage = ph.developmental_stage + 1
	}
	return
}

// C++: anthesisBetweenStages/isAnthesisDay on values.
is_anthesis_stage_change :: proc(no_of_dev_stages, old_dev_stage, new_dev_stage: int) -> bool {
	a, b := -1, -1
	if no_of_dev_stages == 6 {
		a, b = 3, 4
	} else if no_of_dev_stages == 7 {
		a, b = 4, 5
	}
	return a == old_dev_stage && b == new_dev_stage
}

// C++: isMaturityDay on values.
is_maturity_stage_change :: proc(no_of_dev_stages, old_dev_stage, new_dev_stage: int) -> bool {
	// corn crops
	if no_of_dev_stages == 6 {
		return old_dev_stage == 4 && new_dev_stage == 5
		// maize, sorghum and other crops with 7 developmental stages
	} else if no_of_dev_stages == 7 {
		return old_dev_stage == 5 && new_dev_stage == 6
	}
	return false
}

// C++: double WangEngelTemperatureResponse(double, double, double, double,
// double) - a file-scope free function in crop-module.cpp, used only by
// fcCropDevelopmentalStage below.
@(private)
wang_engel_temperature_response :: proc(t, tmin, topt, tmax, betacoeff: f64) -> f64 {
	// MP: what is this beta coefficient?
	// prevent nan values with t < tmin
	if t < tmin || t > tmax {
		return 0.0
	}

	alfa := cr_math.log(f64(2.0)) / cr_math.log((tmax - tmin) / (topt - tmin))
	numerator :=
		2 * cr_math.pow(t - tmin, alfa) * cr_math.pow(topt - tmin, alfa) - cr_math.pow(t - tmin, 2 * alfa)
	denominator := cr_math.pow(topt - tmin, 2 * alfa)

	// MP: beta coefficient should be 2*alfa
	return cr_math.pow(numerator / denominator, betacoeff)
}

// What crop_daylength_factor needs (plan-component-interfaces.md D2).
Daylength_Factor_In :: struct {
	// > 0: long-day plant; < 0: short-day plant, the critical day length
	daylength_requirement:   f64 `unit:"h" src:"cultivar"`,
	base_daylength:          f64 `unit:"h" src:"cultivar"`,
	effective_day_length:    f64 `unit:"h" src:"Radiation"`,
	photoperiodic_daylength: f64 `unit:"h" src:"Radiation"`,
}

// C++: double monica::cropmodule::fcDaylengthFactor(CropModule*, double
// daylengthRequirement, double effectiveDayLength, double
// photoperiodicDayLength, double baseDaylength)
crop_daylength_factor :: proc(input: Daylength_Factor_In) -> (factor: f64) {
	daylength_requirement := input.daylength_requirement
	effective_day_length := input.effective_day_length
	photoperiodic_day_length := input.photoperiodic_daylength
	base_daylength := input.base_daylength

	if daylength_requirement > 0.0 {
		// Long-day plants: development acceleration by day length (day length
		// requirement is positive).
		factor =
			(photoperiodic_day_length - base_daylength) / (daylength_requirement - base_daylength)
	} else if daylength_requirement < 0.0 {
		// Short-day plants: development acceleration by night length (day
		// length requirement is negative and represents critical day length).
		critical_day_lenght := -daylength_requirement
		maximum_day_length := -base_daylength
		if effective_day_length <= critical_day_lenght {
			factor = 1.0
		} else {
			factor =
				(effective_day_length - maximum_day_length) /
				(critical_day_lenght - maximum_day_length)
		}
	} else {
		factor = 1.0
	}

	factor = max(0.0, min(factor, 1.0))

	return
}

// What crop_vernalisation_factor needs (plan-component-interfaces.md D2).
Vernalisation_In :: struct {
	mean_air_temperature:      f64 `unit:"°C" src:"weather"`,
	vernalisation_requirement: f64 `unit:"d" src:"cultivar"`,
	vernalisation_days:        f64 `unit:"d" src:"Phenology" cat:"state"`, // so far
	time_step:                 f64 `unit:"d"`,
	enable_fix:                bool `unit:"" src:"crop parameters"`, // vernalisation_factor_fix_enabled
}

// The crop's own `__enable_vernalisation_factor_fix__`, else the crop module
// parameters' (the C++ reads it inside fcVernalisationFactor).
vernalisation_factor_fix_enabled :: proc(cm: ^Crop_Module) -> bool {
	enable, ok := cm.crop_params.__enable_vernalisation_factor_fix__.?
	if !ok {
		enable = cm.mod_params.__enable_vernalisation_factor_fix__
	}
	return enable
}

// C++: pair<double,double> monica::cropmodule::fcVernalisationFactor(CropModule*,
// double meanAirTemperature, double vernalisationRequirement, double vernalisationDays)
crop_vernalisation_factor :: proc(
	input: Vernalisation_In,
) -> (
	factor: f64,
	vernalisation_days: f64,
) {
	mean_air_temperature := input.mean_air_temperature
	vernalisation_requirement := input.vernalisation_requirement
	vernalisation_days = input.vernalisation_days
	enable_vernalisation_factor_fix := input.enable_fix
	effective_vernalisation: f64

	if vernalisation_requirement == 0.0 {
		factor = 1.0
	} else {
		switch {
		case mean_air_temperature > -4.0 && mean_air_temperature <= 0.0:
			effective_vernalisation = (mean_air_temperature + 4.0) / 4.0
		case mean_air_temperature > 0.0 && mean_air_temperature <= 3.0:
			effective_vernalisation = 1.0
		case mean_air_temperature > 3.0 && mean_air_temperature <= 7.0:
			effective_vernalisation = 1.0 - (0.2 * (mean_air_temperature - 3.0) / 4.0)
		case mean_air_temperature > 7.0 && mean_air_temperature <= 9.0:
			effective_vernalisation = 0.8 - (0.4 * (mean_air_temperature - 7.0) / 2.0)
		case mean_air_temperature > 9.0 && mean_air_temperature <= 18.0:
			effective_vernalisation = 0.4 - (0.4 * (mean_air_temperature - 9.0) / 9.0)
		case mean_air_temperature <= -4.0 || mean_air_temperature > 18.0:
			effective_vernalisation = 0.0
		case:
			effective_vernalisation = 1.0
		}

		// old VERNTAGE
		vernalisation_days += effective_vernalisation * input.time_step

		// old VERSCHWELL
		vernalisation_threshold := min(vernalisation_requirement, 9.0) - 1.0

		if vernalisation_threshold >= 1 {
			factor =
				(vernalisation_days - vernalisation_threshold) /
				(vernalisation_requirement - vernalisation_threshold)

			if enable_vernalisation_factor_fix {
				factor = min(max(0.0, factor), 1.0)
			}
			if factor < 0 {
				factor = 0.0 // MP: Vernalisation kann nie negativ sein
			}
		} else {
			factor = 1.0
			// MP: Vernalisation hat keinen Effekt, wenn kein (bzw. 0 als)
			// Threshold definiert ist.
		}
	}

	return
}
