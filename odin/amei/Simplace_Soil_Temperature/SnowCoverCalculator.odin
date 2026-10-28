package simplace_soil_temperature

import "core:math"
import cr_math "../../support/cr_math"

// - Name: SnowCoverCalculator -Version: 001, -Time step: 1
// - Description:
//             * Title: SnowCoverCalculator model
//             * Authors: Gunther Krauss
//             * Reference: ('http://www.simplace.net/doc/simplace_modules/',)
//             * Institution: INRES Pflanzenbau, Uni Bonn
//             * ExtendedDescription: as given in the documentation
//             * ShortDescription: None
Snow_Cover_Calculator :: struct {
	c_carbon_content: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/percent"`, // Carbon content of upper soil layer
	c_initial_age_of_snow: int `unit:"http://www.wurvoc.org/vocabularies/om-1.8/percent"`, // Initial age of snow
	c_initial_snow_water_content: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/percent"`, // Initial snow water content
	albedo: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/one"`, // Albedo
	c_snow_isolation_factor_a: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/one"`, // Static part of the snow isolation index calculation
	c_snow_isolation_factor_b: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/one"`, // Dynamic part of the snow isolation index calculation
}

make_snow_cover_calculator :: proc(allocator := context.allocator) -> (m: Snow_Cover_Calculator) {
	context.allocator = allocator
	m.c_carbon_content = 0.5
	m.c_initial_age_of_snow = 0
	m.c_initial_snow_water_content = 0.0
	m.c_snow_isolation_factor_a = 2.3
	m.c_snow_isolation_factor_b = 0.22
	return
}

destroy_snow_cover_calculator :: proc(m: ^Snow_Cover_Calculator) {
}

snow_cover_calculator_init :: proc(m: ^Snow_Cover_Calculator, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	s.snow_water_content = 0.0
	s.soil_surface_temperature = 0.0
	s.age_of_snow = 0
	s.p_internal_albedo = 0.0
	tmean: f64
	tampl: f64
	dst: f64
	if m.albedo == f64(0) {
		s.p_internal_albedo = 0.0226 * (cr_math.log(m.c_carbon_content) / cr_math.log(f64(10))) + 0.1502
	} else {
		s.p_internal_albedo = m.albedo
	}
	tmean = 0.5 * (ex.i_temp_max + ex.i_temp_min)
	tampl = 0.5 * (ex.i_temp_max - ex.i_temp_min)
	dst = tmean + tampl * (ex.i_radiation * (1 - s.p_internal_albedo) - 14) / 20
	s.soil_surface_temperature = dst
	s.age_of_snow = m.c_initial_age_of_snow
	s.snow_water_content = m.c_initial_snow_water_content
}

// - inputs:
//             * name: cCarbonContent
//                           ** description : Carbon content of upper soil layer
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 20.0
//                           ** min : 0.5
//                           ** default : 0.5
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/percent
//             * name: cInitialAgeOfSnow
//                           ** description : Initial age of snow
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/percent
//             * name: cInitialSnowWaterContent
//                           ** description : Initial snow water content
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 1500.0
//                           ** min : 0.0
//                           ** default : 0.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/percent
//             * name: Albedo
//                           ** description : Albedo
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 1.0
//                           ** min : 0.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/one
//             * name: pInternalAlbedo
//                           ** description : Albedo privat
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 1.0
//                           ** min : 0.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/one
//             * name: cSnowIsolationFactorA
//                           ** description : Static part of the snow isolation index calculation
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 10.0
//                           ** min : 0.0
//                           ** default : 2.3
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/one
//             * name: cSnowIsolationFactorB
//                           ** description : Dynamic part of the snow isolation index calculation
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 1.0
//                           ** min : 0.0
//                           ** default : 0.22
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/one
//             * name: iTempMax
//                           ** description : Daily maximum air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -40.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: iTempMin
//                           ** description : Daily minimum air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -40.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: iRadiation
//                           ** description : Global Solar radiation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 2000.0
//                           ** min : 0.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/megajoule_per_square_metre
//             * name: iRAIN
//                           ** description : Rain amount
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 60.0
//                           ** min : 0.0
//                           ** default : 0.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/millimetre
//             * name: iCropResidues
//                           ** description : Crop residues plus above ground biomass
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 20000.0
//                           ** min : 0.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/gram_per_square_metre
//             * name: iPotentialSoilEvaporation
//                           ** description : Potenial Evaporation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 12.0
//                           ** min : 0.0
//                           ** default : 0.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/millimetre
//             * name: iLeafAreaIndex
//                           ** description : Leaf area index
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 10.0
//                           ** min : 0.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/square_metre_per_square_metre
//             * name: iSoilTempArray
//                           ** description : Soil Temp array of last day
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 35.0
//                           ** min : -15.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: SnowWaterContent
//                           ** description : Snow water content
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 1500.0
//                           ** min : 0.0
//                           ** default : 0.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/millimetre
//             * name: SoilSurfaceTemperature
//                           ** description : Soil surface temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 70.0
//                           ** min : -40.0
//                           ** default : 0.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: AgeOfSnow
//                           ** description : Age of snow
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : INT
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/day
// - outputs:
//             * name: SnowWaterContent
//                           ** description : Snow water content
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max : 1500.0
//                           ** min : 0.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/millimetre
//             * name: SoilSurfaceTemperature
//                           ** description : Soil surface temperature
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max : 70.0
//                           ** min : -40.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: AgeOfSnow
//                           ** description : Age of snow
//                           ** datatype : INT
//                           ** variablecategory : state
//                           ** max :
//                           ** min : 0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/day
//             * name: rSnowWaterContentRate
//                           ** description : daily snow water content change rate
//                           ** datatype : DOUBLE
//                           ** variablecategory : rate
//                           ** max : 1500.0
//                           ** min : -1500.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/millimetre_per_day
//             * name: rSoilSurfaceTemperatureRate
//                           ** description : daily soil surface temperature change rate
//                           ** datatype : DOUBLE
//                           ** variablecategory : rate
//                           ** max : 70.0
//                           ** min : -40.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius_per_day
//             * name: rAgeOfSnowRate
//                           ** description : daily age of snow change rate
//                           ** datatype : INT
//                           ** variablecategory : rate
//                           ** max :
//                           ** min :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/one
//             * name: SnowIsolationIndex
//                           ** description : Snow isolation index
//                           ** datatype : DOUBLE
//                           ** variablecategory : auxiliary
//                           ** max : 1.0
//                           ** min : 0.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/one
snow_cover_calculator_calculate_model :: proc(m: ^Snow_Cover_Calculator, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	ti_crop_residues: f64
	ti_soil_temp_array: f64
	tmean: f64
	tampl: f64
	dst: f64
	t_soil_surface_temperature: f64
	t_snow_isolation_index: f64
	snowevaporation: f64
	snowmelt: f64
	eaj: f64
	age_of_snow_factor: f64
	snpkt: f64
	ti_crop_residues = ex.i_crop_residues * 10.0
	ti_soil_temp_array = a.i_soil_temp_array[0]
	tmean = 0.5 * (ex.i_temp_max + ex.i_temp_min)
	tampl = 0.5 * (ex.i_temp_max - ex.i_temp_min)
	dst = tmean + tampl * (ex.i_radiation * (1 - s.p_internal_albedo) - 14) / 20
	if ex.i_rain > f64(0) && (ti_soil_temp_array < f64(1) || (s.snow_water_content > f64(3) || s.soil_surface_temperature < f64(0))) {
		s.snow_water_content = s.snow_water_content + ex.i_rain
	}
	t_snow_isolation_index = 1.0
	if ti_crop_residues < f64(10) {
		t_snow_isolation_index = ti_crop_residues / (ti_crop_residues + cr_math.exp(5.34 - 2.4 * ti_crop_residues))
	}
	if s.snow_water_content < 1E-10 {
		t_snow_isolation_index = t_snow_isolation_index * 0.85
		t_soil_surface_temperature = 0.5 * (dst + (1 - t_snow_isolation_index) * dst + t_snow_isolation_index * ti_soil_temp_array)
	} else {
		t_snow_isolation_index = max(s.snow_water_content / (s.snow_water_content + cr_math.exp(m.c_snow_isolation_factor_a - m.c_snow_isolation_factor_b * s.snow_water_content)), t_snow_isolation_index)
		t_soil_surface_temperature = (1 - t_snow_isolation_index) * dst + t_snow_isolation_index * ti_soil_temp_array
	}
	if s.snow_water_content == f64(0) && !(ex.i_rain > f64(0) && ti_soil_temp_array < f64(1)) {
		s.snow_water_content = f64(0)
	} else {
		eaj = 0.5
		if s.snow_water_content < f64(5) {
			eaj = cr_math.exp(-max(0.4 * ex.i_leaf_area_index, 0.1 * (ti_crop_residues + 0.1)))
		}
		snowevaporation = ex.i_potential_soil_evaporation * eaj
		age_of_snow_factor = f64(s.age_of_snow) / (f64(s.age_of_snow) + cr_math.exp(5.34 - 2.395 * f64(s.age_of_snow)))
		snpkt = 0.3333 * (2 * min(t_soil_surface_temperature, ti_soil_temp_array) + ex.i_temp_max)
		if tmean > f64(0) {
			snowmelt = max(0, math.sqrt(ex.i_temp_max * ex.i_radiation) * (1.52 + 0.54 * age_of_snow_factor * snpkt))
		} else {
			snowmelt = f64(0)
		}
		if snowmelt + snowevaporation > s.snow_water_content {
			snowmelt = snowmelt / (snowmelt + snowevaporation) * s.snow_water_content
			snowevaporation = snowevaporation / (snowmelt + snowevaporation) * s.snow_water_content
		}
		s.snow_water_content = s.snow_water_content - (snowmelt + snowevaporation)
		if s.snow_water_content < f64(0) {
			s.snow_water_content = f64(0)
		}
		if s.snow_water_content < f64(5) {
			s.age_of_snow = 0
		} else {
			s.age_of_snow = s.age_of_snow + 1
		}
	}
	r.r_snow_water_content_rate = s.snow_water_content - s.snow_water_content
	r.r_soil_surface_temperature_rate = t_soil_surface_temperature - s.soil_surface_temperature
	r.r_age_of_snow_rate = s.age_of_snow - s.age_of_snow
	s.soil_surface_temperature = t_soil_surface_temperature
	a.snow_isolation_index = t_snow_isolation_index
}