#include "photosynthesis-hourly.h"
#include "cr_math.h"

#include "tools/helper.h"

using namespace hPhoto;
using namespace Tools;
using namespace std;

double hPhoto::convert_MJpm2ph_to_unit(double value, hPhoto::unit out_unit)
{
  switch (out_unit) {
  case hPhoto::unit::umolpm2ps:
      // W m-2 -> µmol m-2 s-1
      value *= 4.56;
      [[fallthrough]];
  case hPhoto::unit::Wpm2:
      // J m-2 h-1 -> W m-2
      value /= 3600.0;
      [[fallthrough]];
  case hPhoto::unit::Jpm2ph:
      // MJ m-2 h-1 -> J m-2 h-1
      value *= 1e6;
      [[fallthrough]];
  case hPhoto::unit::MJpm2ph:
      // base unit — no conversion
      break;
  }
  return value;
}


double hPhoto::convert_unit_to_MJpm2ph(double value, hPhoto::unit in_unit)
{
  switch (in_unit) {
  case hPhoto::unit::umolpm2ps:
      // µmol m-2 s-1 -> W m-2
      value /= 4.56;
      [[fallthrough]];
  case hPhoto::unit::Wpm2:
      // W m-2 -> J m-2 h-1
      value *= 3600.0;
      [[fallthrough]];
  case hPhoto::unit::Jpm2ph:
      // J m-2 h-1 -> MJ m-2 h-1
      value /= 1e6;
      [[fallthrough]];
  case hPhoto::unit::MJpm2ph:
      // base unit — no conversion
      break;
  }
  return value;
}


double hPhoto::diffuse_fraction_daily_f(double globrad, double extra_terr_rad, double solar_elev)
{
  // Spitters et al. 1986 eq. 2 (daily)
  assert(extra_terr_rad > eps);
  double glob_extra_ratio = globrad / extra_terr_rad;

  if (glob_extra_ratio < 0.07)        // eq. 2a
  {
    return 1.;
  }
  else if (glob_extra_ratio < 0.35)   // eq. 2b
  {
    return 1. - 2.3 * cr::pow((glob_extra_ratio - 0.07), 2);
  }
  else if (glob_extra_ratio < 0.75)   // eq. 2c
  {
    return 1.33 - 1.46 * glob_extra_ratio;
  }
  else                                // eq. 2d
  {
    return 0.23;
  }
}


double hPhoto::diffuse_fraction_hourly_f(double globrad, double extra_terr_rad, double solar_elev)
{
  // Spitters et al. 1986 eq. 20 (hourly)
  assert(extra_terr_rad > eps);
  double glob_extra_ratio = globrad / extra_terr_rad;
  double R = (0.847 - 1.61 * cr::sin(solar_elev) + 1.04 * cr::pow(cr::sin(solar_elev), 2));
  double K = (1.47 - R) / 1.66;

  if (glob_extra_ratio <= 0.22)       // eq. 20a
  {
    return 1.;
  }
  else if (glob_extra_ratio <= 0.35)  // eq. 20b
  {
    return 1. - 6.4 * cr::pow((glob_extra_ratio - 0.22), 2);
  }
  else if (glob_extra_ratio <= K)     // eq. 20c
  {
    return 1.47 - 1.66 * glob_extra_ratio;
  }
  else                                // eq. 20d
  {
    return R;
  }
}

double hPhoto::diffuse_fraction_cscor(double diffuse_fraction, double solar_elevation)
{
  assert((diffuse_fraction >= 0.) && (diffuse_fraction <= 1.)); // 0 <= diffuse_fraction <= 1 required!
  /*if ((diffuse_fraction < 0) || (diffuse_fraction > 1))  // ((diffuse_fraction < -epsilon) || (diffuse_fraction > 1 + epsilon))
  {
    throw invalid_argument("diffuse_fraction_cscor calculation failed! requires 0 <= diffuse_fraction <= 1");
  }*/
  assert((solar_elevation >= 0.) && (solar_elevation <= (0.5 * M_PI))); // 0 <= solar_elevation <= pi/2 required!
  /*if ((solar_elevation < 0) && (solar_elevation > (0.5 * M_PI)))  // ((solar_elevation < -epsilon) && (solar_elevation > (0.5 * pi) + epsilon))
  {
    throw invalid_argument("diffuse_fraction_cscor calculation failed! requires 0 <= solar_elevation <= 0.5*pi");
  }*/
  return 1. / (1. + (1. - cr::pow(diffuse_fraction, 2)) * cr::pow(cr::cos(0.5 * M_PI - solar_elevation), 2) * cr::pow(cr::cos(solar_elevation), 3)); // Spitters eq. 9
}

double hPhoto::diffuse_fraction_parcor(double diffuse_fraction)
{
  assert((diffuse_fraction >= 0.) && (diffuse_fraction <= 1.)); // 0 <= diffuse_fraction <= 1 required!
  /*if ((diffuse_fraction < 0) || (diffuse_fraction > 1))  // ((diffuse_fraction < 0 - epsilon) || (diffuse_fraction > 1 + epsilon))
  {
    throw invalid_argument("diffuse_fraction_cscor calculation failed! requires 0 <= diffuse_fraction <= 1");
  }*/
  return 1. + 0.3 * (1. - cr::pow(diffuse_fraction, 2)); // Spitters eq. 10
}

hPhoto::PAR_radiation_result hPhoto::PAR_radiation(double global_rad, double extra_terr_rad, double solar_el, bool cscor, bool parcor, double parfrac, hPhoto::unit out_unit)
{
  PAR_radiation_result result;

  double diffuse_fraction, hourly_diffuse_rad, hourly_direct_rad;

  if (global_rad < eps) {
    result.diffuse = 0.;
    result.direct = 0.;
    return result;
  }

  if (solar_el < eps) {
    diffuse_fraction = 1.;
  } else {
    diffuse_fraction = diffuse_fraction_hourly_f(global_rad, extra_terr_rad, solar_el);
  }

  double diffuse_fraction_nocor = diffuse_fraction; //store value of diffuse_fraction without the corrections

  // circumsolar correction
  if (cscor) {
    double f_cscor = diffuse_fraction_cscor(diffuse_fraction_nocor, solar_el);
    diffuse_fraction *= f_cscor;
  }
  // PAR wavelengths correction
  if (parcor) {
    double f_parcor = diffuse_fraction_parcor(diffuse_fraction_nocor);
    diffuse_fraction *= f_parcor;
  }

  hourly_diffuse_rad = global_rad * diffuse_fraction;
  hourly_direct_rad = global_rad - hourly_diffuse_rad;

  // PAR fraction
  hourly_diffuse_rad *= parfrac;
  hourly_direct_rad  *= parfrac;

  result.diffuse = convert_MJpm2ph_to_unit(hourly_diffuse_rad, out_unit);
  result.direct = convert_MJpm2ph_to_unit(hourly_direct_rad, out_unit);
  return result;
}


dL_result hPhoto::Spitters_canop_photo_dL(double beta, double L, double I0_dr, double I0_df, double A_m, double epsilon, double k_df, double sigma, bool kgpha, hPhoto::la_integ_style leaf_angle_integration_style, hPhoto::lrc_style lrc)
{
  assert(L > 0.);
  assert(epsilon > 0.);

  //beta *= deg2rad                     // convert [deg] to [rad] if input is in [deg]
  assert((beta >= 0.) && (beta <= (0.5 * M_PI)));
  double sinbeta = max(hPhoto::eps, cr::sin(beta));  // beta is the solar elevation angle [rad]  // assert(sinbeta >= hPhoto::eps);

  // eq. 1
  // "sigma = scattering coefficient of single leaves and for visible radiation [...]. Hence, a fraction 1-sigma of the incoming flux is potentially available for absorption by the canopy."
  double first = (1. - cr::pow(1 - sigma, 0.5)) / (1. + cr::pow(1. - sigma, 0.5));  // reflection of a canopy of horizontal leaves (Goudriaan, 1977, p. 14,31, cited from Spitters 1986)
  double second = 2. / (1. + 1.6 * sinbeta);                                // approximate correction factor for a spherical leaf angle distribution (personal communication between Spitters and Goudriaan, cited from Spitters 1986)
  double rho = first * second;                                              // reflection coefficient of a green, closed vegetation

  /*
  // eq. 2, eq. 3, eq. 4
  // I_L = (1. - rho) * I0 * exp(-k * L)
  // I_df = (1. - rho) * I0_df * exp(-k_df * L)
  // I_dr = (1. - rho) * I0_dr * exp(- pow(1. - sigma, 0.5) * k_bl * L)

  // "On its way through the canopy a part of the direct flux which is intercepted by the leaves is scattered by those leaves;
  // hence, the direct flux segregates into a diffused, scattered component and another component which remains direct."
  // eq. 5, eq. 6
  // I_drdr = I0_dr * exp(-k_bl * L)
  // I_drdf = I_dr - I_drdr

  // "For a spherical leaf angle distribution with leaves distributed randomly within the canopy volume, the extinction coefficients of the diffuse flux
  // and that of the direct component of the direct flux are approximated, respectively, by (Goudriaan, 1977, 1982):"
  // eq. 7
  // k_df = 0.8 * pow(1. - sigma, 0.5)  // extinction coefficient diffuse
  // k_bl = 0.5 / sinbeta               // extinction coefficient for black leaves (neither transmission nor reflection); "0.5 points to the average projection on the ground surface of leaves showing a spherical angle distribution,
                                        // and 0.8 is the value of 0.5/sinbeta averaged over inclination beta of incident radiation under an overcast sky."

  // "Actual extinction coefficients deviate from the above theoretical values, mainly because leaves are clustered rather than randomly distributed.
  // Assuming that the leaf angle distribution is still spherical, eq. 7 gives as an approximation for the extinction coefficient of black leaves:
  // eq.8
  // k_bl = 0.5 * k_df / (0.8 * pow(1. - sigma, 0.5) * sinbeta)
  // where the empirical, measured value of k_df is used rather than the theoretical value of eq. 7."
  */
  double clusterfactor = k_df / (0.8 * cr::pow(1. - sigma, 0.5)); // empirical k_df (clustered leaves) / theoretical k_df (randomly distributed leaves); clusterfactor (Spitters et al. 1989, p. 171)
  double k_bl = 0.5 / sinbeta * clusterfactor;
  double k_dr = k_bl * cr::pow(1. - sigma, 0.5);                 // extinction coefficient for total direct PAR flux; non-black leaves correction from Spitters et al. (1989)

  // eq. 9
  // L = canopy depth
  // Ia = -dI/dL = (1. - rho) * I0 *  * k * exp(-k * L)       // absorbed light energy per unit leaf area [J m-2 leaf s-1]; derived from eq. 2

  // eq. 10, eq. 11, eq. 12
  double Ia_df = (1. - rho) * I0_df * k_df * cr::exp(-k_df * L);                                                    // absorption of the diffuse flux, derived from eq. 3
  // double Ia_dr = (1. - rho) * I0_dr * pow(1. - sigma, 0.5) * k_bl * exp(- pow(1. - sigma, 0.5) * k_bl * L);  // absorption of the direct flux, derived from eq. 4
  double Ia_dr = (sinbeta <= hPhoto::eps) ? 0.0 : (1. - rho) * I0_dr * k_dr * cr::exp(-k_dr * L);                   // eq. 11, re-arranged to use k_dr similarly to Spitters et al. (1989, p. 154), safeguard added
  // double Ia_drdr = (1. - sigma) * I0_dr * k_bl * exp(- pow(1. - sigma, 0.5) * k_bl * L);                     // Of the direct component of the direct flux (eq. 5), the non-scattered part 1-sigma is absorbed
  double Ia_drdr = (sinbeta <= hPhoto::eps) ? 0.0 : (1. - sigma) * I0_dr * k_bl * cr::exp(- k_bl * L);              // eq. 11, but with k_bl (instead of k_dr) in the exponent; similar to implementation in Spitters et al. (1989, p. 154), safeguard added

  // shaded leaf area
  // eq. 13
  double Ia_sh = max(0.0, Ia_df + (Ia_dr - Ia_drdr));           // absorbed light energy shaded leaf area (absorbs the diffuse flux and the diffused component of the direct flux) [J m-2 leaf s-1]

  assert(A_m > 0.); // this should usually be true, since MONICA does something like this: A_m = max(0.1, A_m);
  double A_sh = 0.;
  // if (leaf_angle_integration_style >= 10) {
  if (lrc == hPhoto::lrc_style::rectangular_hyperbola) {
    // eq. 24
    A_sh = A_m * (epsilon * Ia_sh / (epsilon * Ia_sh + A_m)); // rectangular hyperbola function light respone curve
  } else if (lrc == hPhoto::lrc_style::exponential) {
    // eq. 15
    A_sh = A_m * (1. - cr::exp(-epsilon * Ia_sh / A_m));         // negative exponential function with negative exponent light response curve
    // A_sh = (A_m < eps) ? 0. : A_m * (1. - exp(-epsilon * Ia_sh / A_m));  // added safeguard for A_m, since MONICA has this: if (vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation) {vc_AssimilationRate = 0.0;}
  } else if (lrc == hPhoto::lrc_style::nonrectangular_hyperbola) {
    throw runtime_error("Light response curve style nonrectangular_hyperbola not implemented");
  } else {
    throw invalid_argument("Invalid light response curve style!");
  }

  double A_sl = 0.;
  if (leaf_angle_integration_style == hPhoto::la_integ_style::none) {  // None
    // sunlit leaf area
    // eq. 14
    double Ia_sl = (sinbeta <= hPhoto::eps) ? Ia_sh : Ia_sh + (1. - sigma) * k_bl * I0_dr;         // absorbed light energy sunlit leaf area (receives diffuse and direct radiation) [J m-2 leaf s-1], added safeguard
    if (lrc == hPhoto::lrc_style::exponential) { // (leaf_angle_integration_style == 10) {
      A_sl = A_m * (1. - cr::exp(-epsilon * Ia_sl / A_m));          // inaccurate; leads to overestimation according to Spitters 1986 // FS: Averaging Ia_sl is inaccurate, because photosynthesis light response is not linear!
    } else {  // else if (lrc == hPhoto::lrc_style::rectangular_hyperbola) {
      A_sl = A_m * (epsilon * Ia_sl / (epsilon * Ia_sl + A_m)); // None, but with rectangular hyperbola function
    } // else if (lrc == hPhoto::lrc_style::nonrectangular_hyperbola) {
    //   throw runtime_error("Light response curve style nonrectangular_hyperbola not implemented");
    // } else {
    //   throw invalid_argument("Invalid light response curve style!");
    // }
  } else {
    // correction to account for the variation in leaf angle and thus in illumination intensity for sunlit leaf area
    // FS: This is crucial, since photosynthesis is not linear. It is not sufficient to average irradiances over leaf angles beforehand - instead, photosynthesis should be averaged by integrating over leaf angles.
    // eq. 16, eq. 17
    double Ia_sldr = (sinbeta <= hPhoto::eps) ? 0.0 : (1. - sigma) * I0_dr / sinbeta;  // direct flux is absorbed by a leaf perpendicular to the direct beam, safeguard added
    if (leaf_angle_integration_style == hPhoto::la_integ_style::spitters86_custom) {          // Spitters 1986, custom implementation including Wageningen school implementations-inspired numerical safeguards;
      const double A_m_nsmin = kgpha ? 2.0 : 0.2; // 0.2 [g CO2 m-2 leaf h-1] or 2 [kg CO2 ha-1 leaf h-1]; Wageningen school-style numerical safeguard;
                                                  // see e.g. WOFOST (https://github.com/ajwdewit/WOFOST/blob/deac197d3c74741832b815581699a6c825894758/sources/w60lib/assim.for)
                                                  // or python crop smulation environment (pcse = WOFOST pure python implementation, https://github.com/ajwdewit/pcse/blob/4d9f0e4f542e9062db338aaf1a227a75f1b03949/pcse/crop/assimilation.py),
                                                  // which both use 2.0 [kg CO2 ha-1 leaf h-1]
      if (Ia_sldr <= hPhoto::eps) {
        A_sl = A_sh;  // this is the limit of eq. 17 when Ia_sldr -> 0
      } else {
        if (lrc == hPhoto::lrc_style::exponential) {
          // eq. 17
          A_sl = A_m * (1. - (A_m - A_sh) * (1. - cr::exp(-epsilon * Ia_sldr / max(A_m_nsmin, A_m))) / (epsilon * Ia_sldr));  // corrected version of A_sl, integrating over leaf angles (assuming spherical distribution)
        } else {  // else if (lrc == hPhoto::lrc_style::rectangular_hyperbola) {
        // integration over leaf angle distribution, but with rectengular hyperbola
        // A_sl = A_m * (1. - (A_m / (epsilon * Ia_sldr)) * log1p((epsilon * Ia_sldr * (A_m - A_sh)) / (A_m * A_m)));     // integrating rectangular hyperbola over leaf angles (assuming spherical distribution),
                                                                                                                          // in a similar way as done in eq.17 for the exponential light response curve
        A_sl = A_m * (1. - (max(A_m_nsmin, A_m) / (epsilon * Ia_sldr)) * log1p((epsilon * Ia_sldr * (A_m - A_sh)) / (A_m * max(A_m_nsmin, A_m))));  // added numerical safeguard
        } // else if (lrc == hPhoto::lrc_style::nonrectangular_hyperbola) {
        //   throw runtime_error("Light response curve style nonrectangular_hyperbola not implemented");
        // } else {
        //   throw invalid_argument("Invalid light response curve style!");
        // }
      }
    } else if (leaf_angle_integration_style == hPhoto::la_integ_style::sucros87_3pt) { // SUCROS87 Subroutine ASS (Spitters et al. 1989) integration over leaf angle distribution
      static const double gaussian_distances[3] = {0.112702, 0.5, 0.887298};
      static const double gaussian_weights[3] = {0.277778, 0.444444, 0.277778};
      //selection of canopy depths (LAIC from top)
      for (int i = 0; i < 3; ++i) {
        double Ia_sl = Ia_sh + Ia_sldr * gaussian_distances[i];
        // 3-point gaussian integration
        if (lrc == hPhoto::lrc_style::exponential) {
          A_sl += A_m * (1. - cr::exp(-epsilon * Ia_sl / A_m)) * gaussian_weights[i];
        } else {  // if (lrc == hPhoto::lrc_style::rectangular_hyperbola) {
          A_sl += A_m * (epsilon * Ia_sl / (epsilon * Ia_sl + A_m)) * gaussian_weights[i];
        } // else if (lrc == hPhoto::lrc_style::nonrectangular_hyperbola) {
        //   throw runtime_error("Light response curve style nonrectangular_hyperbola not implemented");
        // } else {
        //   throw invalid_argument("Invalid light response curve style!");
        // }
      }
    } else {
      throw invalid_argument("Incvalid leaf_angle_integration_style!");
    }
  }

  // eq. 19
  double f_sl = cr::exp(-k_bl * L);                     // fraction sunlit leaf area; equals the fraction of the direct beam reaching that layer (see eq. 5)
  // double f_sl = exp(-k_bl * L) * clusterfactor;   // Spitters et al. 1989, p. 154 seems to use eq. 19 (Spitters 1986), corrected with clusterfactor (Spitters et al. 1989, p. 171)
  /* FS: This seems somewhat strange, since k_bl has already been corrected with clusterfactor before (k_bl = 0.5 / sinbeta * clusterfactor),
      so Spitters et al. 1989, p. 154 (=SUCROS87?) seem to do f_sl = np.exp(-(0.5 / sinbeta * clusterfactor) * L) * clusterfactor
      WOFOST however just uses f_sl = exp(-k_bl * L), which is exp(-(0.5 / sinbeta * clusterfactor) * L) in the FORTRAN version (https://github.com/ajwdewit/WOFOST/blob/deac197d3c74741832b815581699a6c825894758/sources/w60lib/assim.for#L81)
      as well as the python versions (WOFOST-PCSE assim7 and assim8, see https://github.com/ajwdewit/pcse/blob/4d9f0e4f542e9062db338aaf1a227a75f1b03949/pcse/crop/assimilation.py#L147 and https://github.com/ajwdewit/pcse/blob/4d9f0e4f542e9062db338aaf1a227a75f1b03949/pcse/crop/assimilation.py#L387)
  */

  // // eq. 18
  // double A = f_sl * A_sl + (1. - f_sl) * A_sh;
  // return A;

  return {f_sl, A_sl, A_sh};
}


photo_result hPhoto::Spitters_canop_photo_multilayer(double beta, double LAI, double I0_dr, double I0_df, double A_m, double epsilon, double k_df, double sigma, bool kgpha, hPhoto::la_integ_style leaf_angle_integration_style, hPhoto::lrc_style lrc, int n_canopy_layers)
{
  assert(n_canopy_layers > 0);

  if (LAI <= 0.0) {
    // return 0.0;
    return {0.0, 0.0, 0.0, 0.0, 0.0};
  }

  double dl = 1. / n_canopy_layers;

  // vector<double> canopy_layers;
  double A_canop = 0.;
  double f_sl_canop = 0., A_sl_canop = 0., A_sh_canop = 0.;
  for (int i = 0; i < n_canopy_layers; ++i) {
    // canopy_layers.push_back((i + 0.5) * dl);

    double L = LAI * ((i + 0.5) * dl); // partial cumulated leaf area index at various canopy depths (= LAIC from Spitters et al. 1989)

    // photosynthesis of canopy layer dL
    // auto A = Spitters_canop_photo_dL(beta, L, I0_dr, I0_df, A_m, epsilon, k_df, sigma, kgpha, leaf_angle_integration_style);
    auto dL_res = Spitters_canop_photo_dL(beta, L, I0_dr, I0_df, A_m, epsilon, k_df, sigma, kgpha, leaf_angle_integration_style, lrc);

    // eq. 18
    double A = dL_res.f_sl * dL_res.A_sl + (1. - dL_res.f_sl) * dL_res.A_sh;

    // sum over canopy layers (= numerical integration)
    A_canop += A * dl;
    f_sl_canop += dL_res.f_sl * dl;
    A_sl_canop += dL_res.f_sl * dL_res.A_sl * dl;
    A_sh_canop += (1. - dL_res.f_sl) * dL_res.A_sh * dl;
  }

  double LAI_sl_canop = f_sl_canop * LAI;

  // return A_canop * LAI;
  return {A_canop * LAI, LAI_sl_canop, f_sl_canop, A_sl_canop * LAI, A_sh_canop * LAI};
}

photo_result hPhoto::Spitters_canop_photo_3p(double beta, double LAI, double I0_dr, double I0_df, double A_m, double epsilon, double k_df, double sigma, bool kgpha, hPhoto::la_integ_style leaf_angle_integration_style, hPhoto::lrc_style lrc)
{
  if (LAI <= 0.0) {
    // return 0.0;
    return {0.0, 0.0, 0.0, 0.0, 0.0};
  }

  // 3-point integration procedure from Spitters (1986)
  // eq. 20, eq. 21
  // L = (0.5 + p * pow(0.15, 0.5)) * LAI;               // p = 1,0,1
  // A_h = LAI * _A(_L) + 1.6*_A_(_L_) + A_(L_)) / 3.6;

  static const double canopy_layers[3] = {0.112702, 0.5, 0.887298};
  static const double gaussian_weights[3] = {0.277778, 0.444444, 0.277778};

  // selection of canopy depths (LAIC from top)
  double A_canop = 0., L, A = 0.;
  double f_sl_canop = 0., A_sl_canop = 0., A_sh_canop = 0.;

  for (int l = 0; l < 3; ++l) {
    L = LAI * canopy_layers[l];  // partial cumulated leaf area index at various canopy depths (= LAIC from Spitters et al. 1989)

    // photosynthesis of canopy layer at gaussian integration point l
    // A = Spitters_canop_photo_dL(beta, L, I0_dr, I0_df, A_m, epsilon, k_df, sigma, kgpha, leaf_angle_integration_style);
    auto dL_res = Spitters_canop_photo_dL(beta, L, I0_dr, I0_df, A_m, epsilon, k_df, sigma, kgpha, leaf_angle_integration_style, lrc);

    // eq. 18
    double A = dL_res.f_sl * dL_res.A_sl + (1. - dL_res.f_sl) * dL_res.A_sh;

    // 3-point gaussian integration
    A_canop += A * gaussian_weights[l];
    f_sl_canop += dL_res.f_sl * gaussian_weights[l];
    A_sl_canop += dL_res.f_sl * dL_res.A_sl* gaussian_weights[l];
    A_sh_canop += (1. - dL_res.f_sl) * dL_res.A_sh * gaussian_weights[l];
  }

  double LAI_sl_canop = f_sl_canop * LAI;

  // return A_canop * LAI;
  return {A_canop * LAI, LAI_sl_canop, f_sl_canop, A_sl_canop * LAI, A_sh_canop * LAI};
}


Spitters_canop_radiation_dL_result Spitters_canop_radiation_dL(double beta, double L, double I0_dr, double I0_df, double k_df, double sigma)
{
  assert((beta >= 0.) && (beta <= (0.5 * M_PI)));
  double sinbeta = max(eps, cr::sin(beta));                                                             // beta is the solar elevation angle [rad]
  double first = (1. - cr::pow(1 - sigma, 0.5)) / (1. + cr::pow(1. - sigma, 0.5));                          // reflection of a canopy of horizontal leaves (Goudriaan, 1977, p. 14,31, cited from Spitters 1986)
  double second = 2. / (1. + 1.6 * sinbeta);                                                        // approximate correction factor for a spherical leaf angle distribution (personal communication between Spitters and Goudriaan, cited from Spitters 1986)
  double rho = first * second;                                                                      // reflection coefficient of a green, closed vegetation
  double clusterfactor = k_df / (0.8 * cr::pow(1. - sigma, 0.5));                                       // empirical k_df (clustered leaves) / theoretical k_df (randomly distributed leaves); clusterfactor (Spitters et al. 1989, p. 171)
  double k_bl = 0.5 / sinbeta * clusterfactor;
  double f_sl = cr::exp(-k_bl * L);                                                                     // fraction sunlit leaf area; equals the fraction of the direct beam reaching that layer (see eq. 5)
  // double f_sl = exp(-k_bl * L) * clusterfactor;                                                  // Spitters et al. 1989, p. 154 seems to use eq. 19 (Spitters 1986), corrected with clusterfactor (Spitters et al. 1989, p. 171)
  /* FS: This seems somewhat strange, since k_bl has already been corrected with clusterfactor before (k_bl = 0.5 / sinbeta * clusterfactor),
      so Spitters et al. 1989, p. 154 (=SUCROS87?) seem to do f_sl = np.exp(-(0.5 / sinbeta * clusterfactor) * L) * clusterfactor
      WOFOST however just uses f_sl = np.exp(-k_bl * L), which is np.exp(-(0.5 / sinbeta * clusterfactor) * L) in the FORTRAN version (https://github.com/ajwdewit/WOFOST/blob/deac197d3c74741832b815581699a6c825894758/sources/w60lib/assim.for#L81)
      as well as the python versions (pcse assim7 and assim8, see https://github.com/ajwdewit/pcse/blob/4d9f0e4f542e9062db338aaf1a227a75f1b03949/pcse/crop/assimilation.py#L147 and https://github.com/ajwdewit/pcse/blob/4d9f0e4f542e9062db338aaf1a227a75f1b03949/pcse/crop/assimilation.py#L387)
  */
  double k_dr = k_bl * cr::pow(1. - sigma, 0.5);                                                        // extinction coefficient for total direct PAR flux; non-black leaves correction from Spitters et al. (1989)
  double Ia_df = (1. - rho) * I0_df * k_df * cr::exp(-k_df * L);                                        // absorption of the diffuse flux, derived from eq. 3
  double Ia_dr = (sinbeta <= hPhoto::eps) ? 0.0 : (1. - rho) * I0_dr * k_dr * cr::exp(-k_dr * L);       // eq. 11, re-arranged to use k_dr similarly to Spitters et al. (1989, p. 154), safeguard added
  double Ia_drdr = (sinbeta <= hPhoto::eps) ? 0.0 : (1. - sigma) * I0_dr * k_bl * cr::exp(- k_bl * L);  // eq. 11, but with k_bl (instead of k_dr) in the exponent; similar to implementation in Spitters et al. (1989, p. 154), safeguard added
  double Ia_sh = max(0.0, Ia_df + (Ia_dr - Ia_drdr));                                               // absorbed light energy shaded leaf area (absorbs the diffuse flux and the diffused component of the direct flux) [J m-2 leaf s-1], safeguard added
  double Ia_sldr = (sinbeta <= hPhoto::eps) ? 0.0 : (1. - sigma) * I0_dr / sinbeta;                 // direct flux is absorbed by a leaf perpendicular to the direct beam, safeguard added
  return {Ia_sh, Ia_sldr, f_sl, Ia_dr};
}

double Spitters_A_sh_dL(double Ia_sh, double Amax_sh, double epsilon_sh, hPhoto::lrc_style lrc)
{
  if (lrc == hPhoto::lrc_style::exponential) {
    // exponential light respone curve
    return(Amax_sh < eps) ? 0. : Amax_sh * (1. - cr::exp(-epsilon_sh * Ia_sh / Amax_sh));
  } else if (lrc == hPhoto::lrc_style::rectangular_hyperbola) {
    // rectangular hyperbola light respone curve
    return(Amax_sh < eps) ? 0. : Amax_sh * (epsilon_sh * Ia_sh / (epsilon_sh * Ia_sh + Amax_sh));
  } else if (lrc == hPhoto::lrc_style::nonrectangular_hyperbola) {
    // non-rectangular hyperbola
    throw runtime_error("Light response curve style nonrectangular_hyperbola not implemented!");
  } else {
    throw invalid_argument("Incvalid light response curve style!");
  }
}

double Spitters_A_sl_dL(double A_sh, double Ia_sldr, double Amax_sl, double epsilon_sl, bool kgpha, hPhoto::lrc_style lrc)
{
  const double A_m_nsmin = kgpha ? 2.0 : 0.2; // 0.2 [g CO2 m-2 leaf h-1] or 2 [kg CO2 ha-1 leaf h-1]; Wageningen school-style numerical safeguard;
                                              // see e.g. WOFOST (https://github.com/ajwdewit/WOFOST/blob/deac197d3c74741832b815581699a6c825894758/sources/w60lib/assim.for)
                                              // or python crop smulation environment (pcse = WOFOST pure python implementation, https://github.com/ajwdewit/pcse/blob/4d9f0e4f542e9062db338aaf1a227a75f1b03949/pcse/crop/assimilation.py),
                                              // which both use 2.0 [kg CO2 ha-1 leaf h-1]
  if (lrc == hPhoto::lrc_style::exponential) {
    // exponential light respone curve integrated over leaf angles and with numerical safeguards
    // Spitters 1986 eq.17, custom implementation including Wageningen school implementations-inspired numerical safeguards;
    // if (Ia_sldr <= hPhoto::eps) {return A_sh};  // this is the limit of eq. 17 when Ia_sldr -> 0
    return (Ia_sldr <= hPhoto::eps) ? A_sh : Amax_sl * (1. - (Amax_sl - A_sh) * (1. - cr::exp(-epsilon_sl * Ia_sldr / max(A_m_nsmin, Amax_sl))) / (epsilon_sl * Ia_sldr));
  } else if (lrc == hPhoto::lrc_style::rectangular_hyperbola) {
    // rectangular hyperbola light respone curve integrated over leaf angles and with numerical safeguards
    // A_sl = A_m * (1. - (A_m / (epsilon * Ia_sldr)) * log1p((epsilon * Ia_sldr * (A_m - A_sh)) / (A_m * A_m))); // integrating rectangular hyperbola over leaf angles (assuming spherical distribution),
                                                                                                                  // in a similar way as done in Spitters 1986 eq.17 for the exponential light response curve
    // if (Ia_sldr <= hPhoto::eps) {return A_sh};  // this is the limit of eq. 17 when Ia_sldr -> 0
    return (Ia_sldr <= hPhoto::eps) ? A_sh : Amax_sl * (1. - (max(A_m_nsmin, Amax_sl) / (epsilon_sl * Ia_sldr)) * log1p((epsilon_sl * Ia_sldr * (Amax_sl - A_sh)) / (Amax_sl * max(A_m_nsmin, Amax_sl))));
  } else if (lrc == hPhoto::lrc_style::nonrectangular_hyperbola) {
    // non-rectangular hyperbola
    throw runtime_error("Light response curve style nonrectangular_hyperbola not implemented!");
  } else {
    throw invalid_argument("Incvalid light response curve style!");
  }
}


// // photosynthesis on canopy level, solved per canopy layer for shaded and sunlit leaves, integrated over canopy layers using 3 point integration
// photo_result canop_photo_3p(
//     double pc_CarboxylationPathway,
//     double pc_MaxAssimilationRate,
//     double beta,
//     double LAI,
//     double I0_dr,
//     double I0_df,
//     double k_df,
//     double sigma,
//     bool kgpha)
// {
//     static const double canopy_layers[3]   = {0.112702, 0.5, 0.887298};
//     static const double gaussian_weights[3] = {0.277778, 0.444444, 0.277778};

//     double A_canop = 0.0;
//     double gs_canop = 0.0;
//     for (int l = 0; l < 3; ++l)
//     {
//         double L = LAI * canopy_layers[l];          // cumulated LAI

//         // radiation (once per layer)
//         auto rad = Spitters_canop_radiation_dL(beta, L, I0_dr, I0_df, k_df, sigma);

//         /* for one layer dL
//           if (pc_CarboxylationPathway == 1) {
//             // calculate temperature-dependent parameters shaded & sunlit
//             // ...

//             // shaded
//             double Vcmax_sh = calcVcmax(pc_MaxAssimilationRate, kin_sh.KTvmax);
//             Gamma_sh = calcCO2CompensationPoint(Oi, Vcmax_sh, kin_sh.Mkc, kin_sh.Mko);
//             double Ci_sh_eff = std::max(std::max(Ci_sh, eps), Gamma_sh + eps);
//             epsilon_sh = calcRUE_C3(Ci_sh_eff, Gamma_sh);
//             Amax_sh = calcAmaxC3(T_sh, Ci_sh_eff, Oi, Gamma_sh, Vcmax_sh, kin_sh.Mkc, kin_sh.Mko, pc_MinimumTemperatureForAssimilation);

//             // sunlit
//             double Vcmax_sl = calcVcmax(pc_MaxAssimilationRate, kin_sl.KTvmax);
//             Gamma_sl = calcCO2CompensationPoint(Oi, Vcmax_sl, kin_sl.Mkc, kin_sl.Mko);
//             double Ci_sl_eff = std::max(std::max(Ci_sl, eps), Gamma_sl + eps);
//             epsilon_sl = calcRUE_C3(Ci_sl_eff, Gamma_sl);
//             Amax_sl = calcAmaxC3(T_sl, Ci_sl_eff, Oi, Gamma_sl, Vcmax_sl, kin_sl.Mkc, kin_sl.Mko, pc_MinimumTemperatureForAssimilation);

//             // calculate photosynthesis
//             A_sh = Spitters_A_sh_dL(rad.Ia_sh, Amax_sh, epsilon_sh, hPhoto::lrc_style::rectangular_hyperbola);                // Spitters_A_sh_dL(rad.Ia_sh, Amax_sh, epsilon_sh);
//             A_sl = Spitters_A_sl_dL(A_sh, rad.Ia_sldr, Amax_sl, epsilon_sl, kgpha, hPhoto::lrc_style::rectangular_hyperbola); // Spitters_A_sl_dL(A_sh, rad.Ia_sldr, Amax_sl, epsilon_sl, A_m_nsmin);
//           } else: {
//             // calculate temperature-dependent parameters shaded & sunlit
//             // ...

//             // calculate photosynthesis
//             // ...
//           }
//         */

//         // integrate
//         A_canop += res_dL.A * gaussian_weights[l];
//         gs_canop += res_dL.gs * gaussian_weights[l];
//     }
//     return A_canop * LAI;
// }


// double hPhoto::ASSIM(double AMAX, double EFF, double LAI, double KDIF, double SINB, double PARDIR, double PARDIF) {
//   // Gaussian points and weights
//   static const double XGAUSS[3] = {0.1127017, 0.5, 0.8872983};
//   static const double WGAUSS[3] = {0.2777778, 0.4444444, 0.2777778};
//   static const double SCV = 0.2;

//   // --- Extinction coefficients ---
//   double REFH   = (1.0 - std::sqrt(1.0 - SCV)) / (1.0 + std::sqrt(1.0 - SCV));
//   double REFS   = REFH * 2.0 / (1.0 + 1.6 * SINB);

//   double KDIRBL = (0.5 / SINB) * KDIF / (0.8 * std::sqrt(1.0 - SCV));
//   double KDIRT  = KDIRBL * std::sqrt(1.0 - SCV);

//   // --- Gaussian integration over canopy depth ---
//   double FGROS = 0.0;
//   for (int i = 0; i < 3; ++i) {
//     double LAIC = LAI * XGAUSS[i];

//     // Absorbed radiation components (J m-2 s-1)
//     double VISDF  = (1.0 - REFS) * PARDIF * KDIF   * std::exp(-KDIF   * LAIC);
//     double VIST   = (1.0 - REFS) * PARDIR * KDIRT  * std::exp(-KDIRT  * LAIC);
//     double VISD   = (1.0 - SCV ) * PARDIR * KDIRBL * std::exp(-KDIRBL * LAIC);

//     // Shaded leaves
//     double VISSHD = VISDF + VIST - VISD;
//     double FGRSH  = AMAX *
//         (1.0 - std::exp(-VISSHD * EFF / std::max(2.0, AMAX)));

//     // Sunlit leaves
//     double VISPP = (1.0 - SCV) * PARDIR / SINB;
//     double FGRSUN;
//     if (VISPP <= 0.0) {
//       FGRSUN = FGRSH;
//     } else {
//       FGRSUN = AMAX *
//                 (1.0 - (AMAX - FGRSH) *
//                 (1.0 - std::exp(-VISPP * EFF / std::max(2.0, AMAX))) /
//                 (EFF * VISPP));
//     }

//     double FSLLA = std::exp(-KDIRBL * LAIC);              // Fraction sunlit leaf area
//     double FGL = FSLLA * FGRSUN + (1.0 - FSLLA) * FGRSH;  // Local assimilation rate

//     FGROS += FGL * WGAUSS[i];
//   }

//   return FGROS *= LAI;  // Multiply by total LAI
// }