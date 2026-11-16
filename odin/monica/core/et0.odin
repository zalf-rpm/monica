package core

// The FAO-56 Penman-Monteith reference evapotranspiration, shared by the
// crop's ET0 (crop_et0, with the stomata resistance from the crop's reference
// photosynthesis) and soil moisture's (reference_evapotranspiration, with the
// FAO default STOMATA_RESISTANCE). The two used to be two copies of the same
// formulas (architecture.md §4 coupling 6, §6 step 6); the AMEI module
// Monica_Evapotranspiration splits it the same way (StomataResistance /
// CalcStomataResistance feeding one ReferenceEvapotranspiration).
//
// Allen, Pereira, Raes & Smith 1998, FAO Irrigation and Drainage Paper 56.

import libc "core:c/libc"
import "../../support/cr_math"

// What fao56_et0 reads besides the stomata resistance.
Et0_In :: struct {
	max_air_temperature:        f64 `unit:"°C" src:"weather"`,
	min_air_temperature:        f64 `unit:"°C" src:"weather"`,
	relative_humidity:          f64 `unit:"0-1" src:"weather"`, // <= 0: unknown
	mean_air_temperature:       f64 `unit:"°C" src:"weather"`,
	wind_speed:                 f64 `unit:"m s-1" src:"weather"`,
	wind_speed_height:          f64 `unit:"m" src:"site"`,
	height_nn:                  f64 `unit:"m" src:"site"`,
	extraterrestrial_radiation: f64 `unit:"MJ m-2" src:"Radiation"`,
	global_radiation:           f64 `unit:"MJ m-2" src:"weather"`,
	reference_albedo:           f64 `unit:"0-1" src:"crop module parameters"`,
}

// NOTE(deviation): the intermediates of an ET0 calculation that the
// evapotranspiration module Monica_AMEI_potential_Evapotranspiration takes as
// inputs.
Et0_Terms :: struct {
	vapour_pressure:           f64 `unit:"kPa"`,
	saturated_vapour_pressure: f64 `unit:"kPa"`,
	net_radiation:             f64 `unit:"MJ m-2"`,
	stomata_resistance:        f64 `unit:"s m-1"`,
}

// The actual and the saturated vapour pressure [kPa] - also what the crop's
// stomata resistance needs (the saturation deficit, their difference).
et0_vapour_pressures :: proc(input: Et0_In) -> (vapour_pressure, saturated_vapour_pressure: f64) {
	// Calc. of saturated water vapour pressure at daily max/min temperature
	saturated_vapour_pressure_max :=
		0.6108 *
		cr_math.exp((17.27 * input.max_air_temperature) / (237.3 + input.max_air_temperature))
	saturated_vapour_pressure_min :=
		0.6108 *
		cr_math.exp((17.27 * input.min_air_temperature) / (237.3 + input.min_air_temperature))

	saturated_vapour_pressure = (saturated_vapour_pressure_max + saturated_vapour_pressure_min) / 2.0

	if input.relative_humidity <= 0.0 {
		// Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
		vapour_pressure = saturated_vapour_pressure_min
	} else {
		vapour_pressure = input.relative_humidity * saturated_vapour_pressure
	}
	return
}

// C++: the common part of monica::cropmodule::fcReferenceEvapotranspiration
// and monica::soilmoisture::referenceEvapotranspiration, which differ only in
// the stomata resistance [s m-1].
fao56_et0 :: proc(input: Et0_In, stomata_resistance: f64) -> (et0: f64, terms: Et0_Terms) {
	mean_air_temperature := input.mean_air_temperature
	min_air_temperature := input.min_air_temperature
	max_air_temperature := input.max_air_temperature

	// Calculation of atmospheric pressure
	atmospheric_pressure :=
		101.3 * cr_math.pow((293.0 - (0.0065 * input.height_nn)) / 293.0, 5.26)

	// Calculation of psychrometer constant
	psycrometer_constant := 0.000665 * atmospheric_pressure

	vapour_pressure, saturated_vapour_pressure := et0_vapour_pressures(input)
	saturation_deficit := saturated_vapour_pressure - vapour_pressure

	// Slope of saturation water vapour pressure-to-temperature relation
	saturated_vapour_pressure_slope :=
		(4098.0 *
			(0.6108 *
					cr_math.exp((17.27 * mean_air_temperature) / (mean_air_temperature + 237.3)))) /
		((mean_air_temperature + 237.3) * (mean_air_temperature + 237.3))

	// 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO
	wind_speed_2m := max(
		0.5,
		input.wind_speed * (4.87 / cr_math.log(67.8 * input.wind_speed_height - 5.42)),
	)

	aerodynamic_resistance := 208.0 / wind_speed_2m

	surface_resistance := stomata_resistance / 1.44

	clear_sky_shortwave_radiation :=
		(0.75 + 0.00002 * input.height_nn) * input.extraterrestrial_radiation

	// C++: capped at 1.0, as FAO-56 has it
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
	et0 =
		((0.408 * saturated_vapour_pressure_slope * net_radiation) +
			(psycrometer_constant *
					(900.0 / (mean_air_temperature + 273.0)) *
					wind_speed_2m *
					saturation_deficit)) /
		(saturated_vapour_pressure_slope +
				psycrometer_constant * (1.0 + (surface_resistance / aerodynamic_resistance)))

	if et0 < 0.0 {
		et0 = 0.0
	}

	terms = Et0_Terms {
		vapour_pressure           = vapour_pressure,
		saturated_vapour_pressure = saturated_vapour_pressure,
		net_radiation             = net_radiation,
		stomata_resistance        = stomata_resistance,
	}
	return
}
