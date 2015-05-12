/**
 * @file cc_germany_methods.cpp
 */

#if defined RUN_CC_GERMANY  ||  defined RUN_GIS
#include <mutex>

#include "cc_germany_methods.h"
#include "debug.h"

#include "tools/auto-deleter.h"
#include "tools/algorithms.h"
#include "tools/helper.h"
#include "db/abstract-db-connections.h"
#include "crop.h"

using namespace Db;
using namespace std;
using namespace Monica;
using namespace Tools;
using namespace Climate;
using namespace Soil;

/**
 * @brief Overloaded function that returns soil parameter for ucker.
 *
 * Parameters are read from database.
 *
 * @param str
 * @param gps General parameters
 * @return Soil parameters
 */
const SoilPMs*
Monica::readBUEKDataFromMonicaDB(const int leg1000_id,  const GeneralParameters& gps)
{
  debug() << "----------------------------------------------------------------" << endl;
  debug() << "CC: Getting soilparameters for BUEK: " << leg1000_id << endl;
  debug() << "----------------------------------------------------------------" << endl;

  int layer_thickness_cm = gps.ps_LayerThickness.front() * 100; //cm
  int max_depth_cm = gps.ps_ProfileDepth * 100; //cm
  int number_of_layers = int(double(max_depth_cm) / double(layer_thickness_cm));

  static mutex lockable;
	lock_guard<mutex> lock(lockable);
	
  vector<SoilParameters>* sps = new vector<SoilParameters> ;

  ostringstream request;
  request << "SELECT leg1000, hornum, otief, utief, boart, ton, schluff, sand, ph, rohd, humus "
      << "FROM monica.horizontdaten h where leg1000=" << leg1000_id;
  debug() << request.str().c_str()<< endl;

  // connect to database eva2
  DB *con = newConnection("monica_buek");
  DBRow row;
  con->select(request.str().c_str());

  int index = 0;
  int layer_count = 0;
  int row_count = con->getNumberOfRows();
  while (!(row = con->getRow()).empty()) {
      //              cout << "leg1000_id:\t" << leg1000_id << endl;
      //              cout << "ho:\t" << row[2] << endl;
      //              cout << "hu:\t" << row[3] << endl;
      //              cout << "soiltype:\t" << row[4] << endl;
      //              cout << "clay:\t" << row[5] << endl;
      //              cout << "sand:\t" << row[7] << endl;
      //              cout << "ph:\t" << row[8] << endl;
      //              cout << "raw_dens:\t" << row[9] << endl;
      //              cout << "humus:\t" << row[10] << endl;
      // row counter
      index++;

      // string id = row[0];
      int horiz_oben = satof(row[2]);
      int horiz_unten = satof(row[3]);
      std::string soil_type = row[4];
      double clay = satof(row[5])/100.0;
      double sand = satof(row[7])/100.0;
      double ph = satof(row[8]);
      double raw_density = satof(row[9]);
      double corg = satof(row[10])/1.72;

      if (sand < 0) {
          sand = KA52sand(soil_type);
      }
      if (clay < 0) {
          clay = KA52clay(soil_type);
      }

      // calc number of layers that lay in the horizon
      double horiz_width = double(horiz_unten - horiz_oben);
      horiz_width = (round(horiz_width / 10.0)) * 10.0;
      int layer_in_horiz = int(horiz_width) / layer_thickness_cm;

      // read soil parameter
      SoilParameters soil_param;
      soil_param.vs_SoilSandContent = sand;
      soil_param.vs_SoilClayContent = clay;
      soil_param.vs_SoilpH = ph;
      soil_param.set_vs_SoilOrganicCarbon(corg / 100.0);
      soil_param.set_vs_SoilRawDensity(raw_density);
      soil_param.vs_Lambda = soil_param.texture2lambda(soil_param.vs_SoilSandContent, soil_param.vs_SoilClayContent);
      soil_param.vs_SoilTexture = soil_type;
      soil_param.vs_SoilStoneContent = 0;

      soilCharacteristicsKA5(soil_param);

      // test validity of soil parameters
      bool valid_soil_params = soil_param.isValid();
      if (!valid_soil_params) {
          return NULL;
      }

      // for each layer in horizon the soil parameter set is equal
      // so for each layer the same soil parameters are added to SoilPMs vector
      layer_count += layer_in_horiz;
      for (int i = 0; i < layer_in_horiz; i++) {
          sps->push_back(soil_param);
      }

      // last horizon, so add same parameter set to all remaining layers
      if (index == row_count) {
          for (int i = layer_count; i < number_of_layers; i++) {
              sps->push_back(soil_param);
          }
      }
  }
  
  if (sps->size() == 0) {
      cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
      cout << "Error: Found no soil parameters for BUEK id (leg1000) = " << leg1000_id << endl;
      cout << "Exiting now running simulation ..." << endl;
      cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
  }
  delete con;
  return sps;
}


/**
 * Create theoretical crop rotation based on julian sowing date.
 * Harvesting is always at 31.07., tillage on day after harvest.
 * @param start_date Start date of simulation
 * @param end_date End date of simulation
 */
std::vector<ProductionProcess>
Monica::getCropManagementData(int crop_id, std::string start_date_s, std::string end_date_s, double julian_sowing_date)
{
  debug() << "----------------------------------------------------------------" << endl;
  debug() << "CC: getCropManagementData: " << crop_id << endl;
  debug() << "Start: " << start_date_s << endl;
  debug() << "End: " << end_date_s << endl;
  debug() << "----------------------------------------------------------------" << endl;
  static mutex lockable;
  lock_guard<mutex> lock(lockable);
  
  Tools::Date start_date = Tools::fromMysqlString(start_date_s.c_str());
  Tools::Date end_date = Tools::fromMysqlString(end_date_s.c_str());

  unsigned int current_year = start_date.year();
  unsigned int end_year = end_date.year();
  debug() << "Current year: " << current_year << endl;

  vector<ProductionProcess> ff;

  while (current_year<end_year) {

      // get crop pointer with correct crop od
      std::string crop_name = "Winterweizen";
      CropPtr crop = CropPtr(new Crop(crop_id, crop_name.c_str()));
      debug() << "CropId:\t" << crop->id() << endl;

      // sowing and harvest date
      Tools::Date sowing_date(1,1,current_year);
      sowing_date += julian_sowing_date;
      Tools::Date harvest_date(31,7,current_year+1);
      debug() << "Sowing Date:\t" << sowing_date.toString() << endl;
      debug() << "Harvest Date:\t" << harvest_date.toString() << endl;

      crop->setSeedAndHarvestDate(sowing_date, harvest_date);
      crop->setCropParameters(getCropParametersFromMonicaDB(crop->id()));
      crop->setResidueParameters(getResidueParametersFromMonicaDB(crop->id()));

      ProductionProcess pp(crop_name, crop);

      // tillages directly one day after harvest
      Tools::Date tillages_date = harvest_date + 1;
      pp.addApplication(TillageApplication(tillages_date, 0.3));
      ff.push_back(pp);

      current_year++;
  } // while

  return ff;
}

/**
 *
 */
Climate::DataAccessor
Monica::climateDataForCCGermany(int stat_id, std::string start_date_s, std::string end_date_s, std::string realisation, CentralParameterProvider& cpp)
{
//  debug() << "----------------------------------------------------------------" << endl;
//  debug() << "CC: climateDataForCCGermany: " << stat_id << "\t" << start_date_s.c_str() << "\t" << end_date_s.c_str()<< endl;
//  debug() << "----------------------------------------------------------------" << endl;
//  static L lockable;
//  L::Lock lock(lockable);
//  
//  Tools::Date start_date = Tools::fromMysqlString(start_date_s.c_str());
//  Tools::Date end_date = Tools::fromMysqlString(end_date_s.c_str());
//
//  std::string scenario_name="A1B";
//  std::string realisation_name = realisation;
//  debug() << "realisation:\t" << realisation.c_str() << endl;
//  LatLngCoord llc = getGeoCorrdOfStatId(stat_id);
//  std::string station_name = getStationName(stat_id);
//
//  ClimateSimulation* climate_simulation = new WettRegSimulation(toMysqlDB(Db::newConnection("wettreg")));
//  if(!climate_simulation) {
//      cout << "There is no simulation for stat_id: " << stat_id << " supported right now!" << endl;
//      return DataAccessor();
//  }
//
//  ClimateScenario* scenario = climate_simulation->scenario(scenario_name);
//  if(!scenario) {
//      if(climate_simulation->scenarios().size() == 1) {
//
//          scenario = climate_simulation->scenarios().front();
//          cout << "There is no scenario: " << scenario_name
//              << " but there is only one anyway, so choosing: "
//              << scenario->name() << " instead of ending." << endl;
//      }
//      else {
//          cout << "There is no scenario: " << scenario_name.c_str() << "." << endl;
//          return DataAccessor();
//      }
//  }
//  AvailableClimateData acds[] = {tmin, tavg, tmax, globrad, relhumid, wind, precip, sunhours};
//  typedef map<string, DataAccessor> DAS;
//  DAS das;
//
//  BOOST_FOREACH(ClimateRealization* r, scenario->realizations()) {
//    das[r->name()] = ((ClimateRealization*)r)->dataAccessorFor(vector<ACD > (acds, acds + 8), llc, start_date, end_date);
//  }
//  delete climate_simulation;
  //return das[realisation_name];
}

LatLngCoord
Monica::getGeoCorrdOfStatId(int stat_id)
{
  debug() << "----------------------------------------------------------------" << endl;
  debug() << "CC: getGeoCorrdOfStatId: " << stat_id << endl;
  debug() << "----------------------------------------------------------------" << endl;
  LatLngCoord llc;

  static mutex lockable;

  lock_guard<mutex> lock(lockable);

  ostringstream request;
  request << "SELECT breite_dez, laenge_dez FROM header h where stat_id=" << stat_id;
  debug() << request << endl;

  // connect to database eva2
  DB *con = newConnection("wettreg");
  DBRow row;
  con->select(request.str().c_str());

  double longitude = 0.0;
  double lattitude = 0.0;
  while (!(row = con->getRow()).empty()) {
      lattitude = Tools::atof_comma(row[0].c_str());
      longitude = Tools::atof_comma(row[1].c_str());
  }
  debug() << "getGeoCorrdOfStatId: " << lattitude << "\t" << longitude << endl;
  llc=LatLngCoord(lattitude, longitude);
  
  delete con;
  return llc;
}

double
Monica::getLatitudeOfStatId(int stat_id)
{
  debug() << "----------------------------------------------------------------" << endl;
  debug() << "CC: getLatitudeOfStatId: " << stat_id << endl;
  debug() << "----------------------------------------------------------------" << endl;

  LatLngCoord llc;

  static mutex lockable;

  lock_guard<mutex> lock(lockable);

  ostringstream request;
  request << "SELECT breite_dez FROM header h where stat_id=" << stat_id;
  debug() << request << endl;

  // connect to database eva2
  DB *con = newConnection("wettreg");
  DBRow row;
  con->select(request.str().c_str());

  double lattitude = 0.0;
  while (!(row = con->getRow()).empty()) {
      lattitude = Tools::atof_comma(row[0].c_str());
  }
  debug() << "getLAttitude: " << lattitude << endl;
  delete con;
  return lattitude;
}


std::string
Monica::getStationName(int stat_id)
{
  debug() << "----------------------------------------------------------------" << endl;
  debug() << "CC: getSTationName: " << stat_id << endl;
  debug() << "----------------------------------------------------------------" << endl;
  LatLngCoord llc;

  static mutex lockable;
  lock_guard<mutex> lock(lockable);

  ostringstream request;
  request << "SELECT datei_name FROM wettreg_stolist h where stat_id=" << stat_id;
  debug() << request << endl;

  // connect to database eva2
  DB *con = newConnection("wettreg");
  DBRow row;
  con->select(request.str().c_str());

  std::string name;
  while (!(row = con->getRow()).empty()) {
      name = row[0];
  }

  delete con;
  return name;
}

int
Monica::getDatId(int stat_id)
{
  debug() << "----------------------------------------------------------------" << endl;
  debug() << "CC: getSTationName: " << stat_id << endl;
  debug() << "----------------------------------------------------------------" << endl;


  debug () << "1" << endl;
  LatLngCoord llc;
  debug () << "2" << endl;
  static mutex lockable;
  debug () << "3" << endl;
  lock_guard<mutex> lock(lockable);
  debug () << "4" << endl;
  ostringstream request;
  request << "SELECT dat_id FROM wettreg_stolist h where stat_KE=\"Klim\" AND stat_id=" << stat_id;
  debug() << request.str().c_str() << endl;
  debug () << "5" << endl;
  // connect to database eva2
  DB *con = newConnection("wettreg");
  DBRow row;
  con->select(request.str().c_str());

  int dat_id;
  while (!(row = con->getRow()).empty()) {
      dat_id = satoi(row[0]);
  }

    debug () << "dat_id: " << dat_id << endl;

  delete con;
  return dat_id;
}


Climate::DataAccessor
Monica::climateDataForCCGermany2(int stat_id, std::string start_date_s, std::string end_date_s, std::string realisation, CentralParameterProvider& cpp)
{
  static mutex lockable;
  lock_guard<mutex> lock(lockable);

  debug() << "----------------------------------------------------------------" << endl;
  debug() << "CC: climateDataForCCGermany2: " << stat_id << "\t" << start_date_s.c_str() << "\t" << end_date_s.c_str()<< endl;
  debug() << "----------------------------------------------------------------" << endl;

  int dat_id = stat_id; //getDatId(stat_id);

  Tools::Date start_date = Tools::fromMysqlString(start_date_s.c_str());
  Tools::Date end_date = Tools::fromMysqlString(end_date_s.c_str());

  Date date = start_date;

  std::string scenario_name="A1B";
  std::string realisation_name = realisation;
  debug() << "realisation:\t" << realisation.c_str() << endl;


  // temporary vectors
  vector<double> _tmin;
  vector<double> _tmax;
  vector<double> _tavg;
  vector<double> _relhumid;
  vector<double> _wind;
  vector<double> _precip;
  vector<double> _sunhours;

  ostringstream request;
  request << "SELECT tx, tm, tn, rf, rr, sd, ff FROM wettreg_data WHERE ";
  request << "dat_id=" << dat_id << " AND Jahr>=" << start_date.year() << " AND ";
  request << "Jahr<=" << end_date.year() << " AND ";
  request << "Realisierung=\"" << realisation << "\" AND ";
  request << "Szenario=\"" << scenario_name << "\" order by jahr, monat, tag ASC ";


  debug() << "\n" << request.str() << endl <<  endl;

  DB* con = newConnection("wettreg");
  DBRow row;
  con->select(request.str().c_str());
  while (!(row = con->getRow()).empty()) {

      _tmax.push_back(satof(row[0]));
      _tavg.push_back(satof(row[1]));
      _tmin.push_back(satof(row[2]));
      _relhumid.push_back(satof(row[3]));

      // correction of precipitation values
      double precip = satof(row[4]);
      precip*=cpp.getPrecipCorrectionValue(date.month()-1);
      _precip.push_back(precip);
      date++;


      _sunhours.push_back(satof(row[5]));
      _wind.push_back(satof(row[6]));
  }
  // number of days between start date and end date
  unsigned int days = (unsigned int)start_date.numberOfDaysTo(end_date) + 1;

  debug() << "Days: " << days <<
      "\tWIND " << _wind.size() <<
      "\tTMIN " << _tmin.size() <<
      "\tTMAX " << _tmax.size() <<
      "\tTAVG " << _tavg.size() <<
      "\tRELHUMID " << _relhumid.size() <<
      "\tPRECIP " << _precip.size() <<
      "\tSUNHOURS " << _sunhours.size() <<

      endl;

  // assert that enough values have been read from data base
  //    assert(_tmin.size()==days);
  //    assert(_tmax.size()==days);
  //    assert(_tavg.size()==days);
  //    assert(_relhumid.size()==days);
  //    assert(_wind.size()==days);
  //    assert(_precip.size()==days);


  DataAccessor da(start_date, end_date);

  // add climate data to data accessor
  da.addClimateData(tmin, _tmin);
  da.addClimateData(tmax, _tmax);
  da.addClimateData(tavg, _tavg);
  da.addClimateData(relhumid, _relhumid);
  da.addClimateData(wind, _wind);
  da.addClimateData(precip, _precip);
  da.addClimateData(sunhours, _sunhours);

  delete con;
  return da;
}





int 
Monica::numberOfPossibleSteps(std::string start_date_s, std::string end_date_s)
{
  Tools::Date start_date = Tools::fromMysqlString(start_date_s.c_str());
  Tools::Date end_date = Tools::fromMysqlString(end_date_s.c_str());
  return start_date.numberOfDaysTo(end_date);
}



#endif
