package sq_soil_temperature

import cr_math "../../support/cr_math"

// - Name: CalculateSoilTemperature -Version: 001, -Time step: 1
// - Description:
//             * Title: CalculateSoilTemperature model
//             * Authors: loic.manceau@inra.fr
//             * Reference: ('http://biomamodelling.org',)
//             * Institution: INRA
//             * ExtendedDescription: Calculation of minimum and maximum Soil temperature, Further used in shoot temperature estimate.
//             * ShortDescription: None
Calculate_Soil_Temperature :: struct {
	lambda_: f64, // Latente heat of water vaporization at 20Â°C [MJ.kg-1]
}

make_calculate_soil_temperature :: proc(allocator := context.allocator) -> (m: Calculate_Soil_Temperature) {
	context.allocator = allocator
	m.lambda_ = 2.454
	return
}

destroy_calculate_soil_temperature :: proc(m: ^Calculate_Soil_Temperature) {
}

calculate_soil_temperature_init :: proc(m: ^Calculate_Soil_Temperature, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	s.deep_layer_t = 20.0
	s.deep_layer_t = ex.mean_annual_air_temp
}

// - inputs:
//             * name: meanTAir
//                           ** description : Mean Air Temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 80
//                           ** min : -30
//                           ** default : 22
//                           ** unit : Â°C
//             * name: minTAir
//                           ** description : Minimum Air Temperature from Weather files
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 80
//                           ** min : -30
//                           ** default : 20
//                           ** unit : Â°C
//             * name: lambda_
//                           ** description : Latente heat of water vaporization at 20Â°C
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 10
//                           ** min : 0
//                           ** default : 2.454
//                           ** unit : MJ.kg-1
//             * name: deepLayerT
//                           ** description : Temperature of the last soil layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 80
//                           ** min : -30
//                           ** default : 20
//                           ** unit : Â°C
//             * name: meanAnnualAirTemp
//                           ** description : Annual Mean Air Temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 80
//                           ** min : -30
//                           ** default : 22
//                           ** unit : Â°C
//             * name: heatFlux
//                           ** description : Soil Heat Flux from Energy Balance Component
//                           ** inputtype : variable
//                           ** variablecategory : rate
//                           ** datatype : DOUBLE
//                           ** max : 100
//                           ** min : 0
//                           ** default : 50
//                           ** unit : g m-2 d-1
//             * name: maxTAir
//                           ** description : Maximum Air Temperature from Weather Files
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 80
//                           ** min : -30
//                           ** default : 25
//                           ** unit : Â°C
// - outputs:
//             * name: minTSoil
//                           ** description : Minimum Soil Temperature
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max : 80
//                           ** min : -30
//                           ** unit : Â°C
//             * name: deepLayerT
//                           ** description : Temperature of the last soil layer
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max : 80
//                           ** min : -30
//                           ** unit : Â°C
//             * name: maxTSoil
//                           ** description : Maximum Soil Temperature
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max : 80
//                           ** min : -30
//                           ** unit : Â°C
calculate_soil_temperature_calculate_model :: proc(m: ^Calculate_Soil_Temperature, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	tmp: f64
	tmp = ex.mean_annual_air_temp
	if ex.max_t_air == f64(-999) && ex.min_t_air == f64(999) {
		s.min_t_soil = f64(999)
		s.max_t_soil = f64(-999)
		s.deep_layer_t = 0.0
	} else {
		s.min_t_soil = soil_minimum_temperature(ex.max_t_air, ex.mean_t_air, ex.min_t_air, r.heat_flux, m.lambda_, s.deep_layer_t)
		s.max_t_soil = soil_maximum_temperature(ex.max_t_air, ex.mean_t_air, ex.min_t_air, r.heat_flux, m.lambda_, s.deep_layer_t)
		s.deep_layer_t = update_temperature(s.min_t_soil, s.max_t_soil, s.deep_layer_t)
	}
}

@(private="file")
soil_minimum_temperature :: proc(weather_max_temp: f64, weather_mean_temp: f64, weather_min_temp: f64, soil_heat_flux: f64, lambda_: f64, deep_temperature: f64) -> f64 {
	return min(soil_temp_a(weather_max_temp, weather_mean_temp, soil_heat_flux, lambda_), soil_temp_b(weather_min_temp, deep_temperature))
}

@(private="file")
soil_temp_b :: proc(weather_min_temp: f64, deep_temperature: f64) -> f64 {
	return (weather_min_temp + deep_temperature) / 2.0
}

@(private="file")
soil_temp_a :: proc(weather_max_temp: f64, weather_mean_temp: f64, soil_heat_flux: f64, lambda_: f64) -> f64 {
	temp_adjustment: f64
	soil_available_energy: f64
	temp_adjustment = weather_mean_temp < 8.0 ? -0.5 * weather_mean_temp + 4.0 : f64(0)
	soil_available_energy = soil_heat_flux * lambda_ / 1000
	result: f64
	result = weather_max_temp + 11.2 * (1.0 - cr_math.exp(-0.07 * (soil_available_energy - 5.5))) + temp_adjustment
	return result
}

@(private="file")
soil_maximum_temperature :: proc(weather_max_temp: f64, weather_mean_temp: f64, weather_min_temp: f64, soil_heat_flux: f64, lambda_: f64, deep_temperature: f64) -> f64 {
	return max(soil_temp_a(weather_max_temp, weather_mean_temp, soil_heat_flux, lambda_), soil_temp_b(weather_min_temp, deep_temperature))
}

@(private="file")
update_temperature :: proc(min_soil_temp: f64, max_soil_temp: f64, temperature: f64) -> f64 {
	temperature := temperature
	mean_temp: f64
	mean_temp = (min_soil_temp + max_soil_temp) / 2.0
	temperature = (9.0 * temperature + mean_temp) / 10.0
	return temperature
}