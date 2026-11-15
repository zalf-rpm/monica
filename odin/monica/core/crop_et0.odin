package core

// The crop's reference evapotranspiration (plan-component-interfaces.md E3):
// FAO-56 Penman-Monteith with the crop's stomata resistance. A pure function:
// crop_module_step stores its results in Crop_Et0, and the evapotranspiration
// modules (evapotranspiration_impl.odin) call it for the ET0 terms instead of
// running fcReferenceEvapotranspiration on a copy of the whole crop.

import libc "core:c/libc"
import "../../support/cr_math"

// The crop's reference evapotranspiration (crop_et0).
Crop_Et0 :: struct {
	reference_evapotranspiration: f64 `unit:"mm"`,
	stomata_resistance:           f64 `unit:"s m-1"`, //! old RSTOM
}

// What crop_et0 reads.
Crop_Et0_In :: struct {
	max_air_temperature:                f64 `unit:"°C" src:"weather"`,
	min_air_temperature:                f64 `unit:"°C" src:"weather"`,
	relative_humidity:                  f64 `unit:"0-1" src:"weather"`, // <= 0: unknown
	mean_air_temperature:               f64 `unit:"°C" src:"weather"`,
	wind_speed:                         f64 `unit:"m s-1" src:"weather"`,
	wind_speed_height:                  f64 `unit:"m" src:"site"`,
	atmospheric_co2_concentration:      f64 `unit:"ppm" src:"weather"`,
	height_nn:                          f64 `unit:"m" src:"site"`,
	gross_photosynthesis_reference_mol: f64 `unit:"mol m-2 s-1" src:"Photosynthesis"`,
	carboxylation_pathway:              int `unit:"" src:"species"`,
	extraterrestrial_radiation:         f64 `unit:"MJ m-2" src:"Radiation"`,
	global_radiation:                   f64 `unit:"MJ m-2" src:"Radiation"`,
	saturation_beta:                    f64 `unit:"kPa" src:"crop module parameters"`,
	stomata_conductance_alpha:          f64 `unit:"1" src:"crop module parameters"`,
	reference_albedo:                   f64 `unit:"0-1" src:"crop module parameters"`,
}

// Fills Crop_Et0_In from the crop (after the day's photosynthesis) and the weather.
crop_et0_in :: proc(
	cm: ^Crop_Module,
	max_air_temperature, min_air_temperature, relative_humidity, mean_air_temperature: f64,
	wind_speed, wind_speed_height, atmospheric_co2_concentration: f64,
) -> Crop_Et0_In {
	return {
		max_air_temperature = max_air_temperature,
		min_air_temperature = min_air_temperature,
		relative_humidity = relative_humidity,
		mean_air_temperature = mean_air_temperature,
		wind_speed = wind_speed,
		wind_speed_height = wind_speed_height,
		atmospheric_co2_concentration = atmospheric_co2_concentration,
		height_nn = cm.site_params.height_nn,
		gross_photosynthesis_reference_mol = cm.gross_photosynthesis_reference_mol,
		carboxylation_pathway = cm.crop_params.species_params.carboxylation_pathway,
		extraterrestrial_radiation = cm.extraterrestrial_radiation,
		global_radiation = cm.global_radiation,
		saturation_beta = cm.mod_params.saturation_beta,
		stomata_conductance_alpha = cm.mod_params.stomata_conductance_alpha,
		reference_albedo = cm.mod_params.reference_albedo,
	}
}

// C++: double monica::cropmodule::fcReferenceEvapotranspiration(CropModule*,
// double vw_MaxAirTemperature, double vw_MinAirTemperature, double
// vw_RelativeHumidity, double vw_MeanAirTemperature, double vw_WindSpeed,
// double vw_WindSpeedHeight, double vw_AtmosphericCO2Concentration)
//
// FAO-56 Penman-Monteith reference evapotranspiration (Allen, Pereira, Raes
// & Smith 1998, FAO Irrigation and Drainage Paper 56).
//
// NOTE(deviation): returns the stomata resistance with the other terms
// (Et0_Terms) instead of writing it into the crop.
crop_et0 :: proc(input: Crop_Et0_In) -> (et0: f64, terms: Et0_Terms) {
	max_air_temperature := input.max_air_temperature
	min_air_temperature := input.min_air_temperature
	relative_humidity := input.relative_humidity
	mean_air_temperature := input.mean_air_temperature
	wind_speed := input.wind_speed
	wind_speed_height := input.wind_speed_height
	atmospheric_co2_concentration := input.atmospheric_co2_concentration
	stomata_resistance: f64

	// Calculation of atmospheric pressure
	atmospheric_pressure :=
		101.3 * cr_math.pow((293.0 - (0.0065 * input.height_nn)) / 293.0, 5.26)

	// Calculation of psychrometer constant
	psycrometer_constant := 0.000665 * atmospheric_pressure

	// Calc. of saturated water vapour pressure at daily max/min temperature
	saturated_vapour_pressure_max :=
		0.6108 * cr_math.exp((17.27 * max_air_temperature) / (237.3 + max_air_temperature))
	saturated_vapour_pressure_min :=
		0.6108 * cr_math.exp((17.27 * min_air_temperature) / (237.3 + min_air_temperature))

	saturated_vapour_pressure :=
		(saturated_vapour_pressure_max + saturated_vapour_pressure_min) / 2.0

	vapour_pressure: f64
	if relative_humidity <= 0.0 {
		// Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
		vapour_pressure = saturated_vapour_pressure_min
	} else {
		vapour_pressure = relative_humidity * saturated_vapour_pressure
	}

	saturation_deficit := saturated_vapour_pressure - vapour_pressure

	// Slope of saturation water vapour pressure-to-temperature relation
	saturated_vapour_pressure_slope :=
		(4098.0 *
			(0.6108 *
					cr_math.exp((17.27 * mean_air_temperature) / (mean_air_temperature + 237.3)))) /
		((mean_air_temperature + 237.3) * (mean_air_temperature + 237.3))

	// 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO
	wind_speed_2m := max(0.5, wind_speed * (4.87 / cr_math.log(67.8 * wind_speed_height - 5.42)))

	aerodynamic_resistance := 208.0 / wind_speed_2m

	if input.gross_photosynthesis_reference_mol <= 0.0 {
		stomata_resistance = 999999.9
	} else {
		// Yu et al. 2001: input.saturation_beta = 3.5, input.stomata_conductance_alpha = 0.06
		if input.carboxylation_pathway == 1 {
			stomata_resistance =
				(atmospheric_co2_concentration *
					(1.0 + saturation_deficit / input.saturation_beta)) /
				(input.stomata_conductance_alpha * input.gross_photosynthesis_reference_mol)
		} else {
			stomata_resistance =
				(atmospheric_co2_concentration *
					(1.0 + saturation_deficit / input.saturation_beta)) /
				(input.stomata_conductance_alpha * input.gross_photosynthesis_reference_mol)
		}
	}

	surface_resistance := stomata_resistance / 1.44

	clear_sky_shortwave_radiation :=
		(0.75 + 0.00002 * input.height_nn) * input.extraterrestrial_radiation

	// C++: capped at 1.0, as the soil moisture's ET0 and FAO-56 have it
	relative_shortwave_radiation :=
		clear_sky_shortwave_radiation > 0 ? min(input.global_radiation / clear_sky_shortwave_radiation, 1.0) : 1.0

	// input.reference_albedo: FAO green grass reference albedo, Allen et al. 1998
	net_shortwave_radiation := (1.0 - input.reference_albedo) * input.global_radiation

	BOLZMAN_CONSTANT :: 0.0000000049 // 4.903 * 10^-9 MJ m-2 K-4 d-1
	net_radiation :=
		net_shortwave_radiation -
		(BOLZMAN_CONSTANT *
				(cr_math.pow(min_air_temperature + 273.16, 4.0) +
						cr_math.pow(max_air_temperature + 273.16, 4.0)) /
				2.0 *
				(1.35 * relative_shortwave_radiation - 0.35) *
				(0.34 - 0.14 * libc.sqrt(vapour_pressure)))

	// Penman-Monteith-Method FAO
	reference_evapotranspiration :=
		((0.408 * saturated_vapour_pressure_slope * net_radiation) +
			(psycrometer_constant *
					(900.0 / (mean_air_temperature + 273.0)) *
					wind_speed_2m *
					saturation_deficit)) /
		(saturated_vapour_pressure_slope +
				psycrometer_constant * (1.0 + (surface_resistance / aerodynamic_resistance)))

	if reference_evapotranspiration < 0.0 {
		reference_evapotranspiration = 0.0
	}

	terms = Et0_Terms {
		vapour_pressure           = vapour_pressure,
		saturated_vapour_pressure = saturated_vapour_pressure,
		net_radiation             = net_radiation,
		stomata_resistance        = stomata_resistance,
	}
	et0 = reference_evapotranspiration
	return
}
