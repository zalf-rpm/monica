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
import "../../support/cr_math"

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
	vcmax_25: f64,
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
	lai:            f64, // m2 m-2
	leaf_temp:      f64, // degC
	vpd:            f64, // KPa
	ca:             f64, // ambient CO2 partial pressure, ubar or umol mol-1
}

// C++: struct FvCB::FvCB_leaf_fraction
Fvcb_Leaf_Fraction :: struct {
	lai:    f64, // m2 m-2
	gs:     f64, // mol m-2 s-1 bar-1 (unit ground area)
	kc:     f64, // umol mol-1 mbar-1
	ko:     f64, // umol mol-1 mbar-1
	oi:     f64, // umol m-2
	ci:     f64, // umol m-2
	cc:     f64, // umol m-2
	comp:   f64, // umol mol-1
	vc_max: f64, // umol m-2 s-1
	j_max:  f64, // umol m-2 s-1
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
	r := 0.847 - 1.61*cr_math.sin(solar_elev) + 1.04*cr_math.pow(cr_math.sin(solar_elev), 2)
	k := (1.47 - r) / 1.66

	if glob_extra_ratio <= 0.22 {
		return 1
	} else if glob_extra_ratio <= 0.35 {
		return 1 - 6.4*cr_math.pow(glob_extra_ratio-0.22, 2)
	} else if glob_extra_ratio <= k {
		return 1.47 - 1.66*glob_extra_ratio
	} else {
		return r
	}
}

// direct beam absorbed by sunlit leaves
@(private)
fvcb_abs_sunlit_direct_f :: proc(i_dir_beam, solar_elev, lai: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
	} else {
		kb = 0.5 / cr_math.sin(solar_elev)
	}

	sigma := 0.15 // scattering coefficient
	return i_dir_beam * (1 - sigma) * (1 - cr_math.exp(-kb*lai))
}

// diffuse radiation absorbed by sunlit leaves
@(private)
fvcb_abs_sunlit_diffuse_f :: proc(i_dif, solar_elev, lai: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy
	rho_cd := 0.036 // reflection coeff diffuse PAR
	k1_d := 0.719 // diffuse par extinction coeff

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
	} else {
		kb = 0.5 / cr_math.sin(solar_elev)
	}
	return i_dif * (1 - rho_cd) * (1 - cr_math.exp(-(k1_d+kb)*lai)) * k1_d / (k1_d + kb)
}

// scattered beam absorbed by sunlit leaves
@(private)
fvcb_abs_sunlit_scattered_f :: proc(i_dir_beam, solar_elev, lai: f64) -> f64 {
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
		kb = 0.5 / cr_math.sin(solar_elev)
		k1_b = 0.46 / cr_math.sin(solar_elev)
	}
	rho_h := 0.041 // reflection coeff beam irradiance (horizontal leaves)
	rho_cb = 1 - cr_math.exp(2*rho_h*kb/(1+kb))
	sigma = 0.15

	line_1 := (1 - rho_cb) * (1 - cr_math.exp(-(k1_b+kb)*lai)) * k1_b / (k1_b + kb)
	line_2 := (1 - sigma) * (1 - cr_math.exp(-2*kb*lai)) / 2

	return i_dir_beam * (line_1 - line_2)
}

// irradiance absorbed by the canopy
@(private)
fvcb_ic_f :: proc(i_dir_beam, i_dif, solar_elev, lai: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy
	k1_b: f64 // beam and scattered beam PAR extinction coefficient

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
		k1_b = 1000
	} else {
		kb = 0.5 / cr_math.sin(solar_elev)
		k1_b = 0.46 / cr_math.sin(solar_elev)
	}

	rho_h := 0.041
	rho_cb := 1 - cr_math.exp(2*rho_h*kb/(1+kb))
	rho_cd := 0.036 // reflection coeff diffuse PAR
	k1_d := 0.719 // diffuse par extinction coeff

	ic_dir := (1 - rho_cb) * i_dir_beam * (1 - cr_math.exp(-k1_b*lai))
	ic_dif := (1 - rho_cd) * i_dif * (1 - cr_math.exp(-k1_d*lai))

	return min(ic_dir+ic_dif, i_dir_beam+i_dif)
}

// irradiance absorbed by sunlit LAI
@(private)
fvcb_ic_sun_f :: proc(i_dir_beam, i_dif, solar_elev, lai: f64) -> f64 {
	return(
		fvcb_abs_sunlit_direct_f(i_dir_beam, solar_elev, lai) +
		fvcb_abs_sunlit_diffuse_f(i_dif, solar_elev, lai) +
		fvcb_abs_sunlit_scattered_f(i_dir_beam, solar_elev, lai) \
	)
}

// irradiance absorbed by shaded LAI
@(private)
fvcb_ic_shade_f :: proc(i_dir_beam, i_dif, solar_elev, lai: f64) -> f64 {
	return(
		fvcb_ic_f(i_dir_beam, i_dif, solar_elev, lai) -
		fvcb_ic_sun_f(i_dir_beam, i_dif, solar_elev, lai) \
	)
}

@(private)
fvcb_lai_sunlit_shaded_f :: proc(lai, solar_elev: f64) -> (lai_sunlit: f64, lai_shaded: f64) {
	kb: f64 // beam radiation extinction coefficient of canopy

	if solar_elev < 0 {
		return 0, lai
	} else if solar_elev == 0 {
		kb = 1000
	} else {
		kb = 0.5 / cr_math.sin(solar_elev)
	}

	lai_sunlit = (1 - cr_math.exp(-kb*lai)) / kb
	return lai_sunlit, lai - lai_sunlit
}

// T response
@(private)
fvcb_tresp_bernacchi_f :: proc(c, delta_h, leaf_t: f64) -> f64 {
	tk := leaf_t + 273
	r := 8.314472 * cr_math.pow(10, f64(-3)) // kJ K-1 mol-1
	return cr_math.exp(c - delta_h/(r*tk))
}

// C++: double FvCB::Vcmax_bernacchi_f(double, double)
fvcb_vcmax_bernacchi_f :: proc(leaf_t, vcmax_25: f64) -> f64 {
	return vcmax_25 * fvcb_tresp_bernacchi_f(fvcb_c_bernacchi(.VCMAX), fvcb_delta_h_bernacchi(.VCMAX), leaf_t)
}

// C++: double FvCB::Jmax_bernacchi_f(double, double)
fvcb_jmax_bernacchi_f :: proc(leaf_t, jmax_25: f64) -> f64 {
	return jmax_25 * fvcb_tresp_bernacchi_f(fvcb_c_bernacchi(.JMAX), fvcb_delta_h_bernacchi(.JMAX), leaf_t)
}

@(private)
fvcb_j_bernacchi_f :: proc(q, leaf_t, jmax: f64) -> f64 {
	alfa := 0.85 // total leaf absorbance
	beta := 0.5 // fraction of absorbed quanta reaching PSII
	theta_ps2 := 0.76 + 0.018*leaf_t - 3.7*cr_math.pow(10, f64(-4))*cr_math.pow(leaf_t, 2)
	phi_ps2max := 0.352 + 0.022*leaf_t - 3.4*cr_math.pow(10, f64(-4))*cr_math.pow(leaf_t, 2)
	q2 := q * alfa * phi_ps2max * beta

	numerator := q2 + jmax - libc.sqrt(cr_math.pow(q2+jmax, 2)-4*theta_ps2*q2*jmax)
	denominator := 2 * theta_ps2
	return numerator / denominator
}

// unused by FvCB_canopy_hourly_C3 (the C++ leaves the alternative J_c_sun/
// J_c_sh call commented out) but a real, exported-from-the-.cpp function -
// ported for completeness.
@(private)
fvcb_j_grote_f :: proc(q, jmax: f64) -> f64 {
	species_theta := 0.85 // curvature parameter
	tmp_var := ((q + jmax) * (q + jmax)) - (4.0 * species_theta * q * jmax)
	// NOTE(c++-quirk): the C++ comment claims tmp_var should be the inverse
	// sqrt, but the code only takes the plain sqrt - reproduced as-is.
	jj := tmp_var > 0.0 ? (q + jmax - libc.sqrt(tmp_var)) / (2.0 * species_theta) : 0.0
	return jj
}

@(private)
fvcb_rd_bernacchi_f :: proc(leaf_t: f64) -> f64 {
	return fvcb_tresp_bernacchi_f(fvcb_c_bernacchi(.RD), fvcb_delta_h_bernacchi(.RD), leaf_t)
}

@(private)
fvcb_vomax_bernacchi_f :: proc(leaf_t, vcmax_25: f64) -> f64 {
	return vcmax_25 * fvcb_tresp_bernacchi_f(fvcb_c_bernacchi(.VOMAX), fvcb_delta_h_bernacchi(.VOMAX), leaf_t)
}

@(private)
fvcb_kc_bernacchi_f :: proc(leaf_t: f64) -> f64 {
	return fvcb_tresp_bernacchi_f(fvcb_c_bernacchi(.KC), fvcb_delta_h_bernacchi(.KC), leaf_t)
}

@(private)
fvcb_ko_bernacchi_f :: proc(leaf_t: f64) -> f64 {
	return fvcb_tresp_bernacchi_f(fvcb_c_bernacchi(.KO), fvcb_delta_h_bernacchi(.KO), leaf_t)
}

@(private)
fvcb_oi_f :: proc(leaf_t: f64) -> f64 {
	t1 := 1.3087 * cr_math.pow(10, f64(-3)) * leaf_t
	t2 := 2.5603 * cr_math.pow(10, f64(-5)) * cr_math.pow(leaf_t, 2)
	t3 := 2.1441 * cr_math.pow(10, f64(-7)) * cr_math.pow(leaf_t, 3)
	return 210 * (4.7*cr_math.pow(10, f64(-2)) - t1 + t2 - t3) / (2.6934 * cr_math.pow(10, f64(-2)))
}

@(private)
fvcb_gamma_bernacchi_f :: proc(leaf_t, vcmax, vomax: f64) -> f64 {
	numerator := 0.5 * vomax * fvcb_kc_bernacchi_f(leaf_t) * fvcb_oi_f(leaf_t)
	denominator := vcmax * fvcb_ko_bernacchi_f(leaf_t)
	return tl.flt_equal_zero(denominator) ? 0.0 : numerator / denominator
}

// canopy photosynthetic capacity
@(private)
fvcb_canopy_ps_capacity_f :: proc(lai, vcmax, kn: f64) -> f64 {
	return lai * vcmax * (1 - cr_math.exp(-kn)) / kn
}

@(private)
fvcb_canopy_ps_capacity_sunlit_f :: proc(lai, solar_elev, vcmax, kn: f64) -> f64 {
	kb: f64 // beam radiation extinction coefficient of canopy

	if solar_elev < 0 {
		return 0
	} else if solar_elev == 0 {
		kb = 1000
	} else {
		kb = 0.5 / cr_math.sin(solar_elev)
	}

	return lai * vcmax * (1 - cr_math.exp(-kn-kb*lai)) / (kn + kb*lai)
}

@(private)
fvcb_canopy_ps_capacity_shaded_f :: proc(lai, solar_elev, vcmax, kn: f64) -> f64 {
	return(
		fvcb_canopy_ps_capacity_f(lai, vcmax, kn) -
		fvcb_canopy_ps_capacity_sunlit_f(lai, solar_elev, vcmax, kn) \
	)
}

// unused by FvCB_canopy_hourly_C3 (gm_t is hardcoded to 0.4 there, the real
// call is commented out as "TODO: check correctness") but a real function -
// ported for completeness.
@(private)
fvcb_gm_bernacchi_f :: proc(leaf_t, gm_25: f64) -> f64 {
	c := 20.0
	delta_ha := 49.6
	delta_hd := 437.4
	delta_s := 1.4
	r := 0.008314 // kJ J-1 mol-1

	tk := leaf_t + 273.15

	numerator := cr_math.exp(c - delta_ha/(r*tk))
	denominator := 1 + cr_math.exp((delta_s*tk-delta_hd)/(r*tk))

	return gm_25 * numerator / denominator
}

// Coupled photosynthesis-stomatal conductance: Yin, Struik, 2009. NJAS 57
// (2009) 27-38.
@(private)
fvcb_f_vpd_f :: proc(vpd: f64) -> f64 {
	// VPD in KPa
	a1 := 0.9
	b1 := 0.15 // kPa-1
	return 1 / (1/(a1-b1*vpd) - 1)
}

// Lumped coefficients cubic equation C3
@(private)
fvcb_x_rubisco :: proc(leaf_t, vcmax: f64) -> (x1: f64, x2: f64) {
	x1 = vcmax
	x2 = fvcb_kc_bernacchi_f(leaf_t) * (1 + fvcb_oi_f(leaf_t)/fvcb_ko_bernacchi_f(leaf_t))
	return
}

@(private)
fvcb_x_electron :: proc(j, gamma: f64) -> (x1: f64, x2: f64) {
	x1 = j / 4.0
	x2 = 2 * gamma
	return
}

@(private)
Fvcb_Lumped_Coeffs :: struct {
	p:   f64,
	q:   f64,
	psi: f64,
}

@(private)
fvcb_calculate_lumped_coeffs :: proc(
	x1, x2, f_vpd, ca, gamma, rd, g0, gm_c3, gb: f64,
) -> Fvcb_Lumped_Coeffs {
	lumped_coeffs: Fvcb_Lumped_Coeffs
	// m
	first := 1 / gm_c3
	second := g0/gm_c3 + f_vpd
	third := 1/gm_c3 + 1/gb
	m := first + second*third

	// d
	d := x2 + gamma + (x1-rd)/gm_c3

	// c
	c := ca + x2 + (1/gm_c3+1/gb)*(x1-rd)

	// b
	b := ca*(x1-rd) - gamma*x1 - rd*x2

	// a
	first = g0 * (x2 + gamma)
	second = g0/gm_c3 + f_vpd
	third = x1 - rd
	a := first + second*third

	// r
	r := -a * b / m

	// q
	q := (d*(x1-rd) + a*c + (g0/gm_c3+f_vpd)*b) / m

	// p
	lumped_coeffs.p = -(d + (x1-rd)/gm_c3 + a*(1/gm_c3+1/gb) + (g0/gm_c3+f_vpd)*c) / m

	// U
	u := (2*cr_math.pow(lumped_coeffs.p, 3) - 9*lumped_coeffs.p*q + 27*r) / 54

	// Q
	lumped_coeffs.q = (cr_math.pow(lumped_coeffs.p, 2) - 3*q) / 9

	// psi
	lumped_coeffs.psi = cr_math.acos(u / libc.sqrt(cr_math.pow(lumped_coeffs.q, 3)))

	return lumped_coeffs
}

// Cubic equation solutions
@(private)
fvcb_a1_f :: proc(lc: Fvcb_Lumped_Coeffs) -> f64 {
	return -2*libc.sqrt(lc.q)*cr_math.cos(lc.psi/3) - lc.p/3
}

// unused by FvCB_canopy_hourly_C3 (only A1_f is called) but real functions -
// ported for completeness.
@(private)
fvcb_a2_f :: proc(lc: Fvcb_Lumped_Coeffs) -> f64 {
	PI :: 3.14159265358979323846
	return -2*libc.sqrt(lc.q)*cr_math.cos((lc.psi+2*PI)/3) - lc.p/3
}

@(private)
fvcb_a3_f :: proc(lc: Fvcb_Lumped_Coeffs) -> f64 {
	PI :: 3.14159265358979323846
	return -2*libc.sqrt(lc.q)*cr_math.cos((lc.psi+4*PI)/3) - lc.p/3
}

@(private)
fvcb_derive_ci_cc_gs_f :: proc(
	a, x1, x2, gamma, rd, gm, f_vpd, g0: f64,
) -> (
	ci: f64,
	cc: f64,
	gs: f64,
) {
	numerator := -(a*x2 + rd*x2 + gamma*x1)
	denominator := a + rd - x1
	cc = numerator / denominator
	ci = cc + (a / gm)
	ci_star := gamma - rd/gm
	gs = g0 + (a+rd)/(ci-ci_star)*f_vpd
	return
}

@(private)
fvcb_derive_jv_f :: proc(a, rd, gamma, cc: f64) -> f64 {
	numerator := (a + rd) * (cc + 10.5/4.5*gamma) * 4.5
	denominator := cc - gamma
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
	out.sunlit.vc_max = 0.0
	out.sunlit.j_max = 0.0
	out.sunlit.jj = 0.0

	out.shaded.vc_max = 0.0
	out.shaded.j_max = 0.0
	out.shaded.jj = 0.0

	out.sunlit.jj1000 = 0.0
	out.shaded.jj1000 = 0.0

	out.sunlit.jv = 0.0
	out.shaded.jv = 0.0

	// 1. calculate diffuse and direct radiation
	diffuse_fraction := fvcb_diffuse_fraction_hourly_f(in_.global_rad, in_.extra_terr_rad, in_.solar_el)
	hourly_diffuse_rad := in_.global_rad * diffuse_fraction
	hourly_direct_rad := in_.global_rad - hourly_diffuse_rad
	inst_diff_rad := hourly_diffuse_rad * cr_math.pow(10, f64(6)) / 3600.0 * 4.56 * 0.45 // umol m-2 s-1 (unit ground area)
	inst_dir_rad := hourly_direct_rad * cr_math.pow(10, f64(6)) / 3600.0 * 4.56 * 0.45 // 1 W m-2 = 4.56 umol m-2 s-1; PAR = 0.45 * global radiation

	// 2. calculate Radiation absorbed by sunlit / shaded canopy
	ic_sun := fvcb_ic_sun_f(inst_dir_rad, inst_diff_rad, in_.solar_el, in_.lai) // umol m-2 s-1 (unit ground area)
	ic_sh := fvcb_ic_shade_f(inst_dir_rad, inst_diff_rad, in_.solar_el, in_.lai) // umol m-2 s-1 (unit ground area)

	// 2.1. calculate sunlit/shaded LAI
	out.sunlit.lai, out.shaded.lai = fvcb_lai_sunlit_shaded_f(in_.lai, in_.solar_el)

	// For each fraction:
	// 3. canopy photosynthetic capacity
	vcmax := fvcb_vcmax_bernacchi_f(in_.leaf_temp, par.vcmax_25)
	vcmax_25 := fvcb_vcmax_bernacchi_f(25.0, par.vcmax_25) // the value at 25degC calculated with bernacchi slightly deviates from par.Vcmax_25

	vc_25 := fvcb_canopy_ps_capacity_f(in_.lai, vcmax_25, par.kn) // umol m-2 s-1 (unit ground area)
	vc_sun_25 := fvcb_canopy_ps_capacity_sunlit_f(in_.lai, in_.solar_el, vcmax_25, par.kn)
	vc_sh_25 := vc_25 - vc_sun_25
	vc := fvcb_canopy_ps_capacity_f(in_.lai, vcmax, par.kn)
	vc_sun := fvcb_canopy_ps_capacity_sunlit_f(in_.lai, in_.solar_el, vcmax, par.kn)
	vc_sh := vc - vc_sun

	// 4. canopy electron transport capacity
	jmax_c_sun_25 := 1.6 * vc_sun_25 // umol m-2 s-1 (unit ground area)
	jmax_c_sh_25 := 1.6 * vc_sh_25

	jmax_c_sun := fvcb_jmax_bernacchi_f(in_.leaf_temp, jmax_c_sun_25)
	jmax_c_sh := fvcb_jmax_bernacchi_f(in_.leaf_temp, jmax_c_sh_25)
	out.jmax_c = jmax_c_sun + jmax_c_sh

	j_c_sun := fvcb_j_bernacchi_f(ic_sun, in_.leaf_temp, jmax_c_sun) // umol m-2 s-1 (unit ground area)
	j_c_sh := fvcb_j_bernacchi_f(ic_sh, in_.leaf_temp, jmax_c_sh)

	// 5. canopy respiration
	rd_sun := fvcb_rd_bernacchi_f(in_.leaf_temp) * out.sunlit.lai // umol m-2 s-1 (unit ground area)
	rd_sh := fvcb_rd_bernacchi_f(in_.leaf_temp) * out.shaded.lai

	out.canopy_resp = (rd_sun + rd_sh) * 3600.0

	// 6. Coupled photosynthesis - stomatal conductance
	// 6.1. estimate inputs (for solving cubic equation)
	// 6.1.1 Gamma
	vomax_sun := fvcb_vomax_bernacchi_f(in_.leaf_temp, vc_sun_25)
	vomax_sh := fvcb_vomax_bernacchi_f(in_.leaf_temp, vc_sh_25)
	gamma_sun := fvcb_gamma_bernacchi_f(in_.leaf_temp, vc_sun, vomax_sun)
	gamma_sh := fvcb_gamma_bernacchi_f(in_.leaf_temp, vc_sh, vomax_sh)

	// calculate some outputs to be used in VOCE modules
	out.sunlit.kc = fvcb_kc_bernacchi_f(in_.leaf_temp)
	out.shaded.kc = out.sunlit.kc
	out.sunlit.ko = fvcb_ko_bernacchi_f(in_.leaf_temp)
	out.shaded.ko = out.sunlit.ko
	out.sunlit.oi = fvcb_oi_f(in_.leaf_temp)
	out.shaded.oi = out.sunlit.oi
	out.sunlit.comp = gamma_sun
	out.shaded.comp = gamma_sh
	hourly_globrad := in_.global_rad * cr_math.pow(10, f64(6)) / 3600.0 // W m-2
	out.sunlit.rad = hourly_globrad > 0 ? hourly_globrad*ic_sun/(ic_sun+ic_sh) : 0.0
	out.shaded.rad = hourly_globrad > 0 ? hourly_globrad*ic_sh/(ic_sun+ic_sh) : 0.0

	if out.sunlit.lai > 0 {
		out.sunlit.vc_max = vc_sun / out.sunlit.lai
		out.sunlit.j_max = jmax_c_sun / out.sunlit.lai
		out.sunlit.jj = j_c_sun / out.sunlit.lai
		out.sunlit.jj1000 = fvcb_j_bernacchi_f(1000, in_.leaf_temp, out.sunlit.j_max)
	}
	if out.shaded.lai > 0 {
		out.shaded.vc_max = vc_sh / out.shaded.lai
		out.shaded.j_max = jmax_c_sh / out.shaded.lai
		out.shaded.jj = j_c_sh / out.shaded.lai
		out.shaded.jj1000 = fvcb_j_bernacchi_f(1000, in_.leaf_temp, out.shaded.j_max)
	}

	// 6.1.2 x1, x2 rubisco
	x1_rub_sun, x2_rub_sun := fvcb_x_rubisco(in_.leaf_temp, vc_sun)
	x1_rub_sh, x2_rub_sh := fvcb_x_rubisco(in_.leaf_temp, vc_sh)

	// 6.1.2 x1, x2 electron
	x1_el_sun, x2_el_sun := fvcb_x_electron(j_c_sun, gamma_sun)
	x1_el_sh, x2_el_sh := fvcb_x_electron(j_c_sh, gamma_sh)

	// 6.1.3 g0, gm, gb
	gb_sun := par.gb * out.sunlit.lai // mol m-2 s-1 bar-1 per unit ground area
	gb_sh := par.gb * out.shaded.lai
	g0_sun := par.g0 * out.sunlit.lai
	g0_sh := par.g0 * out.shaded.lai
	gm_t := 0.4 // fvcb_gm_bernacchi_f(in_.leaf_temp, par.gm_25) // TODO: check correctness of fvcb_gm_bernacchi_f
	gm_sun := gm_t * out.sunlit.lai
	gm_sh := gm_t * out.shaded.lai

	if in_.global_rad <= 0.0 {
		// handle cases where no photosynthesis can occur
		out.canopy_gross_photos = 0.0
		out.canopy_net_photos = out.canopy_gross_photos - out.canopy_resp
		out.sunlit.gs = g0_sun
		out.shaded.gs = g0_sh
	} else {
		// 6.1.4 fVPD
		f_vpd := fvcb_f_vpd_f(in_.vpd)

		// 6.2 calculate lumped coeffs (sun/shade)
		lumped_rub_sun := fvcb_calculate_lumped_coeffs(x1_rub_sun, x2_rub_sun, f_vpd, in_.ca, gamma_sun, rd_sun, g0_sun, gm_sun, gb_sun)
		lumped_el_sun := fvcb_calculate_lumped_coeffs(x1_el_sun, x2_el_sun, f_vpd, in_.ca, gamma_sun, rd_sun, g0_sun, gm_sun, gb_sun)

		lumped_rub_sh := fvcb_calculate_lumped_coeffs(x1_rub_sh, x2_rub_sh, f_vpd, in_.ca, gamma_sh, rd_sh, g0_sh, gm_sh, gb_sh)
		lumped_el_sh := fvcb_calculate_lumped_coeffs(x1_el_sh, x2_el_sh, f_vpd, in_.ca, gamma_sh, rd_sh, g0_sh, gm_sh, gb_sh)

		// 6.3 calculate assimilation
		a_rub_sun := fvcb_a1_f(lumped_rub_sun) // umol CO2 m-2 s-1 (unit ground area)
		a_el_sun := fvcb_a1_f(lumped_el_sun)

		a_rub_sh := fvcb_a1_f(lumped_rub_sh) // umol CO2 m-2 s-1 (unit ground area)
		a_el_sh := fvcb_a1_f(lumped_el_sh)

		a_sun := min(a_rub_sun, a_el_sun)
		a_sh := min(a_rub_sh, a_el_sh)

		out.canopy_net_photos = (a_sun + a_sh) * 3600.0
		out.canopy_gross_photos = out.canopy_net_photos + out.canopy_resp

		// 6.4 derive stomatal conductance
		// 6.4.1 determine whether photosynthesis is rubisco or electron limited
		x1_sun, x2_sun: f64
		if a_sun == a_el_sun {
			x1_sun, x2_sun = x1_el_sun, x2_el_sun
		} else {
			x1_sun, x2_sun = x1_rub_sun, x2_rub_sun
		}
		x1_sh, x2_sh: f64
		if a_sh == a_el_sh {
			x1_sh, x2_sh = x1_el_sh, x2_el_sh
		} else {
			x1_sh, x2_sh = x1_rub_sh, x2_rub_sh
		}

		// 6.4.2 gs
		sun_ci, sun_cc, sun_gs := fvcb_derive_ci_cc_gs_f(a_sun, x1_sun, x2_sun, gamma_sun, rd_sun, gm_sun, f_vpd, par.g0)
		out.sunlit.ci = sun_ci
		out.sunlit.cc = sun_cc
		out.sunlit.gs = sun_gs
		sh_ci, sh_cc, sh_gs := fvcb_derive_ci_cc_gs_f(a_sh, x1_sh, x2_sh, gamma_sh, rd_sh, gm_sh, f_vpd, par.g0)
		out.shaded.ci = sh_ci
		out.shaded.cc = sh_ci // NOTE(c++-quirk): should be sh_cc - see the package comment
		out.shaded.gs = sh_gs

		// 6.5 derive jv
		if out.sunlit.lai > 0 {
			out.sunlit.jv = fvcb_derive_jv_f(a_sun, rd_sun, gamma_sun, sun_cc) / out.sunlit.lai
		}
		if out.shaded.lai > 0 {
			out.shaded.jv = fvcb_derive_jv_f(a_sh, rd_sh, gamma_sh, sh_cc) / out.shaded.lai
		}
	}

	return out
}
