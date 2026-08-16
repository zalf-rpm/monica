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

import libc "core:c/libc"
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
nit_fNH4 :: proc(NH4, nh4_min, w, Kamm: f64) -> f64 {
	return max(0.0, NH4-nh4_min) / (max(0.0, NH4-nh4_min) + (w * Kamm))
}

@(private)
nit_fpH :: proc(pHminnit, pH, pHmaxnit: f64) -> f64 {
	return stepwise_linear_function3(pH, pHminnit, pHmaxnit)
}

@(private)
nit_fTgauss :: proc(t, tnitopt_gauss, scale_tnitopt: f64) -> f64 {
	return libc.exp(-1 * libc.pow(t-tnitopt_gauss, 2) / libc.pow(scale_tnitopt, 2))
}

@(private)
nit_fTstep :: proc(t, tnitmin, tnitopt, tnitopt2, tnitmax: f64) -> f64 {
	return max(0.0, stepwise_linear_function4(t, tnitmin, tnitopt, tnitopt2, tnitmax))
}

@(private)
nit_fWFPS :: proc(wfps, hminn, hoptn, fc, sat: f64) -> f64 {
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
	NH4, pH, soilT, wfps, soilWaterContent, fc, sat: f64,
) -> f64 {
	vnitpot := 0.0
	fNH4res := 0.0
	switch ps.code_vnit {
	case 1:
		vnitpot = ps.fnx * max(0.0, NH4-ps.nh4_min)
		fNH4res = 1
	case 2:
		vnitpot = ps.vnitmax
		fNH4res = nit_fNH4(NH4, ps.nh4_min, soilWaterContent, ps.Kamm)
	}

	fTres := 0.0
	switch ps.code_tnit {
	case 1:
		fTres = nit_fTstep(soilT, ps.tnitmin, ps.tnitopt, ps.tnitop2, ps.tnitmax)
	case 2:
		fTres = nit_fTgauss(soilT, ps.tnitopt_gauss, ps.scale_tnitopt)
	}

	return vnitpot * fNH4res * nit_fpH(ps.pHminnit, pH, ps.pHmaxnit) * fTres * nit_fWFPS(wfps, ps.hminn, ps.hoptn, fc, sat)
}

@(private)
denit_fNO3 :: proc(NO3, w, Kd: f64) -> f64 {
	return NO3 / (NO3 + (w * Kd))
}

@(private)
denit_fT :: proc(t, tdenitopt_gauss, scale_tdenitopt: f64) -> f64 {
	delta := t - tdenitopt_gauss
	return libc.exp(-1 * (delta * delta) / (scale_tdenitopt * scale_tdenitopt))
}

// water-filled pore space
@(private)
denit_fWFPS :: proc(wfps, wfpsc: f64) -> f64 {
	return libc.pow((max(wfps, wfpsc)-wfpsc)/(1-wfpsc), 1.74)
}

// C++: double stics::vdenit(const SticsParameters&, double, double, double,
//        double, double)
//
// denitrification [mg-N/kg-soil/day].
stics_vdenit :: proc(ps: ^p.Stics_Parameters, corg, NO3, soilT, wfps, soilWaterContent: f64) -> f64 {
	vdenitpot := 0.0
	switch ps.code_pdenit {
	case 1:
		vdenitpot = ps.vpotdenit
	case 2:
		vdenitpot = stepwise_linear_function3(corg, ps.cmin_pdenit, ps.cmax_pdenit, ps.min_pdenit, ps.max_pdenit)
	}

	return vdenitpot * denit_fNO3(NO3, soilWaterContent, ps.Kd) * denit_fT(soilT, ps.tdenitopt_gauss, ps.scale_tdenitopt) * denit_fWFPS(wfps, ps.wfpsc)
}

@(private)
n2o_fpH :: proc(pH, pHminden, pHmaxden: f64) -> f64 {
	return stepwise_linear_function3(pH, pHminden, pHmaxden, 1.0, 0.0)
}

@(private)
n2o_fWFPS :: proc(wfps, wfpsc: f64) -> f64 {
	return 1.0 - (wfps-wfpsc)/(1.0-wfpsc)
}

@(private)
n2o_rcor :: proc(wfpsc, pH, pHminden, pHmaxden: f64) -> f64 {
	rest := n2o_fpH(pH, pHminden, pHmaxden)
	return rest / n2o_fWFPS(0.815, wfpsc)
}

@(private)
n2o_fNO3 :: proc(NO3: f64) -> f64 {
	return NO3 / (NO3 + 1)
}

// C++: stics::NitDenitN2O stics::N2O(const SticsParameters&, double, double,
//        double, double, double)
//
// N2O emissions [mg-N2O-N/kg-soil/day]. vnit [mg-n/kg-soil/day], vdenit
// [mg-N/kg-soil/day]. Returns (N2Onit, N2Odenit).
stics_n2o :: proc(ps: ^p.Stics_Parameters, NO3, wfps, pH, vnit, vdenit: f64) -> (f64, f64) {
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
		r = n2o_rcor(ps.wfpsc, pH, ps.pHminden, ps.pHmaxden) * n2o_fWFPS(wfps, ps.wfpsc) * n2o_fNO3(NO3)
	}

	N2Onit := z * vnit
	N2Odenit := r * vdenit

	return N2Onit, N2Odenit
}

// C++: stics::NitDenitN2O stics::N2O(const SticsParameters&, double, double,
//        double, double, double, double, double, double, double) (9-arg
//        convenience overload that computes vnit/vdenit itself)
stics_n2o_full :: proc(
	ps: ^p.Stics_Parameters,
	corg, NO3, soilT, wfps, soilWaterContent, NH4, pH, fc, sat: f64,
) -> (
	f64,
	f64,
) {
	nit := stics_vnit(ps, NH4, pH, soilT, wfps, soilWaterContent, fc, sat)
	denit := stics_vdenit(ps, corg, NO3, soilT, wfps, soilWaterContent)
	return stics_n2o(ps, NO3, wfps, pH, nit, denit)
}
