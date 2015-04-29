/**
 * @file cc_germany_methods.h
 */

#ifndef CC_GERMANY_METHODS_H_
#define CC_GERMANY_METHODS_H_

#if defined RUN_CC_GERMANY || defined RUN_GIS

#include "monica-parameters.h"
#include "tools/date.h"
#include "climate/climate.h"
#include "tools/coord-trans.h"
#include "monica.h"
#include "interpolation/interpol.h"
#include "grid/grid.h"
#include "soil/soil.h"

namespace Monica
{
  const Soil::SoilPMs* readBUEKDataFromMonicaDB(const int leg1000_id,  const GeneralParameters& gps);
  std::vector<ProductionProcess> getCropManagementData(int crop_id, std::string start_date_s, std::string end_date_s, double julian_sowing_date);
  Tools::LatLngCoord getGeoCorrdOfStatId(int stat_id);
  double getLatitudeOfStatId(int stat_id);
  std::string getStationName(int stat_id);
  int getDatId(int stat_id);
  Climate::DataAccessor climateDataForCCGermany(int stat_id, std::string start_date_s, std::string end_date_s, std::string realisation,CentralParameterProvider& cpp);
  Climate::DataAccessor climateDataForCCGermany2(int stat_id, std::string start_date_s, std::string end_date_s, std::string realisation,CentralParameterProvider& cpp);

  int numberOfPossibleSteps(std::string start_date_s, std::string end_date_s);

  inline double GK52Latitude(double rechtswert, double hochwert)
  {
    Tools::GK5Coord gk5c(rechtswert, hochwert);
    double lat = Tools::GK52latLng(gk5c).lat;
	return lat;
  }




} // namespace Monica


#endif /*#ifdef RUN_EVA2*/

#endif
