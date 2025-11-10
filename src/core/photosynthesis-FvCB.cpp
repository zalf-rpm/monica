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

#include "photosynthesis-FvCB.h"

#include <tuple>
#include <cmath>

#include "tools/helper.h"

using namespace FvCB;
using namespace Tools;
using namespace std;


// FS: What is the source of these values?
std::map<FvCB_Model_Consts, double> FvCB::c_bernacchi = {{Rd, 18.72},{Vcmax, 26.35},{Vomax, 22.98},{Gamma, 19.02},{Kc, 38.05},{Ko, 20.30},{Jmax, 17.57}}; //dimensionless
std::map<FvCB_Model_Consts, double> FvCB::deltaH_bernacchi = {{Rd, 46.39},{Vcmax, 65.33},{Vomax, 60.11},{Gamma, 37.83},{Kc, 79.43},{Ko, 36.38},{Jmax, 43.54}}; //kJ mol - 1

/**
 * @brief estimate fraction of diffuse radiation (hourly)
 * 
 * Spitters et al. (1986) Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis.
 * Part I. Components of incoming radiation. https://doi.org/10.1016/0168-1923(86)90060-2
 * 
 * original source seems to be de Jong 1980 p.55 (in Dutch): Een karakterisering van de zonnestraling in Nederland
 * Doctoraalverslag Vakgroep Fysische Aspecten van de Gebouwde Omgeving afd. Bouwkunde en Vakgroep Warmte- en Stromingstechnieken afd,
 * Werktuigbouwkunde, Technische Hogeschool (Techn. Univ.), Eindhoven, Netherlands (1980), p. 97 + 67
 * 
 * (requires hourly inputs!)
 * @param globrad hourly global irradiance [W m-2]
 * @param extra_terr_rad hourly extra-terrestrial radiation [W m-2]
 * @param solar_elev hourly solar elevation angle [rad]
 * @return hourly fraction of diffuse radiation 
 */
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


// FS: Where are these eqautions from? Apparently not https://doi.org/10.1016/0168-1923(86)90061-4, but maybe something related? Multi-layer canopy model ???

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
    kb = 0.5 / sin(solar_elev); // FS: https://doi.org/10.1016/0168-1923(86)90061-4 eq. 7; Goudriaan, 1977, 1982 ? https://edepot.wur.nl/166537 https://edepot.wur.nl/167315
  }
  return I_dif * (1 - rho_cd) * (1 - exp(-(k1_d + kb)*LAI)) * k1_d / (k1_d + kb); // FS: is this similar to canopy model https://doi.org/10.1016/0168-1923(86)90061-4 eq. 25  ?
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

/**
 * @brief generic temperature response function
 * 
 * source seems to be Bernachhi et al. 2003 https://doi.org/10.1046/j.0016-8025.2003.01050.x eq. 9 or https://doi.org/10.1111/j.1365-3040.2001.00668.x eq. 8
 * 
 * apply like this: Parameter = Parameter25 * Tresp_bernacchi_f(c, deltaH, T[°C])
 * "Parameter25 is the absolute value of the parameter at 25 °C."
 * "The terms c and ΔHa are fit to the values for each of the six parameters measured at a range of temperatures."
 * "Use of this equation assumes that the activity will continue to increase exponentially with temperature."
 * For parameters that decrease at higher temperatures, an adjusted equiation is needed instead (e.g https://doi.org/10.1046/j.0016-8025.2003.01050.x eq. 10).
 * 
 * @param c       scaling constant
 * @param deltaH  energy of activation
 * @param leafT   leaf Temperature [°C]
 * @return Temperature response factor 
 */
double Tresp_bernacchi_f(double c, double deltaH, double leafT)
{
  double Tk = leafT + 273; // FS: [°C] -> [K]
  double R = 8.314472 * pow(10, -3); //kJ K - 1 mol - 1
  return exp(c - deltaH / (R * Tk));
}

/**
 * @brief maximum rate of carboxylation with Bernacchi temperature response
 * 
 * source seems to be https://doi.org/10.1046/j.0016-8025.2003.01050.x eq. 9 or https://doi.org/10.1111/j.1365-3040.2001.00668.x eq. 8
 * 
 * @param leafT     leaf Temperature [°C]
 * @param Vcmax_25  maximum rate of carboxylation at 25°C
 * @return maximum rate of carboxylation
 */
double FvCB::Vcmax_bernacchi_f(double leafT, double Vcmax_25)
{
  return Vcmax_25 * Tresp_bernacchi_f(c_bernacchi[Vcmax], deltaH_bernacchi[Vcmax], leafT);
}

/**
 * @brief maximum rate of electron transport with Bernacchi temperature response
 * 
 * @param leafT   leaf Temperature [°C]
 * @param Jmax_25 maximum rate of electron transport that the leaf can support at 25°C
 * @return maximum rate of electron transport
 */
double FvCB::Jmax_bernacchi_f(double leafT, double Jmax_25)
{
  return Jmax_25 * Tresp_bernacchi_f(c_bernacchi[Jmax], deltaH_bernacchi[Jmax], leafT);
}

/**
 * @brief 
 * 
 * @param Q 
 * @param leafT leaf Temperature [°C]
 * @param Jmax  actual electron transport capacity (unit leaf area) [J�mol m-2 s-1]
 * @return double 
 */
double J_bernacchi_f(double Q, double leafT, double Jmax)
{
  double alfa = 0.85; //total leaf absorbance 
  double beta = 0.5; //fraction of absorbed quanta reaching PSII
  double theta_ps2 = 0.76 + 0.018 * leafT - 3.7 * pow(10, -4) * pow(leafT, 2);
  double phi_ps2max = 0.352 + 0.022 *leafT - 3.4 * pow(10, -4) * pow(leafT, 2);
  double Q2 = Q * alfa * phi_ps2max * beta;

  double numerator = Q2 + Jmax - sqrt(pow((Q2 + Jmax), 2) - 4 * theta_ps2 * Q2 * Jmax);
  double denominator = 2 * theta_ps2;
  return numerator / denominator;
}

/**
 * @brief 
 * 
 * @param Q     :FS; What is Q? Maybe relation between maximum electron transport rate and Rubisco saturated rate of carboxylation?
 * @param Jmax  actual electron transport capacity (unit leaf area) [�mol m-2 s-1]
 * @return double 
 */
double J_grote_f(double Q, double Jmax)
{
  double species_THETA = 0.85; //!< curvature parameter
  const double tmp_var = ((Q + Jmax) * (Q + Jmax)) - (4.0 * species_THETA * Q * Jmax);
  // FS: fw seems to be Felix Wiss (=Felix Wiß, now Felix Havermann)
  // fw: In Grote et al. 2014 tmp_var is stated as the inverse sqrt even though it is only the sqrt // FS: Which publication is this? Maybe this one https://doi.org/10.1111/pce.12326 ? If so, in which eq.?
  double  jj = tmp_var > 0.0 ? (Q + Jmax - sqrt(tmp_var)) / (2.0 * species_THETA) : 0.0;
  return jj; //FS: electron provision (unit leaf area) [umol m-2 s-1]
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

/**
 * @brief 
 * 
 * @param leafT 
 * @return double 
 */
double Ko_bernacchi_f(double leafT)
{
  return Tresp_bernacchi_f(c_bernacchi[Ko], deltaH_bernacchi[Ko], leafT);
}

/**
 * @brief 
 * 
 * @param leafT 
 * @return double 
 */
double Oi_f(double leafT)
{
  double T1 = 1.3087 * pow(10, -3) * leafT;
  double T2 = 2.5603 * pow(10, -5) * pow(leafT, 2);
  double T3 = 2.1441 * pow(10, -7) * pow(leafT, 3);
  return 210 * (4.7 * pow(10, -2) - T1 + T2 - T3) / (2.6934 * pow(10, -2));
}

/**
 * @brief 
 * 
 * @param leafT 
 * @param Vcmax maximum rate of carboxylation
 * @param Vomax 
 * @return double 
 */
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
// conductance functions

/**
 * @brief 
 * 
 * @param leafT leaf temperature [°C]
 * @param gm_25 
 * @return double 
 */
double gm_bernacchi_f(double leafT, double gm_25)
{
  double c = 20.0; //it should be put into constants data struct.
  double deltaHa = 49.6;//  ||
  double deltaHd = 437.4;// ||
  double deltaS = 1.4;//  ||
  double R = 0.008314;//  || kJ J - 1 mol - 1
    
  double Tk = leafT + 273.15; // FS: [°C] -> [K]

  double numerator = exp(c - deltaHa / (R * Tk));
  double denominator = 1 + exp((deltaS * Tk - deltaHd) / (R * Tk));

  return gm_25 * numerator / denominator;
}


#pragma endregion conductance functions

#pragma region 
//Coupled photosynthesis-stomatal conductance
//Yin, Struik, 2009. NJAS 57 (2009) 27-38

/**
 * @brief empirical function for the effect of leaf-to-air vapour pressure difference (VPD)
 * 
 * Yin, Struik, 2009. NJAS 57 (2009) 27-38 (https://doi.org/10.1016/j.njas.2009.07.001) eq. 15a
 * 
 * @param VPD leaf-to-air vapour pressure difference [kPa]
 * @return Factor for describing the effect of leaf-to-air vapour difference on gs
 */
double fVPD_f(double VPD)
{
  // FS: a1 and b1 are empirical constants
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
  //double x1 = J / 4.5;
  //double x2 = 10.5 / 4.5 * gamma;

  double x1 = J / 4.0;
  double x2 = 2 * gamma;

  return std::make_tuple(x1, x2);
}

// coefficients from Yin, Struik, 2009. NJAS 57 (2009) 27-38 (https://doi.org/10.1016/j.njas.2009.07.001) Appendix B. Lumped coefficients in Eq. (19) for the coupled
// C3 photosynthesis and diffusional conductance model
struct Lumped_Coeffs {
  double p{ 0.0 };
  double Q{ 0.0 };
  double psi{ 0.0 };
};

/**
 * @brief 
 * 
 * Yin, Struik, 2009. NJAS 57 (2009) 27-38 (https://doi.org/10.1016/j.njas.2009.07.001) Appendix B. Lumped coefficients in Eq. (19) for the coupled 
 * C3 photosynthesis and diffusional conductance model
 * 
 * @param x1 
 * @param x2 
 * @param fVPD  Factor for describing the effect of leaf-to-air vapour difference on gs [-]
 * @param Ca    ambient CO2 partial pressure, [μbar or μmol mol-1]
 * @param gamma Cc-based CO2 compensation point in the absence of Rd [μbar]
 * @param Rd    Day respiration (respiratory CO2 release other than by photorespiration) [μmol CO2 m−2 s−1]
 * @param g0    Residual stomatal conductance when irradiance approaches zero [mol m-2 s-1 bar-1]
 * @param gm_C3 Mesophyll diffusion conductance (C3) [mol m−2 s−1 bar−1]
 * @param gb    boundary layer conductance, [mol m-2 s-1 bar-1]
 * @return Lumped_Coeffs 
 */
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
  // lumped_coeffs.psi = acos(U / sqrt(pow(lumped_coeffs.Q, 3)));
  double acos_arg = U / sqrt(pow(lumped_coeffs.Q, 3)); // FS: argument of acos should be in intervall [-1, 1]; otherwise undefined behaviour resulting in nan
  if ((acos_arg < (-1. - FvCB::epsilon)) || (acos_arg > (1. + FvCB::epsilon))) // FS: stop in case of too small or too big values (something must have gone wrong before)
  {
    throw runtime_error("hourly FvCB C3 lumped coeffs psi calculation failed!");
  }
  else
  {
    acos_arg = min(max(acos_arg, -1.), 1.); // FS: clip to intervall [-1, 1]
  }
  lumped_coeffs.psi = acos(acos_arg);

  return lumped_coeffs;
}
#pragma endregion Lumped coefficients C3


#pragma region
// Lumped Coefficients C4


// coefficients from Yin, Struik, 2009. NJAS 57 (2009) 27-38 (https://doi.org/10.1016/j.njas.2009.07.001) Appendix C. Lumped coefficients in Eq. (19) for the coupled
// C4 photosynthesis and diffusional conductance model
struct Lumped_Coeffs_C4 {
  double p{ 0.0 };
  double Q{ 0.0 };
  double psi{ 0.0 };};

/**
 * @brief 
 * 
 * Yin, Struik, 2009. NJAS 57 (2009) 27-38 (https://doi.org/10.1016/j.njas.2009.07.001) Appendix C. Lumped coefficients in Eq. (19) for the coupled 
 * C4 photosynthesis and diffusional conductance model
 * 
 * @param x1 
 * @param x2 
 * @param x3 
 * @param fVPD        Factor for describing the effect of leaf-to-air vapour difference on gs [-]
 * 
 * @param Cs_s        Cs-based CO2 compensation point in the absence of Rd (=Table 1 Cs*) [μbar]
 * 
 * @param Ca          Ambient air CO2 partial pressure [μbar or μmol mol−1]
 * @param gamma_lc_s  Half of the reciprocal of Sc/o [bar bar−1] (Sc/o Relative CO2/O2 specificity factor for Rubisco [bar bar−1])
 * according to Table 2: Sc/o25 [bar bar−1] = 2800 for C3 (orig. source: https://doi.org/10.1104/pp.008250) and 2590 for C4 (orig. source: https://doi.org/10.1071/9780643103405)
 * With Bernachhi T response this could maybe be used to calculate Sc/o, and then gamma_lc_s?
 * 
 * @param Rd          Day respiration (respiratory CO2 release other than by photorespiration) [μmol CO2 m−2 s−1]
 * 
 * @param g0          Residual stomatal conductance when irradiance approaches zero [mol m−2 s−1 bar−1]
 * @param gb          Boundary-layer conductance [mol m−2 s−1 bar−1]
 * 
 * @param gbs         Bundle-sheath conductance [mol m−2 s−1 bar−1]
 * 
 * @param kp          Initial carboxylation efficiency of the PEP carboxylase [mol m−2 s−1 bar−1]
 * @param Vpmax       Maximum PEP carboxylation rate [μmol CO2 m−2 s−1]
 * @param Ci          Intercellular CO2 partial pressure [μbar]
 * @param Oi          Intercellular oxygen partial pressure [μbar]
 * @param Rm          Day respiration in the mesophyll [μmol CO2 m−2 s−1]
 * @param alpha       fraction of O2 evolution occurring in the bundle sheath. See text on p.34 after eq. 24: "For maize and sorghum, alpha will be zero whereas it will approach or
 * even exceed 0.5 in other cases"
 * 
 * @return Lumped_Coeffs_C4 
 */
Lumped_Coeffs_C4 calculate_lumped_coeffs_C4(double x1, double x2, double x3, double fVPD, double Cs_s, double Ca, double gamma_lc_s, double Rd, double g0, double gb,
double kp, double Vpmax, double Ci, double gbs, double Oi, double Rm, double alpha) {
  Lumped_Coeffs_C4 lumped_coeffs_C4;
  // throw runtime_error("Not implemented yet");

  // Appendix C: ... x1, x2 and x3 are defined in the texts following Eq. (23),
  // a and b are defined in the equations below Eq. (22)

  double Vp = min(kp*Ci, Vpmax);
  //a, b
  double a;
  double b;
  if (Vp>=Vpmax) {
    a = 1;
    b = 0;
  }
  else {
    a = 1+(kp/gbs);
    b = Vpmax;
  }
  // alternative way of calculating a and b if Vp is calculated using eq. 20d
  // double a = 1;
  // double b = VpJ2;


  //d
  double d = g0 * Ca - g0 * Cs_s + fVPD * Rd;

  //m
  double m = fVPD - (g0/gb);



  //f
  double f = (b - Rm - gamma_lc_s * Oi* gbs) * x1 * d + a * gbs*x1*Ca*d;

  //g
  double g = (b - Rm - gamma_lc_s * Oi* gbs) * x1 * m - (((alpha * gamma_lc_s) / 0.047) +1) * x1 * d
  + a* gbs*x1* (Ca*m - (d/gb) - (Ca - Cs_s));

  //h
  double h = - ((((alpha * gamma_lc_s) / 0.047) +1) * x1 * m + ((a*gbs*x1*(m-1))/gb));

  //i
  double i = (b - Rm + gbs*x3 + x2*gbs*Oi) * d + a*gbs*Ca * d;

  //j
  double j = (b - Rm + gbs*x3 + x2*gbs*Oi) * m + (((alpha * x2) / 0.047) -1) * d
  + a* gbs* (Ca*m - (d/gb) - (Ca - Cs_s));

  //l
  double l = (((alpha * x2) / 0.047) -1) * m - ((a*gbs*(m-1))/gb);


  //q
  double q = (i + j*Rd - g) / l;

  //p
  lumped_coeffs_C4.p = (j - (h - l*Rd)) / l;

    //r
  double r = - (f - i*Rd) / l;


  //U
  double U = (2 * pow(lumped_coeffs_C4.p, 3) - 9 * lumped_coeffs_C4.p*q + 27 * r) / 54;

  //Q
  lumped_coeffs_C4.Q = (pow(lumped_coeffs_C4.p, 2) - 3 * q) / 9;

  //psi
  lumped_coeffs_C4.psi = acos(U / sqrt(pow(lumped_coeffs_C4.Q, 3)));

  return lumped_coeffs_C4;
}

#pragma endregion Lumped coefficients C4


#pragma region
//Cubic equation solutions

/**
 * @brief Analytical solution of a cubic equation A1
 * 
 * Yin, Struik, 2009. NJAS 57 (2009) 27-38 (https://doi.org/10.1016/j.njas.2009.07.001) Appendix A. Analytical solution of a cubic equation—Eq. (19)
 * 
 * @param lumped_coeffs 
 * @return double 
 */
double A1_f(Lumped_Coeffs lumped_coeffs)
{
  return -2 * sqrt(lumped_coeffs.Q) * cos(lumped_coeffs.psi / 3) - lumped_coeffs.p / 3;
}

/**
 * @brief Analytical solution of a cubic equation A2
 * 
 * Yin, Struik, 2009. NJAS 57 (2009) 27-38 (https://doi.org/10.1016/j.njas.2009.07.001) Appendix A. Analytical solution of a cubic equation—Eq. (19)
 * 
 * @param lumped_coeffs 
 * @return double 
 */
double A2_f(Lumped_Coeffs lumped_coeffs)
{
  return -2 * sqrt(lumped_coeffs.Q) * cos((lumped_coeffs.psi + 2 * M_PI) / 3) - lumped_coeffs.p / 3;
}

/**
 * @brief Analytical solution of a cubic equation A3
 * 
 * Yin, Struik, 2009. NJAS 57 (2009) 27-38 (https://doi.org/10.1016/j.njas.2009.07.001) Appendix A. Analytical solution of a cubic equation—Eq. (19)
 * 
 * @param lumped_coeffs 
 * @return double 
 */
double A3_f(Lumped_Coeffs lumped_coeffs)
{
  return -2 * sqrt(lumped_coeffs.Q) * cos((lumped_coeffs.psi + 4 * M_PI) / 3) - lumped_coeffs.p / 3;
}
#pragma endregion Cubic equation solutions

std::tuple<double, double, double> derive_ci_cc_gs_f(double A, double x1, double x2, double gamma, double Rd, double gm, double fVPD, double g0)
{
  double numerator = -(A * x2 + Rd * x2 + gamma * x1);
  double denominator = A + Rd - x1;
  double Cc = numerator / denominator;
  double Ci = Cc + (A / gm);
  double Ci_star = gamma - Rd / gm;
  double gs = g0 + (A + Rd) / (Ci - Ci_star) * fVPD;
  return std::make_tuple(Ci, Cc, gs);
}

/**
 * @brief 
 * 
 * @param A 
 * @param Rd 
 * @param gamma CO2 compensation point ... ?
 * @param Cc Chloroplast CO2 partial pressure
 * @return double 
 */
double derive_jv_f(double A, double Rd, double gamma, double Cc)
{
  double numerator = (A + Rd)*(Cc + 10.5 / 4.5 * gamma)*4.5;
  double denominator = (Cc - gamma);
  return numerator/denominator;
}
#pragma endregion Coupled photosynthesis-stomatal conductance

#ifdef TEST_FVCB_HOURLY_OUTPUT
#include <fstream>
ostream& FvCB::tout(bool closeFile)
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
    out.open("fvcb_hourly_data.csv");
    failed = out.fail();
    (failed ? cout : out) <<
      "iso-date"
      ",hour"
      ",crop-name"
      ",co2"
      ",leaf_temp"
      ",in:LAI_sun"
      ",in:LAI_sh"
      ",out:Ic_sun"
      ",out:Ic_sh"
      ",out:A_rub_sun"
      ",out:A_el_sun"
      ",out:A_rub_sh"
      ",out:A_el_sh"
      ",out.sunlit.ci"
      ",out.sunlit.cc"
      ",out.shaded.ci"
      ",out.shaded.cc"
      ",gb_sun"
      ",gm_sun"
      ",gb_sh"
      ",gm_sh"
      ",out.sunlit.gs"
      ",out.shaded.gs"
      ",A_sun"
      ",Rd_sun"
      ",gamma_sun"
      << endl;

    init = true;
  }

  return failed ? cout : out;
}
#endif

#pragma region
//Model composition (C3)
FvCB_canopy_hourly_out FvCB::FvCB_canopy_hourly_C3(FvCB_canopy_hourly_in in, FvCB_canopy_hourly_params par)
{
  FvCB_canopy_hourly_out out;
  //0. initialize VOCE out
  out.sunlit.vcMax = 0.0;
  out.sunlit.jMax = 0.0;
  out.sunlit.jj = 0.0;

  out.shaded.vcMax = 0.0;
  out.shaded.jMax = 0.0;
  out.shaded.jj = 0.0;

  out.sunlit.jj1000 = 0.0;
  out.shaded.jj1000 = 0.0;
  
  out.sunlit.jv = 0.0;
  out.shaded.jv = 0.0;

  //1. calculate diffuse and direct radiation
  double diffuse_fraction = diffuse_fraction_hourly_f(in.global_rad, in.extra_terr_rad, in.solar_el);
  double hourly_diffuse_rad = in.global_rad * diffuse_fraction;
  double hourly_direct_rad = in.global_rad - hourly_diffuse_rad;

  /* FS: for agri-pv: test if adjusting hourly direct and diffuse radiation has any impact at all  !!! debug
  hourly_diffuse_rad *= 0.7;  // !!! debug
  hourly_direct_rad *= 0.;    // !!! debug
  //*/

  // FS: convert [MJ m-2 h-1] -> [W m-2] -> [μmol m-2 s-1] -> [μmol m-2 s-1 PAR]
  // 1 [MJ m-2 h-1] = pow(10, 6) / 3600.0 [W m-2]; 1 [W m-2] = 4.56 [μmol m-2 s-1]; PAR = 0.45 * global radiation 
  double inst_diff_rad = hourly_diffuse_rad * pow(10, 6) / 3600.0 * 4.56 * 0.45;  //[μmol m-2 s-1 PAR] (unit ground area)
  double inst_dir_rad = hourly_direct_rad * pow(10, 6) / 3600.0 * 4.56 * 0.45;    //[μmol m-2 s-1 PAR] (unit ground area)
  
  //2. calculate Radiation absorbed by sunlit / shaded canopy
  double Ic_sun = Ic_sun_f(inst_dir_rad, inst_diff_rad, in.solar_el, in.LAI); //�mol m - 2 s - 1 (unit ground area)
  double Ic_sh = Ic_shade_f(inst_dir_rad, inst_diff_rad, in.solar_el, in.LAI); //�mol m - 2 s - 1 (unit ground area)

  //2.1. calculate sunlit/shaded LAI
  std::tuple<double, double> sun_shade_LAI = LAI_sunlit_shaded_f(in.LAI, in.solar_el);
  out.sunlit.LAI = std::get<0>(sun_shade_LAI);
  out.shaded.LAI = std::get<1>(sun_shade_LAI);

#ifdef TEST_FVCB_HOURLY_OUTPUT
  tout()
    << "," << in.leaf_temp
    << "," << out.sunlit.LAI
    << "," << out.shaded.LAI
    << "," << Ic_sun
    << "," << Ic_sh;
#endif

  //For each fraction :
  //-------------------
  //3. canopy photosynthetic capacity
  double Vcmax = Vcmax_bernacchi_f(in.leaf_temp, par.Vcmax_25);
  double Vcmax_25 = Vcmax_bernacchi_f(25.0, par.Vcmax_25); //the value at 25°C calculated with bernacchi slightly deviates from par.Vcmax_25

  //test
  //Vcmax = 100.0;
    
  double Vc_25 = canopy_ps_capacity_f(in.LAI, Vcmax_25, par.kn); //�mol m - 2 s - 1 (unit ground area)
  double Vc_sun_25 = canopy_ps_capacity_sunlit_f(in.LAI, in.solar_el, Vcmax_25, par.kn);
  double Vc_sh_25 = Vc_25 - Vc_sun_25;
  double Vc = canopy_ps_capacity_f(in.LAI, Vcmax, par.kn); 
  double Vc_sun = canopy_ps_capacity_sunlit_f(in.LAI, in.solar_el, Vcmax, par.kn);
  double Vc_sh = Vc - Vc_sun;
  //cout << Vc << endl;

  //4. canopy electron transport capacity
  double Jmax_c_sun_25 = 1.6 * Vc_sun_25; // �mol m - 2 s - 1 (unit ground area)
  double Jmax_c_sh_25 = 1.6 * Vc_sh_25; 
  
  double Jmax_c_sun = Jmax_bernacchi_f(in.leaf_temp, Jmax_c_sun_25);
  double Jmax_c_sh = Jmax_bernacchi_f(in.leaf_temp, Jmax_c_sh_25);
  out.jmax_c = Jmax_c_sun + Jmax_c_sh;

  double J_c_sun = J_bernacchi_f(Ic_sun, in.leaf_temp, Jmax_c_sun); //�mol m - 2 s - 1 (unit ground area)
  double J_c_sh = J_bernacchi_f(Ic_sh, in.leaf_temp, Jmax_c_sh);
  //double J_c_sun = J_grote_f(Ic_sun, Jmax_c_sun); //�mol m - 2 s - 1 (unit ground area)
  //double J_c_sh = J_grote_f(Ic_sh, Jmax_c_sh);
  
  //5. canopy respiration
  double Rd_sun = Rd_bernacchi_f(in.leaf_temp)* out.sunlit.LAI; //�mol m - 2 s - 1 (unit ground area)
  double Rd_sh = Rd_bernacchi_f(in.leaf_temp)* out.shaded.LAI;

  out.canopy_resp = (Rd_sun + Rd_sh) * 3600.0;
  
  //6. Coupled photosynthesis - stomatal conductance
  //6.1. estimate inputs (for solving cubic equation)
  //6.1.1 Gamma
  double Vomax_sun = Vomax_bernacchi_f(in.leaf_temp, Vc_sun_25);
  double Vomax_sh = Vomax_bernacchi_f(in.leaf_temp, Vc_sh_25);
  double gamma_sun = Gamma_bernacchi_f(in.leaf_temp, Vc_sun, Vomax_sun);
  double gamma_sh = Gamma_bernacchi_f(in.leaf_temp, Vc_sh, Vomax_sh);

  //calculate some outputs to be used in VOCE modules
  out.sunlit.kc = out.shaded.kc = Kc_bernacchi_f(in.leaf_temp);
  out.sunlit.ko = out.shaded.ko = Ko_bernacchi_f(in.leaf_temp);
  out.sunlit.oi = out.shaded.oi = Oi_f(in.leaf_temp);
  out.sunlit.comp = gamma_sun; 
  out.shaded.comp = gamma_sh;
  //out.sunlit.rad = Ic_sun / 4.56 / 0.45 / 0.860; //W m - 2 (glob rad, 1 W m-2 = 4.56 �mol m-2 s-1; PAR = 0.45 * global radiation, 0.860 = adsorberd fraction in JJV model)
  //out.shaded.rad = Ic_sh / 4.56 / 0.45 / 0.860; //W m-2
  double hourly_globrad = in.global_rad * pow(10, 6) / 3600.0; //W m - 2
  out.sunlit.rad = hourly_globrad > 0 ? hourly_globrad * Ic_sun / (Ic_sun + Ic_sh): 0.0;
  out.shaded.rad = hourly_globrad > 0 ? hourly_globrad * Ic_sh / (Ic_sun + Ic_sh) : 0.0;

  if (out.sunlit.LAI > 0)
  {
    out.sunlit.vcMax = Vc_sun / out.sunlit.LAI; //Vcmax;
    out.sunlit.jMax = Jmax_c_sun / out.sunlit.LAI; //Jmax_bernacchi_f(in.leaf_temp, Vcmax_25*2.1);
    out.sunlit.jj = J_c_sun / out.sunlit.LAI;
    out.sunlit.jj1000 = J_bernacchi_f(1000, in.leaf_temp, out.sunlit.jMax);
  }	
  if (out.shaded.LAI > 0)
  {
    out.shaded.vcMax = Vc_sh / out.shaded.LAI;    // Vcmax;
    out.shaded.jMax = Jmax_c_sh / out.shaded.LAI; // Jmax_bernacchi_f(in.leaf_temp, Vcmax_25*2.1);
    out.shaded.jj = J_c_sh / out.shaded.LAI;
    out.shaded.jj1000 = J_bernacchi_f(1000, in.leaf_temp, out.shaded.jMax);
  }
  
  //6.1.2 x1, x2 rubisco
  std::tuple<double, double> x1_x2_rub_sun = x_rubisco(in.leaf_temp, Vc_sun);
  std::tuple<double, double> x1_x2_rub_sh = x_rubisco(in.leaf_temp, Vc_sh);

  //6.1.2 x1, x2 electron
  std::tuple<double, double> x1_x2_el_sun = x_electron(J_c_sun, gamma_sun);
  std::tuple<double, double> x1_x2_el_sh = x_electron(J_c_sh, gamma_sh);

  // 6.1.3 g0, gm, gb
  double gb_sun = par.gb * out.sunlit.LAI;  // mol m-2 s-1 bar-1 per unit ground area
  double gb_sh = par.gb * out.shaded.LAI;
  double g0_sun = par.g0 * out.sunlit.LAI;
  double g0_sh = par.g0 * out.shaded.LAI;
  double gm_t = 0.4;                        // gm_bernacchi_f(in.leaf_temp, par.gm_25); //TODO: check correctness of gm_bernacchi_f
  double gm_sun = gm_t * out.sunlit.LAI;
  double gm_sh = gm_t * out.shaded.LAI;

  if (in.global_rad <= 0.0)
  {
    //handle cases where no photosynthesis can occur
    out.canopy_gross_photos = 0.0;                                      // [μmol CO2 m-2 h-1]
    out.canopy_net_photos = out.canopy_gross_photos - out.canopy_resp;  // [μmol CO2 m-2 h-1]
    out.sunlit.gs = g0_sun;
    out.shaded.gs = g0_sh;
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
    double A_rub_sun = A1_f(lumped_rub_sun); //�mol CO2 m-2 s-1 (unit ground area)
    double A_el_sun = A1_f(lumped_el_sun);

    double A_rub_sh = A1_f(lumped_rub_sh); //�mol CO2 m-2 s-1 (unit ground area)
    double A_el_sh = A1_f(lumped_el_sh);

#ifdef TEST_FVCB_HOURLY_OUTPUT
    tout()
      << "," << A_rub_sun
      << "," << A_el_sun
      << "," << A_rub_sh
      << "," << A_el_sh;
#endif

    //double A_sun = std::fmin(A_rub_sun * in.fO3 * in.fls, A_el_sun);
    //double A_sh = std::fmin(A_rub_sh * in.fO3 * in.fls, A_el_sh);

    double A_sun = std::fmin(A_rub_sun, A_el_sun);
    double A_sh = std::fmin(A_rub_sh, A_el_sh);

    out.canopy_net_photos = (A_sun + A_sh) * 3600.0;                    // [μmol CO2 m-2 h-1]
    out.canopy_gross_photos = out.canopy_net_photos + out.canopy_resp;  // [μmol CO2 m-2 h-1]

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
    auto sun_ci_cc_gs = derive_ci_cc_gs_f(A_sun, x1_sun, x2_sun, gamma_sun, Rd_sun, gm_sun, fVPD, par.g0);
    out.sunlit.ci = get<0>(sun_ci_cc_gs);
    out.sunlit.cc = get<1>(sun_ci_cc_gs);
    out.sunlit.gs = get<2>(sun_ci_cc_gs);
    auto sh_ci_cc_gs = derive_ci_cc_gs_f(A_sh, x1_sh, x2_sh, gamma_sh, Rd_sh, gm_sh, fVPD, par.g0);
    out.shaded.ci = get<0>(sh_ci_cc_gs);
    out.shaded.cc = get<0>(sh_ci_cc_gs);
    out.shaded.gs = get<2>(sh_ci_cc_gs);

#ifdef TEST_FVCB_HOURLY_OUTPUT
    tout()
      << "," << out.sunlit.ci
      << "," << out.sunlit.cc
      << "," << out.shaded.ci
      << "," << out.shaded.cc
      << "," << gb_sun
      << "," << gm_sun
      << "," << gb_sh
      << "," << gm_sh
      << "," << out.sunlit.gs
      << "," << out.shaded.gs
      << "," << A_sun
      << "," << Rd_sun
      << "," << gamma_sun;
#endif

    //6.5 derive jv
    if (out.sunlit.LAI > 0)
    {
      out.sunlit.jv = derive_jv_f(A_sun, Rd_sun, gamma_sun, get<1>(sun_ci_cc_gs)) / out.sunlit.LAI;
    }
    if (out.shaded.LAI > 0)
    {
      out.shaded.jv = derive_jv_f(A_sh, Rd_sh, gamma_sh, get<1>(sh_ci_cc_gs)) / out.shaded.LAI;
    }

  }	
  
  #ifdef TEST_FVCB_HOURLY_OUTPUT
    tout() << endl;
  #endif

  return out;
}

#pragma endregion Model composition
