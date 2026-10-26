package simplace_soil_temperature

import "core:math"
import cr_math "../../support/cr_math"

// - Name: STMPsimCalculator -Version: 001, -Time step: 1
// - Description:
//             * Title: STMPsimCalculator model
//             * Authors: Gunther Krauss
//             * Reference: ('http://www.simplace.net/doc/simplace_modules/',)
//             * Institution: INRES Pflanzenbau, Uni Bonn
//             * ExtendedDescription: as given in the documentation
//             * ShortDescription: None
Stm_Psim_Calculator :: struct {
	c_soil_layer_depth: [dynamic]f64, // Depth of soil layer [http://www.wurvoc.org/vocabularies/om-1.8/metre]
	c_first_day_mean_temp: f64, // Mean air temperature on first day [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	c_avt: f64, // Constant Temperature of deepest soil layer - use long term mean air temperature [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	c_abd: f64, // Mean bulk density [http://www.wurvoc.org/vocabularies/om-1.8/tonne_per_cubic_metre]
	c_damping_depth: f64, // Initial value for damping depth of soil [http://www.wurvoc.org/vocabularies/om-1.8/metre]
}

make_stm_psim_calculator :: proc(allocator := context.allocator) -> (m: Stm_Psim_Calculator) {
	context.allocator = allocator
	m.c_abd = 2.0
	m.c_damping_depth = 6.0
	return
}

destroy_stm_psim_calculator :: proc(m: ^Stm_Psim_Calculator) {
	delete(m.c_soil_layer_depth)
}

stm_psim_calculator_init :: proc(m: ^Stm_Psim_Calculator, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	clear(&s.soil_temp_array)
	clear(&s.r_soil_temp_array_rate)
	clear(&s.p_soil_layer_depth)
	t_profile_depth: f64
	additional_depth: f64
	first_additional_layer_hight: f64
	layers: int
	t_stmp: [dynamic]f64
	defer delete(t_stmp)
	t_stmp_rate: [dynamic]f64
	defer delete(t_stmp_rate)
	tz: [dynamic]f64
	defer delete(tz)
	i: int
	depth: f64
	t_profile_depth = m.c_soil_layer_depth[len(m.c_soil_layer_depth) - 1]
	additional_depth = m.c_damping_depth - t_profile_depth
	first_additional_layer_hight = additional_depth - f64(int(math.floor(additional_depth)))
	layers = int(abs(f64(int(math.ceil(additional_depth))))) + len(m.c_soil_layer_depth)
	resize(&t_stmp, layers)
	resize(&t_stmp_rate, layers)
	resize(&tz, layers)
	for i = 0; i < len(t_stmp); i += 1 {
		if i < len(m.c_soil_layer_depth) {
			depth = m.c_soil_layer_depth[i]
		} else {
			depth = t_profile_depth + first_additional_layer_hight + f64(i) - f64(len(m.c_soil_layer_depth))
		}
		tz[i] = depth
		t_stmp[i] = (m.c_first_day_mean_temp * (m.c_damping_depth - depth) + m.c_avt * depth) / m.c_damping_depth
	}
	resize(&s.r_soil_temp_array_rate, len(t_stmp_rate[:]))
	copy(s.r_soil_temp_array_rate[:], t_stmp_rate[:])
	resize(&s.soil_temp_array, len(t_stmp[:]))
	copy(s.soil_temp_array[:], t_stmp[:])
	resize(&s.p_soil_layer_depth, len(tz[:]))
	copy(s.p_soil_layer_depth[:], tz[:])
}

// - inputs:
//             * name: cSoilLayerDepth
//                           ** description : Depth of soil layer
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 20.0
//                           ** min : 0.03
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/metre
//             * name: cFirstDayMeanTemp
//                           ** description : Mean air temperature on first day
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -40.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: cAVT
//                           ** description : Constant Temperature of deepest soil layer - use long term mean air temperature
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 20.0
//                           ** min : -10.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: cABD
//                           ** description : Mean bulk density
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 4.0
//                           ** min : 1.0
//                           ** default : 2.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/tonne_per_cubic_metre
//             * name: cDampingDepth
//                           ** description : Initial value for damping depth of soil
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 20.0
//                           ** min : 1.5
//                           ** default : 6.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/metre
//             * name: iSoilWaterContent
//                           ** description : Water content, sum of whole soil profile
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 20.0
//                           ** min : 1.5
//                           ** default : 5.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/millimetre
//             * name: iSoilSurfaceTemperature
//                           ** description : Temperature at soil surface
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 20.0
//                           ** min : 1.5
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: SoilTempArray
//                           ** description : Array of soil temperatures in layers
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 50.0
//                           ** min : -40.0
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: rSoilTempArrayRate
//                           ** description : Array of daily temperature change
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 20
//                           ** min : -20
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius_per_day
//             * name: pSoilLayerDepth
//                           ** description : Depth of soil layer plus additional depth
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 20.0
//                           ** min : 0.03
//                           ** default :
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/metre
// - outputs:
//             * name: SoilTempArray
//                           ** description : Array of soil temperatures in layers
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len :
//                           ** max : 50.0
//                           ** min : -40.0
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius
//             * name: rSoilTempArrayRate
//                           ** description : Array of daily temperature change
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len :
//                           ** max : 20
//                           ** min : -20
//                           ** unit : http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius_per_day
stm_psim_calculator_calculate_model :: proc(m: ^Stm_Psim_Calculator, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	xlag: f64
	xlg1: f64
	dp: f64
	wc: f64
	dd: f64
	z1: f64
	i: int
	zd: f64
	rate: f64
	xlag = 0.8
	xlg1 = 1 - xlag
	dp = 1 + 2.5 * m.c_abd / (m.c_abd + cr_math.exp(6.53 - 5.63 * m.c_abd))
	wc = 0.001 * ex.i_soil_water_content / ((0.356 - 0.144 * m.c_abd) * m.c_soil_layer_depth[len(m.c_soil_layer_depth) - 1])
	dd = cr_math.exp(cr_math.log(0.5 / dp) * ((1 - wc) / (1 + wc)) * 2) * dp
	z1 = f64(0)
	for i = 0; i < len(s.soil_temp_array); i += 1 {
		zd = 0.5 * (z1 + s.p_soil_layer_depth[i]) / dd
		rate = zd / (zd + cr_math.exp(-0.8669 - 2.0775 * zd)) * (m.c_avt - a.i_soil_surface_temperature)
		rate = xlg1 * (rate + a.i_soil_surface_temperature - s.soil_temp_array[i])
		z1 = s.p_soil_layer_depth[i]
		s.r_soil_temp_array_rate[i] = rate
		s.soil_temp_array[i] = s.soil_temp_array[i] + s.r_soil_temp_array_rate[i]
	}
}