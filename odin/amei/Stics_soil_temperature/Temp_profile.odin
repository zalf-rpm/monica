package stics_soil_temperature

import "core:math"
import "core:slice"
import cr_math "../../support/cr_math"

// - Name: temp_profile -Version: 1.0, -Time step: 1
// - Description:
//             * Title: tempprofile model
//             * Authors: None
//             * Reference: doi:http://dx.doi.org/10.1016/j.agrformet.2014.05.002
//             * Institution: INRAE
//             * ExtendedDescription: None
//             * ShortDescription: None
Temp_Profile :: struct {
	air_temp_day1: f64 `unit:"degC"`, // Mean temperature on first day
	layer_thick: [dynamic]int `unit:"cm"`, // layers thickness
}

make_temp_profile :: proc(allocator := context.allocator) -> (m: Temp_Profile) {
	context.allocator = allocator
	m.air_temp_day1 = 0.0
	return
}

destroy_temp_profile :: proc(m: ^Temp_Profile) {
	delete(m.layer_thick)
}

temp_profile_init :: proc(m: ^Temp_Profile, s: ^Soil_Temp_State, s1: ^Soil_Temp_State, r: ^Soil_Temp_Rate, a: ^Soil_Temp_Auxiliary, ex: ^Soil_Temp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	s.temp_amp = 0.0
	clear(&s.prev_temp_profile)
	s.prev_canopy_temp = 0.0
	soil_depth: int
	soil_depth = math.sum(m.layer_thick[:])
	resize(&s.prev_temp_profile, soil_depth)
	resize(&s.prev_temp_profile, soil_depth)
	slice.fill(s.prev_temp_profile[:], m.air_temp_day1)
	s.prev_canopy_temp = m.air_temp_day1
}

// - inputs:
//             * name: temp_amp
//                           ** description : current temperature amplitude
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 100.0
//                           ** min : 0.0
//                           ** default : 0.0
//                           ** unit : degC
//             * name: prev_temp_profile
//                           ** description : previous soil temperature profile (for 1 cm layers)
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default :
//                           ** unit : degC
//             * name: prev_canopy_temp
//                           ** description : previous crop temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : 0.0
//                           ** default :
//                           ** unit : degC
//             * name: min_air_temp
//                           ** description : current minimum air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default :
//                           ** unit : degC
//             * name: air_temp_day1
//                           ** description : Mean temperature on first day
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 100.0
//                           ** min : 0.0
//                           ** default : 0.0
//                           ** unit : degC
//             * name: layer_thick
//                           ** description : layers thickness
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INTARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm
// - outputs:
//             * name: temp_profile
//                           ** description : current soil profile temperature (for 1 cm layers)
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len :
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** unit : degC
temp_profile_calculate_model :: proc(m: ^Temp_Profile, s: ^Soil_Temp_State, s1: ^Soil_Temp_State, r: ^Soil_Temp_Rate, a: ^Soil_Temp_Auxiliary, ex: ^Soil_Temp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	z: int
	n: int
	vexp: [dynamic]f64
	defer delete(vexp)
	therm_diff: f64 = 5.37e-3
	temp_freq: f64 = 7.272e-5
	therm_amp: f64
	n = len(s.prev_temp_profile)
	resize(&s.temp_profile, n)
	resize(&vexp, n)
	therm_amp = math.sqrt(temp_freq / 2 / therm_diff)
	for z = 1; z < n + 1; z += 1 {
		vexp[z - 1] = cr_math.exp(-(f64(z) * therm_amp))
	}
	for z = 1; z < n + 1; z += 1 {
		s.temp_profile[z - 1] = s.prev_temp_profile[z - 1] - vexp[z - 1] * (s.prev_canopy_temp - ex.min_air_temp) + 0.1 * (s.prev_canopy_temp - s.prev_temp_profile[z - 1]) + s.temp_amp * vexp[z - 1] / 2
	}
}