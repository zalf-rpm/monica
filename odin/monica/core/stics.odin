// Port of src/core/stics-nit-denit-n2o.{h,cpp}: nitrification, denitrification
// and N2O production per the STICS model (used by soilorganic.cpp when
// SticsParameters.use_nit/use_denit/use_n2o are enabled - false by default and
// in every fixture in this repo, so soil_organic.odin's own oracle never
// exercises this branch, but it must still compile and be correct).
//
// Fully self-contained: only depends on p.Stics_Parameters (phase 1c, already
// ported). The C++ file-local `nit::`/`denit::`/`n2o::` namespaces and the two
// stepwiseLinearFunction helpers become @(private) procs prefixed with their
// namespace, since Odin has no nested namespaces.
package core

import "../../support/cr_math"
import p "../params"

@(private)
stepwise_linear_function3 :: proc(x, xmin, xmax: f64, ymin: f64 = 0.0, ymax: f64 = 1.0) -> f64 {
	if x < xmin {
		return ymin
	}
	if x > xmax {
		return ymax
	}
	return ymin + (ymax-ymin)/(xmax-xmin)*(x-xmin)
}

@(private)
stepwise_linear_function4 :: proc(
	x, xmin, x1, x2, xmax: f64,
	ymin: f64 = 0.0,
	ymax: f64 = 1.0,
) -> f64 {
	if x <= xmin || x > xmax {
		return ymin
	}
	if xmin < x && x < x1 {
		return ymin + (ymax-ymin)/(x1-xmin)*(x-xmin)
	}
	if x1 <= x && x <= x2 {
		return ymax
	}
	return ymax + ((ymin - ymax) / (xmax - x2) * (x - x2))
}

@(private)
nit_f_nh4 :: proc(nh4, nh4_min, w, kamm: f64) -> f64 {
	return max(0.0, nh4-nh4_min) / (max(0.0, nh4-nh4_min) + (w * kamm))
}

@(private)
nit_f_ph :: proc(ph_min_nit, ph, ph_max_nit: f64) -> f64 {
	return stepwise_linear_function3(ph, ph_min_nit, ph_max_nit)
}

@(private)
nit_f_tgauss :: proc(t, tnitopt_gauss, scale_tnitopt: f64) -> f64 {
	return cr_math.exp(-1 * cr_math.pow(t-tnitopt_gauss, 2) / cr_math.pow(scale_tnitopt, 2))
}

@(private)
nit_f_tstep :: proc(t, tnitmin, tnitopt, tnitopt2, tnitmax: f64) -> f64 {
	return max(0.0, stepwise_linear_function4(t, tnitmin, tnitopt, tnitopt2, tnitmax))
}

@(private)
nit_f_wfps :: proc(wfps, hminn, hoptn, fc, sat: f64) -> f64 {
	return stepwise_linear_function4(wfps, hminn*fc/sat, hoptn*fc/sat, fc/sat, sat/sat)
}

// C++: double stics::vnit(const SticsParameters&, double, double, double,
//        double, double, double, double)
//
// nitrification [mg-N/kg-soil/day]. NH4 [mg-NH4-N/kg-soil], wfps =
// water-filled pore space [] = soil-water-content/saturation, soilWaterContent
// = gravimetric soil water content [kg-water/kg-soil], fc = fieldcapacity
// [m3-water/m3-soil], sat = saturation [m3-water/m3-soil].
stics_vnit :: proc(
	ps: ^p.Stics_Parameters,
	nh4, ph, soil_t, wfps, soil_water_content, fc, sat: f64,
) -> f64 {
	vnitpot := 0.0
	f_nh4_res := 0.0
	switch ps.code_vnit {
	case 1:
		vnitpot = ps.fnx * max(0.0, nh4-ps.nh4_min)
		f_nh4_res = 1
	case 2:
		vnitpot = ps.vnitmax
		f_nh4_res = nit_f_nh4(nh4, ps.nh4_min, soil_water_content, ps.kamm)
	}

	f_tres := 0.0
	switch ps.code_tnit {
	case 1:
		f_tres = nit_f_tstep(soil_t, ps.tnitmin, ps.tnitopt, ps.tnitop2, ps.tnitmax)
	case 2:
		f_tres = nit_f_tgauss(soil_t, ps.tnitopt_gauss, ps.scale_tnitopt)
	}

	return vnitpot * f_nh4_res * nit_f_ph(ps.ph_min_nit, ph, ps.ph_max_nit) * f_tres * nit_f_wfps(wfps, ps.hminn, ps.hoptn, fc, sat)
}

@(private)
denit_f_no3 :: proc(no3, w, kd: f64) -> f64 {
	return no3 / (no3 + (w * kd))
}

@(private)
denit_f_t :: proc(t, tdenitopt_gauss, scale_tdenitopt: f64) -> f64 {
	delta := t - tdenitopt_gauss
	return cr_math.exp(-1 * (delta * delta) / (scale_tdenitopt * scale_tdenitopt))
}

// water-filled pore space
@(private)
denit_f_wfps :: proc(wfps, wfpsc: f64) -> f64 {
	return cr_math.pow((max(wfps, wfpsc)-wfpsc)/(1-wfpsc), 1.74)
}

// C++: double stics::vdenit(const SticsParameters&, double, double, double,
//        double, double)
//
// denitrification [mg-N/kg-soil/day].
stics_vdenit :: proc(ps: ^p.Stics_Parameters, corg, no3, soil_t, wfps, soil_water_content: f64) -> f64 {
	vdenitpot := 0.0
	switch ps.code_pdenit {
	case 1:
		vdenitpot = ps.vpotdenit
	case 2:
		vdenitpot = stepwise_linear_function3(corg, ps.cmin_pdenit, ps.cmax_pdenit, ps.min_pdenit, ps.max_pdenit)
	}

	return vdenitpot * denit_f_no3(no3, soil_water_content, ps.kd) * denit_f_t(soil_t, ps.tdenitopt_gauss, ps.scale_tdenitopt) * denit_f_wfps(wfps, ps.wfpsc)
}

@(private)
n2o_f_ph :: proc(ph, ph_min_den, ph_max_den: f64) -> f64 {
	return stepwise_linear_function3(ph, ph_min_den, ph_max_den, 1.0, 0.0)
}

@(private)
n2o_f_wfps :: proc(wfps, wfpsc: f64) -> f64 {
	return 1.0 - (wfps-wfpsc)/(1.0-wfpsc)
}

@(private)
n2o_rcor :: proc(wfpsc, ph, ph_min_den, ph_max_den: f64) -> f64 {
	rest := n2o_f_ph(ph, ph_min_den, ph_max_den)
	return rest / n2o_f_wfps(0.815, wfpsc)
}

@(private)
n2o_f_no3 :: proc(no3: f64) -> f64 {
	return no3 / (no3 + 1)
}

// C++: stics::NitDenitN2O stics::N2O(const SticsParameters&, double, double,
//        double, double, double)
//
// N2O emissions [mg-N2O-N/kg-soil/day]. vnit [mg-n/kg-soil/day], vdenit
// [mg-N/kg-soil/day]. Returns (N2Onit, N2Odenit).
stics_n2o :: proc(ps: ^p.Stics_Parameters, no3, wfps, ph, vnit, vdenit: f64) -> (f64, f64) {
	z := 0.0
	switch ps.code_rationit {
	case 1:
		z = ps.rationit
	case 2:
		z = 0.16 * (0.4*wfps - 1.04) / (wfps - 1.04) / 100.0
	}

	r := 0.0
	switch ps.code_ratiodenit {
	case 1:
		r = ps.ratiodenit
	case 2:
		r = n2o_rcor(ps.wfpsc, ph, ps.ph_min_den, ps.ph_max_den) * n2o_f_wfps(wfps, ps.wfpsc) * n2o_f_no3(no3)
	}

	n2o_nit := z * vnit
	n2o_denit := r * vdenit

	return n2o_nit, n2o_denit
}

// C++: stics::NitDenitN2O stics::N2O(const SticsParameters&, double, double,
//        double, double, double, double, double, double, double) (9-arg
//        convenience overload that computes vnit/vdenit itself)
stics_n2o_full :: proc(
	ps: ^p.Stics_Parameters,
	corg, no3, soil_t, wfps, soil_water_content, nh4, ph, fc, sat: f64,
) -> (
	f64,
	f64,
) {
	nit := stics_vnit(ps, nh4, ph, soil_t, wfps, soil_water_content, fc, sat)
	denit := stics_vdenit(ps, corg, no3, soil_t, wfps, soil_water_content)
	return stics_n2o(ps, no3, wfps, ph, nit, denit)
}
