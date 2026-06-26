#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include <assert.h>
#include <stdexcept>

using namespace std;

#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;  // M_PI = 4 * atan(1.)
#endif

namespace hPhoto {

const double eps = 1e-6; // machine epsilon


enum class unit {// supported units: MJpm2ps, Jpm2ps, Wpm2ps, umolpm2ps
    MJpm2ps = 0,     // MJ   m-2 h-1 PAR
    Jpm2ps,          // J    m-2 h-1 PAR
    Wpm2ps,          // W    m-2     PAR
    umolpm2ps        // µmol m-2 s-1 PAR
};
static_assert(
    static_cast<int>(unit::MJpm2ps)  == 0 &&
    static_cast<int>(unit::Jpm2ps)   == 1 &&
    static_cast<int>(unit::Wpm2ps)   == 2 &&
    static_cast<int>(unit::umolpm2ps)== 3,
    "out_unit order must match conversion pipeline"
);

/**
 * @brief 
 * 
 * @param value    input value to be converted [MJ m-2 h-1]
 * @param out_unit output unit (supported units: MJpm2ps, Jpm2ps, Wpm2ps, umolpm2ps). Default is hPhoto::unit::MJpm2ps.
 * @return double converted value
 */
double convert_MJpm2ps_to_unit(double value, hPhoto::unit out_unit);


/**
 * @brief estimate fraction of diffuse radiation (daily)
 * (requires daily inputs!)
 * 
 * Source: Spitters et al. 1986; original source seems to be de Jong 1980, p.79 (according to Spitters et al. 1986).
 * 
 * - Spitters CJT, Toussaint HAJM & Goudriaan J (1986). Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis. Part I. Components of incoming radiation. Agricultural and Forest Meteorology, 38(1-3), 217-229.
 *   https://doi.org/10.1016/0168-1923(86)90060-2
 * 
 * - de Jong JBRM (1980). Een karakterisering van de zonnestraling in Nederland. Doctoraalverslag Vakgroep Fysische Aspecten van de Gebouwde Omgeving afd. Bouwkunde en Vakgroep Warmte- en Stromingstechnieken afd, Werktuigbouwkunde, Technische Hogeschool (Techn. Univ.), Eindhoven, Netherlands (1980), p. 97 + 67 [in Dutch].
 * 
 * @param globrad        daily global irradiance [W m-2]
 * @param extra_terr_rad daily extra-terrestrial radiation [W m-2]
 * @param solar_elev     daily solar elevation angle [rad]
 * @return daily fraction of diffuse radiation 
 */
double diffuse_fraction_daily_f(double globrad, double extra_terr_rad, double solar_elev);


/**
 * @brief estimate fraction of diffuse radiation (hourly)
 * (requires hourly inputs!)
 * 
 * Source: Spitters et al. 1986; original source seems to be de Jong 1980, p.55 (according to Spitters et al. 1986).
 * A comparison using different models for the diffuse fraction of
 * the global insolation can be found in [2]_ in the context of Sweden.
 * 
 * - Spitters CJT, Toussaint HAJM & Goudriaan J (1986). Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis. Part I. Components of incoming radiation. Agricultural and Forest Meteorology, 38(1-3), 217-229.
 *   https://doi.org/10.1016/0168-1923(86)90060-2
 * 
 * - de Jong JBRM (1980). Een karakterisering van de zonnestraling in Nederland. Doctoraalverslag Vakgroep Fysische Aspecten van de Gebouwde Omgeving afd. Bouwkunde en Vakgroep Warmte- en Stromingstechnieken afd, Werktuigbouwkunde, Technische Hogeschool (Techn. Univ.), Eindhoven, Netherlands (1980), p. 97 + 67 [in Dutch].
 * 
 * - Ma Lu S, Zainali S, Stridh B, Avelin A, Amaducci S, Colauzzi M & Campana PE (2022). Photosynthetically active radiation decomposition models for agrivoltaic systems applications. Solar Energy, 244, 536-549.
 * https://doi.org/10.1016/j.solener.2022.05.046
 * 
 * @param globrad        hourly global irradiance [W m-2]
 * @param extra_terr_rad hourly extra-terrestrial radiation [W m-2]
 * @param solar_elev     hourly solar elevation angle [rad]
 * @return hourly fraction of diffuse radiation 
 */
double diffuse_fraction_hourly_f(double globrad, double extra_terr_rad, double solar_elev);

/**
 * @brief Circumsolar correction
 * 
 * Diffuse irradiance can only be assumed to be isotropic for uniform overcast sky conditions. For clear-sky conditions,
 * however, there usually is an anisotropy with higher intensity in the direction of the sun is caused by forward-scattering.
 * For some use-cases, it is advantageous to treat this circumsolar component (which is part of the diffuse irradiance) as
 * direct irradiance instead, leading to a reduced diffuse fraction.
 * 
 * See eq. 9 in:
 * - Spitters CJT, Toussaint HAJM & Goudriaan J (1986). Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis. Part I. Components of incoming radiation. Agricultural and Forest Meteorology, 38(1-3), 217-229.
 *   https://doi.org/10.1016/0168-1923(86)90060-2
 * 
 * @param diffuse_fraction (= diffuse irradiance / global radiation) [-]
 * @param solar            elevation angle [rad]
 * @return circumsolar correction factor
 */
double diffuse_fraction_cscor(double diffuse_fraction, double solar_elevation);


/**
 * @brief Wavelength adjustments for PAR region
 * 
 * When considering global radiation for crop modeling purposes, usually a broad wavelength spectrum (UV or UV-A, VIS, NIR, SWIR up to IR-B)
 * is used as input data. However, only the region around 400-700 nm are photosynthetically active radiation (PAR). However, depending on
 * sky conditions, the effects of scattering processes in the atmosphere can affect diffuse fraction in these wavebands to a different extent
 * than total global wavelength spectrum ( leading to a increased diffuse fraction in the PAR region, especially for clear-sky conditions).
 * 
 * See eq. 10 in:
 * - Spitters CJT, Toussaint HAJM & Goudriaan J (1986). Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis. Part I. Components of incoming radiation. Agricultural and Forest Meteorology, 38(1-3), 217-229.
 *   https://doi.org/10.1016/0168-1923(86)90060-2
 * 
 * @param diffuse fraction (= diffuse irradiance / global radiation) [-]
 * @return PAR wavelengths correction factor
 */
double diffuse_fraction_parcor(double diffuse_fraction);


struct PAR_radiation_result {
       double direct;       // direct irradiance [μmol m-2 s-1 PAR] (unit ground area)
       double diffuse;      // diffuse irradiance [μmol m-2 s-1 PAR] (unit ground area)
};
/**
 * @brief PAR radiation estimation (top of canopy direct and diffuse)
 * 
 * Optional corrections (cscor, pwcor) are often neglectes, as Spitters et al. 1986 suggest that they cancel out (if both are used).
 * However, in the Agrivoltaics context, these might be useful (FS personal insights while reading the original literature; the Ma Lu et al. 2022 paper suggests the same?)
 * 
 * see Spitters et al. 1986 for the original approach (including the optional corrections) and Ma Lu et al. for application in the Agrivoltaics context
 * 
 * - Spitters CJT,  Toussaint HAJM & Goudriaan J (1986). Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis. Part I. Components of incoming radiation. Agricultural and Forest Meteorology, 38(1-3), 217-229.
 *   https://doi.org/10.1016/0168-1923(86)90060-2
 * - Ma Lu S, Zainali S, Stridh B, Avelin A, Amaducci S, Colauzzi M & Campana, PE (2022). Photosynthetically active radiation decomposition models for agrivoltaic systems applications. Solar Energy, 244, 536-549.
 *   https://doi.org/10.1016/j.solener.2022.05.046
 * 
 * @param global_rad     global radiation [MJ m-2 h-1]
 * @param extra_terr_rad extra-terrestrial radiation [MJ m-2 h-1]
 * @param solar_el       solar elevation angle [rad]
 * @param cscor          circumsolar correction. Default is true (active).
 * @param parcor         PAR wavelength correction. Default is true (active).
 * @param parfrac        photosynthetically active fraction of the light spectrum. 0.5 according to Spitters 1986; 0.48 according to BF5 Sunshine sensor manual? Default is 0.45.
 * @param out_unit       output unit (supported units: MJpm2ps, Jpm2ps, Wpm2ps, umolpm2ps). Default is hPhoto::unit::MJpm2ps.
 * @return hPhoto::PAR_radiation_result direct [out_unit PAR], diffuse [out_unit PAR]
 */
PAR_radiation_result PAR_radiation(double global_rad, double extra_terr_rad, double solar_el, bool cscor=true, bool parcor=true, double parfrac=0.45, unit out_unit=unit::MJpm2ps);


/**
 * @brief SUCROS87-inspired Wageningen-style hourly canopy gross photosynthesis (has to be integrated over the canopy layers!).
 * 
 * Based on Spitters 1986 and Spitters et al. 1989 (SUCROS87) leaf and canopy photosynthesis. Can be used either with multiple canopy layers or with 3-point gaussian integration.
 * 
 * - Spitters CJT (1986). Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis Part II. Calculation of canopy photosynthesis. Agricultural and Forest Meteorology, 38(1-3), 231-242.
 *   https://doi.org/10.1016/0168-1923(86)90061-4
 * 
 * - Spitters CJT, van Keulen H & van Kraalingen DWG (1989). A simple and universal crop growth simulator: SUCROS87. In: R Rabbinge, SA Ward & HH van Laar (eds), Simulation and systems management in crop protection. Simulation monographs, Pudoc, Wageningen, pp. 147-181.
 *   https://edepot.wur.nl/171923
 * 
 * @param beta    solar elevation angle [rad].
 * @param L       partial cumulated leaf area index at various canopy depths (= LAIC from Spitters et al. 1989).
 *                  leaf area index [m2 m-2] * canopy layer depth [0...1, starting from the top]
 * @param I0_dr   direct PAR flux light intensity at the top of the canopy [J m-2 ground s-1] (=direct PAR irradiance on a horizontal plane).
 * @param I0_df   diffuse PAR flux light intensity at the top of the canopy [J m-2 ground s-1] (=diffuse PAR irradiance).
 * 
 * @param A_m     assimilation rate at light saturation [g CO2 m-2 leaf h-1] (=asymptote of light response curve). Temperature-dependent.
 * @param epsilon light-use efficiency [g CO2 J-1 absorbed] (=initial slope of light response curve). Temperature-dependent.
 *                  ARCWHEAT1: "dA/dI at I = 0".
 *                  MONICA: "transition between photosynthetic quantum use efficiency and light saturated photosynthesis".
 * @param k_df    empirical extinction coefficient for diffuse radiation. Default is 0.6.
 *                  0.60 for spring wheat, 0.65 for maize, 1.00 for potato and 0.69 for sugar beet, according to Spitters et al. (1989), p. 151 and pp. 178-180.
 *                  See Spitters et al. (1989), Chapter 4.1.4 "Crop species and site characteristics", pp. 171-172:
 *                  "Typical values of k are 0.4 to 0.7 for monocotyledons and 0.65 to 1.1 for broadleaved dicotyledons (Monteith, 1969).
 *                   The extinction coefficient can be estimated from measurements of PAR above and below a canopy with a known LAI (Equation 62 [I_L = (1 - rho) * I_0 *exp(-k * L)]),
 *                   making sure that PAR is measured rather than total global radiation. The extinction coefficient for total radiation is about 2/3 that of PAR.
 *                   The extinction coefficient is best measured under a uniform overcast sky; then all radiation is diffuse so that the extinction coefficient is not affected by solar elevation."
 *                  In WOFOST, this seems to be development-stage dependent, (https://github.com/ajwdewit/WOFOST_crop_parameters/blob/ec57fc0ddd3f0924b707ec4482e46fa987e8ee0a/wheat.yaml#L202).
 * @param sigma   scattering coefficient of single leaves and for visible radiation [-]. Default is 0.2.
 *                  See Spitters et al. (1989). In the order of 0.20. 0.20 for spring wheat, maize, potato, sugar beet.
 * @param kgpha   input (A_m, epsilon) and output unit in [kg ha-1] instead of [g m-2]. Default is false.
 * @param leaf_angle_integration_style style of the integration over all leaf angles. Default is 1.
 *                  0 = None (leads to overestimation according to Spitters 1986!);
 *                  1 = Spitters 1986, custom implementation, including Wageningen school implementations-inspired numerical safeguards;
 *                  2 = Spitters 1989, SUCROS87 implementation
 * @return hourly photosynthesis of the canopy layer dL [g CO2 m-2 ground h-1].
 */
double Spitters_canop_photo_dL(double beta, double L, double I0_dr, double I0_df, double A_m, double epsilon, double k_df=0.6, double sigma=0.2, bool kgpha=false, int leaf_angle_integration_style=1);


/**
 * @brief SUCROS87-inspired Wageningen-style hourly canopy gross photosynthesis.
 * 
 * Based on Spitters 1986 and Spitters et al. 1989 (SUCROS87) leaf and canopy photosynthesis, but with multiple canopy layers instead of 3-point gaussian integration.
 * 
 * - Spitters CJT (1986). Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis Part II. Calculation of canopy photosynthesis. Agricultural and Forest Meteorology, 38(1-3), 231-242.
 *   https://doi.org/10.1016/0168-1923(86)90061-4
 * 
 * - Spitters CJT, van Keulen H & van Kraalingen DWG (1989). A simple and universal crop growth simulator: SUCROS87. In: R Rabbinge, SA Ward & HH van Laar (eds), Simulation and systems management in crop protection. Simulation monographs, Pudoc, Wageningen, pp. 147-181.
 *   https://edepot.wur.nl/171923
 * 
 * @param beta    solar elevation angle [rad].
 * @param LAI     leaf area index [m2 m-2]
 * @param I0_dr   direct PAR flux light intensity at the top of the canopy [J m-2 ground s-1] (=direct PAR irradiance on a horizontal plane).
 * @param I0_df   diffuse PAR flux light intensity at the top of the canopy [J m-2 ground s-1] (=diffuse PAR irradiance).
 * @param A_m     assimilation rate at light saturation [g CO2 m-2 leaf h-1] (=asymptote of light response curve).
 * @param epsilon light-use efficiency [g CO2 J-1 absorbed] (=initial slope of light response curve).
 *                  arcwheat1: "dA/dI at I = 0".
 *                  monica: "transition between photosynthetic quantum use efficiency and light saturated photosynthesis".
 * @param k_df    empirical extinction coefficient for diffuse radiation. Default is 0.6.
 *                  0.60 for spring wheat, 0.65 for maize, 1.00 for potato and 0.69 for sugar beet, according to Spitters et al. (1989), p. 151 and pp. 178-180.
 *                  See Spitters et al. (1989), Chapter 4.1.4 "Crop species and site characteristics", pp. 171-172:
 *                  "Typical values of k are 0.4 to 0.7 for monocotyledons and 0.65 to 1.1 for broadleaved dicotyledons (Monteith, 1969).
 *                   The extinction coefficient can be estimated from measurements of PAR above and below a canopy with a known LAI (Equation 62 [I_L = (1 - rho) * I_0 *exp(-k * L)]),
 *                   making sure that PAR is measured rather than total global radiation. The extinction coefficient for total radiation is about 2/3 that of PAR.
 *                   The extinction coefficient is best measured under a uniform overcast sky; then all radiation is diffuse so that the extinction coefficient is not affected by solar elevation."
 * @param sigma   scattering coefficient of single leaves and for visible radiation [-]. Default is 0.2.
 *                  See Spitters et al. (1989). In the order of 0.20. 0.20 for spring wheat, maize, potato, sugar beet.
 * @param kgpha   input (A_m, epsilon) and output unit in [kg ha-1] instead of [g m-2]. Default is false.
 * @param leaf_angle_integration_style style of the integration over all leaf angles. Default is 1.
 *                  0 = None (leads to overestimation according to Spitters 1986!);
 *                  1 = Spitters 1986, custom implementation, including Wageningen school implementations-inspired numerical safeguards;
 *                  2 = Spitters 1989, SUCROS87 implementation
 * @param n_canopy_layers number of canopy layers. Used for midpoint-integrtion over the photosynthesis per layer (non-linear, exponential). Default is 10.
        Usually in the order of 10 to 20 (accuracy/computation time trade-off).
 * @return hourly photosynthesis of the canopy [g CO2 m-2 ground h-1].
 */
double Spitters_canop_photo_multilayer(double beta, double LAI, double I0_dr, double I0_df, double A_m, double epsilon, double k_df=0.6, double sigma=0.2, bool kgpha=false, int leaf_angle_integration_style=1, int n_canopy_layers=10);



/**
 * @brief SUCROS87-inspired Wageningen-style hourly canopy gross photosynthesis.
 * 
 * Based on Spitters 1986 and Spitters et al. 1989 (SUCROS87) leaf and canopy photosynthesis, with 3-point gaussian integration over the canopy layers.
 * 
 * - Spitters CJT (1986). Separating the diffuse and direct component of global radiation and its implications for modeling canopy photosynthesis Part II. Calculation of canopy photosynthesis. Agricultural and Forest Meteorology, 38(1-3), 231-242.
 *   https://doi.org/10.1016/0168-1923(86)90061-4
 * 
 * - Spitters CJT, van Keulen H & van Kraalingen DWG (1989). A simple and universal crop growth simulator: SUCROS87. in R Rabbinge, SA Ward & HH van Laar (eds), Simulation and systems management in crop protection. Simulation monographs, Pudoc, Wageningen, pp. 147-181.
 *   https://edepot.wur.nl/171923
 * 
 * @param beta    solar elevation angle [rad].
 * @param LAI     leaf area index [m2 m-2]
 * @param I0_dr   direct PAR flux light intensity at the top of the canopy [J m-2 ground s-1] (=direct PAR irradiance on a horizontal plane).
 * @param I0_df   diffuse PAR flux light intensity at the top of the canopy [J m-2 ground s-1] (=diffuse PAR irradiance).
 * @param A_m     assimilation rate at light saturation [g CO2 m-2 leaf h-1] (=asymptote of light response curve).
 * @param epsilon light-use efficiency [g CO2 J-1 absorbed] (=initial slope of light response curve).
 *                  arcwheat1: "dA/dI at I = 0".
 *                  monica: "transition between photosynthetic quantum use efficiency and light saturated photosynthesis".
 * @param k_df    empirical extinction coefficient for diffuse radiation. Default is 0.6.
 *                  0.60 for spring wheat, 0.65 for maize, 1.00 for potato and 0.69 for sugar beet, according to Spitters et al. (1989), p. 151 and pp. 178-180.
 *                  See Spitters et al. (1989), Chapter 4.1.4 "Crop species and site characteristics", pp. 171-172:
 *                  "Typical values of k are 0.4 to 0.7 for monocotyledons and 0.65 to 1.1 for broadleaved dicotyledons (Monteith, 1969).
 *                   The extinction coefficient can be estimated from measurements of PAR above and below a canopy with a known LAI (Equation 62 [I_L = (1 - rho) * I_0 *exp(-k * L)]),
 *                   making sure that PAR is measured rather than total global radiation. The extinction coefficient for total radiation is about 2/3 that of PAR.
 *                   The extinction coefficient is best measured under a uniform overcast sky; then all radiation is diffuse so that the extinction coefficient is not affected by solar elevation."
 * @param sigma   scattering coefficient of single leaves and for visible radiation [-]. Default is 0.2.
 *                  See Spitters et al. (1989). In the order of 0.20. 0.20 for spring wheat, maize, potato, sugar beet.
 * @param kgpha   input (A_m, epsilon) and output unit in [kg ha-1] instead of [g m-2]. Default is false.
 * @param leaf_angle_integration_style style of the integration over all leaf angles. Default is 1.
 *                   0  = exponential light response curve, no leaf angle integration (leads to overestimation according to Spitters 1986!)
 *                   1  = exponential light response curve, Spitters 1986, custom implementation, including Wageningen school implementations-inspired numerical safeguards
 *                   2  = exponential light response curve, Spitters 1989, SUCROS87 implementation (using 3pt gauss integration over leaf angles)
 *                   10 = rectangular hyperbola light response curve, no leaf angle integration (overestimation should not as bad as with exponential light response curve accoring to Spitters 1986; inspired by style 0)
 *                   11 = rectangular hyperbola light response curve, custom implementation with custom leaf angle integration and numerical safeguards (inspired by style 1)
 *                   12 = rectangular hyperbola light response curve, using 3pt gauss integration over leaf angles (inspired by style 2)
 * @return hourly photosynthesis of the canopy [g CO2 m-2 ground h-1].
 */
double Spitters_canop_photo_3p(double beta, double LAI, double I0_dr, double I0_df, double A_m, double epsilon, double k_df=0.6, double sigma=0.2, bool kgpha=false, int leaf_angle_integration_style=1);


/**
 * @brief Gross assimilation ported from SUCROS87/WOFOST; for testing purposes and compariosn only
 * 
 * using a negative exponential light response curve with negative exponent
 * 
 * @param AMAX   (=A_m) maximum assimilation rate at light saturation (asymptote)
 * @param EFF    (=epsilon) light use efficiency
 * @param LAI    leaf area index
 * @param KDIF   (=kdf) empirical extinction coefficient for diffuse radiation
 * @param SINB   sin(solar_elevation_angle)
 * @param PARDIR (=I0_dr) direct PAR top of the canopy
 * @param PARDIF (=I0_df) diffuse PAR top of the canopy
 * @return double FGROS
 */
double ASSIM(double AMAX, double EFF, double LAI, double KDIF, double SINB, double PARDIR, double PARDIF);

}




