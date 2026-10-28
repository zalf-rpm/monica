package dssat_st_standalone

import "core:slice"
import cr_math "../../support/cr_math"

// - Name: STEMP -Version:  1.0, -Time step:  1
// - Description:
//             * Title: Model of STEMP
//             * Authors: DSSAT
//             * Reference: https://doi.org/10.2134/agronj1994.00021962008600060014x
//             * Institution: DSSAT Florida
//             * ExtendedDescription: None
//             * ShortDescription: Determines soil temperature by layer
Stemp :: struct {
	nl: int `unit:"dimensionless"`, // Number of soil layers
	iswwat: string `unit:"dimensionless"`, // Water simulation control switch
	bd: [dynamic]f64 `unit:"g [soil] / cm3 [soil]"`, // Bulk density, soil layer NL
	dlayr: [dynamic]f64 `unit:"cm"`, // Thickness of soil layer L
	ds: [dynamic]f64 `unit:"cm"`, // Cumulative depth in soil layer L
	dul: [dynamic]f64 `unit:"cm3[water]/cm3[soil]"`, // Volumetric soil water content at Drained Upper Limit in soil layer L
	ll: [dynamic]f64 `unit:"cm3 [water] / cm3 [soil]"`, // Volumetric soil water content in soil layer L at lower limit
	nlayr: int `unit:"dimensionless"`, // Actual number of soil layers
	msalb: f64 `unit:"dimensionless"`, // Soil albedo with mulch and soil water effects
	sw: [dynamic]f64 `unit:"cm3 [water] / cm3 [soil]"`, // Volumetric soil water content in layer L
	xlat: f64 `unit:"degC"`, // Latitude
}

make_stemp :: proc(allocator := context.allocator) -> (m: Stemp) {
	context.allocator = allocator
	m.iswwat = "Y"
	return
}

destroy_stemp :: proc(m: ^Stemp) {
	delete(m.bd)
	delete(m.dlayr)
	delete(m.ds)
	delete(m.dul)
	delete(m.ll)
	delete(m.sw)
}

stemp_init :: proc(m: ^Stemp, s: ^Stemp_State, s1: ^Stemp_State, r: ^Stemp_Rate, a: ^Stemp_Auxiliary, ex: ^Stemp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	resize(&s.dsmid, m.nl)
	resize(&s.tma, 5)
	resize(&s.st, m.nl)
	s.cumdpt = 0.0
	resize(&s.dsmid, m.nl)
	slice.fill(s.dsmid[:], 0.0)
	s.tdl = 0.0
	resize(&s.tma, 5)
	slice.fill(s.tma[:], 0.0)
	s.atot = 0.0
	s.srftemp = 0.0
	resize(&s.st, m.nl)
	slice.fill(s.st[:], 0.0)
	s.hday = 0.0
	i: int
	l: int
	abd: f64
	albedo: f64
	b: f64
	dp: f64
	fx: f64
	pesw: f64
	tbd: f64
	ww: f64
	tll: f64
	tsw: f64
	dli := make([dynamic]f64, m.nl)
	defer delete(dli)
	dsi := make([dynamic]f64, m.nl)
	defer delete(dsi)
	swi := make([dynamic]f64, m.nl)
	defer delete(swi)
	resize(&swi, len(m.sw[:]))
	copy(swi[:], m.sw[:])
	resize(&dsi, len(m.ds[:]))
	copy(dsi[:], m.ds[:])
	if m.xlat < 0.0 {
		s.hday = 20.0
	} else {
		s.hday = 200.0
	}
	tbd = 0.0
	tll = 0.0
	tsw = 0.0
	s.tdl = 0.0
	s.cumdpt = 0.0
	for l = 1; l < m.nlayr + 1; l += 1 {
		if l == 1 {
			dli[l - 1] = dsi[l - 1]
		} else {
			dli[l - 1] = dsi[l - 1] - dsi[l - 1 - 1]
		}
		s.dsmid[l - 1] = s.cumdpt + dli[l - 1] * 5.0
		s.cumdpt = s.cumdpt + dli[l - 1] * 10.0
		tbd = tbd + m.bd[l - 1] * dli[l - 1]
		tll = tll + m.ll[l - 1] * dli[l - 1]
		tsw = tsw + swi[l - 1] * dli[l - 1]
		s.tdl = s.tdl + m.dul[l - 1] * dli[l - 1]
	}
	if m.iswwat == "Y" {
		pesw = max(0.0, tsw - tll)
	} else {
		pesw = max(0.0, s.tdl - tll)
	}
	abd = tbd / dsi[m.nlayr - 1]
	fx = abd / (abd + 686.0 * cr_math.exp(-(5.63 * abd)))
	dp = 1000.0 + 2500.0 * fx
	ww = 0.356 - 0.144 * abd
	b = cr_math.log(500.0 / dp)
	albedo = m.msalb
	for i = 1; i < 5 + 1; i += 1 {
		s.tma[i - 1] = f64(int(ex.tavg * 10000.0)) / 10000.0
	}
	s.atot = s.tma[1 - 1] * 5.0
	for l = 1; l < m.nlayr + 1; l += 1 {
		s.st[l - 1] = ex.tavg
	}
	for i = 1; i < 8 + 1; i += 1 {
		{
			_r0, _r1, _r2, _r3 := soilt(m.nl, albedo, b, s.cumdpt, ex.doy, dp, s.hday, m.nlayr, pesw, ex.srad, ex.tamp, ex.tav, ex.tavg, ex.tmax, ww, s.dsmid[:], s.atot, s.tma[:])
			s.atot = _r0
			resize(&s.tma, len(_r1))
			copy(s.tma[:], _r1)
			s.srftemp = _r2
			resize(&s.st, len(_r3))
			copy(s.st[:], _r3)
			delete(_r3)
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
//                           ** description : Water simulation control switch
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
//                           ** description : Thickness of soil layer L
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm
//             * name: DS
//                           ** description : Cumulative depth in soil layer L
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
//                           ** description : Volumetric soil water content in soil layer L at lower limit
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
//             * name: MSALB
//                           ** description : Soil albedo with mulch and soil water effects
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : dimensionless
//             * name: SRAD
//                           ** description : Solar radiation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : MJ/m2-d
//             * name: SW
//                           ** description : Volumetric soil water content in layer L
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
//             * name: XLAT
//                           ** description : Latitude
//                           ** inputtype : parameter
//                           ** parametercategory : constant
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
//             * name: TAMP
//                           ** description : Amplitude of temperature function used to calculate soil temperatures
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
//                           ** description : Depth to midpoint of soil layer L
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
//                           ** description : Array of previous 5 days of average soil temperatures
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : 5
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: ATOT
//                           ** description : Sum of TMA array (last 5 days soil temperature)
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
//                           ** description : Soil temperature in soil layer L
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : degC
//             * name: DOY
//                           ** description : Current day of simulation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : d
//             * name: HDAY
//                           ** description : Haverst day
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : day
// - outputs:
//             * name: CUMDPT
//                           ** description : Cumulative depth of soil profile
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max :
//                           ** min :
//                           ** unit : mm
//             * name: DSMID
//                           ** description : Depth to midpoint of soil layer L
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
//                           ** description : Array of previous 5 days of average soil temperatures
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len : 5
//                           ** max :
//                           ** min :
//                           ** unit : degC
//             * name: ATOT
//                           ** description : Sum of TMA array (last 5 days soil temperature)
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
//                           ** description : Soil temperature in soil layer L
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len : NL
//                           ** max :
//                           ** min :
//                           ** unit : degC
stemp_calculate_model :: proc(m: ^Stemp, s: ^Stemp_State, s1: ^Stemp_State, r: ^Stemp_Rate, a: ^Stemp_Auxiliary, ex: ^Stemp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	l: int
	abd: f64
	albedo: f64
	b: f64
	dp: f64
	fx: f64
	pesw: f64
	tbd: f64
	ww: f64
	tll: f64
	tsw: f64
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
	albedo = m.msalb
	if m.iswwat == "Y" {
		pesw = max(0.0, tsw - tll)
	} else {
		pesw = max(0.0, s.tdl - tll)
	}
	{
		_r0, _r1, _r2, _r3 := soilt(m.nl, albedo, b, s.cumdpt, ex.doy, dp, s.hday, m.nlayr, pesw, ex.srad, ex.tamp, ex.tav, ex.tavg, ex.tmax, ww, s.dsmid[:], s.atot, s.tma[:])
		s.atot = _r0
		resize(&s.tma, len(_r1))
		copy(s.tma[:], _r1)
		s.srftemp = _r2
		resize(&s.st, len(_r3))
		copy(s.st[:], _r3)
		delete(_r3)
	}
}

@(private="file")
soilt :: proc(nl: int, albedo: f64, b: f64, cumdpt: f64, doy: int, dp: f64, hday: f64, nlayr: int, pesw: f64, srad: f64, tamp: f64, tav: f64, tavg: f64, tmax: f64, ww: f64, dsmid: []f64, atot: f64, tma: []f64) -> (f64, []f64, f64, []f64) {
	atot := atot
	k: int
	l: int
	alx: f64
	dd: f64
	dt: f64
	fx: f64
	srftemp: f64
	ta: f64
	wc: f64
	zd: f64
	st := make([dynamic]f64, nl)
	alx = (f64(doy) - hday) * 0.0174
	atot = atot - tma[5 - 1]
	for k = 5; k > 2 - 1; k += -1 {
		tma[k - 1] = tma[k - 1 - 1]
	}
	tma[1 - 1] = tavg
	tma[1 - 1] = f64(int(tma[1 - 1] * 10000.0)) / 10000.0
	atot = atot + tma[1 - 1]
	wc = max(0.01, pesw) / (ww * cumdpt) * 10.0
	fx = cr_math.exp(b * cr_math.pow((1.0 - wc) / (1.0 + wc), f64(2)))
	dd = fx * dp
	ta = tav + tamp * cr_math.cos(alx) / 2.0
	dt = atot / 5.0 - ta
	for l = 1; l < nlayr + 1; l += 1 {
		zd = -(dsmid[l - 1] / dd)
		st[l - 1] = tav + (tamp / 2.0 * cr_math.cos(alx + zd) + dt) * cr_math.exp(zd)
	}
	srftemp = tav + (tamp / 2.0 * cr_math.cos(alx) + dt)
	return atot, tma, srftemp, st[:]
}