// Port of src/core/photosynthesis-FvCB.{h,cpp}: the Farquhar-von Caemmerer-
// Berry C3 photosynthesis model, coupled with stomatal conductance (Yin &
// Struik 2009).
//
// Fully self-contained - confirmed by reading the whole header: every
// function takes plain struct/scalar parameters, nothing here touches
// CropModule or MonicaModel. That makes this (along with O3-impact,
// voc-guenther, voc-jjv) a natural first phase-5 checkpoint, before the
// ~5,500-line crop-module.cpp itself, which calls into this.
//
// C++'s file-scope `std::map<FvCB_Model_Consts, double> c_bernacchi/
// deltaH_bernacchi` (7 entries each, populated once at static-init time and
// never mutated) become plain switch-accessor procs instead of package-level
// map globals - same values, no global mutable map/init-order question to
// reason about, and every call site (Tresp_bernacchi_f's siblings) already
// only ever reads them.
//
// NOTE(c++-quirk): `FvCB_canopy_hourly_C3`'s shaded branch has a copy-paste
// bug - `out.shaded.cc = get<0>(sh_ci_cc_gs);` uses index 0 (Ci) instead of
// index 1 (Cc), unlike the sunlit branch three lines above which correctly
// uses `get<1>` for `.cc`. So `out.shaded.cc` always duplicates
// `out.shaded.ci` in the real C++. Reproduced exactly, not fixed.
package core

import libc "core:c/libc"
import tl "../../support/tools"

@(private)
Fvcb_Model_Const :: enum {
	RD,
	VCMAX,
	VOMAX,
	GAMMA,
	KC,
	KO,
	JMAX,
}

// C++: std::map<FvCB_Model_Consts, double> FvCB::c_bernacchi (dimensionless)
@(private)
fvcb_c_bernacchi :: proc(k: Fvcb_Model_Const) -> f64 {
	switch k {
	case .RD:
		return 18.72
	case .VCMAX:
		return 26.35
	case .VOMAX:
		return 22.98
	case .GAMMA:
		return 19.02
	case .KC:
		return 38.05
	case .KO:
		return 20.30
	case .JMAX:
		return 17.57
	}
	return 0
}

// C++: std::map<FvCB_Model_Consts, double> FvCB::deltaH_bernacchi (kJ mol-1)
@(private)
fvcb_delta_h_bernacchi :: proc(k: Fvcb_Model_Const) -> f64 {
	switch k {
	case .RD:
		return 46.39
	case .VCMAX:
		return 65.33
	case .VOMAX:
		return 60.11
	case .GAMMA:
		return 37.83
	case .KC:
		return 79.43
	case .KO:
		return 36.38
	case .JMAX:
		return 43.54
	}
	return 0
}

// C++: struct FvCB::FvCB_canopy_hourly_params
Fvcb_Canopy_Hourly_Params :: struct {
	Vcmax_25: f64,
	kn:       f64, // coefficient of leaf nitrogen allocation
	gb:       f64, // boundary layer conductance, mol m-2 s-1 bar-1
	g0:       f64, // residual stomatal conductance, mol m-2 s-1 bar-1
	gm_25:    f64, // mesophyll conductance (C3) at 25degC, mol m-2 s-1 bar-1
}

// C++ in-class initialisers: kn{0.713}, gb{1.5}, g0{0.01}, gm_25{0.10125}
make_fvcb_canopy_hourly_params :: proc() -> Fvcb_Canopy_Hourly_Params {
	return Fvcb_Canopy_Hourly_Params{kn = 0.713, gb = 1.5, g0 = 0.01, gm_25 = 0.10125}
}

// C++: struct FvCB::FvCB_canopy_hourly_in
Fvcb_Canopy_Hourly_In :: struct {
	global_rad:     f64, // MJ m-2 h-1
	extra_terr_rad: f64, // MJ m-2 h-1
	solar_el:       f64, // radians
	LAI:            f64, // m2 m-2
	leaf_temp:      f64, // degC
	VPD:            f64, // KPa
	Ca:             f64, // ambient CO2 partial pressure, ubar or umol mol-1
}

// C++: struct FvCB::FvCB_leaf_fraction
Fvcb_Leaf_Fraction :: struct {
	LAI:    f64, // m2 m-2
	gs:     f64, // mol m-2 s-1 bar-1 (unit ground area)
	kc:     f64, // umol mol-1 mbar-1
	ko:     f64, // umol mol-1 mbar-1
	oi:     f64, // umol m-2
	ci:     f64, // umol m-2
	cc:     f64, // umol m-2
	comp:   f64, // umol mol-1
	vcMax:  f64, // umol m-2 s-1
	jMax:   f64, // umol m-2 s-1
	rad:    f64, // W m-2
	jj:     f64, // umol m-2 s-1
	jv:     f64, // umol m-2 s-1
	jj1000: f64, // umol m-2 s-1
}

// C++: struct FvCB::FvCB_canopy_hourly_out
Fvcb_Canopy_Hourly_Out :: struct {
	canopy_net_photos:   f64, // umol CO2 m-2 h-1
	canopy_resp:         f64, // umol CO2 m-2 h-1
	canopy_gross_photos: f64, // umol CO2 m-2 h-1
	jmax_c:              f64, // umol m-2 s-1
	sunlit:              Fvcb_Leaf_Fraction,
	shaded:              Fvcb_Leaf_Fraction,
}

// C++: double diffuse_fraction_hourly_f(double, double, double)
//
// Estimate the fraction of diffuse radiation; requires hourly input.
@(private)
fvcb_diffuse_fraction_hourly_f :: proc(globrad, extra_terr_rad, solar_elev: f64) -> f64 {
	glob_extra_ratio := globrad / extra_terr_rad
	R := 0.847 - 1.61*libc.sin(solar_elev) + 1.04*libc.pow(libc.sin(solar_elev), 2)
	K := (1.47 - R) / 1.66

	if glob_extra_ratio <= 0.22 {
		return 1
	} else if glob_extra_ratio <= 0.35 {
		return 1 - 6.4*libc.pow(glob_extra_ratio-0.22, 2)
	} else if glob_extra_ratio <= K {
		return 1.47 - 1.66*glob_extra_ratio
	} else {
		return R
	}
}

// direct beam absorbed by sunlit leaves
@(private)
fvcb_abs_sunlit_direct_f :: proc(I_dir_beam, solar_elev, LAI: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
	} else {
		kb = 0.5 / libc.sin(solar_elev)
	}

	sigma := 0.15 // scattering coefficient
	return I_dir_beam * (1 - sigma) * (1 - libc.exp(-kb*LAI))
}

// diffuse radiation absorbed by sunlit leaves
@(private)
fvcb_abs_sunlit_diffuse_f :: proc(I_dif, solar_elev, LAI: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy
	rho_cd := 0.036 // reflection coeff diffuse PAR
	k1_d := 0.719 // diffuse par extinction coeff

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
	} else {
		kb = 0.5 / libc.sin(solar_elev)
	}
	return I_dif * (1 - rho_cd) * (1 - libc.exp(-(k1_d+kb)*LAI)) * k1_d / (k1_d + kb)
}

// scattered beam absorbed by sunlit leaves
@(private)
fvcb_abs_sunlit_scattered_f :: proc(I_dir_beam, solar_elev, LAI: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy
	k1_b: f64 // beam and scattered beam PAR extinction coefficient
	rho_cb: f64 // reflection coefficient beam irradiance (uniform leaf angle distrib)
	sigma: f64 // scattering coefficient

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
		k1_b = 1000
	} else {
		kb = 0.5 / libc.sin(solar_elev)
		k1_b = 0.46 / libc.sin(solar_elev)
	}
	rho_h := 0.041 // reflection coeff beam irradiance (horizontal leaves)
	rho_cb = 1 - libc.exp(2*rho_h*kb/(1+kb))
	sigma = 0.15

	line_1 := (1 - rho_cb) * (1 - libc.exp(-(k1_b+kb)*LAI)) * k1_b / (k1_b + kb)
	line_2 := (1 - sigma) * (1 - libc.exp(-2*kb*LAI)) / 2

	return I_dir_beam * (line_1 - line_2)
}

// irradiance absorbed by the canopy
@(private)
fvcb_Ic_f :: proc(I_dir_beam, I_dif, solar_elev, LAI: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy
	k1_b: f64 // beam and scattered beam PAR extinction coefficient

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
		k1_b = 1000
	} else {
		kb = 0.5 / libc.sin(solar_elev)
		k1_b = 0.46 / libc.sin(solar_elev)
	}

	rho_h := 0.041
	rho_cb := 1 - libc.exp(2*rho_h*kb/(1+kb))
	rho_cd := 0.036 // reflection coeff diffuse PAR
	k1_d := 0.719 // diffuse par extinction coeff

	Ic_dir := (1 - rho_cb) * I_dir_beam * (1 - libc.exp(-k1_b*LAI))
	Ic_dif := (1 - rho_cd) * I_dif * (1 - libc.exp(-k1_d*LAI))

	return min(Ic_dir+Ic_dif, I_dir_beam+I_dif)
}

// irradiance absorbed by sunlit LAI
@(private)
fvcb_Ic_sun_f :: proc(I_dir_beam, I_dif, solar_elev, LAI: f64) -> f64 {
	return(
		fvcb_abs_sunlit_direct_f(I_dir_beam, solar_elev, LAI) +
		fvcb_abs_sunlit_diffuse_f(I_dif, solar_elev, LAI) +
		fvcb_abs_sunlit_scattered_f(I_dir_beam, solar_elev, LAI) \
	)
}

// irradiance absorbed by shaded LAI
@(private)
fvcb_Ic_shade_f :: proc(I_dir_beam, I_dif, solar_elev, LAI: f64) -> f64 {
	return(
		fvcb_Ic_f(I_dir_beam, I_dif, solar_elev, LAI) -
		fvcb_Ic_sun_f(I_dir_beam, I_dif, solar_elev, LAI) \
	)
}

@(private)
fvcb_LAI_sunlit_shaded_f :: proc(LAI, solar_elev: f64) -> (LAI_sunlit: f64, LAI_shaded: f64) {
	kb: f64 // beam radiation extinction coefficient of canopy

	if solar_elev < 0 {
		return 0, LAI
	} else if solar_elev == 0 {
		kb = 1000
	} else {
		kb = 0.5 / libc.sin(solar_elev)
	}

	LAI_sunlit = (1 - libc.exp(-kb*LAI)) / kb
	return LAI_sunlit, LAI - LAI_sunlit
}

// T response
@(private)
fvcb_Tresp_bernacchi_f :: proc(c, deltaH, leafT: f64) -> f64 {
	Tk := leafT + 273
	R := 8.314472 * libc.pow(10, f64(-3)) // kJ K-1 mol-1
	return libc.exp(c - deltaH/(R*Tk))
}

// C++: double FvCB::Vcmax_bernacchi_f(double, double)
fvcb_Vcmax_bernacchi_f :: proc(leafT, Vcmax_25: f64) -> f64 {
	return Vcmax_25 * fvcb_Tresp_bernacchi_f(fvcb_c_bernacchi(.VCMAX), fvcb_delta_h_bernacchi(.VCMAX), leafT)
}

// C++: double FvCB::Jmax_bernacchi_f(double, double)
fvcb_Jmax_bernacchi_f :: proc(leafT, Jmax_25: f64) -> f64 {
	return Jmax_25 * fvcb_Tresp_bernacchi_f(fvcb_c_bernacchi(.JMAX), fvcb_delta_h_bernacchi(.JMAX), leafT)
}

@(private)
fvcb_J_bernacchi_f :: proc(Q, leafT, Jmax: f64) -> f64 {
	alfa := 0.85 // total leaf absorbance
	beta := 0.5 // fraction of absorbed quanta reaching PSII
	theta_ps2 := 0.76 + 0.018*leafT - 3.7*libc.pow(10, f64(-4))*libc.pow(leafT, 2)
	phi_ps2max := 0.352 + 0.022*leafT - 3.4*libc.pow(10, f64(-4))*libc.pow(leafT, 2)
	Q2 := Q * alfa * phi_ps2max * beta

	numerator := Q2 + Jmax - libc.sqrt(libc.pow(Q2+Jmax, 2)-4*theta_ps2*Q2*Jmax)
	denominator := 2 * theta_ps2
	return numerator / denominator
}

// unused by FvCB_canopy_hourly_C3 (the C++ leaves the alternative J_c_sun/
// J_c_sh call commented out) but a real, exported-from-the-.cpp function -
// ported for completeness.
@(private)
fvcb_J_grote_f :: proc(Q, Jmax: f64) -> f64 {
	species_THETA := 0.85 // curvature parameter
	tmp_var := ((Q + Jmax) * (Q + Jmax)) - (4.0 * species_THETA * Q * Jmax)
	// NOTE(c++-quirk): the C++ comment claims tmp_var should be the inverse
	// sqrt, but the code only takes the plain sqrt - reproduced as-is.
	jj := tmp_var > 0.0 ? (Q + Jmax - libc.sqrt(tmp_var)) / (2.0 * species_THETA) : 0.0
	return jj
}

@(private)
fvcb_Rd_bernacchi_f :: proc(leafT: f64) -> f64 {
	return fvcb_Tresp_bernacchi_f(fvcb_c_bernacchi(.RD), fvcb_delta_h_bernacchi(.RD), leafT)
}

@(private)
fvcb_Vomax_bernacchi_f :: proc(leafT, Vcmax_25: f64) -> f64 {
	return Vcmax_25 * fvcb_Tresp_bernacchi_f(fvcb_c_bernacchi(.VOMAX), fvcb_delta_h_bernacchi(.VOMAX), leafT)
}

@(private)
fvcb_Kc_bernacchi_f :: proc(leafT: f64) -> f64 {
	return fvcb_Tresp_bernacchi_f(fvcb_c_bernacchi(.KC), fvcb_delta_h_bernacchi(.KC), leafT)
}

@(private)
fvcb_Ko_bernacchi_f :: proc(leafT: f64) -> f64 {
	return fvcb_Tresp_bernacchi_f(fvcb_c_bernacchi(.KO), fvcb_delta_h_bernacchi(.KO), leafT)
}

@(private)
fvcb_Oi_f :: proc(leafT: f64) -> f64 {
	T1 := 1.3087 * libc.pow(10, f64(-3)) * leafT
	T2 := 2.5603 * libc.pow(10, f64(-5)) * libc.pow(leafT, 2)
	T3 := 2.1441 * libc.pow(10, f64(-7)) * libc.pow(leafT, 3)
	return 210 * (4.7*libc.pow(10, f64(-2)) - T1 + T2 - T3) / (2.6934 * libc.pow(10, f64(-2)))
}

@(private)
fvcb_Gamma_bernacchi_f :: proc(leafT, Vcmax, Vomax: f64) -> f64 {
	numerator := 0.5 * Vomax * fvcb_Kc_bernacchi_f(leafT) * fvcb_Oi_f(leafT)
	denominator := Vcmax * fvcb_Ko_bernacchi_f(leafT)
	return tl.flt_equal_zero(denominator) ? 0.0 : numerator / denominator
}

// canopy photosynthetic capacity
@(private)
fvcb_canopy_ps_capacity_f :: proc(LAI, Vcmax, kn: f64) -> f64 {
	return LAI * Vcmax * (1 - libc.exp(-kn)) / kn
}

@(private)
fvcb_canopy_ps_capacity_sunlit_f :: proc(LAI, solar_elev, Vcmax, kn: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
	} else {
		kb = 0.5 / libc.sin(solar_elev)
	}

	return LAI * Vcmax * (1 - libc.exp(-kn-kb*LAI)) / (kn + kb*LAI)
}

@(private)
fvcb_canopy_ps_capacity_shaded_f :: proc(LAI, solar_elev, Vcmax, kn: f64) -> f64 {
	return(
		fvcb_canopy_ps_capacity_f(LAI, Vcmax, kn) -
		fvcb_canopy_ps_capacity_sunlit_f(LAI, solar_elev, Vcmax, kn) \
	)
}

// unused by FvCB_canopy_hourly_C3 (gm_t is hardcoded to 0.4 there, the real
// call is commented out as "TODO: check correctness") but a real function -
// ported for completeness.
@(private)
fvcb_gm_bernacchi_f :: proc(leafT, gm_25: f64) -> f64 {
	c := 20.0
	deltaHa := 49.6
	deltaHd := 437.4
	deltaS := 1.4
	R := 0.008314 // kJ J-1 mol-1

	Tk := leafT + 273.15

	numerator := libc.exp(c - deltaHa/(R*Tk))
	denominator := 1 + libc.exp((deltaS*Tk-deltaHd)/(R*Tk))

	return gm_25 * numerator / denominator
}

// Coupled photosynthesis-stomatal conductance: Yin, Struik, 2009. NJAS 57
// (2009) 27-38.
@(private)
fvcb_fVPD_f :: proc(VPD: f64) -> f64 {
	// VPD in KPa
	a1 := 0.9
	b1 := 0.15 // kPa-1
	return 1 / (1/(a1-b1*VPD) - 1)
}

// Lumped coefficients cubic equation C3
@(private)
fvcb_x_rubisco :: proc(leafT, Vcmax: f64) -> (x1: f64, x2: f64) {
	x1 = Vcmax
	x2 = fvcb_Kc_bernacchi_f(leafT) * (1 + fvcb_Oi_f(leafT)/fvcb_Ko_bernacchi_f(leafT))
	return
}

@(private)
fvcb_x_electron :: proc(J, gamma: f64) -> (x1: f64, x2: f64) {
	x1 = J / 4.0
	x2 = 2 * gamma
	return
}

@(private)
Fvcb_Lumped_Coeffs :: struct {
	p:   f64,
	Q:   f64,
	psi: f64,
}

@(private)
fvcb_calculate_lumped_coeffs :: proc(
	x1, x2, fVPD, Ca, gamma, Rd, g0, gm_C3, gb: f64,
) -> Fvcb_Lumped_Coeffs {
	lumped_coeffs: Fvcb_Lumped_Coeffs
	// m
	first := 1 / gm_C3
	second := g0/gm_C3 + fVPD
	third := 1/gm_C3 + 1/gb
	m := first + second*third

	// d
	d := x2 + gamma + (x1-Rd)/gm_C3

	// c
	c := Ca + x2 + (1/gm_C3+1/gb)*(x1-Rd)

	// b
	b := Ca*(x1-Rd) - gamma*x1 - Rd*x2

	// a
	first = g0 * (x2 + gamma)
	second = g0/gm_C3 + fVPD
	third = x1 - Rd
	a := first + second*third

	// r
	r := -a * b / m

	// q
	q := (d*(x1-Rd) + a*c + (g0/gm_C3+fVPD)*b) / m

	// p
	lumped_coeffs.p = -(d + (x1-Rd)/gm_C3 + a*(1/gm_C3+1/gb) + (g0/gm_C3+fVPD)*c) / m

	// U
	U := (2*libc.pow(lumped_coeffs.p, 3) - 9*lumped_coeffs.p*q + 27*r) / 54

	// Q
	lumped_coeffs.Q = (libc.pow(lumped_coeffs.p, 2) - 3*q) / 9

	// psi
	lumped_coeffs.psi = libc.acos(U / libc.sqrt(libc.pow(lumped_coeffs.Q, 3)))

	return lumped_coeffs
}

// Cubic equation solutions
@(private)
fvcb_A1_f :: proc(lc: Fvcb_Lumped_Coeffs) -> f64 {
	return -2*libc.sqrt(lc.Q)*libc.cos(lc.psi/3) - lc.p/3
}

// unused by FvCB_canopy_hourly_C3 (only A1_f is called) but real functions -
// ported for completeness.
@(private)
fvcb_A2_f :: proc(lc: Fvcb_Lumped_Coeffs) -> f64 {
	PI :: 3.14159265358979323846
	return -2*libc.sqrt(lc.Q)*libc.cos((lc.psi+2*PI)/3) - lc.p/3
}

@(private)
fvcb_A3_f :: proc(lc: Fvcb_Lumped_Coeffs) -> f64 {
	PI :: 3.14159265358979323846
	return -2*libc.sqrt(lc.Q)*libc.cos((lc.psi+4*PI)/3) - lc.p/3
}

@(private)
fvcb_derive_ci_cc_gs_f :: proc(
	A, x1, x2, gamma, Rd, gm, fVPD, g0: f64,
) -> (
	Ci: f64,
	Cc: f64,
	gs: f64,
) {
	numerator := -(A*x2 + Rd*x2 + gamma*x1)
	denominator := A + Rd - x1
	Cc = numerator / denominator
	Ci = Cc + (A / gm)
	Ci_star := gamma - Rd/gm
	gs = g0 + (A+Rd)/(Ci-Ci_star)*fVPD
	return
}

@(private)
fvcb_derive_jv_f :: proc(A, Rd, gamma, Cc: f64) -> f64 {
	numerator := (A + Rd) * (Cc + 10.5/4.5*gamma) * 4.5
	denominator := Cc - gamma
	return numerator / denominator
}

// C++: FvCB_canopy_hourly_out FvCB::FvCB_canopy_hourly_C3(
//        FvCB_canopy_hourly_in, FvCB_canopy_hourly_params)
//
// Model composition (C3).
fvcb_canopy_hourly_c3 :: proc(
	in_: Fvcb_Canopy_Hourly_In,
	par: Fvcb_Canopy_Hourly_Params,
) -> Fvcb_Canopy_Hourly_Out {
	out: Fvcb_Canopy_Hourly_Out
	// 0. initialize VOCE out
	out.sunlit.vcMax = 0.0
	out.sunlit.jMax = 0.0
	out.sunlit.jj = 0.0

	out.shaded.vcMax = 0.0
	out.shaded.jMax = 0.0
	out.shaded.jj = 0.0

	out.sunlit.jj1000 = 0.0
	out.shaded.jj1000 = 0.0

	out.sunlit.jv = 0.0
	out.shaded.jv = 0.0

	// 1. calculate diffuse and direct radiation
	diffuse_fraction := fvcb_diffuse_fraction_hourly_f(in_.global_rad, in_.extra_terr_rad, in_.solar_el)
	hourly_diffuse_rad := in_.global_rad * diffuse_fraction
	hourly_direct_rad := in_.global_rad - hourly_diffuse_rad
	inst_diff_rad := hourly_diffuse_rad * libc.pow(10, f64(6)) / 3600.0 * 4.56 * 0.45 // umol m-2 s-1 (unit ground area)
	inst_dir_rad := hourly_direct_rad * libc.pow(10, f64(6)) / 3600.0 * 4.56 * 0.45 // 1 W m-2 = 4.56 umol m-2 s-1; PAR = 0.45 * global radiation

	// 2. calculate Radiation absorbed by sunlit / shaded canopy
	Ic_sun := fvcb_Ic_sun_f(inst_dir_rad, inst_diff_rad, in_.solar_el, in_.LAI) // umol m-2 s-1 (unit ground area)
	Ic_sh := fvcb_Ic_shade_f(inst_dir_rad, inst_diff_rad, in_.solar_el, in_.LAI) // umol m-2 s-1 (unit ground area)

	// 2.1. calculate sunlit/shaded LAI
	out.sunlit.LAI, out.shaded.LAI = fvcb_LAI_sunlit_shaded_f(in_.LAI, in_.solar_el)

	// For each fraction:
	// 3. canopy photosynthetic capacity
	Vcmax := fvcb_Vcmax_bernacchi_f(in_.leaf_temp, par.Vcmax_25)
	Vcmax_25 := fvcb_Vcmax_bernacchi_f(25.0, par.Vcmax_25) // the value at 25degC calculated with bernacchi slightly deviates from par.Vcmax_25

	Vc_25 := fvcb_canopy_ps_capacity_f(in_.LAI, Vcmax_25, par.kn) // umol m-2 s-1 (unit ground area)
	Vc_sun_25 := fvcb_canopy_ps_capacity_sunlit_f(in_.LAI, in_.solar_el, Vcmax_25, par.kn)
	Vc_sh_25 := Vc_25 - Vc_sun_25
	Vc := fvcb_canopy_ps_capacity_f(in_.LAI, Vcmax, par.kn)
	Vc_sun := fvcb_canopy_ps_capacity_sunlit_f(in_.LAI, in_.solar_el, Vcmax, par.kn)
	Vc_sh := Vc - Vc_sun

	// 4. canopy electron transport capacity
	Jmax_c_sun_25 := 1.6 * Vc_sun_25 // umol m-2 s-1 (unit ground area)
	Jmax_c_sh_25 := 1.6 * Vc_sh_25

	Jmax_c_sun := fvcb_Jmax_bernacchi_f(in_.leaf_temp, Jmax_c_sun_25)
	Jmax_c_sh := fvcb_Jmax_bernacchi_f(in_.leaf_temp, Jmax_c_sh_25)
	out.jmax_c = Jmax_c_sun + Jmax_c_sh

	J_c_sun := fvcb_J_bernacchi_f(Ic_sun, in_.leaf_temp, Jmax_c_sun) // umol m-2 s-1 (unit ground area)
	J_c_sh := fvcb_J_bernacchi_f(Ic_sh, in_.leaf_temp, Jmax_c_sh)

	// 5. canopy respiration
	Rd_sun := fvcb_Rd_bernacchi_f(in_.leaf_temp) * out.sunlit.LAI // umol m-2 s-1 (unit ground area)
	Rd_sh := fvcb_Rd_bernacchi_f(in_.leaf_temp) * out.shaded.LAI

	out.canopy_resp = (Rd_sun + Rd_sh) * 3600.0

	// 6. Coupled photosynthesis - stomatal conductance
	// 6.1. estimate inputs (for solving cubic equation)
	// 6.1.1 Gamma
	Vomax_sun := fvcb_Vomax_bernacchi_f(in_.leaf_temp, Vc_sun_25)
	Vomax_sh := fvcb_Vomax_bernacchi_f(in_.leaf_temp, Vc_sh_25)
	gamma_sun := fvcb_Gamma_bernacchi_f(in_.leaf_temp, Vc_sun, Vomax_sun)
	gamma_sh := fvcb_Gamma_bernacchi_f(in_.leaf_temp, Vc_sh, Vomax_sh)

	// calculate some outputs to be used in VOCE modules
	out.sunlit.kc = fvcb_Kc_bernacchi_f(in_.leaf_temp)
	out.shaded.kc = out.sunlit.kc
	out.sunlit.ko = fvcb_Ko_bernacchi_f(in_.leaf_temp)
	out.shaded.ko = out.sunlit.ko
	out.sunlit.oi = fvcb_Oi_f(in_.leaf_temp)
	out.shaded.oi = out.sunlit.oi
	out.sunlit.comp = gamma_sun
	out.shaded.comp = gamma_sh
	hourly_globrad := in_.global_rad * libc.pow(10, f64(6)) / 3600.0 // W m-2
	out.sunlit.rad = hourly_globrad > 0 ? hourly_globrad*Ic_sun/(Ic_sun+Ic_sh) : 0.0
	out.shaded.rad = hourly_globrad > 0 ? hourly_globrad*Ic_sh/(Ic_sun+Ic_sh) : 0.0

	if out.sunlit.LAI > 0 {
		out.sunlit.vcMax = Vc_sun / out.sunlit.LAI
		out.sunlit.jMax = Jmax_c_sun / out.sunlit.LAI
		out.sunlit.jj = J_c_sun / out.sunlit.LAI
		out.sunlit.jj1000 = fvcb_J_bernacchi_f(1000, in_.leaf_temp, out.sunlit.jMax)
	}
	if out.shaded.LAI > 0 {
		out.shaded.vcMax = Vc_sh / out.shaded.LAI
		out.shaded.jMax = Jmax_c_sh / out.shaded.LAI
		out.shaded.jj = J_c_sh / out.shaded.LAI
		out.shaded.jj1000 = fvcb_J_bernacchi_f(1000, in_.leaf_temp, out.shaded.jMax)
	}

	// 6.1.2 x1, x2 rubisco
	x1_rub_sun, x2_rub_sun := fvcb_x_rubisco(in_.leaf_temp, Vc_sun)
	x1_rub_sh, x2_rub_sh := fvcb_x_rubisco(in_.leaf_temp, Vc_sh)

	// 6.1.2 x1, x2 electron
	x1_el_sun, x2_el_sun := fvcb_x_electron(J_c_sun, gamma_sun)
	x1_el_sh, x2_el_sh := fvcb_x_electron(J_c_sh, gamma_sh)

	// 6.1.3 g0, gm, gb
	gb_sun := par.gb * out.sunlit.LAI // mol m-2 s-1 bar-1 per unit ground area
	gb_sh := par.gb * out.shaded.LAI
	g0_sun := par.g0 * out.sunlit.LAI
	g0_sh := par.g0 * out.shaded.LAI
	gm_t := 0.4 // fvcb_gm_bernacchi_f(in_.leaf_temp, par.gm_25) // TODO: check correctness of fvcb_gm_bernacchi_f
	gm_sun := gm_t * out.sunlit.LAI
	gm_sh := gm_t * out.shaded.LAI

	if in_.global_rad <= 0.0 {
		// handle cases where no photosynthesis can occur
		out.canopy_gross_photos = 0.0
		out.canopy_net_photos = out.canopy_gross_photos - out.canopy_resp
		out.sunlit.gs = g0_sun
		out.shaded.gs = g0_sh
	} else {
		// 6.1.4 fVPD
		fVPD := fvcb_fVPD_f(in_.VPD)

		// 6.2 calculate lumped coeffs (sun/shade)
		lumped_rub_sun := fvcb_calculate_lumped_coeffs(x1_rub_sun, x2_rub_sun, fVPD, in_.Ca, gamma_sun, Rd_sun, g0_sun, gm_sun, gb_sun)
		lumped_el_sun := fvcb_calculate_lumped_coeffs(x1_el_sun, x2_el_sun, fVPD, in_.Ca, gamma_sun, Rd_sun, g0_sun, gm_sun, gb_sun)

		lumped_rub_sh := fvcb_calculate_lumped_coeffs(x1_rub_sh, x2_rub_sh, fVPD, in_.Ca, gamma_sh, Rd_sh, g0_sh, gm_sh, gb_sh)
		lumped_el_sh := fvcb_calculate_lumped_coeffs(x1_el_sh, x2_el_sh, fVPD, in_.Ca, gamma_sh, Rd_sh, g0_sh, gm_sh, gb_sh)

		// 6.3 calculate assimilation
		A_rub_sun := fvcb_A1_f(lumped_rub_sun) // umol CO2 m-2 s-1 (unit ground area)
		A_el_sun := fvcb_A1_f(lumped_el_sun)

		A_rub_sh := fvcb_A1_f(lumped_rub_sh) // umol CO2 m-2 s-1 (unit ground area)
		A_el_sh := fvcb_A1_f(lumped_el_sh)

		A_sun := min(A_rub_sun, A_el_sun)
		A_sh := min(A_rub_sh, A_el_sh)

		out.canopy_net_photos = (A_sun + A_sh) * 3600.0
		out.canopy_gross_photos = out.canopy_net_photos + out.canopy_resp

		// 6.4 derive stomatal conductance
		// 6.4.1 determine whether photosynthesis is rubisco or electron limited
		x1_sun, x2_sun: f64
		if A_sun == A_el_sun {
			x1_sun, x2_sun = x1_el_sun, x2_el_sun
		} else {
			x1_sun, x2_sun = x1_rub_sun, x2_rub_sun
		}
		x1_sh, x2_sh: f64
		if A_sh == A_el_sh {
			x1_sh, x2_sh = x1_el_sh, x2_el_sh
		} else {
			x1_sh, x2_sh = x1_rub_sh, x2_rub_sh
		}

		// 6.4.2 gs
		sun_ci, sun_cc, sun_gs := fvcb_derive_ci_cc_gs_f(A_sun, x1_sun, x2_sun, gamma_sun, Rd_sun, gm_sun, fVPD, par.g0)
		out.sunlit.ci = sun_ci
		out.sunlit.cc = sun_cc
		out.sunlit.gs = sun_gs
		sh_ci, sh_cc, sh_gs := fvcb_derive_ci_cc_gs_f(A_sh, x1_sh, x2_sh, gamma_sh, Rd_sh, gm_sh, fVPD, par.g0)
		out.shaded.ci = sh_ci
		out.shaded.cc = sh_ci // NOTE(c++-quirk): should be sh_cc - see the package comment
		out.shaded.gs = sh_gs

		// 6.5 derive jv
		if out.sunlit.LAI > 0 {
			out.sunlit.jv = fvcb_derive_jv_f(A_sun, Rd_sun, gamma_sun, sun_cc) / out.sunlit.LAI
		}
		if out.shaded.LAI > 0 {
			out.shaded.jv = fvcb_derive_jv_f(A_sh, Rd_sh, gamma_sh, sh_cc) / out.shaded.LAI
		}
	}

	return out
}
