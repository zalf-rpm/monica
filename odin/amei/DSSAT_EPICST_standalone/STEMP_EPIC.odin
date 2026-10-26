package dssat_epicst_standalone

import "core:math"
import "core:slice"
import cr_math "../../support/cr_math"

// - Name: STEMP_EPIC -Version:  1.0, -Time step:  1
// - Description:
//             * Title: Model of STEMP_EPIC
//             * Authors: Kenneth N. Potter , Jimmy R. Williams
//             * Reference: https://doi.org/10.2134/agronj1994.00021962008600060014x
//             * Institution: USDA-ARS, USDA-ARS
//             * ExtendedDescription: None
//             * ShortDescription: Determines soil temperature by layer test encore
Stemp_Epic :: struct {
	nl: int, // Number of soil layers [dimensionless]
	iswwat: string, // Water simulation control switch (Y or N) [dimensionless]
	bd: [dynamic]f64, // Bulk density, soil layer NL [g [soil] / cm3 [soil]]
	dlayr: [dynamic]f64, // Thickness of soil layer NL [cm]
	ds: [dynamic]f64, // Cumulative depth in soil layer NL [cm]
	dul: [dynamic]f64, // Volumetric soil water content at Drained Upper Limit in soil layer L [cm3[water]/cm3[soil]]
	ll: [dynamic]f64, // Volumetric soil water content in soil layer NL at lower limit [cm3 [water] / cm3 [soil]]
	nlayr: int, // Actual number of soil layers [dimensionless]
	sw: [dynamic]f64, // Volumetric soil water content in layer NL [cm3 [water] / cm3 [soil]]
}

make_stemp_epic :: proc(allocator := context.allocator) -> (m: Stemp_Epic) {
	context.allocator = allocator
	m.iswwat = "Y"
	return
}

destroy_stemp_epic :: proc(m: ^Stemp_Epic) {
	delete(m.bd)
	delete(m.dlayr)
	delete(m.ds)
	delete(m.dul)
	delete(m.ll)
	delete(m.sw)
}

stemp_epic_init :: proc(m: ^Stemp_Epic, s: ^Stemp_Epic_State, s1: ^Stemp_Epic_State, r: ^Stemp_Epic_Rate, a: ^Stemp_Epic_Auxiliary, ex: ^Stemp_Epic_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	resize(&s.dsmid, m.nl)
	resize(&s.tma, 5)
	resize(&s.wet_day, 30)
	resize(&s.st, m.nl)
	s.cumdpt = 0.0
	resize(&s.dsmid, m.nl)
	slice.fill(s.dsmid[:], 0.0)
	s.tdl = 0.0
	resize(&s.tma, 5)
	slice.fill(s.tma[:], 0.0)
	s.n_days = 0
	resize(&s.wet_day, 30)
	slice.fill(s.wet_day[:], 0)
	s.x2_prev = 0.0
	s.srftemp = 0.0
	resize(&s.st, m.nl)
	slice.fill(s.st[:], 0.0)
	i: int
	l: int
	abd: f64
	b: f64
	dp: f64
	fx: f64
	pesw: f64
	tbd: f64
	ww: f64
	tll: f64
	tsw: f64
	x2_avg: f64
	wft: f64
	bcv: f64
	cv: f64
	bcv1: f64
	bcv2: f64
	swi := make([dynamic]f64, m.nl)
	defer delete(swi)
	resize(&swi, len(m.sw[:]))
	copy(swi[:], m.sw[:])
	tbd = 0.0
	tll = 0.0
	tsw = 0.0
	s.tdl = 0.0
	s.cumdpt = 0.0
	for l = 1; l < m.nlayr + 1; l += 1 {
		s.dsmid[l - 1] = s.cumdpt + m.dlayr[l - 1] * 5.0
		s.cumdpt = s.cumdpt + m.dlayr[l - 1] * 10.0
		tbd = tbd + m.bd[l - 1] * m.dlayr[l - 1]
		tll = tll + m.ll[l - 1] * m.dlayr[l - 1]
		tsw = tsw + swi[l - 1] * m.dlayr[l - 1]
		s.tdl = s.tdl + m.dul[l - 1] * m.dlayr[l - 1]
	}
	if m.iswwat == "Y" {
		pesw = max(0.0, tsw - tll)
	} else {
		pesw = max(0.0, s.tdl - tll)
	}
	abd = tbd / m.ds[m.nlayr - 1]
	fx = abd / (abd + 686.0 * cr_math.exp(-(5.63 * abd)))
	dp = 1000.0 + 2500.0 * fx
	ww = 0.356 - 0.144 * abd
	b = cr_math.log(500.0 / dp)
	for i = 1; i < 5 + 1; i += 1 {
		s.tma[i - 1] = f64(int(ex.tavg * 10000.0)) / 10000.0
	}
	x2_avg = s.tma[1 - 1] * 5.0
	for l = 1; l < m.nlayr + 1; l += 1 {
		s.st[l - 1] = ex.tavg
	}
	wft = 0.1
	resize(&s.wet_day, 30)
	slice.fill(s.wet_day[:], 0)
	s.n_days = 0
	cv = ex.mulchmass / 1000.0
	bcv1 = cv / (cv + cr_math.exp(5.3396 - 2.3951 * cv))
	bcv2 = ex.snow / (ex.snow + cr_math.exp(2.303 - 0.2197 * ex.snow))
	bcv = max(bcv1, bcv2)
	for i = 1; i < 8 + 1; i += 1 {
		{
			_r0, _r1, _r2, _r3, _r4 := soilt_epic(m.nl, b, bcv, s.cumdpt, dp, s.dsmid[:], m.nlayr, pesw, ex.tav, ex.tavg, ex.tmax, ex.tmin, 0, wft, ww, s.tma[:], s.st[:], s.x2_prev)
			resize(&s.tma, len(_r0))
			copy(s.tma[:], _r0)
			s.srftemp = _r1
			resize(&s.st, len(_r2))
			copy(s.st[:], _r2)
			x2_avg = _r3
			s.x2_prev = _r4
		}
	}
}

// - inputs:
//             * name: NL
//                           ** description : Number of soil layers
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : dimensionless
//             * name: ISWWAT
//                           ** description : Water simulation control switch (Y or N)
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : STRING
//                           ** max :
//                           ** min :
//                           ** default : Y
//                           ** unit : dimensionless
//             * name: BD
//                           ** description : Bulk density, soil layer NL
//                           ** inputtype : parameter
//                           ** parametercategory : soil
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : g [soil] / cm3 [soil]
//             * name: DLAYR
//                           ** description : Thickness of soil layer NL
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm
//             * name: DS
//                           ** description : Cumulative depth in soil layer NL
//                           ** inputtype : parameter
//                           ** parametercategory : soil
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm
//             * name: DUL
//                           ** description : Volumetric soil water content at Drained Upper Limit in soil layer L
//                           ** inputtype : parameter
//                           ** parametercategory : soil
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm3[water]/cm3[soil]
//             * name: LL
//                           ** description : Volumetric soil water content in soil layer NL at lower limit
//                           ** inputtype : parameter
//                           ** parametercategory : soil
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm3 [water] / cm3 [soil]
//             * name: NLAYR
//                           ** description : Actual number of soil layers
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : dimensionless
//             * name: TAMP
//                           ** description : Annual amplitude of the average air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: RAIN
//                           ** description : daily rainfall
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: SW
//                           ** description : Volumetric soil water content in layer NL
//                           ** inputtype : parameter
//                           ** parametercategory : soil
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm3 [water] / cm3 [soil]
//             * name: TAVG
//                           ** description : Average daily temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: TMAX
//                           ** description : Maximum daily temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: TMIN
//                           ** description : Minimum Temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: TAV
//                           ** description : Average annual soil temperature, used with TAMP to calculate soil temperature.
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: CUMDPT
//                           ** description : Cumulative depth of soil profile
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: DSMID
//                           ** description : Depth to midpoint of soil layer NL
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm
//             * name: TDL
//                           ** description : Total water content of soil at drained upper limit
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm
//             * name: TMA
//                           ** description : Array of previous 5 days of average soil temperatures.
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : 5
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: NDays
//                           ** description : Number of days ...
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : day
//             * name: WetDay
//                           ** description : Wet Days
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : INTARRAY
//                           ** len : 30
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : day
//             * name: X2_PREV
//                           ** description : Temperature of soil surface at precedent timestep
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: SRFTEMP
//                           ** description : Temperature of soil surface litter
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: ST
//                           ** description : Soil temperature in soil layer NL
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: DEPIR
//                           ** description : Depth of irrigation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: BIOMAS
//                           ** description : Biomass
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : kg/ha
//             * name: MULCHMASS
//                           ** description : Mulch Mass
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : kg/ha
//             * name: SNOW
//                           ** description : Snow cover
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
// - outputs:
//             * name: CUMDPT
//                           ** description : Cumulative depth of soil profile
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max :
//                           ** min :
//                           ** unit : mm
//             * name: DSMID
//                           ** description : Depth to midpoint of soil layer NL
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** unit : cm
//             * name: TDL
//                           ** description : Total water content of soil at drained upper limit
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max :
//                           ** min :
//                           ** unit : cm
//             * name: TMA
//                           ** description : Array of previous 5 days of average soil temperatures.
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len : 5
//                           ** max :
//                           ** min :
//                           ** unit : degC
//             * name: NDays
//                           ** description : Number of days ...
//                           ** datatype : INT
//                           ** variablecategory : state
//                           ** max :
//                           ** min :
//                           ** unit : day
//             * name: WetDay
//                           ** description : Wet Days
//                           ** datatype : INTARRAY
//                           ** variablecategory : state
//                           ** len : 30
//                           ** max :
//                           ** min :
//                           ** unit : day
//             * name: X2_PREV
//                           ** description : Temperature of soil surface at precedent timestep
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max :
//                           ** min :
//                           ** unit : degC
//             * name: SRFTEMP
//                           ** description : Temperature of soil surface litter
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max :
//                           ** min :
//                           ** unit : degC
//             * name: ST
//                           ** description : Soil temperature in soil layer NL
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** unit : degC
stemp_epic_calculate_model :: proc(m: ^Stemp_Epic, s: ^Stemp_Epic_State, s1: ^Stemp_Epic_State, r: ^Stemp_Epic_Rate, a: ^Stemp_Epic_Auxiliary, ex: ^Stemp_Epic_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	i: int
	l: int
	n_wet_days: int
	abd: f64
	b: f64
	dp: f64
	fx: f64
	pesw: f64
	tbd: f64
	ww: f64
	tll: f64
	tsw: f64
	x2_avg: f64
	wft: f64
	bcv: f64
	cv: f64
	bcv1: f64
	bcv2: f64
	tbd = 0.0
	tll = 0.0
	tsw = 0.0
	for l = 1; l < m.nlayr + 1; l += 1 {
		tbd = tbd + m.bd[l - 1] * m.dlayr[l - 1]
		s.tdl = s.tdl + m.dul[l - 1] * m.dlayr[l - 1]
		tll = tll + m.ll[l - 1] * m.dlayr[l - 1]
		tsw = tsw + m.sw[l - 1] * m.dlayr[l - 1]
	}
	abd = tbd / m.ds[m.nlayr - 1]
	fx = abd / (abd + 686.0 * cr_math.exp(-(5.63 * abd)))
	dp = 1000.0 + 2500.0 * fx
	ww = 0.356 - 0.144 * abd
	b = cr_math.log(500.0 / dp)
	if m.iswwat == "Y" {
		pesw = max(0.0, tsw - tll)
	} else {
		pesw = max(0.0, s.tdl - tll)
	}
	if s.n_days == 30 {
		for i = 1; i < 29 + 1; i += 1 {
			s.wet_day[i - 1] = s.wet_day[i + 1 - 1]
		}
	} else {
		s.n_days = s.n_days + 1
	}
	if ex.rain + ex.depir > 1.E-6 {
		s.wet_day[s.n_days - 1] = 1
	} else {
		s.wet_day[s.n_days - 1] = 0
	}
	n_wet_days = math.sum(s.wet_day[:])
	wft = f64(n_wet_days) / f64(s.n_days)
	cv = (ex.biomas + ex.mulchmass) / 1000.0
	bcv1 = cv / (cv + cr_math.exp(5.3396 - 2.3951 * cv))
	bcv2 = ex.snow / (ex.snow + cr_math.exp(2.303 - 0.2197 * ex.snow))
	bcv = max(bcv1, bcv2)
	{
		_r0, _r1, _r2, _r3, _r4 := soilt_epic(m.nl, b, bcv, s.cumdpt, dp, s.dsmid[:], m.nlayr, pesw, ex.tav, ex.tavg, ex.tmax, ex.tmin, s.wet_day[s.n_days - 1], wft, ww, s.tma[:], s.st[:], s.x2_prev)
		resize(&s.tma, len(_r0))
		copy(s.tma[:], _r0)
		s.srftemp = _r1
		resize(&s.st, len(_r2))
		copy(s.st[:], _r2)
		x2_avg = _r3
		s.x2_prev = _r4
	}
}

@(private="file")
soilt_epic :: proc(nl: int, b: f64, bcv: f64, cumdpt: f64, dp: f64, dsmid: []f64, nlayr: int, pesw: f64, tav: f64, tavg: f64, tmax: f64, tmin: f64, wet_day: int, wft: f64, ww: f64, tma: []f64, st: []f64, x2_prev: f64) -> ([]f64, f64, []f64, f64, f64) {
	x2_prev := x2_prev
	k: int
	l: int
	dd: f64
	fx: f64
	srftemp: f64
	wc: f64
	zd: f64
	x1: f64
	x2: f64
	x3: f64
	f: f64
	x2_avg: f64
	lag: f64
	lag = 0.5
	wc = max(0.01, pesw) / (ww * cumdpt) * 10.0
	fx = cr_math.exp(b * cr_math.pow((1.0 - wc) / (1.0 + wc), f64(2)))
	dd = fx * dp
	if wet_day > 0 {
		x2 = wft * (tavg - tmin) + tmin
	} else {
		x2 = wft * (tmax - tavg) + tavg + 2.0
	}
	tma[1 - 1] = x2
	for k = 5; k > 2 - 1; k += -1 {
		tma[k - 1] = tma[k - 1 - 1]
	}
	x2_avg = math.sum(tma[:]) / 5.0
	x3 = (1.0 - bcv) * x2_avg + bcv * x2_prev
	srftemp = min(x2_avg, x3)
	x1 = tav - x3
	for l = 1; l < nlayr + 1; l += 1 {
		zd = dsmid[l - 1] / dd
		f = zd / (zd + cr_math.exp(-0.8669 - 2.0775 * zd))
		st[l - 1] = lag * st[l - 1] + (1.0 - lag) * (f * x1 + x3)
	}
	x2_prev = x2_avg
	return tma, srftemp, st, x2_avg, x2_prev
}