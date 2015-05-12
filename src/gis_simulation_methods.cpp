/**
 * @file cc_germany_methods.cpp
 */
#if defined RUN_GIS

#include <mutex>

#include "gis_simulation_methods.h"
#include "cc_germany_methods.h"
#include "tools/debug.h"
#include "db/abstract-db-connections.h"
#include "soil/soil.h"

using namespace Db;
using namespace std;
using namespace Monica;
using namespace Tools;
using namespace Climate;
using namespace Soil;

// thuringia simulation

interpolation* inter(NULL);
  
Result
Monica::createGISSimulation(int i, int j, std::string start_date_s, std::string end_date_s, double julian_sowing_date, char* hdf_filename, char* hdf_voronoi, std::string path, int ext_buek_id)
{
    static grid map_height(0);
    static grid map_soil(0);
    static grid map_groundwater_depth(0);
    static grid map_slope(0);
    static grid map_voronoi(0);

  Tools::Date start_date = Tools::fromMysqlString(start_date_s.c_str());
  Tools::Date end_date = Tools::fromMysqlString(end_date_s.c_str());
  int crop_id = 1;  // 1 - Winterweizen
  //Monica::activateDebug = true;
	

  if (map_height.rgr == 0) {
      map_height.read_hdf(hdf_filename,"d25_thu");
  }

  if (map_soil.rgr == 0) {
    map_soil.read_hdf(hdf_filename,"buek1000_thu");
  }

  if (map_groundwater_depth.rgr == 0) {
    map_groundwater_depth.read_hdf(hdf_filename,"water");
  }

  if (map_slope.rgr == 0) {
    map_slope.read_hdf(hdf_filename,"d25sloprz_thu");
  }
  if (map_voronoi.rgr == 0) {
      map_voronoi.read_hdf(hdf_voronoi,"code");
  }

  double grid_rows = map_height.nrows;

  // initialisation of climate interpolation object
  if (inter == NULL) {
      debug() << "Initialization of interpolation library" << endl;
      inter = new interpolation(&map_voronoi, &map_height);
  }
  
  int buek_id = ext_buek_id;
  if (buek_id<=0) {
    buek_id = int(map_soil.get_xy(i,j));
    debug() << buek_id << endl;	
  }

  double rwert = map_height.xcorner+(j+0.5)*map_height.csize;
  double hwert = map_height.ycorner+(grid_rows-(i+0.5))*map_height.csize;
  double height_nn = map_height.get_xy(i,j);
  double slope = map_slope.get_xy(i,j);

  debug() << height_nn << endl;
 if ( (height_nn == map_height.nodata) or (buek_id==72)) {
   debug() << "Breaking" << endl;
	return Result();
 }

  double llc = GK52Latitude(rwert, hwert);
  double gw = map_groundwater_depth.get_xy(i,j);
  if (gw<0) { 
     gw = 20.0;
  }
  debug() << "--------------------------------------" << endl;
  debug() << "BUEK_ID:\t" << buek_id << endl;
  debug() << "Sowing Day:\t" << julian_sowing_date << endl;
  debug() << "Groundwater:\t" << gw << endl;
  debug() << "Period:\t\t" << start_date.toString().c_str() << " - " << end_date.toString().c_str() << endl;
  debug() << "Crop Id:\t" << crop_id << endl;
  debug() << "Height:\t" << height_nn << endl;
  debug() << "i:\t" << i << endl;
  debug() << "j:\t" << j << endl;
  debug() << "--------------------------------------" << endl;

  // read user defined values from database
  CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase(Env::MODE_HERMES);
  centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepth=gw;
  centralParameterProvider.userEnvironmentParameters.p_MaxGroundwaterDepth=gw;

  double leaching_depth = centralParameterProvider.userEnvironmentParameters.p_LeachingDepth;
  if (gw<leaching_depth) {
      centralParameterProvider.userEnvironmentParameters.p_LeachingDepth = gw-0.2;
  }
  SiteParameters site_params;  
  site_params.vs_Latitude = llc;
  site_params.vs_Slope = (slope/100.0);

  double layer_thickness = centralParameterProvider.userEnvironmentParameters.p_LayerThickness;
  double profile_depth = layer_thickness * float(centralParameterProvider.userEnvironmentParameters.p_NumberOfLayers);
  GeneralParameters general_parameters(layer_thickness, profile_depth);
      
  // read soil parameters for a special profile e.g. Dornburg
  const SoilPMs* sps = readBUEKDataFromMonicaDB(buek_id, general_parameters);
  if (sps == NULL) {
      cout << "Error while reading soil data from BUEK database. Received NULL SoilParameters.\nTrying again ..." << endl;
      sleep(1000);
      sps = readBUEKDataFromMonicaDB(buek_id, general_parameters);
      if (sps == NULL) {
        return Result();
      }
  }

  // fruchtfolge
  vector<ProductionProcess> ff = getCropManagementData(crop_id, start_date_s, end_date_s, julian_sowing_date);

  int days = start_date.numberOfDaysTo(end_date);

  std::vector<double> _tmin;
  std::vector<double> _tmax;
  std::vector<double> _tavg;
  std::vector<double> _relhumid;
  std::vector<double> _wind;
  std::vector<double> _globrad;
  std::vector<double> _precip;

  for (int day=0; day<days; day++) {
    _tavg.push_back(inter->get_TM(day,hwert, rwert, height_nn));
    _wind.push_back(inter->get_FF(day,hwert, rwert,height_nn));
    _tmax.push_back(inter->get_TX(day, hwert, rwert, height_nn));
    _tmin.push_back(inter->get_TN(day, hwert, rwert,height_nn));
    _precip.push_back(inter->get_RR(day, hwert, rwert,height_nn));
    _globrad.push_back(inter->get_GS(day, hwert, rwert, height_nn) * 0.01);     // J cm-2 --> MJ m-2 d-1
    _relhumid.push_back(inter->get_RF(day, hwert, rwert, height_nn));
  }

  DataAccessor da(start_date, end_date);

  da.addClimateData(tmin, _tmin);
  da.addClimateData(tmax, _tmax);
  da.addClimateData(tavg, _tavg);
  da.addClimateData(relhumid, _relhumid);
  da.addClimateData(wind, _wind);
  da.addClimateData(precip, _precip);
  da.addClimateData(globrad, _globrad);
  
    
  // build up the monica environment
  Env env(sps, centralParameterProvider);
  env.da = da;  
  env.general = general_parameters;
  env.site = site_params;
  env.pathToOutputDir = path;
  env.setCropRotation(ff);
  env.useNMinMineralFertilisingMethod=1;
  env.setMode(Env::MODE_CC_GERMANY);

  env.nMinFertiliserPartition = getMineralFertiliserParametersFromMonicaDB(1);
  env.nMinUserParams.min = 10;
  env.nMinUserParams.max = 100;
  env.nMinUserParams.delayInDays = 30;

  if (env.soilParams != NULL) {
      const Monica::Result result = runMonica(env);
      return result;
  }

  delete inter;

  return Result();
}


Monica::Result
Monica::createGISSimulationSingleStation(int i, int j, std::string start_date_s, std::string end_date_s, double julian_sowing_date, char* station_id,  char* hdf_filename, std::string path, int soiltype)
{
  Tools::Date start_date = Tools::fromMysqlString(start_date_s.c_str());
  Tools::Date end_date = Tools::fromMysqlString(end_date_s.c_str());
  int crop_id = 1;  // 1 - Winterweizen


    static grid map_height(0);
    static grid map_soil(0);
    static grid map_groundwater_depth(0);
    static grid map_slope(0);
    static grid map_voronoi(0);

  if (map_height.nrows == 0) {
      cout << "Reading height map" << endl;
      map_height.read_hdf(hdf_filename,"d25_thu");
  }

  if (map_soil.nrows == 0 && soiltype<0) {
      cout << "Reading soil map" << endl;
    map_soil.read_hdf(hdf_filename,"buek1000_thu");
  }

  if (map_groundwater_depth.nrows == 0) {
      cout << "Reading groundwater map" << endl;
    map_groundwater_depth.read_hdf(hdf_filename,"water");
  }

  if (map_slope.nrows == 0) {
      cout << "Reading slope map" << endl;
    map_slope.read_hdf(hdf_filename,"d25sloprz_thu");
  }

  double grid_rows = map_height.nrows;


  
  int buek_id =0;
  if (soiltype>0) {
    buek_id = soiltype;
  } else {
    buek_id = int(map_soil.get_xy(i,j));
  }
  double height_nn = map_height.get_xy(i,j);
  double slope = map_slope.get_xy(i,j);

  //cout << "buek_id " << buek_id << "\t" << int(buek_id!=72) << endl;
  //cout << "height_nn " << height_nn << "\t" << int(height_nn != map_height.nodata) << endl;

  double rwert;
  double hwert;      
  double llc;
  double gw;


  // only calc when there is a valid height and buek id (buek = 72 --> torf boden)
  if ( (height_nn != map_height.nodata) and (buek_id!=72)) {
                    
    rwert = map_height.xcorner+(j+0.5)*map_height.csize;
    hwert = map_height.ycorner+(grid_rows-(i+0.5))*map_height.csize;
    llc = GK52Latitude(rwert, hwert);
    gw = map_groundwater_depth.get_xy(i,j);
    if (gw<0) gw = 20.0;

  } else {
    return Result();
  }
  

  debug() << "--------------------------------------" << endl;
  cout << "BUEK_ID:\t" << buek_id << endl;
  debug() << "Sowing Day:\t" << julian_sowing_date << endl;
  cout << "Groundwater:\t" << gw << endl;
  debug() << "Period:\t\t" << start_date.toString().c_str() << " - " << end_date.toString().c_str() << endl;
  debug() << "Crop Id:\t" << crop_id << endl;
  cout << "Height:\t\t" << height_nn << endl;
  debug() << "i:\t" << i << endl;
  debug() << "j:\t" << j << endl;
  debug() << "--------------------------------------" << endl;

  // read user defined values from database
  CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase(Env::MODE_HERMES);
  centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepth=gw;
  centralParameterProvider.userEnvironmentParameters.p_MaxGroundwaterDepth=gw;

  double leaching_depth = centralParameterProvider.userEnvironmentParameters.p_LeachingDepth;
  if (gw<leaching_depth) {
      centralParameterProvider.userEnvironmentParameters.p_LeachingDepth = gw-0.2;
  }
  SiteParameters site_params;  
  site_params.vs_Latitude = llc;
  site_params.vs_Slope = (slope/100.0);

  double layer_thickness = centralParameterProvider.userEnvironmentParameters.p_LayerThickness;
  double profile_depth = layer_thickness * float(centralParameterProvider.userEnvironmentParameters.p_NumberOfLayers);
  GeneralParameters general_parameters(layer_thickness, profile_depth);
      
  // read soil parameters for a special profile e.g. Dornburg

  static std::map<int, const SoilPMs*> buek_map;

  const SoilPMs* sps;
  std::map<int, const SoilPMs*>::iterator sps_it = buek_map.find(buek_id);

  if (sps_it==buek_map.end()) {
    // sps not available 
    cout << "Look up new buek data for " << buek_id << endl;
    sps = readBUEKDataFromMonicaDB(buek_id, general_parameters);
    buek_map[buek_id] = sps;
    
  } else {
    sps = sps_it->second;
  }

  if (sps == NULL) {
      cout << "Error while reading soil data from BUEK database. Received NULL SoilParameters.\nAborting simulation ..." << endl;
      return Result();
  }

  // fruchtfolge
  vector<ProductionProcess> ff = getCropManagementData(crop_id, start_date_s, end_date_s, julian_sowing_date);
 
  // get climate data
  static std::map<string, DataAccessor> climate_map;
  std::map<string, DataAccessor>::iterator climate_it = climate_map.find(string(station_id));

  DataAccessor da(start_date, end_date);

  if (climate_it == climate_map.end()) {
    cout << "Look up new climate data for station " << station_id << endl;
    da = getClimateDateOfThuringiaStation(station_id, start_date_s, end_date_s, centralParameterProvider);
    cout << "Adding " << station_id << " to climate map" << endl;    
    climate_map.insert ( pair<string, DataAccessor>(station_id,da));
    
  } else {
    da = climate_it->second;
  }

    
  // build up the monica environment
  Env env(sps, centralParameterProvider);
  env.da = da;  
  env.general = general_parameters;
  env.site = site_params;
  env.pathToOutputDir = path;
  env.setCropRotation(ff);
  env.useNMinMineralFertilisingMethod=1;
  env.setMode(Env::MODE_CC_GERMANY);

  env.nMinFertiliserPartition = getMineralFertiliserParametersFromMonicaDB(1);
  env.nMinUserParams.min = 10;
  env.nMinUserParams.max = 100;
  env.nMinUserParams.delayInDays = 30;

  if (env.soilParams != NULL) {
      const Monica::Result result = runMonica(env);
      return result;
  }

  return Result();
}

/********************************************************************
 *
 *******************************************************************/


Monica::Result
Monica::runSinglePointSimulation(char* station_id, int soiltype, double slope, double height_nn, double gw, std::string start_date_s, std::string end_date_s, double julian_sowing_date, std::string path)
{
  Tools::Date start_date = Tools::fromMysqlString(start_date_s.c_str());
  Tools::Date end_date = Tools::fromMysqlString(end_date_s.c_str());
  int crop_id = 1;  // 1 - Winterweizen



  // read user defined values from database
  CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase(Env::MODE_HERMES);
  centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepth=gw;
  centralParameterProvider.userEnvironmentParameters.p_MaxGroundwaterDepth=gw;

  double leaching_depth = centralParameterProvider.userEnvironmentParameters.p_LeachingDepth;
  if (gw<leaching_depth) {
      centralParameterProvider.userEnvironmentParameters.p_LeachingDepth = gw-0.2;
  }
  SiteParameters site_params;  
  site_params.vs_Latitude = 51.0;
  site_params.vs_Slope = (slope);

  double layer_thickness = centralParameterProvider.userEnvironmentParameters.p_LayerThickness;
  double profile_depth = layer_thickness * float(centralParameterProvider.userEnvironmentParameters.p_NumberOfLayers);
  GeneralParameters general_parameters(layer_thickness, profile_depth);
      
  // read soil parameters for a special profile e.g. Dornburg


  static std::map<int, const SoilPMs*> buek_map;
  const SoilPMs* sps;
  sps = readBUEKDataFromMonicaDB(soiltype, general_parameters);

  if (sps == NULL) {
      cout << "Error while reading soil data from BUEK database. Received NULL SoilParameters.\nAborting simulation ..." << endl;
      return Result();
  }

  // fruchtfolge
  vector<ProductionProcess> ff = getCropManagementData(crop_id, start_date_s, end_date_s, julian_sowing_date);
 

  DataAccessor da = getClimateDateOfThuringiaStation(station_id, start_date_s, end_date_s, centralParameterProvider);
    
  // build up the monica environment
  Env env(sps, centralParameterProvider);
  env.da = da;  
  env.general = general_parameters;
  env.site = site_params;
  env.pathToOutputDir = path;
  env.setCropRotation(ff);
  env.useNMinMineralFertilisingMethod=1;
  env.setMode(Env::MODE_CC_GERMANY);
  //env.setMode(Env::MODE_ACTIVATE_OUTPUT_FILES);

  env.nMinFertiliserPartition = getMineralFertiliserParametersFromMonicaDB(1);
  env.nMinUserParams.min = 10;
  env.nMinUserParams.max = 100;
  env.nMinUserParams.delayInDays = 30;

  if (env.soilParams != NULL) {
      const Monica::Result result = runMonica(env);
      return result;
  }

  return Result();
}





/**
 *
 */
DataAccessor  
Monica::getClimateDateOfThuringiaStation(char *station, std::string start_date_s, std::string end_date_s, CentralParameterProvider& cpp)
{
  static mutex lockable;
  lock_guard<mutex> lock(lockable);
  
  std::string station_name = getThurStationName(std::atoi(station));

  std::cout << "Climate data from " << station_name.c_str() << endl;

  Tools::Date start_date = Tools::fromMysqlString(start_date_s.c_str());
  Tools::Date end_date = Tools::fromMysqlString(end_date_s.c_str());

  std::cout << "Start: " << start_date.toString().c_str() << endl;
  std::cout << "End: " << end_date.toString().c_str() << endl;

  DataAccessor da(start_date, end_date);

  std::vector<double> _tmin;
  std::vector<double> _tmax;
  std::vector<double> _tavg;
  std::vector<double> _relhumid;
  std::vector<double> _wind;
  std::vector<double> _globrad;
  std::vector<double> _precip;

  ostringstream request;
  request << "SELECT tx, tm, tn, rf, rr, gs, ff FROM st_" << station << " WHERE ";
  request << "Jahr>=" << start_date.year() << " AND "; // TODO sz entfernen, nur drin zum testen der algorithmen an der WETTREG2010 datenbank
  request << "Jahr<=" << end_date.year() << " order by jahr, mo, ta ASC ";

  debug() << "\n" << request.str() << endl <<  endl;

  DB* con = newConnection("thuringia");
  DBRow row;
  con->select(request.str().c_str());
    
  Date date = start_date;
  while (!(row = con->getRow()).empty()) {

      _tmax.push_back(satof(row[0]));
      _tavg.push_back(satof(row[1]));
      _tmin.push_back(satof(row[2]));
      _relhumid.push_back(satof(row[3]));

      // correction of precipitation values
      double precip = satof(row[4]);
      precip*=cpp.getPrecipCorrectionValue(date.month()-1);
      _precip.push_back(precip);      
      _globrad.push_back(satof(row[5]) * 0.01);             // J cm-2 --> MJ m-2 d-1
       //cout << "globrad: " << satof(row[5]) * 0.0036 << "\trelhumid: " << satof(row[3])/100.0 << "\tprecip: " << precip << endl;
      _wind.push_back(satof(row[6]));

      date++;
  }

  delete con;

  
  // number of days between start date and end date
  unsigned int days = (unsigned int)start_date.numberOfDaysTo(end_date) + 1;
  debug() << "Days: " << days <<
      "\tWIND " << _wind.size() <<
      "\tTMIN " << _tmin.size() <<
      "\tTMAX " << _tmax.size() <<
      "\tTAVG " << _tavg.size() <<
      "\tRELHUMID " << _relhumid.size() <<
      "\tPRECIP " << _precip.size() <<
      "\tGLOBRAD " << _globrad.size() <<

      endl;

  da.addClimateData(tmin, _tmin);
  da.addClimateData(tmax, _tmax);
  da.addClimateData(tavg, _tavg);
  da.addClimateData(relhumid, _relhumid);
  da.addClimateData(wind, _wind);
  da.addClimateData(precip, _precip);
  da.addClimateData(globrad, _globrad);
  
  return da;
}



/**
 * Return climate station name from thuringia database
 */
std::string
Monica::getThurStationName(int stat_id)
{
  static L lockable;
  L::Lock lock(lockable);

  ostringstream request;
  request << "SELECT name FROM statlist where id=" << stat_id;
  debug() << request << endl;

  // connect to database eva2
  DB *con = newConnection("thuringia");
  DBRow row;
  con->select(request.str().c_str());

  std::string name;
  while (!(row = con->getRow()).empty()) {
      name = row[0];
  }

  delete con;
  return name;
}


/**
 *
 */
std::vector<double>
Monica::getClimateInformation(int x, int y, int date_index, char* hdf_filename, char* hdf_voronoi)
{
    static grid map_height(0);
    static grid map_soil(0);
    static grid map_groundwater_depth(0);
    static grid map_slope(0);
    static grid map_voronoi(0);

  if (map_height.rgr == 0) {
      map_height.read_hdf(hdf_filename,"d25_thu");
  }

  if (map_voronoi.rgr == 0) {
      map_voronoi.read_hdf(hdf_voronoi,"code");
  }

  double grid_rows = map_height.nrows;

  // initialisation of climate interpolation object
  if (inter == NULL) {

      debug() << "Initialization of interpolation library" << endl;
      inter = new interpolation(&map_voronoi, &map_height);
  }
  
  double height_nn = map_height.get_xy(x,y);
  double xpos;
  double ypos;      

  // only calc when there is a valid height and buek id (buek = 72 --> torf boden)
                    
  xpos = map_height.xcorner+x*map_height.csize;
  ypos = map_height.ycorner+(grid_rows-y)*map_height.csize;

    
  std::vector<double> climate_params;

  climate_params.push_back(inter->get_TN(date_index, ypos, xpos, height_nn));    // tmin
  climate_params.push_back(inter->get_TX(date_index, ypos, xpos, height_nn));    // tmax
  climate_params.push_back(inter->get_TM(date_index, ypos, xpos, height_nn));    // tavg
  climate_params.push_back(inter->get_RR(date_index, ypos, xpos, height_nn));    // precip
  climate_params.push_back(inter->get_GS(date_index, ypos, xpos, height_nn) * 0.01); // globrad J cm-2 --> MJ m-2 d-1
  climate_params.push_back(inter->get_FF(date_index, ypos, xpos, height_nn));    // wind
  climate_params.push_back(inter->get_RF(date_index, ypos, xpos, height_nn));    // relhumid


  return climate_params;
}



#endif
