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

#include <tuple>
#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>

#include  "photosynthesis-FvCB.h"
#include "tools/helper.h"

using namespace FvCB;
using namespace Tools;
using namespace std;

std::map<FvCB_Model_Consts, double> FvCB::c_bernacchi = {{Rd, 18.72},{Vcmax, 26.35},{Vomax, 22.98},{Gamma, 19.02},{Kc, 38.05},{Ko, 20.30},{Jmax, 17.57}}; //dimensionless
std::map<FvCB_Model_Consts, double> FvCB::deltaH_bernacchi = {{Rd, 46.39},{Vcmax, 65.33},{Vomax, 60.11},{Gamma, 37.83},{Kc, 79.43},{Ko, 36.38},{Jmax, 43.54}}; //kJ mol - 1

//estimate the fraction of diffuse radiation; it requires hourly input
double diffuse_fraction_hourly_f(double globrad, double extra_terr_rad, double solar_elev)
{
	double glob_extra_ratio = globrad / extra_terr_rad;
	double R = (0.847 - 1.61 * sin(solar_elev) + 1.04 * pow(sin(solar_elev), 2));
	double K = (1.47 - R) / 1.66;

	if (glob_extra_ratio <= 0.22)
	{
		return 1;
	}
	else if (glob_extra_ratio <= 0.35)
	{
		return 1 - 6.4 * pow((glob_extra_ratio - 0.22), 2);
	}
	else if (glob_extra_ratio <= K)
	{
		return 1.47 - 1.66 * glob_extra_ratio;
	}
	else
	{
		return R;
	}
}

#pragma region
//Radiation absorbed by sun/shade leaves
//Ic = Ic_sun + Ic_sh
//Ic_sun = direct + diffuse + scattered

#pragma region
//direct beam absorberd by sunlit leaves
double abs_sunlit_direct_f(double I_dir_beam, double solar_elev, double LAI)
{
	double kb; //beam radiation extinction coefficient of canopy

	if (solar_elev < 0)
	{
		return 0;
	}
	else if (solar_elev == 0)
	{
		kb = 1000;
	}
	else
	{
		kb = 0.5 / sin(solar_elev);
	}

	double sigma = 0.15; //scattering coefficient
	return I_dir_beam * (1 - sigma) * (1 - exp(-kb * LAI));
}

//diffuse radiation absorbed by sunlit leaves
double abs_sunlit_diffuse_f(double I_dif, double solar_elev, double LAI)
{
	double kb; //beam radiation extinction coefficient of canopy
	double rho_cd = 0.036; //reflection coeff diffuse PAR
	double k1_d = 0.719; //diffuse par extinction coeff

	if (solar_elev < 0)
	{
		return 0;
	}
	else if (solar_elev == 0)
	{
		kb = 1000;
	}
	else
	{
		kb = 0.5 / sin(solar_elev);
	}
	return I_dif * (1 - rho_cd) * (1 - exp(-(k1_d + kb)*LAI)) * k1_d / (k1_d + kb);
}

//scattered beam absorberd by sunlit leaves
double abs_sunlit_scattered_f(double I_dir_beam, double solar_elev, double LAI)
{
	double kb; //beam radiation extinction coefficient of canopy
	double k1_b; //beam and scattered beam PAR extinction coefficient
	double rho_cb; //reflection coefficient beam irradiance (uniform leaf angle distrib)
	double sigma; //scattering coefficient

	if (solar_elev < 0)
	{
		return 0;
	}
	else if (solar_elev == 0)
	{
		kb = 1000;
		k1_b = 1000;
	}
	else
	{
		kb = 0.5 / sin(solar_elev);
		k1_b = 0.46 / sin(solar_elev);
	}
	double rho_h = 0.041; //reflection coeff beam irradiance (horizontal leaves)
	rho_cb = 1 - exp(2 * rho_h * kb / (1 + kb));
	sigma = 0.15;

	double line_1 = (1 - rho_cb) * (1 - exp(-(k1_b + kb) * LAI)) * k1_b / (k1_b + kb);
	double line_2 = (1 - sigma) * (1 - exp(-2 * kb *LAI)) / 2;

	return I_dir_beam * (line_1 - line_2);
}
#pragma endregion direct, diffuse, scattered (sunlit)

//irradiance absorbed by the canopy
double Ic_f(double I_dir_beam, double I_dif, double solar_elev, double LAI)
{
	double kb; //beam radiation extinction coefficient of canopy
	double k1_b; //beam and scattered beam PAR extinction coefficient

	if (solar_elev < 0)
	{
		return 0;
	}
	else if (solar_elev == 0)
	{
		kb = 1000;
		k1_b = 1000;
	}
	else
	{
		kb = 0.5 / sin(solar_elev);
		k1_b = 0.46 / sin(solar_elev);
	}

	double rho_h = 0.041;
	double rho_cb = 1 - exp(2 * rho_h * kb / (1 + kb));
	double rho_cd = 0.036; //reflection coeff diffuse PAR
	double k1_d = 0.719; //diffuse par extinction coeff

	double Ic_dir = (1 - rho_cb) * I_dir_beam * (1 - exp(-k1_b * LAI));
	double Ic_dif = (1 - rho_cd) * I_dif * (1 - exp(-k1_d * LAI));

	return fmin((Ic_dir + Ic_dif), (I_dir_beam + I_dif));
}

//irradiance absorbed by sunlit LAI
double Ic_sun_f(double I_dir_beam, double I_dif, double solar_elev, double LAI)
{
	return (abs_sunlit_direct_f(I_dir_beam, solar_elev, LAI) +
		abs_sunlit_diffuse_f(I_dif, solar_elev, LAI) +
		abs_sunlit_scattered_f(I_dir_beam, solar_elev, LAI));
}

//irradiance absorbed by shaded LAI
double Ic_shade_f(double I_dir_beam, double I_dif, double solar_elev, double LAI)
{
	return Ic_f(I_dir_beam, I_dif, solar_elev, LAI) - Ic_sun_f(I_dir_beam, I_dif, solar_elev, LAI);
}

std::tuple<double, double> LAI_sunlit_shaded_f(double LAI, double solar_elev)
{
	double kb; //beam radiation extinction coefficient of canopy

	if (solar_elev < 0)
	{
		return std::make_tuple(0, LAI);
	}
	else if (solar_elev == 0)
	{
		kb = 1000;
	}
	else
	{
		kb = 0.5 / sin(solar_elev);
	}

	double LAI_sunlit = (1 - exp(-kb * LAI)) / kb;
	return std::make_tuple(LAI_sunlit, (LAI - LAI_sunlit));
}

#pragma endregion Radiation absorbed by sun/shade leaves

#pragma region 
//FvCB model params

//T response
double Tresp_bernacchi_f(double c, double deltaH, double leafT)
{
	double Tk = leafT + 273;
	double R = 8.314472 * pow(10, -3); //kJ K - 1 mol - 1
	return exp(c - deltaH / (R * Tk));
}

double Vcmax_bernacchi_f(double leafT, double Vcmax_25)
{
	return Vcmax_25 * Tresp_bernacchi_f(c_bernacchi[Vcmax], deltaH_bernacchi[Vcmax], leafT);
}

double Jmax_bernacchi_f(double leafT, double Jmax_25)
{
	return Jmax_25 * Tresp_bernacchi_f(c_bernacchi[Jmax], deltaH_bernacchi[Jmax], leafT);
}

double J_bernacchi_f(double Q, double leafT, double Jmax)
{
	double alfa = 0.5; //theoretical leaf absorbance
	double beta = 0.77; //fraction of absorbed quanta reaching PSII
	double theta_ps2 = 0.76 + 0.018 * leafT - 3.7 * pow(10, -4) * pow(leafT, 2);
	double theta_ps2max = 0.352 + 0.022 *leafT - 3.4 * pow(10, -4) * pow(leafT, 2);
	double Q2 = Q * alfa * theta_ps2max * beta;

	double numerator = Q2 + Jmax - sqrt(pow((Q2 + Jmax), 2) - 4 * theta_ps2 * Q2 * Jmax);
	double denominator = 2 * theta_ps2;
	return numerator / denominator;
}
	
double Rd_bernacchi_f(double leafT)
{
	return Tresp_bernacchi_f(c_bernacchi[Rd], deltaH_bernacchi[Rd], leafT);
}

double Vomax_bernacchi_f(double leafT, double Vcmax_25)
{
	return Vcmax_25 * Tresp_bernacchi_f(c_bernacchi[Vomax], deltaH_bernacchi[Vomax], leafT);
}

double Kc_bernacchi_f(double leafT)
{
	return Tresp_bernacchi_f(c_bernacchi[Kc], deltaH_bernacchi[Kc], leafT);
}

double Ko_bernacchi_f(double leafT)
{
	return Tresp_bernacchi_f(c_bernacchi[Ko], deltaH_bernacchi[Ko], leafT);
}

double Oi_f(double leafT)
{
	double T1 = 1.3087 * pow(10, -3) * leafT;
	double T2 = 2.5603 * pow(10, -5) * pow(leafT, 2);
	double T3 = 2.1441 * pow(10, -7) * pow(leafT, 3);
	return 210 * (4.7 * pow(10, -2) - T1 + T2 - T3) / (2.6934 * pow(10, -2));
}

double Gamma_bernacchi_f(double leafT, double Vcmax, double Vomax)
{
	double numerator = 0.5 * Vomax * Kc_bernacchi_f(leafT) * Oi_f(leafT);
	double denominator = Vcmax * Ko_bernacchi_f(leafT);
	return flt_equal_zero(denominator) ? 0.0 : numerator / denominator;
}

#pragma endregion FvCB model params

#pragma region 
//canopy photosynthetic capacity
double canopy_ps_capacity_f(double LAI, double Vcmax, double kn)
{
	return LAI * Vcmax * (1 - exp(-kn)) / kn;
}

double canopy_ps_capacity_sunlit_f(double LAI, double solar_elev, double Vcmax, double kn)
{
	double kb; //beam radiation extinction coefficient of canopy

	if (solar_elev < 0)
	{
		return 0;
	}
	else if (solar_elev == 0)
	{
		kb = 1000;
	}
	else
	{
		kb = 0.5 / sin(solar_elev);
	}

	return LAI * Vcmax * (1 - exp(-kn - kb*LAI)) / (kn + kb*LAI);
}

double canopy_ps_capacity_shaded_f(double LAI, double solar_elev, double Vcmax, double kn)
{
	return (canopy_ps_capacity_f(LAI, Vcmax, kn) - canopy_ps_capacity_sunlit_f(LAI, solar_elev, Vcmax, kn));
}
	
#pragma endregion canopy photosynthetic capacity

#pragma region
//conductance functions
double gm_bernacchi_f(double leafT, double gm_25)
{
	double c = 20.0; //it should be put into constants data struct.
	double deltaHa = 49.6;//  ||
	double deltaHd = 437.4;// ||
	double deltaS = 1.4;//    ||
	double R = 0.008314;//    || kJ J - 1 mol - 1
		
	double Tk = leafT + 273.15;

	double numerator = exp(c - deltaHa / (R * Tk));
	double denominator = 1 + exp((deltaS * Tk - deltaHd) / (R * Tk));

	return gm_25 * numerator / denominator;
}


#pragma endregion conductance functions

#pragma region 
//Coupled photosynthesis-stomatal conductance
//Yin, Struik, 2009. NJAS 57 (2009) 27-38

double fVPD_f(double VPD)
{
	//VPD in KPa
	double a1 = 0.9;
	double b1 = 0.15; //kPa - 1
	return 1 / (1 / (a1 - b1*VPD) - 1);
}

#pragma region 
//Lumped coefficients cubic equation C3

std::tuple<double, double> x_rubisco(double leafT, double Vcmax)
{
	double x1 = Vcmax;
	double x2 = Kc_bernacchi_f(leafT) * (1 + Oi_f(leafT) / Ko_bernacchi_f(leafT));

	return std::make_tuple(x1, x2);
}

std::tuple<double, double> x_electron(double J, double gamma)
{
	double x1 = J / 4.5;
	double x2 = 10.5 / 4.5 * gamma;

	return std::make_tuple(x1, x2);
}

struct Lumped_Coeffs {
	double p{ 0.0 };
	double Q{ 0.0 };
	double psi{ 0.0 };
};

Lumped_Coeffs calculate_lumped_coeffs(double x1, double x2, double fVPD, double Ca, double gamma, double Rd, double g0, double gm_C3, double gb)
{	
	Lumped_Coeffs lumped_coeffs;
	//m
	double first = 1 / gm_C3;
	double second = g0 / gm_C3 + fVPD;
	double third = 1 / gm_C3 + 1 / gb;
	double m = first + second * third;

	//d
	double d = x2 + gamma + (x1 - Rd) / gm_C3;

	//c
	double c = Ca + x2 + (1 / gm_C3 + 1 / gb) * (x1 - Rd);

	//b
	double b = Ca * (x1 - Rd) - gamma * x1 - Rd * x2;

	//a
	first = g0*(x2 + gamma);
	second = g0 / gm_C3 + fVPD;
	third = x1 - Rd;
	double a = first + second * third;

	//r
	double r = -a*b / m;

	//q
	double q = (d*(x1 - Rd) + a*c + (g0 / gm_C3 + fVPD)*b) / m;

	//p
	lumped_coeffs.p = -(d + (x1 - Rd) / gm_C3 + a*(1 / gm_C3 + 1 / gb) + (g0 / gm_C3 + fVPD)*c) / m;

	//U
	double U = (2 * pow(lumped_coeffs.p, 3) - 9 * lumped_coeffs.p*q + 27 * r) / 54;

	//Q
	lumped_coeffs.Q = (pow(lumped_coeffs.p, 2) - 3 * q) / 9;

	//psi
	lumped_coeffs.psi = acos(U / sqrt(pow(lumped_coeffs.Q, 3)));

	return lumped_coeffs;
}
#pragma endregion Lumped coefficients C3

#pragma region
//Cubic equation solutions
double A1_f(Lumped_Coeffs lumped_coeffs)
{
	return -2 * sqrt(lumped_coeffs.Q) * cos(lumped_coeffs.psi / 3) - lumped_coeffs.p / 3;
}

double A2_f(Lumped_Coeffs lumped_coeffs)
{
	return -2 * sqrt(lumped_coeffs.Q) * cos((lumped_coeffs.psi + 2 * M_PI) / 3) - lumped_coeffs.p / 3;
}

double A3_f(Lumped_Coeffs lumped_coeffs)
{
	return -2 * sqrt(lumped_coeffs.Q) * cos((lumped_coeffs.psi + 4 * M_PI) / 3) - lumped_coeffs.p / 3;
}
#pragma endregion Cubic equation solutions

double derive_gs_f(double A, double x1, double x2, double gamma, double Rd, double gm, double fVPD, double g0)
{
	double numerator = -(A * x2 + Rd * x2 + gamma * x1);
	double denominator = A + Rd - x1;
	double Cc = numerator / denominator;
	double Ci = Cc + (A / gm);
	double Ci_star = gamma - Rd / gm;
	return g0 + (A + Rd) / (Ci - Ci_star) * fVPD;
}

#pragma endregion Coupled photosynthesis-stomatal conductance

#pragma region
//Model composition (C3)
FvCB_canopy_hourly_out FvCB::FvCB_canopy_hourly_C3(FvCB_canopy_hourly_in in, FvCB_canopy_hourly_params par)
{
	FvCB_canopy_hourly_out out;

	//1. calculate diffuse and direct radiation
	double diffuse_fraction = diffuse_fraction_hourly_f(in.global_rad, in.extra_terr_rad, in.solar_el);	
	double hourly_diffuse_rad = in.global_rad * diffuse_fraction;
	double hourly_direct_rad = in.global_rad - hourly_diffuse_rad;
	double inst_diff_rad = hourly_diffuse_rad * pow(10, 6) / 3600.0 * 4.56; //µmol m - 2 s - 1 (unit ground area)
	double inst_dir_rad = hourly_direct_rad * pow(10, 6) / 3600.0 * 4.56; //1 W m-2 = 4.56 µmol m-2 s-1
	
	//2. calculate Radiation absorbed by sunlit / shaded canopy
	double Ic_sun = Ic_sun_f(inst_dir_rad, inst_diff_rad, in.solar_el, in.LAI); //µmol m - 2 s - 1 (unit ground area)
	double Ic_sh = Ic_shade_f(inst_dir_rad, inst_diff_rad, in.solar_el, in.LAI); //µmol m - 2 s - 1 (unit ground area)
	
	//2.1. calculate sunlit/shaded LAI
	std::tuple<double, double> sun_shade_LAI = LAI_sunlit_shaded_f(in.LAI, in.solar_el);
	out.LAI_sun = std::get<0>(sun_shade_LAI);
	out.LAI_sh = std::get<1>(sun_shade_LAI);

	//For each fraction :
	//-------------------
	//3. canopy photosynthetic capacity
	double Vcmax = Vcmax_bernacchi_f(in.leaf_temp, par.Vcmax_25);
	double Vcmax_25 = Vcmax_bernacchi_f(25.0, par.Vcmax_25); //the value at 25°C calculated with bernacchi slightly deviates from par.Vcmax_25

	//test
	//Vcmax = 100.0;
		
	double Vc_25 = canopy_ps_capacity_f(in.LAI, Vcmax_25, par.kn); //µmol m - 2 s - 1 (unit ground area)
	double Vc_sun_25 = canopy_ps_capacity_sunlit_f(in.LAI, in.solar_el, Vcmax_25, par.kn);
	double Vc_sh_25 = Vc_25 - Vc_sun_25;
	double Vc = canopy_ps_capacity_f(in.LAI, Vcmax, par.kn); 
	double Vc_sun = canopy_ps_capacity_sunlit_f(in.LAI, in.solar_el, Vcmax, par.kn);
	double Vc_sh = Vc - Vc_sun;
	cout << Vc << endl;

	//4. canopy electron transport capacity
	double Jmax_c_sun_25 = 2.1 * Vc_sun_25; // µmol m - 2 s - 1 (unit ground area)
	double Jmax_c_sh_25 = 2.1 * Vc_sh_25; 
	
	double Jmax_c_sun = Jmax_bernacchi_f(in.leaf_temp, Jmax_c_sun_25);
	double Jmax_c_sh = Jmax_bernacchi_f(in.leaf_temp, Jmax_c_sh_25);
	out.jmax_c = Jmax_c_sun + Jmax_c_sh;

	double J_c_sun = J_bernacchi_f(Ic_sun, in.leaf_temp, Jmax_c_sun); //µmol m - 2 s - 1 (unit ground area)
	double J_c_sh = J_bernacchi_f(Ic_sh, in.leaf_temp, Jmax_c_sh);

	//5. canopy respiration
	double Rd_sun = Rd_bernacchi_f(in.leaf_temp)* out.LAI_sun; //µmol m - 2 s - 1 (unit ground area)
	double Rd_sh = Rd_bernacchi_f(in.leaf_temp)* out.LAI_sh;

	out.canopy_resp = (Rd_sun + Rd_sh) * 3600.0;
	
	//6. Coupled photosynthesis - stomatal conductance
	//6.1. estimate inputs (for solving cubic equation)
	//6.1.1 Gamma
	double Vomax_sun = Vomax_bernacchi_f(in.leaf_temp, Vc_sun_25);
	double Vomax_sh = Vomax_bernacchi_f(in.leaf_temp, Vc_sh_25);
	double gamma_sun = Gamma_bernacchi_f(in.leaf_temp, Vc_sun, Vomax_sun);
	double gamma_sh = Gamma_bernacchi_f(in.leaf_temp, Vc_sh, Vomax_sh);

	//calculate some outputs for to be used in VOCE modules
	out.kc = Kc_bernacchi_f(in.leaf_temp);
	out.ko = Ko_bernacchi_f(in.leaf_temp);
	out.oi = Oi_f(in.leaf_temp);
	out.comp = gamma_sun + gamma_sh;
	out.vcMax = Vcmax;
	out.jMax = Jmax_c_sun + Jmax_c_sh;
	
	//6.1.2 x1, x2 rubisco
	std::tuple<double, double> x1_x2_rub_sun = x_rubisco(in.leaf_temp, Vc_sun);
	std::tuple<double, double> x1_x2_rub_sh = x_rubisco(in.leaf_temp, Vc_sh);

	//6.1.2 x1, x2 electron
	std::tuple<double, double> x1_x2_el_sun = x_electron(J_c_sun, gamma_sun);
	std::tuple<double, double> x1_x2_el_sh = x_electron(J_c_sh, gamma_sh);

	// 6.1.3 g0, gm, gb
	double gb_sun = par.gb * out.LAI_sun; //mol m-2 s-1 bar-1 per unit ground area
	double gb_sh = par.gb * out.LAI_sh;
	double g0_sun = par.g0 * out.LAI_sun;
	double g0_sh = par.g0 * out.LAI_sh;
	double gm_t = gm_bernacchi_f(in.leaf_temp, par.gm_25);
	double gm_sun = gm_t * out.LAI_sun;
	double gm_sh = gm_t * out.LAI_sh;

	if (in.global_rad <= 0.0)
	{
		//handle cases where no photosynthesis can occur
		out.canopy_gross_photos = 0.0;
		out.canopy_net_photos = out.canopy_gross_photos - out.canopy_resp;
		out.gs_sun = g0_sun;
		out.gs_sh = g0_sh;
	}
	else
	{
		//6.1.4 fVPD
		double fVPD = fVPD_f(in.VPD);

		//6.2 calculate lumped coeffs (sun/shade)
		Lumped_Coeffs lumped_rub_sun = calculate_lumped_coeffs(std::get<0>(x1_x2_rub_sun), std::get<1>(x1_x2_rub_sun), fVPD, in.Ca, gamma_sun, Rd_sun, g0_sun, gm_sun, gb_sun);
		Lumped_Coeffs lumped_el_sun = calculate_lumped_coeffs(std::get<0>(x1_x2_el_sun), std::get<1>(x1_x2_el_sun), fVPD, in.Ca, gamma_sun, Rd_sun, g0_sun, gm_sun, gb_sun);

		Lumped_Coeffs lumped_rub_sh = calculate_lumped_coeffs(std::get<0>(x1_x2_rub_sh), std::get<1>(x1_x2_rub_sh), fVPD, in.Ca, gamma_sh, Rd_sh, g0_sh, gm_sh, gb_sh);
		Lumped_Coeffs lumped_el_sh = calculate_lumped_coeffs(std::get<0>(x1_x2_el_sh), std::get<1>(x1_x2_el_sh), fVPD, in.Ca, gamma_sh, Rd_sh, g0_sh, gm_sh, gb_sh);

		//6.3 calculate assimilation
		double A_rub_sun = A1_f(lumped_rub_sun); //µmol CO2 m-2 s-1 (unit ground area)
		double A_el_sun = A1_f(lumped_el_sun);

		double A_rub_sh = A1_f(lumped_rub_sh); //µmol CO2 m-2 s-1 (unit ground area)
		double A_el_sh = A1_f(lumped_el_sh);

		double A_sun = std::fmin(A_rub_sun * in.fO3 * in.fls, A_el_sun);
		double A_sh = std::fmin(A_rub_sh * in.fO3 * in.fls, A_el_sh);

		out.canopy_net_photos = (A_sun + A_sh) * 3600.0;
		out.canopy_gross_photos = out.canopy_net_photos + out.canopy_resp;

		//6.4 derive stomatal conductance
		//6.4.1 determine whether photosynthesis is rubisco or electron limited
		double x1_sun;
		double x2_sun;
		if (A_sun == A_el_sun)
		{
			x1_sun = std::get<0>(x1_x2_el_sun);
			x2_sun = std::get<1>(x1_x2_el_sun);
		}
		else
		{
			x1_sun = std::get<0>(x1_x2_rub_sun);
			x2_sun = std::get<1>(x1_x2_rub_sun);
		}
		double x1_sh;
		double x2_sh;
		if (A_sh == A_el_sh)
		{
			x1_sh = std::get<0>(x1_x2_el_sh);
			x2_sh = std::get<1>(x1_x2_el_sh);
		}
		else
		{
			x1_sh = std::get<0>(x1_x2_rub_sh);
			x2_sh = std::get<1>(x1_x2_rub_sh);
		}
		//6.4.2 gs
		out.gs_sun = derive_gs_f(A_sun, x1_sun, x2_sun, gamma_sun, Rd_sun, gm_sun, fVPD, par.g0);
		out.gs_sh = derive_gs_f(A_sh, x1_sh, x2_sh, gamma_sh, Rd_sh, gm_sh, fVPD, par.g0);
	}	
		
	return out;
}

#pragma endregion Model composition
	
	