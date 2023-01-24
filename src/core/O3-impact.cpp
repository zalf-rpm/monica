/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Tommaso Stella <tommaso.stella@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include  "O3-impact.h"

#include <tuple>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <iostream>

using namespace O3impact;
using namespace std;

//from Ewert and Porter, 2000. Global Change Biology, 6(7), 735-750
double O3_uptake(double O3a, double gsc, double f_WS)
{
	//O3_up should be nmol m - 2 s - 1, set input accordingly
	double fDO3 = 0.93; //ratio of diffusion rates for O3 and CO2
	return O3a * gsc * f_WS * fDO3;
}

double hourly_O3_reduction_Ac(double O3_up, double gamma1, double gamma2)
{
	//mind the units : O3_up should be nmol m - 2 s - 1
	double fO3s_h = 1.0;

	if (O3_up > gamma1 / gamma2 && O3_up < (1.0 + gamma1) / gamma2)
	{
		fO3s_h = 1.0 + gamma1 - gamma2 * O3_up;
	}
	else if (O3_up > (1.0 + gamma1) / gamma2)
	{
		fO3s_h = 0;
	}
	
	return fO3s_h;
}

double cumulative_O3_reduction_Ac(double fO3s_d, double fO3s_h, double rO3s, int h)
{	
	if (h == 0)
	{
		fO3s_d = fO3s_h * rO3s;
	}
	else
	{
		fO3s_d *= fO3s_h;
	}

	return fO3s_d;
}

double O3_damage_recovery(double fO3s_d, double fLA)
{
	double rO3s = fO3s_d + (1.0 - fO3s_d) * fLA;
	return rO3s;
}

double O3_recovery_factor_leaf_age(double reldev)
{
	//since MONICA does not have leaf age/classes/span
	//we define fLA as a function of development
	double crit_reldev = 0.2; //young leaves can recover fully from O3 damage
	double fLA = 1.0;
	if (reldev > crit_reldev)
	{
		fLA = std::max(0.0, 1.0 - (reldev - crit_reldev) / (1.0 - crit_reldev));
	}	
	return fLA;
}

double O3_senescence_factor(double gamma3, double O3_tot_up)
{
	//O3_tot_up �mol m - 2
	//factor accounting for both onset and rate of senescence
	double fO3l = std::max(0.5, 1.0 - gamma3 * O3_tot_up); //0.5 is arbitrary
	return fO3l;
}

double leaf_senescence_reduction_Ac(double fO3l, double reldev, double GDD_flowering, double GDD_maturity)
{
	//senescence is assumed to start at flowering in normal conditions
	double crit_reldev = GDD_flowering / GDD_maturity;
	crit_reldev *= fO3l; //correction of onset due to O3 cumulative uptake
	double senescence_impact_max = 0.4; //arbirary value
	double fLS = 1.0;

	if (reldev > crit_reldev)
	{
		fLS = std::max((1.0 - senescence_impact_max), 1.0 - senescence_impact_max * (reldev - crit_reldev) / (fO3l - crit_reldev)); //correction of rate due to O3 cumulative uptake
	}
	return fLS;
}

double water_stress_stomatal_closure(double upper_thr, double lower_thr, double Fshape, double FC, double WP, double SWC, double ET0)
{
	//Raes et al., 2009.  Agronomy Journal, 101(3), 438-447
	double upper_threshold_adj = upper_thr + (0.04 * (5.0 - ET0)) * std::log10(10.0 - 9.0 * upper_thr);
	if (upper_threshold_adj < 0)
	{
		upper_threshold_adj = 0;
	}
	else if (upper_threshold_adj > 1)
	{
		upper_threshold_adj = 1.0;
	}
	double WHC_adj = lower_thr - upper_threshold_adj;

	double SW_depletion_f;
	if (SWC >= FC)
	{
		SW_depletion_f = 0.0;
	}
	else if (SWC <= WP)
	{
		SW_depletion_f = 1.0;
	}
	else
	{
		SW_depletion_f = 1.0 - (SWC - WP) / (FC - WP);
	}	

	//Drel
	double Drel;
	if (SW_depletion_f <= upper_threshold_adj)
	{
		Drel = 0.0;
	}
	else if (SW_depletion_f >= lower_thr)
	{
		Drel = 1;
	}
	else
	{
		Drel = (SW_depletion_f - upper_threshold_adj) / WHC_adj;
	}
	
	return 1 - (std::exp(Drel * Fshape) - 1.0) / (std::exp(Fshape) - 1.0);
}

#ifdef TEST_O3_HOURLY_OUTPUT
#include <fstream>
ostream& O3impact::tout(bool closeFile)
{
	static ofstream out;
	static bool init = false;
	static bool failed = false;
	if (closeFile)
	{
		init = false;
		failed = false;
		out.close();
		return out;
	}

	if (!init)
	{
		out.open("O3_hourly_data.csv");
		failed = out.fail();
		(failed ? cout : out) <<
			"iso-date"
			",hour"
			",crop-name"
			",co2"
			",o3"
			",in.reldev"
			",fLA"
			",rO3s"
			",WS_st_clos"
			",in.gs"
			",inst_O3_up"
			",fO3s_h"
			",in.fO3s_d_prev"
			",out.fO3s_d"
			",in.sum_O3_up"
			",fO3l"
			",out.fLS"
			<< endl;

		init = true;
	}

	return failed ? cout : out;
}
#endif

#pragma region 
//model composition
O3_impact_out O3impact::O3_impact_hourly(O3_impact_in in, O3_impact_params par, bool WaterDeficitResponseStomata)
{
	O3_impact_out out;

	double fLA = O3_recovery_factor_leaf_age(in.reldev);
	double rO3s = O3_damage_recovery(in.fO3s_d_prev, fLA); //used only the first hour
	out.WS_st_clos = 1.0;
	if (WaterDeficitResponseStomata)
	{
		out.WS_st_clos = water_stress_stomatal_closure(par.upper_thr_stomatal, par.lower_thr_stomatal, par.Fshape_stomatal, in.FC, in.WP, in.SWC, in.ET0);
	}
	
	double inst_O3_up = O3_uptake(in.O3a, in.gs, out.WS_st_clos); //nmol m-2 s-1
	out.hourly_O3_up += inst_O3_up / 1000; //from nmol to �mol //* 3.6; //3.6 converts from nmol to �mol and from s-1 to h-1	
	double fO3s_h = hourly_O3_reduction_Ac(inst_O3_up, par.gamma1, par.gamma2);
	
	//short term O3 effect on Ac	
	out.fO3s_d = cumulative_O3_reduction_Ac(in.fO3s_d_prev, fO3s_h, rO3s, in.h);	

	//sensescence + long term O3 effect on Ac. !!also with [O3]=0, senescence will act to reduce fLS
	out.fO3l = O3_senescence_factor(par.gamma3, in.sum_O3_up);
	out.fLS = leaf_senescence_reduction_Ac(out.fO3l, in.reldev, in.GDD_flo, in.GDD_mat);

#ifdef TEST_O3_HOURLY_OUTPUT
	tout()
	<< "," << in.reldev
	<< "," << fLA
	<< "," << rO3s
	<< "," << out.WS_st_clos
	<< "," << in.gs
	<< "," << inst_O3_up
	<< "," << fO3s_h
	<< "," << in.fO3s_d_prev
	<< "," << out.fO3s_d
	<< "," << in.sum_O3_up
	<< "," << out.fO3l
	<< "," << out.fLS
	<< endl;
#endif

	return out;
}
#pragma endregion Model composition


	
	

	
	
	
	
	