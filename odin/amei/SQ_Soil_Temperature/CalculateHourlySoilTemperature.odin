package sq_soil_temperature

import "core:math"
import cr_math "../../support/cr_math"

// - Name: CalculateHourlySoilTemperature -Version: 001, -Time step: 1
// - Description:
//             * Title: CalculateHourlySoilTemperature model
//             * Authors: loic.manceau@inra.fr
//             * Reference: ('http://biomamodelling.org',)
//             * Institution: INRA
//             * ExtendedDescription: Calculate Soil temperature on a hourly basis.Parton, W.J. and Logan, J.A., 1981. A model for diurnal variation in soil and air temperature. Agric. Meteorol., 23: 205-216
//             * ShortDescription: None
Calculate_Hourly_Soil_Temperature :: struct {
	b: f64, // Delay between sunrise and time when minimum temperature is reached [Hour]
	a: f64, // Delay between sunset and time when maximum temperature is reached [Hour]
	c: f64, // Nighttime temperature coefficient [Dpmensionless]
}

make_calculate_hourly_soil_temperature :: proc(allocator := context.allocator) -> (m: Calculate_Hourly_Soil_Temperature) {
	context.allocator = allocator
	m.b = 1.81
	m.a = 0.5
	m.c = 0.49
	return
}

destroy_calculate_hourly_soil_temperature :: proc(m: ^Calculate_Hourly_Soil_Temperature) {
}

// - inputs:
//             * name: minTSoil
//                           ** description : Minimum Soil Temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 80
//                           ** min : -30
//                           ** default : 20
//                           ** unit : Â°C
//             * name: dayLength
//                           ** description : Length of the day
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 24
//                           ** min : 0
//                           ** default : 12
//                           ** unit : hour
//             * name: b
//                           ** description : Delay between sunrise and time when minimum temperature is reached
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 10
//                           ** min : 0
//                           ** default : 1.81
//                           ** unit : Hour
//             * name: a
//                           ** description : Delay between sunset and time when maximum temperature is reached
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 10
//                           ** min : 0
//                           ** default : 0.5
//                           ** unit : Hour
//             * name: maxTSoil
//                           ** description : Maximum Soil Temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 80
//                           ** min : -30
//                           ** default : 20
//                           ** unit : Â°C
//             * name: c
//                           ** description : Nighttime temperature coefficient
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 10
//                           ** min : 0
//                           ** default : 0.49
//                           ** unit : Dpmensionless
// - outputs:
//             * name: hourlySoilT
//                           ** description : Hourly Soil Temperature
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len : 24
//                           ** max : 80
//                           ** min : -30
//                           ** unit : Â°C
calculate_hourly_soil_temperature_calculate_model :: proc(m: ^Calculate_Hourly_Soil_Temperature, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a_: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	resize(&s.hourly_soil_t, 24)
	i: int
	if s.max_t_soil == f64(-999) && s.min_t_soil == f64(999) {
		for i = 0; i < 12; i += 1 {
			s.hourly_soil_t[i] = f64(999)
		}
		for i = 12; i < 24; i += 1 {
			s.hourly_soil_t[i] = f64(-999)
		}
	} else {
		for i = 0; i < 24; i += 1 {
			s.hourly_soil_t[i] = 0.0
		}
		{
			_r0 := get_hourly_soil_surface_temperature(s.max_t_soil, s.min_t_soil, ex.day_length, m.b, m.a, m.c)
			resize(&s.hourly_soil_t, len(_r0))
			copy(s.hourly_soil_t[:], _r0)
			delete(_r0)
		}
	}
}

@(private="file")
get_hourly_soil_surface_temperature :: proc(t_max: f64, t_min: f64, ady: f64, b: f64, a: f64, c: f64) -> []f64 {
	i: int
	result := make([dynamic]f64, 24)
	ahou: f64
	ani: f64
	bb: f64
	be: f64
	bbd: f64
	ddy: f64
	tsn: f64
	ahou = math.PI * (ady / 24.0)
	ani = 24 - ady
	bb = 12 - ady / 2 + c
	be = 12 + ady / 2
	for i = 0; i < 24; i += 1 {
		if i >= int(bb) && i <= int(be) {
			result[i] = (t_max - t_min) * cr_math.sin(3.14 * (f64(i) - bb) / (ady + 2 * a)) + t_min
		} else {
			if i > int(be) {
				bbd = f64(i) - be
			} else {
				bbd = 24 + be + f64(i)
			}
			ddy = ady - c
			tsn = (t_max - t_min) * cr_math.sin(3.14 * ddy / (ady + 2 * a)) + t_min
			result[i] = t_min + (tsn - t_min) * cr_math.exp(-b * bbd / ani)
		}
	}
	return result[:]
}