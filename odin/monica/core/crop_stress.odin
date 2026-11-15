package core

// The stress factors (plan-component-interfaces.md D4; architecture.md
// section 5): oxygen deficiency, heat stress, frost kill and drought impact
// on fertility. State: Crop_Stress (embedded in Crop_Module); each procedure
// writes only that and reads what its *_In declares, plus the crop
// parameters.

import p "../params"
import "../../support/cr_math"

// Stress factors: oxygen deficiency, heat, frost and drought impact on fertility
// (stress_oxygen_deficiency, stress_heat, stress_frost_kill, stress_drought_impact_on_fertility).
Crop_Stress :: struct {
	oxygen_deficit:                      f64 `unit:"0-1"`, //! old LURED
	time_under_anoxia:                   int `unit:"d"`,
	time_under_anoxia_threshold_default: int,
	crop_heat_redux:                     f64 `unit:"0-1"`,
	days_after_begin_flowering:          int,
	total_crop_heat_impact:              f64,
	crop_frost_redux:                    f64 `unit:"0-1"`,
	lt50:                                f64 `unit:"°C"`,
	lt50_m:                              f64,
	drought_impact_on_fertility:         f64,
}

Oxygen_Deficiency_In :: struct {
	critical_oxygen_content:     f64 `unit:"m3 m-3" src:"species"`, // at yesterday's stage
	developmental_stage:         int `unit:"" src:"Phenology" lag:"1d"`,
	rooting_depth:               int `unit:"layer#" src:"Root_System" lag:"1d"`,
	time_under_anoxia_threshold: []int `unit:"d" src:"crop module parameters"`, // per stage
	time_step:                   f64 `unit:"d"`,
	soil_layers:                 []Soil_Layer `src:"soil"`, // saturation, soil moisture
}

// C++: double monica::cropmodule::fcOxygenDeficiency(CropModule*, double criticalOxygenContent)
stress_oxygen_deficiency :: proc(s: ^Crop_Stress, input: Oxygen_Deficiency_In) {
	critical_oxygen_content := input.critical_oxygen_content
	developmental_stage := input.developmental_stage
	rooting_depth := input.rooting_depth
	time_under_anoxia_threshold_at_stage := s.time_under_anoxia_threshold_default
	if developmental_stage < len(input.time_under_anoxia_threshold) {
		time_under_anoxia_threshold_at_stage =
			input.time_under_anoxia_threshold[developmental_stage]
	}

	// Reduktion bei Luftmangel Stauwasser berücksichtigen!!!!
	sum_saturation := 0.0
	sum_soil_moisture := 0.0
	sum_layers := 0
	// MP: changed to consider at least first 30 cm and then rooting depth
	nols := min(max(3, rooting_depth), len(input.soil_layers))
	for i := 0; i < nols; i += 1 {
		sum_saturation += input.soil_layers[i].saturation
		sum_soil_moisture += input.soil_layers[i].soil_moisture_m3
		sum_layers += 1
	}
	avg_air_filled_pore_volume := (sum_saturation - sum_soil_moisture) / f64(sum_layers)
	if avg_air_filled_pore_volume < critical_oxygen_content {
		// MP: conditions changed for stage-dependent waterlogging
		s.time_under_anoxia += int(input.time_step)
		if s.time_under_anoxia >= time_under_anoxia_threshold_at_stage {
			max_oxygen_deficit := max(0.0, avg_air_filled_pore_volume) / critical_oxygen_content
			s.oxygen_deficit = max(
				0.0,
				1.0 -
				f64(s.time_under_anoxia) /
					f64(time_under_anoxia_threshold_at_stage) *
					(1.0 - max_oxygen_deficit),
			)
		}
	} else {
		s.time_under_anoxia = 0
		s.oxygen_deficit = 1.0
	}
}

Heat_Stress_In :: struct {
	max_air_temperature:           f64 `unit:"°C" src:"weather"`,
	min_air_temperature:           f64 `unit:"°C" src:"weather"`,
	current_total_temperature_sum: f64 `unit:"°C d" src:"Phenology"`,
}

// C++: void monica::cropmodule::fcHeatStressImpact(CropModule*, double
// vw_MaxAirTemperature, double vw_MinAirTemperature)
stress_heat :: proc(s: ^Crop_Stress, ps: ^p.Crop_Parameters, input: Heat_Stress_In) {
	max_air_temperature := input.max_air_temperature
	min_air_temperature := input.min_air_temperature
	cultivar_ps := &ps.cultivar_params

	// AGROSIM night and day temperatures
	photo_temperature :=
		max_air_temperature - ((max_air_temperature - min_air_temperature) / 4.0)
	fraction_open_flowers := 0.0
	yesterdays_fraction_open_flowers := 0.0

	if cultivar_ps.begin_sensitive_phase_heat_stress == 0.0 &&
	   cultivar_ps.end_sensitive_phase_heat_stress == 0.0 {
		s.total_crop_heat_impact = 1.0
	}

	if input.current_total_temperature_sum >= cultivar_ps.begin_sensitive_phase_heat_stress &&
	   input.current_total_temperature_sum < cultivar_ps.end_sensitive_phase_heat_stress {
		// Crop heat redux: Challinor et al. (2005), Agric. and Forest
		// Meteorology 135, 180-189.
		crop_heat_impact :=
			1.0 -
			((photo_temperature - cultivar_ps.critical_temperature_heat_stress) /
					(ps.species_params.limiting_temperature_heat_stress -
							cultivar_ps.critical_temperature_heat_stress))

		if crop_heat_impact > 1.0 {
			crop_heat_impact = 1.0
		}
		if crop_heat_impact < 0.0 {
			crop_heat_impact = 0.0
		}

		// Fraction open flowers: Moriondo et al. (2011), Climatic Change 104
		// (3-4), 679-701.
		fraction_open_flowers =
			1.0 /
			(1.0 + ((1.0 / 0.015) - 1.0) * cr_math.exp(-1.4 * f64(s.days_after_begin_flowering)))
		if s.days_after_begin_flowering > 0 {
			yesterdays_fraction_open_flowers =
				1.0 /
				(1.0 +
						((1.0 / 0.015) - 1.0) *
							cr_math.exp(-1.4 * f64(s.days_after_begin_flowering - 1)))
		} else {
			yesterdays_fraction_open_flowers = 0.0
		}
		daily_flowering_rate := fraction_open_flowers - yesterdays_fraction_open_flowers

		// Total effect: Challinor et al. (2005)
		s.total_crop_heat_impact += crop_heat_impact * daily_flowering_rate

		s.days_after_begin_flowering += 1
	}

	if input.current_total_temperature_sum >= cultivar_ps.end_sensitive_phase_heat_stress ||
	   fraction_open_flowers > 0.999999 {
		if s.total_crop_heat_impact < s.crop_heat_redux {
			s.crop_heat_redux = s.total_crop_heat_impact
		}
	}
}

Frost_Kill_In :: struct {
	max_air_temperature:      f64 `unit:"°C" src:"weather"`,
	min_air_temperature:      f64 `unit:"°C" src:"weather"`,
	snow:                     Snow_For_Crop `src:"soil moisture"`,
	snow_depth_mm:            f64 `unit:"mm" src:"soil"`, // for the respiratory stress
	soil_surface_temperature: f64 `unit:"°C" src:"soil"`,
	top_soil_temperature:     f64 `unit:"°C" src:"soil"`, // layer 0
	developmental_stage:      int `unit:"" src:"Phenology"`,
	vernalisation_factor:     f64 `unit:"0-1" src:"Phenology"`,
	current_temperature_sum:  []f64 `unit:"°C d" src:"Phenology"`, // per stage
}

// C++: void monica::cropmodule::fcFrostKill(CropModule*, double maxAirTemp, double minAirTemp)
//
// Fowler, Byrns & Greer (2014): Overwinter Low-Temperature Responses of
// Cereals. Crop Sci. 54:2395-2405.
stress_frost_kill :: proc(s: ^Crop_Stress, ps: ^p.Crop_Parameters, input: Frost_Kill_In) {
	max_air_temp := input.max_air_temperature
	min_air_temp := input.min_air_temperature
	cultivar_ps := &ps.cultivar_params

	lt50_old := s.lt50
	s.lt50_m = min(s.lt50, s.lt50_m)

	night_temperature := min_air_temp + ((max_air_temp - min_air_temp) / 4.0)
	crown_temperature := night_temperature * 0.8
	snow_depth := input.snow.snow_depth
	temp_under_snow := temperature_under_snow(crown_temperature, snow_depth, input.snow.frost_depth)
	if input.developmental_stage <= 1 {
		crown_temperature =
			(3.0 * input.soil_surface_temperature +
				2.0 * input.top_soil_temperature) /
			5.0
	} else if snow_depth > 0.0 {
		crown_temperature = temp_under_snow
	}

	frost_hardening := 0.0
	threshold_induction_temperature := 3.72135 - 0.401124 * cultivar_ps.lt50_cultivar
	if input.vernalisation_factor < 1.0 && crown_temperature < threshold_induction_temperature {
		frost_hardening =
			cultivar_ps.frost_hardening *
			(threshold_induction_temperature - crown_temperature) *
			(lt50_old - cultivar_ps.lt50_cultivar)
	}

	frost_dehardening := 0.0
	double_ridge_counter := input.current_temperature_sum[1] / cultivar_ps.stage_temperature_sum[1]
	vrt_factor := 1.0 / (1.0 + cr_math.exp(80.0 * (double_ridge_counter - 0.9)))
	if (double_ridge_counter < 1.0 && crown_temperature >= threshold_induction_temperature) ||
	   double_ridge_counter >= 1.0 {
		frost_dehardening =
			cultivar_ps.frost_dehardening / (1.0 + cr_math.exp(4.35 - 0.28 * crown_temperature))
	} else if double_ridge_counter < 1.0 &&
	   -4.0 <= crown_temperature &&
	   crown_temperature < threshold_induction_temperature {
		frost_dehardening =
			(1.0 - vrt_factor) *
			cultivar_ps.frost_dehardening /
			(1.0 + cr_math.exp(4.35 - 0.28 * crown_temperature))
	}

	snow_depth_factor := 1.0
	if input.snow_depth_mm <= 125.0 {
		snow_depth_factor = input.snow_depth_mm / 125.0
	}
	respiration_factor := (cr_math.exp(0.84 + 0.051 * crown_temperature) - 2.0) / 1.85
	respiratory_stress := cultivar_ps.respiratory_stress * respiration_factor * snow_depth_factor

	s.lt50 = lt50_old - frost_hardening + frost_dehardening + respiratory_stress

	if s.lt50 > -3.0 {
		s.lt50 = -3.0
	}
	if crown_temperature < s.lt50_m {
		s.crop_frost_redux *= 0.5
	}
}

Drought_Impact_In :: struct {
	// yesterday's transpiration deficit, clamped to >= 0 (crop_module_step)
	water_stress:        f64 `unit:"0-1" src:"Water_Uptake" lag:"1d"`,
	developmental_stage: int `unit:"" src:"Phenology"`,
	storage_organ:       int `unit:"" src:"Growth"`,
}

// C++: void monica::cropmodule::fcDroughtImpactOnFertility(CropModule*)
stress_drought_impact_on_fertility :: proc(s: ^Crop_Stress, ps: ^p.Crop_Parameters, input: Drought_Impact_In) {
	cultivar_ps := &ps.cultivar_params

	// Fertility of the crop is reduced in cases of severe drought during bloom
	fertility_drought_threshold :=
		ps.species_params.drought_impact_on_fertility_factor *
		cultivar_ps.drought_stress_threshold[input.developmental_stage]
	if input.water_stress < fertility_drought_threshold &&
	   cultivar_ps.assimilate_partitioning_coeff[input.developmental_stage][input.storage_organ] > 0.0 {
		transpiration_deficit_helper := input.water_stress / fertility_drought_threshold

		if s.oxygen_deficit < 1.0 {
			s.drought_impact_on_fertility = 1.0
		} else {
			s.drought_impact_on_fertility =
				1.0 -
				((1.0 - transpiration_deficit_helper) * (1.0 - transpiration_deficit_helper))
		}
	} else {
		s.drought_impact_on_fertility = 1.0
	}
}
