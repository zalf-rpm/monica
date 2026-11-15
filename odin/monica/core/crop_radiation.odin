package core

// Radiation and day length for the crop (plan-component-interfaces.md D1): a
// pure function of the day, the latitude and the weather. State:
// Crop_Radiation (embedded in Crop_Module), today's values.

import tl "../../support/tools"
import libc "core:c/libc"
import "../../support/cr_math"

// Radiation and day length (crop_radiation): a function of the day, latitude and the weather.
Crop_Radiation :: struct {
	declination:                f64 `unit:"°"`, //! old EFF0
	astronomic_day_lenght:      f64 `unit:"h"`, //! old DL
	effective_day_length:       f64 `unit:"h"`, //! old DLE
	photoperiodic_daylength:    f64 `unit:"h"`, //! old DLP
	extraterrestrial_radiation: f64 `unit:"MJ m-2"`,
	clear_day_radiation:        f64 `unit:"J m-2"`, //! old DRC
	overcast_day_radiation:     f64 `unit:"J m-2"`, //! old DRO
	phot_act_radiation_mean:    f64 `unit:"J m-2"`, //! old RDN
	global_radiation:           f64 `unit:"MJ m-2"`,
}

// What crop_radiation needs (plan-component-interfaces.md D1).
Radiation_In :: struct {
	julian_day:       f64 `unit:"d"`,
	latitude:         f64 `unit:"°" src:"site"`,
	// measured; <= 0: estimated from the sunshine hours
	global_radiation: f64 `unit:"MJ m-2" src:"weather"`,
	sunshine_hours:   f64 `unit:"h" src:"weather"`,
}

// C++: void monica::cropmodule::fcRadiation(CropModule*, double julianDay,
// double globalRadiation, double sunshineHours)
//
// Taken from the original HERMES model - a separate, independent
// implementation of day-length/declination/radiation from soilmoisture.odin's
// own copy (soilmoisture computes its own for a different consumer); not
// deduplicated, matching the C++.
//
// A pure function of the day, the latitude and the weather (D1): returns the
// values instead of writing them into the crop.
crop_radiation :: proc(input: Radiation_In) -> (r: Crop_Radiation) {
	julian_day := input.julian_day
	global_radiation := input.global_radiation
	sunshine_hours := input.sunshine_hours

	PI :: 3.14159265358979323

	// Calculation of declination - old DEC
	r.declination = -23.4 * cr_math.cos(2.0 * PI * ((julian_day + 10.0) / 365.0))

	declination_sinus :=
		cr_math.sin(r.declination * PI / 180.0) * cr_math.sin(input.latitude * PI / 180.0) // old SINLD
	declination_cosinus :=
		cr_math.cos(r.declination * PI / 180.0) * cr_math.cos(input.latitude * PI / 180.0) // old COSLD

	// Calculation of the atmospheric day lenght - old DL
	arg_astro_day_length := declination_sinus / declination_cosinus
	arg_astro_day_length = tl.bound(-1.0, arg_astro_day_length, 1.0)
	r.astronomic_day_lenght = 12.0 * (PI + 2.0 * cr_math.asin(arg_astro_day_length)) / PI

	// Calculation of the effective day length - old DLE
	edl_helper := (-cr_math.sin(f64(8.0 * PI / 180.0)) + declination_sinus) / declination_cosinus

	if edl_helper < -1.0 || edl_helper > 1.0 {
		r.effective_day_length = 0.01
	} else {
		r.effective_day_length = 12.0 * (PI + 2.0 * cr_math.asin(edl_helper)) / PI
	}

	// old DLP
	arg_photo_day_length :=
		(-cr_math.sin(f64(-6.0 * PI / 180.0)) + declination_sinus) / declination_cosinus
	arg_photo_day_length = tl.bound(-1.0, arg_photo_day_length, 1.0)
	r.photoperiodic_daylength = 12.0 * (PI + 2.0 * cr_math.asin(arg_photo_day_length)) / PI

	// Calculation of the mean photosynthetically active radiation [J m-2] - old RDN
	arg_phot_act := min(
		1.0,
		(declination_sinus / declination_cosinus) *
		(declination_sinus / declination_cosinus),
	)
	r.phot_act_radiation_mean =
		3600.0 *
		(declination_sinus * r.astronomic_day_lenght +
				24.0 / PI * declination_cosinus * libc.sqrt(1.0 - arg_phot_act))

	// Calculation of radiation on a clear day [J m-2] - old DRC
	if r.phot_act_radiation_mean > 0 && r.astronomic_day_lenght > 0 {
		r.clear_day_radiation =
			0.5 *
			1300.0 *
			r.phot_act_radiation_mean *
			cr_math.exp(-0.14 / (r.phot_act_radiation_mean / (r.astronomic_day_lenght * 3600.0)))
	} else {
		r.clear_day_radiation = 0
	}

	// Calculation of radiation on an overcast day [J m-2] - old DRO
	r.overcast_day_radiation = 0.2 * r.clear_day_radiation

	// Calculation of extraterrestrial radiation - old EXT
	solar_constant := 0.082
	sc :=
		24.0 *
		60.0 /
		PI *
		solar_constant *
		(1.0 + 0.033 * cr_math.cos(2.0 * PI * julian_day / 365.0))

	arg_solar_angle := -cr_math.tan(input.latitude * PI / 180.0) * cr_math.tan(r.declination * PI / 180.0)
	arg_solar_angle = tl.bound(-1.0, arg_solar_angle, 1.0)
	sunset_solar_angle := cr_math.acos(arg_solar_angle)
	r.extraterrestrial_radiation =
		sc *
		(sunset_solar_angle * declination_sinus +
				declination_cosinus * cr_math.sin(sunset_solar_angle)) // [MJ m-2]

	if global_radiation > 0.0 {
		r.global_radiation = global_radiation
	} else if r.astronomic_day_lenght > 0 {
		r.global_radiation =
			r.extraterrestrial_radiation *
			(0.19 + 0.55 * sunshine_hours / r.astronomic_day_lenght)
	} else {
		r.global_radiation = 0
	}	return
}
