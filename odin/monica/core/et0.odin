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
	// <= 0: MONICA's constants, FAO-56's simplification for 20 °C (λ = 2.45 MJ kg-1:
	// 0.408 = 1/λ, psychrometric constant 0.000665 * pressure); else 1/λ and
	// FAO-56 eq. 8 with it (cp P / (ε λ))
	latent_heat_of_vaporization: f64 `unit:"MJ kg-1" src:"crop module parameters"`,
	// <= 0: from the pressure (height_nn) and λ; else this one
	psychrometric_constant:      f64 `unit:"kPa °C-1" src:"crop module parameters"`,
}

// NOTE(deviation): the intermediates of an ET0 calculation that the
// evapotranspiration module Monica_AMEI_potential_Evapotranspiration takes as
// inputs.
Et0_Terms :: struct {
	vapour_pressure:           f64 `unit:"kPa"`,
	saturated_vapour_pressure: f64 `unit:"kPa"`,
	net_radiation:             f64 `unit:"MJ m-2"`,
	stomata_resistance:        f64 `unit:"s m-1"`,
	// the other inputs of fao56_penman_monteith
	saturated_vapour_pressure_slope: f64 `unit:"kPa °C-1"`,
	psychrometric_constant:          f64 `unit:"kPa °C-1"`,
	latent_heat_of_vaporization:     f64 `unit:"MJ kg-1"`, // <= 0: MONICA's 0.408
}

// FAO-56's psychrometric constant [kPa °C-1] at `height_nn` [m]: MONICA's
// 0.000665 * pressure without λ (<= 0), FAO-56 eq. 8 (cp P / (ε λ)) with one.
fao56_psychrometric_constant :: proc(height_nn, latent_heat_of_vaporization: f64) -> f64 {
	// Calculation of atmospheric pressure
	atmospheric_pressure := 101.3 * cr_math.pow((293.0 - (0.0065 * height_nn)) / 293.0, 5.26)
	if latent_heat_of_vaporization > 0 {
		return 1.013e-3 * atmospheric_pressure / (0.622 * latent_heat_of_vaporization)
	}
	// Calculation of psychrometer constant
	return 0.000665 * atmospheric_pressure
}

// Slope of saturation water vapour pressure-to-temperature relation [kPa °C-1]
// (FAO-56 eq. 13) at the mean air temperature [°C].
fao56_saturated_vapour_pressure_slope :: proc(mean_air_temperature: f64) -> f64 {
	return(
		(4098.0 *
			(0.6108 *
					cr_math.exp((17.27 * mean_air_temperature) / (mean_air_temperature + 237.3)))) /
		((mean_air_temperature + 237.3) * (mean_air_temperature + 237.3))
	)
}

// The wind speed at 2 m [m s-1] from one at `wind_speed_height` [m], at least
// the 0.5 m s-1 FAO-56 allows for Penman-Monteith.
fao56_wind_speed_2m :: proc(wind_speed, wind_speed_height: f64) -> f64 {
	return max(0.5, wind_speed * (4.87 / cr_math.log(67.8 * wind_speed_height - 5.42)))
}

// The FAO-56 Penman-Monteith equation itself (eq. 3 with MONICA's
// resistances), from its intermediates - what fao56_et0 calculates them for.
// On its own for the AMEI evapotranspiration exercise, whose standardized
// inputs give the intermediates (the internal evapotranspiration module,
// evapotranspiration_internal.odin).
fao56_penman_monteith :: proc(
	terms: Et0_Terms,
	mean_air_temperature, wind_speed_2m: f64,
) -> (et0: f64) {
	aerodynamic_resistance := 208.0 / wind_speed_2m
	surface_resistance := terms.stomata_resistance / 1.44
	saturation_deficit := terms.saturated_vapour_pressure - terms.vapour_pressure
	slope := terms.saturated_vapour_pressure_slope
	psy := terms.psychrometric_constant
	// 0.408 = 1/2.45 (λ [MJ kg-1]), FAO-56's conversion of MJ m-2 to mm
	inverse_lambda := terms.latent_heat_of_vaporization > 0 ? 1.0 / terms.latent_heat_of_vaporization : 0.408

	// Penman-Monteith-Method FAO
	et0 =
		((inverse_lambda * slope * terms.net_radiation) +
			(psy * (900.0 / (mean_air_temperature + 273.0)) * wind_speed_2m * saturation_deficit)) /
		(slope + psy * (1.0 + (surface_resistance / aerodynamic_resistance)))

	if et0 < 0.0 {
		et0 = 0.0
	}
	return
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

	psycrometer_constant :=
		input.psychrometric_constant > 0 ? input.psychrometric_constant : fao56_psychrometric_constant(input.height_nn, input.latent_heat_of_vaporization)

	vapour_pressure, saturated_vapour_pressure := et0_vapour_pressures(input)

	saturated_vapour_pressure_slope := fao56_saturated_vapour_pressure_slope(mean_air_temperature)

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

	terms = Et0_Terms {
		vapour_pressure                 = vapour_pressure,
		saturated_vapour_pressure       = saturated_vapour_pressure,
		net_radiation                   = net_radiation,
		stomata_resistance              = stomata_resistance,
		saturated_vapour_pressure_slope = saturated_vapour_pressure_slope,
		psychrometric_constant          = psycrometer_constant,
		latent_heat_of_vaporization     = input.latent_heat_of_vaporization,
	}
	et0 = fao56_penman_monteith(
		terms,
		mean_air_temperature,
		fao56_wind_speed_2m(input.wind_speed, input.wind_speed_height),
	)
	return
}
