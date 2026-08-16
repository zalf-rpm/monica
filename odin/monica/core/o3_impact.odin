// Port of src/core/O3-impact.{h,cpp}: hourly ozone impact on assimilation
// (short-term stomatal reduction, long-term senescence) and the FAO-56-style
// water-stress stomatal-closure factor.
//
// Fully self-contained - confirmed by reading the header: every function
// takes plain struct/scalar parameters, nothing here touches CropModule or
// MonicaModel. Part of the same satellite-module checkpoint as
// photosynthesis-FvCB/voc-guenther/voc-jjv.
package core

import libc "core:c/libc"

// C++: struct O3impact::O3_impact_params
O3_Impact_Params :: struct {
	gamma1:            f64, // ozone short-term damage coefficient, unitless
	gamma2:            f64, // ozone short-term damage coefficient, nmol m-2 s-1
	gamma3:            f64, // ozone long-term damage coefficient, (umol m-2)-1
	upper_thr_stomatal: f64,
	lower_thr_stomatal: f64,
	Fshape_stomatal:   f64,
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
		Fshape_stomatal    = 2.5,
	}
}

// C++: struct O3impact::O3_impact_in
O3_Impact_In :: struct {
	FC:          f64, // field capacity, m3 m-3
	WP:          f64, // wilting point, m3 m-3
	SWC:         f64, // soil water content, m3 m-3
	ET0:         f64, // reference ET, mm d-1
	O3a:         f64, // ambient O3 partial pressure, nbar or nmol mol-1
	gs:          f64, // stomatal conductance mol m-2 s-1 bar-1 (unit ground area)
	h:           int, // hour of the day (0-23)
	reldev:      f64, // relative development
	GDD_flo:     f64, // GDD from emergence to flowering
	GDD_mat:     f64, // GDD from emergence to maturity
	fO3s_d_prev: f64, // short term ozone induced reduction of Ac of the previous time step
	sum_O3_up:   f64, // cumulated O3 uptake, umol m-2 (unit ground area)
}

// C++: struct O3impact::O3_impact_out
O3_Impact_Out :: struct {
	hourly_O3_up: f64, // hourly O3 uptake, umol m-2 h-1 (unit ground area)
	fO3s_d:       f64, // short term ozone induced reduction of Ac
	fO3l:         f64, // long term ozone induced senescence
	fLS:          f64, // leaf senescence reduction of Ac, modified by O3 cumulative uptake
	WS_st_clos:   f64, // water deficit factor for stomatal closure
}

// C++ in-class initialisers: fO3s_d{1.0}, fO3l{1.0}, fLS{1.0}, WS_st_clos{1.0}
// (hourly_O3_up{0.0} matches Odin's zero value)
make_o3_impact_out :: proc() -> O3_Impact_Out {
	return O3_Impact_Out{fO3s_d = 1.0, fO3l = 1.0, fLS = 1.0, WS_st_clos = 1.0}
}

// C++: double O3_uptake(double, double, double) - from Ewert and Porter, 2000.
// Global Change Biology, 6(7), 735-750
@(private)
o3_impact_o3_uptake :: proc(O3a, gsc, f_WS: f64) -> f64 {
	// O3_up should be nmol m-2 s-1, set input accordingly
	fDO3 := 0.93 // ratio of diffusion rates for O3 and CO2
	return O3a * gsc * f_WS * fDO3
}

@(private)
o3_impact_hourly_O3_reduction_Ac :: proc(O3_up, gamma1, gamma2: f64) -> f64 {
	// mind the units: O3_up should be nmol m-2 s-1
	fO3s_h := 1.0

	if O3_up > gamma1/gamma2 && O3_up < (1.0+gamma1)/gamma2 {
		fO3s_h = 1.0 + gamma1 - gamma2*O3_up
	} else if O3_up > (1.0+gamma1)/gamma2 {
		fO3s_h = 0
	}

	return fO3s_h
}

@(private)
o3_impact_cumulative_O3_reduction_Ac :: proc(fO3s_d_, fO3s_h, rO3s: f64, h: int) -> f64 {
	fO3s_d := fO3s_d_
	if h == 0 {
		fO3s_d = fO3s_h * rO3s
	} else {
		fO3s_d *= fO3s_h
	}

	return fO3s_d
}

@(private)
o3_impact_O3_damage_recovery :: proc(fO3s_d, fLA: f64) -> f64 {
	rO3s := fO3s_d + (1.0-fO3s_d)*fLA
	return rO3s
}

@(private)
o3_impact_O3_recovery_factor_leaf_age :: proc(reldev: f64) -> f64 {
	// since MONICA does not have leaf age/classes/span we define fLA as a
	// function of development
	crit_reldev := 0.2 // young leaves can recover fully from O3 damage
	fLA := 1.0
	if reldev > crit_reldev {
		fLA = max(0.0, 1.0-(reldev-crit_reldev)/(1.0-crit_reldev))
	}
	return fLA
}

@(private)
o3_impact_O3_senescence_factor :: proc(gamma3, O3_tot_up: f64) -> f64 {
	// O3_tot_up umol m-2; factor accounting for both onset and rate of senescence
	fO3l := max(0.5, 1.0-gamma3*O3_tot_up) // 0.5 is arbitrary
	return fO3l
}

@(private)
o3_impact_leaf_senescence_reduction_Ac :: proc(
	fO3l, reldev, GDD_flowering, GDD_maturity: f64,
) -> f64 {
	// senescence is assumed to start at flowering in normal conditions
	crit_reldev := GDD_flowering / GDD_maturity
	crit_reldev *= fO3l // correction of onset due to O3 cumulative uptake
	senescence_impact_max := 0.4 // arbitrary value
	fLS := 1.0

	if reldev > crit_reldev {
		// correction of rate due to O3 cumulative uptake
		fLS = max(
			1.0 - senescence_impact_max,
			1.0 - senescence_impact_max*(reldev-crit_reldev)/(fO3l-crit_reldev),
		)
	}
	return fLS
}

// C++: double water_stress_stomatal_closure(double, double, double, double,
//        double, double, double) - Raes et al., 2009. Agronomy Journal,
//        101(3), 438-447
@(private)
o3_impact_water_stress_stomatal_closure :: proc(
	upper_thr, lower_thr, Fshape, FC, WP, SWC, ET0: f64,
) -> f64 {
	upper_threshold_adj := upper_thr + (0.04 * (5.0 - ET0)) * libc.log10(10.0-9.0*upper_thr)
	if upper_threshold_adj < 0 {
		upper_threshold_adj = 0
	} else if upper_threshold_adj > 1 {
		upper_threshold_adj = 1.0
	}
	WHC_adj := lower_thr - upper_threshold_adj

	SW_depletion_f: f64
	if SWC >= FC {
		SW_depletion_f = 0.0
	} else if SWC <= WP {
		SW_depletion_f = 1.0
	} else {
		SW_depletion_f = 1.0 - (SWC-WP)/(FC-WP)
	}

	// Drel
	Drel: f64
	if SW_depletion_f <= upper_threshold_adj {
		Drel = 0.0
	} else if SW_depletion_f >= lower_thr {
		Drel = 1
	} else {
		Drel = (SW_depletion_f - upper_threshold_adj) / WHC_adj
	}

	return 1 - (libc.exp(Drel*Fshape) - 1.0) / (libc.exp(Fshape) - 1.0)
}

// C++: O3_impact_out O3impact::O3_impact_hourly(O3_impact_in,
//        O3_impact_params, bool)
//
// Model composition.
o3_impact_hourly :: proc(
	in_: O3_Impact_In,
	par: O3_Impact_Params,
	WaterDeficitResponseStomata: bool,
) -> O3_Impact_Out {
	out := make_o3_impact_out()

	fLA := o3_impact_O3_recovery_factor_leaf_age(in_.reldev)
	rO3s := o3_impact_O3_damage_recovery(in_.fO3s_d_prev, fLA) // used only the first hour
	out.WS_st_clos = 1.0
	if WaterDeficitResponseStomata {
		out.WS_st_clos = o3_impact_water_stress_stomatal_closure(
			par.upper_thr_stomatal,
			par.lower_thr_stomatal,
			par.Fshape_stomatal,
			in_.FC,
			in_.WP,
			in_.SWC,
			in_.ET0,
		)
	}

	inst_O3_up := o3_impact_o3_uptake(in_.O3a, in_.gs, out.WS_st_clos) // nmol m-2 s-1
	out.hourly_O3_up += inst_O3_up / 1000 // from nmol to umol
	fO3s_h := o3_impact_hourly_O3_reduction_Ac(inst_O3_up, par.gamma1, par.gamma2)

	// short term O3 effect on Ac
	out.fO3s_d = o3_impact_cumulative_O3_reduction_Ac(in_.fO3s_d_prev, fO3s_h, rO3s, in_.h)

	// senescence + long term O3 effect on Ac. Even with [O3]=0, senescence
	// will act to reduce fLS.
	out.fO3l = o3_impact_O3_senescence_factor(par.gamma3, in_.sum_O3_up)
	out.fLS = o3_impact_leaf_senescence_reduction_Ac(out.fO3l, in_.reldev, in_.GDD_flo, in_.GDD_mat)

	return out
}
