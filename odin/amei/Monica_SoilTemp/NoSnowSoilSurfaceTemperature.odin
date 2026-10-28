package monica_soiltemp

import cr_math "../../support/cr_math"

// - Name: NoSnowSoilSurfaceTemperature -Version: 1, -Time step: 1
// - Description:
//             * Title: Soil surface temperature
//             * Authors: Michael Berg-Mohnicke
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: It calculates the soil surface temperature without snow cover
No_Snow_Soil_Surface_Temperature :: struct {
	damping_factor: f64 `unit:"dimensionless"`, // dampingFactor
}

make_no_snow_soil_surface_temperature :: proc(allocator := context.allocator) -> (m: No_Snow_Soil_Surface_Temperature) {
	context.allocator = allocator
	m.damping_factor = 0.8
	return
}

destroy_no_snow_soil_surface_temperature :: proc(m: ^No_Snow_Soil_Surface_Temperature) {
}

// - inputs:
//             * name: tmin
//                           ** description : the days min air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 70.0
//                           ** min : -50.0
//                           ** default :
//                           ** unit : °C
//             * name: tmax
//                           ** description : the days max air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 70.0
//                           ** min : -50.0
//                           ** default :
//                           ** unit : °C
//             * name: globrad
//                           ** description : the days global radiation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 30.0
//                           ** min : 0.0
//                           ** default : 0.0
//                           ** unit : MJ/m**2/d
//             * name: soilCoverage
//                           ** description : soilCoverage
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0.0
//                           ** default : 0.0
//                           ** unit : dimensionless
//             * name: dampingFactor
//                           ** description : dampingFactor
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0.8
//                           ** unit : dimensionless
//             * name: soilSurfaceTemperature
//                           ** description : soilSurfaceTemperature of previous day
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0.0
//                           ** unit : °C
// - outputs:
//             * name: soilSurfaceTemperature
//                           ** description : soilSurfaceTemperature
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit : °C
no_snow_soil_surface_temperature_calculate_model :: proc(m: ^No_Snow_Soil_Surface_Temperature, s: ^Soil_Temperature_Comp_State, s1: ^Soil_Temperature_Comp_State, r: ^Soil_Temperature_Comp_Rate, a: ^Soil_Temperature_Comp_Auxiliary, ex: ^Soil_Temperature_Comp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	shading_coefficient: f64
	ex.globrad = max(8.33, ex.globrad)
	shading_coefficient = 0.1 + (ex.soil_coverage * m.damping_factor + (1 - ex.soil_coverage) * (1 - m.damping_factor))
	s.soil_surface_temperature = (1.0 - shading_coefficient) * (ex.tmin + (ex.tmax - ex.tmin) * cr_math.pow(0.03 * ex.globrad, f64(0.5))) + shading_coefficient * s.soil_surface_temperature
	if s.soil_surface_temperature < 0.0 {
		s.soil_surface_temperature = s.soil_surface_temperature * 0.5
	}
}