package monica_evapotranspiration

import "core:math"
import cr_math "../../support/cr_math"

// - Name: Radiation -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA radiation calculations
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference:
//             Taken from the original HERMES model, Kersebaum, K.C. and Richter J.
//             (1991): Modelling nitrogen dynamics in a plant-soil system with a
//             simple model for advisory purposes. Fert. Res. 27 (2-3), 273 - 281.
//
//             * Institution: ZALF e.V.
//             * ExtendedDescription:
//
//             * ShortDescription: Calculates the MONICA reference evapotranspiration
Radiation :: struct {
	latitude: f64 `unit:"degree"`, // latitude
}

make_radiation :: proc(allocator := context.allocator) -> (m: Radiation) {
	context.allocator = allocator
	m.latitude = 0
	return
}

destroy_radiation :: proc(m: ^Radiation) {
}

// - inputs:
//             * name: latitude
//                           ** description : latitude
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 90
//                           ** min : -90
//                           ** default : 0
//                           ** unit : degree
//             * name: global_radiation
//                           ** description : global radiation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50
//                           ** min : 0
//                           ** default : 0
//                           ** unit : MJ/m2
//             * name: julian_day
//                           ** description : day of year
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : INT
//                           ** max : 366
//                           ** min : 1
//                           ** default : 1
//                           ** unit : day
//             * name: sunshine_hours
//                           ** description : number of sunshine hours
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 24
//                           ** min : 0
//                           ** default : 0
//                           ** unit : h
// - outputs:
//             * name: declination
//                           ** description : declination
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit :
//             * name: astronomic_daylength
//                           ** description : astronomic_daylength
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
//             * name: effective_daylength
//                           ** description : effective_daylength
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
//             * name: photoperiodic_daylength
//                           ** description : photoperiodic_daylength
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
//             * name: sunshine_hours_global_radiation
//                           ** description : global radiation calculated from sunshine hours
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : MJ/m2
//             * name: extraterrestrial_radiation
//                           ** description : extraterrestrial_radiation
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : MJ/m2
//             * name: clear_day_radiation
//                           ** description : clear_day_radiation
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : MJ/m2
//             * name: overcast_day_radiation
//                           ** description : overcast_day_radiation
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : MJ/m2
//             * name: phot_act_radiation_mean
//                           ** description : phot_act_radiation_mean
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : J/m2
radiation_calculate_model :: proc(m: ^Radiation, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	decl_sin: f64
	decl_cos: f64
	a.declination, decl_sin, decl_cos = calc_declinations(m.latitude, ex.julian_day)
	a.astronomic_daylength = calc_astronomic_daylength(m.latitude, ex.julian_day)
	a.effective_daylength = calc_effective_daylength(m.latitude, ex.julian_day)
	a.photoperiodic_daylength = calc_photoperiodic_daylength(m.latitude, ex.julian_day)
	arg_phot_act: f64
	arg_phot_act = min(1.0, decl_sin / decl_cos * (decl_sin / decl_cos))
	a.phot_act_radiation_mean = 3600.0 * (decl_sin * a.astronomic_daylength + 24.0 / math.PI * decl_cos * math.sqrt(1.0 - arg_phot_act))
	if a.phot_act_radiation_mean > 0.0 && a.astronomic_daylength > 0.0 {
		a.clear_day_radiation = 0.5 * 1300.0 * a.phot_act_radiation_mean * cr_math.exp(-0.14 / (a.phot_act_radiation_mean / (a.astronomic_daylength * 3600.0)))
	}
	a.overcast_day_radiation = 0.2 * a.clear_day_radiation
	solar_constant: f64
	solar_constant = 0.082
	sc: f64
	sc = 24.0 * 60.0 / math.PI * solar_constant * (1.0 + 0.033 * cr_math.cos(2.0 * math.PI * f64(ex.julian_day) / 365.0))
	solar_angle: f64
	solar_angle = -cr_math.tan(m.latitude * math.PI / 180.0) * cr_math.tan(a.declination * math.PI / 180.0)
	solar_angle = bound(-1.0, solar_angle, 1.0)
	sunset_solar_angle: f64
	sunset_solar_angle = cr_math.acos(solar_angle)
	a.extraterrestrial_radiation = sc * (sunset_solar_angle * decl_sin + decl_cos * cr_math.sin(sunset_solar_angle))
	if ex.sunshine_hours > 0.0 {
		if ex.global_radiation <= 0.0 && a.astronomic_daylength > 0.0 {
			a.sunshine_hours_global_radiation = a.extraterrestrial_radiation * (0.19 + 0.55 * ex.sunshine_hours / a.astronomic_daylength)
		} else {
			a.sunshine_hours_global_radiation = 0.0
		}
	}
}

@(private="file")
bound :: proc(lower: f64, value: f64, upper: f64) -> f64 {
	if value < lower {
		return lower
	}
	if value > upper {
		return upper
	}
	return value
}

@(private="file")
calc_declinations :: proc(latitude: f64, julian_day: int) -> (f64, f64, f64) {
	declination: f64
	declination = -23.4 * cr_math.cos(2.0 * math.PI * ((f64(julian_day) + 10.0) / 365.0))
	decl_sin: f64
	decl_sin = cr_math.sin(declination * math.PI / 180.0) * cr_math.sin(latitude * math.PI / 180.0)
	decl_cos: f64
	decl_cos = cr_math.cos(declination * math.PI / 180.0) * cr_math.cos(latitude * math.PI / 180.0)
	return declination, decl_sin, decl_cos
}

@(private="file")
calc_astronomic_daylength :: proc(latitude: f64, julian_day: int) -> f64 {
	declination: f64
	decl_sin: f64
	decl_cos: f64
	declination, decl_sin, decl_cos = calc_declinations(latitude, julian_day)
	astro_daylength: f64
	astro_daylength = decl_sin / decl_cos
	astro_daylength = bound(-1.0, astro_daylength, 1.0)
	astronomic_daylength: f64
	astronomic_daylength = 12.0 * (math.PI + 2.0 * cr_math.asin(astro_daylength)) / math.PI
	return astronomic_daylength
}

@(private="file")
calc_effective_daylength :: proc(latitude: f64, julian_day: int) -> f64 {
	declination: f64
	decl_sin: f64
	decl_cos: f64
	declination, decl_sin, decl_cos = calc_declinations(latitude, julian_day)
	eff_daylength: f64
	eff_daylength = (-cr_math.sin(f64(8.0 * math.PI / 180.0)) + decl_sin) / decl_cos
	eff_daylength = bound(-1.0, eff_daylength, 1.0)
	effective_daylength: f64
	effective_daylength = 12.0 * (math.PI + 2.0 * cr_math.asin(eff_daylength)) / math.PI
	return effective_daylength
}

@(private="file")
calc_photoperiodic_daylength :: proc(latitude: f64, julian_day: int) -> f64 {
	declination: f64
	decl_sin: f64
	decl_cos: f64
	declination, decl_sin, decl_cos = calc_declinations(latitude, julian_day)
	photo_daylength: f64
	photo_daylength = (-cr_math.sin(f64(-6.0 * math.PI / 180.0)) + decl_sin) / decl_cos
	photo_daylength = bound(-1.0, photo_daylength, 1.0)
	photoperiodic_daylength: f64
	photoperiodic_daylength = 12.0 * (math.PI + 2.0 * cr_math.asin(photo_daylength)) / math.PI
	return photoperiodic_daylength
}