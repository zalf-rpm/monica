// Port of src/core/O3-impact.{h,cpp}: hourly ozone impact on assimilation
// (short-term stomatal reduction, long-term senescence) and the FAO-56-style
// water-stress stomatal-closure factor.
//
// Fully self-contained - confirmed by reading the header: every function
// takes plain struct/scalar parameters, nothing here touches CropModule or
// MonicaModel. Part of the same satellite-module checkpoint as
// photosynthesis-FvCB/voc-guenther/voc-jjv.
package core

import "../../support/cr_math"

// C++: struct O3impact::O3_impact_params
O3_Impact_Params :: struct {
	gamma1:             f64, // ozone short-term damage coefficient, unitless
	gamma2:             f64, // ozone short-term damage coefficient, nmol m-2 s-1
	gamma3:             f64, // ozone long-term damage coefficient, (umol m-2)-1
	upper_thr_stomatal: f64,
	lower_thr_stomatal: f64,
	fshape_stomatal:    f64,
}

// C++ in-class initialisers: gamma1{0.060}, gamma2{0.0045}, gamma3{0.5},
// upper_thr_stomatal{0.4}, lower_thr_stomatal{1}, Fshape_stomatal{2.5}
make_o3_impact_params :: proc() -> O3_Impact_Params {
	return O3_Impact_Params {
		gamma1             = 0.060,
		gamma2             = 0.0045,
		gamma3             = 0.5,
		upper_thr_stomatal = 0.4,
		lower_thr_stomatal = 1,
		fshape_stomatal    = 2.5,
	}
}

// C++: struct O3impact::O3_impact_in
O3_Impact_In :: struct {
	fc:           f64, // field capacity, m3 m-3
	wp:           f64, // wilting point, m3 m-3
	swc:          f64, // soil water content, m3 m-3
	et0:          f64, // reference ET, mm d-1
	o3a:          f64, // ambient O3 partial pressure, nbar or nmol mol-1
	gs:           f64, // stomatal conductance mol m-2 s-1 bar-1 (unit ground area)
	h:            int, // hour of the day (0-23)
	reldev:       f64, // relative development
	gdd_flo:      f64, // GDD from emergence to flowering
	gdd_mat:      f64, // GDD from emergence to maturity
	f_o3s_d_prev: f64, // short term ozone induced reduction of Ac of the previous time step
	sum_o3_up:    f64, // cumulated O3 uptake, umol m-2 (unit ground area)
}

// C++: struct O3impact::O3_impact_out
O3_Impact_Out :: struct {
	hourly_o3_up: f64, // hourly O3 uptake, umol m-2 h-1 (unit ground area)
	f_o3s_d:      f64, // short term ozone induced reduction of Ac
	f_o3l:        f64, // long term ozone induced senescence
	f_ls:         f64, // leaf senescence reduction of Ac, modified by O3 cumulative uptake
	ws_st_clos:   f64, // water deficit factor for stomatal closure
}

// C++ in-class initialisers: fO3s_d{1.0}, fO3l{1.0}, fLS{1.0}, WS_st_clos{1.0}
// (hourly_O3_up{0.0} matches Odin's zero value)
make_o3_impact_out :: proc() -> O3_Impact_Out {
	return O3_Impact_Out{f_o3s_d = 1.0, f_o3l = 1.0, f_ls = 1.0, ws_st_clos = 1.0}
}

// C++: double O3_uptake(double, double, double) - from Ewert and Porter, 2000.
// Global Change Biology, 6(7), 735-750
@(private)
o3_impact_o3_uptake :: proc(o3a, gsc, f_ws: f64) -> f64 {
	// O3_up should be nmol m-2 s-1, set input accordingly
	f_do3 := 0.93 // ratio of diffusion rates for O3 and CO2
	return o3a * gsc * f_ws * f_do3
}

@(private)
o3_impact_hourly_o3_reduction_ac :: proc(o3_up, gamma1, gamma2: f64) -> f64 {
	// mind the units: O3_up should be nmol m-2 s-1
	f_o3s_h := 1.0

	if o3_up > gamma1/gamma2 && o3_up < (1.0+gamma1)/gamma2 {
		f_o3s_h = 1.0 + gamma1 - gamma2*o3_up
	} else if o3_up > (1.0+gamma1)/gamma2 {
		f_o3s_h = 0
	}

	return f_o3s_h
}

@(private)
o3_impact_cumulative_o3_reduction_ac :: proc(f_o3s_d_, f_o3s_h, r_o3s: f64, h: int) -> f64 {
	f_o3s_d := f_o3s_d_
	if h == 0 {
		f_o3s_d = f_o3s_h * r_o3s
	} else {
		f_o3s_d *= f_o3s_h
	}

	return f_o3s_d
}

@(private)
o3_impact_o3_damage_recovery :: proc(f_o3s_d, f_la: f64) -> f64 {
	r_o3s := f_o3s_d + (1.0-f_o3s_d)*f_la
	return r_o3s
}

@(private)
o3_impact_o3_recovery_factor_leaf_age :: proc(reldev: f64) -> f64 {
	// since MONICA does not have leaf age/classes/span we define fLA as a
	// function of development
	crit_reldev := 0.2 // young leaves can recover fully from O3 damage
	f_la := 1.0
	if reldev > crit_reldev {
		f_la = max(0.0, 1.0-(reldev-crit_reldev)/(1.0-crit_reldev))
	}
	return f_la
}

@(private)
o3_impact_o3_senescence_factor :: proc(gamma3, o3_tot_up: f64) -> f64 {
	// O3_tot_up umol m-2; factor accounting for both onset and rate of senescence
	f_o3l := max(0.5, 1.0-gamma3*o3_tot_up) // 0.5 is arbitrary
	return f_o3l
}

@(private)
o3_impact_leaf_senescence_reduction_ac :: proc(
	f_o3l, reldev, gdd_flowering, gdd_maturity: f64,
) -> f64 {
	// senescence is assumed to start at flowering in normal conditions
	crit_reldev := gdd_flowering / gdd_maturity
	crit_reldev *= f_o3l // correction of onset due to O3 cumulative uptake
	senescence_impact_max := 0.4 // arbitrary value
	f_ls := 1.0

	if reldev > crit_reldev {
		// correction of rate due to O3 cumulative uptake
		f_ls = max(
			1.0 - senescence_impact_max,
			1.0 - senescence_impact_max*(reldev-crit_reldev)/(f_o3l-crit_reldev),
		)
	}
	return f_ls
}

// C++: double water_stress_stomatal_closure(double, double, double, double,
//        double, double, double) - Raes et al., 2009. Agronomy Journal,
//        101(3), 438-447
@(private)
o3_impact_water_stress_stomatal_closure :: proc(
	upper_thr, lower_thr, fshape, fc, wp, swc, et0: f64,
) -> f64 {
	upper_threshold_adj := upper_thr + (0.04 * (5.0 - et0)) * cr_math.log10(10.0-9.0*upper_thr)
	if upper_threshold_adj < 0 {
		upper_threshold_adj = 0
	} else if upper_threshold_adj > 1 {
		upper_threshold_adj = 1.0
	}
	whc_adj := lower_thr - upper_threshold_adj

	sw_depletion_f: f64
	if swc >= fc {
		sw_depletion_f = 0.0
	} else if swc <= wp {
		sw_depletion_f = 1.0
	} else {
		sw_depletion_f = 1.0 - (swc-wp)/(fc-wp)
	}

	// Drel
	drel: f64
	if sw_depletion_f <= upper_threshold_adj {
		drel = 0.0
	} else if sw_depletion_f >= lower_thr {
		drel = 1
	} else {
		drel = (sw_depletion_f - upper_threshold_adj) / whc_adj
	}

	return 1 - (cr_math.exp(drel*fshape) - 1.0) / (cr_math.exp(fshape) - 1.0)
}

// C++: O3_impact_out O3impact::O3_impact_hourly(O3_impact_in,
//        O3_impact_params, bool)
//
// Model composition.
o3_impact_hourly :: proc(
	in_: O3_Impact_In,
	par: O3_Impact_Params,
	water_deficit_response_stomata: bool,
) -> O3_Impact_Out {
	out := make_o3_impact_out()

	f_la := o3_impact_o3_recovery_factor_leaf_age(in_.reldev)
	r_o3s := o3_impact_o3_damage_recovery(in_.f_o3s_d_prev, f_la) // used only the first hour
	out.ws_st_clos = 1.0
	if water_deficit_response_stomata {
		out.ws_st_clos = o3_impact_water_stress_stomatal_closure(
			par.upper_thr_stomatal,
			par.lower_thr_stomatal,
			par.fshape_stomatal,
			in_.fc,
			in_.wp,
			in_.swc,
			in_.et0,
		)
	}

	inst_o3_up := o3_impact_o3_uptake(in_.o3a, in_.gs, out.ws_st_clos) // nmol m-2 s-1
	out.hourly_o3_up += inst_o3_up / 1000 // from nmol to umol
	f_o3s_h := o3_impact_hourly_o3_reduction_ac(inst_o3_up, par.gamma1, par.gamma2)

	// short term O3 effect on Ac
	out.f_o3s_d = o3_impact_cumulative_o3_reduction_ac(in_.f_o3s_d_prev, f_o3s_h, r_o3s, in_.h)

	// senescence + long term O3 effect on Ac. Even with [O3]=0, senescence
	// will act to reduce fLS.
	out.f_o3l = o3_impact_o3_senescence_factor(par.gamma3, in_.sum_o3_up)
	out.f_ls = o3_impact_leaf_senescence_reduction_ac(out.f_o3l, in_.reldev, in_.gdd_flo, in_.gdd_mat)

	return out
}
