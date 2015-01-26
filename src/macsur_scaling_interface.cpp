/**
Authors: 
Xenia Specka <xenia.specka@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) 2007-2013, Leibniz Centre for Agricultural Landscape Research (ZALF)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "macsur_scaling_interface.h"
#include "tools/algorithms.h"
#include "db/abstract-db-connections.h"
#include "simulation.h"
#include "conversion.h"

#include "boost/foreach.hpp"

#define LOKI_OBJECT_LEVEL_THREADING
#include "loki/Threads.h"

using namespace Db;
using namespace std;
using namespace Monica;
using namespace Tools;
using namespace Climate;


/**
 * @brief Lockable object
 */
struct L: public Loki::ObjectLevelLockable<L> {};

/**
 * Method for starting a simulation with coordinates from ascii grid.
 * A configuration object is passed that stores all relevant
 * information e.g. location, output path etc.
 *
 * @param simulation_config Configuration object that stores all simulation information
 */
void
Monica::runMacsurScalingSimulation(const MacsurScalingConfiguration *simulation_config)
{
  //std::cout << "Start Macsur Scaling Simulation" << std::endl;

  int phase = simulation_config->getPhase();
  int step = simulation_config->getStep();

  std::string input_path = simulation_config->getInputPath();
  std::string output_path = simulation_config->getOutputPath();

  // read in ini - file ------------------------------------------
  IniParameterMap ipm(input_path + simulation_config->getIniFile());

  // files input
  std::string soil_file = ipm.value("files", "soil");




  std::string crop_rotation_file  = ipm.value("files", "croprotation");
  std::string fertilisation_file = ipm.value("files", "fertiliser");

  //std::cout << "soil_file: " << soil_file.c_str() << "\t" << ipm.value("files", "soil").c_str() << endl;
  //std::cout << "crop_rotation_file: " << crop_rotation_file.c_str() << endl;
  //std::cout << "fertilisation_file: " << fertilisation_file.c_str() << endl << endl;


  bool use_automatic_irrigation = ipm.valueAsInt("automatic_irrigation", "activated") == 1;
  double amount, treshold, nitrate, sulfate;
  if (use_automatic_irrigation) {
      amount = ipm.valueAsDouble("automatic_irrigation", "amount", 0.0);
      treshold = ipm.valueAsDouble("automatic_irrigation", "treshold", 0.15);
      nitrate = ipm.valueAsDouble("automatic_irrigation", "nitrate", 0.0);
      sulfate = ipm.valueAsDouble("automatic_irrigation", "sulfate", 0.0);
  }
  //  std::cout << "use_automatic_irrigation: " << use_automatic_irrigation << endl;
  //  std::cout << "amount: " << amount << endl;
  //  std::cout << "treshold: " << treshold << endl;
  //  std::cout << "nitrate: " << nitrate << endl;
  //  std::cout << "sulfate: " << sulfate << endl << endl;


  //site configuration
  double latitude = ipm.valueAsDouble("site_parameters", "latitude", -1.0);
  double slope = ipm.valueAsDouble("site_parameters", "slope", -1.0);
  double heightNN = ipm.valueAsDouble("site_parameters", "heightNN", -1.0);
  double CN_ratio = ipm.valueAsDouble("site_parameters", "soilCNRatio", -1.0);
  double atmospheric_CO2 = ipm.valueAsDouble("site_parameters", "atmospheric_CO2", -1.0);
  double wind_speed_height = ipm.valueAsDouble("site_parameters", "wind_speed_height", -1.0);
  double leaching_depth = ipm.valueAsDouble("site_parameters", "leaching_depth", -1.0);
  double critical_moisture_depth = ipm.valueAsDouble("site_parameters", "critical_moisture_depth", -1.0);
  double ph = ipm.valueAsDouble("site_parameters", "pH", -1.0);

  //  std::cout << "latitude: " << latitude << endl;
  //  std::cout << "slope: " << slope << endl;
  //  std::cout << "heightNN: " << heightNN << endl;
  //  std::cout << "CN_ratio: " << CN_ratio << endl;
  //  std::cout << "atmospheric_CO2: " << atmospheric_CO2 << endl;
  //  std::cout << "wind_speed_height: " << wind_speed_height << endl;
  //  std::cout << "leaching_depth: " << leaching_depth << endl;
  //  std::cout << "critical_moisture_depth: " << critical_moisture_depth << endl;
  //  std::cout << "ph: " << ph << endl << endl;

  // general parameters
  bool n_response = ipm.valueAsInt("general_parameters", "nitrogen_response_on", 1) == 1;
  bool water_deficit_response = ipm.valueAsInt("general_parameters", "water_deficit_response_on", 1) == 1;
  bool emergence_flooding_control = ipm.valueAsInt("general_parameters", "emergence_flooding_control_on", 1) == 1;
  bool emergence_moisture_control = ipm.valueAsInt("general_parameters", "emergence_moisture_control_on", 1) == 1;
  //  std::cout << "n_response: " << n_response << endl;
  //  std::cout << "water_deficit_response: " << water_deficit_response << endl << endl;

  // initial values
  double init_FC = ipm.valueAsDouble("init_values", "init_percentage_FC", -1.0);


  //  std::cout << "init_FC: " << init_FC << endl;

  // ---------------------------------------------------------------------

  CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase(Env::MODE_MACSUR_SCALING);
  centralParameterProvider.userEnvironmentParameters.p_AthmosphericCO2 = atmospheric_CO2;
  centralParameterProvider.userEnvironmentParameters.p_WindSpeedHeight = wind_speed_height;
  centralParameterProvider.userEnvironmentParameters.p_LeachingDepth = leaching_depth;
  centralParameterProvider.userInitValues.p_initPercentageFC = init_FC;


  SiteParameters siteParams;
  siteParams.vs_Latitude = simulation_config->getLatitude();
  siteParams.vs_Slope = 0.01;
  siteParams.vs_HeightNN = simulation_config->getElevation();
  siteParams.vs_Soil_CN_Ratio = CN_ratio; // TODO: xeh CN_Ratio aus Bodendatei auslesen

  //cout << "Site parameters " << endl;
  //cout << siteParams.toString().c_str() << endl;

  double layer_thickness = centralParameterProvider.userEnvironmentParameters.p_LayerThickness;
  double profile_depth = layer_thickness * double(centralParameterProvider.userEnvironmentParameters.p_NumberOfLayers);
  double max_mineralisation_depth = 0.4;

  GeneralParameters gps = GeneralParameters(layer_thickness, profile_depth, max_mineralisation_depth, n_response, water_deficit_response,emergence_flooding_control,emergence_moisture_control);

  //soil data
  const SoilPMs* sps;

  std::string project_id = simulation_config->getProjectId();
  std::string lookup_project_id = simulation_config->getLookupProjectId();

  if (phase == 1) {
        sps = soilParametersFromFile(soil_file, gps, ph);
    
  } else {
        // phase 2
        if (step == 1) {
            if (simulation_config->getProjectId() == "dominant_soil") {

                cout << "Phase 2, dominant soil" << endl;
                sps = soilParametersFromFile(simulation_config->getSoilFile(), gps, ph);

            } else {

                //cout << "Phase 2, other soil" << endl;
                sps = phase2SoilParametersFromFile(simulation_config->getSoilFile(), gps, centralParameterProvider, ph, project_id);
            }    
        }    
        else {

            if (step == 2) {
                // climate res1, different soil res
                //cout << "Phase 2 step 2\t" << simulation_config->getSoilFile().c_str() << "\t" << lookup_project_id <<  endl;
                sps = phase2SoilParametersFromFile(simulation_config->getSoilFile(), gps, centralParameterProvider, ph, lookup_project_id);

            } else if (step == 3) {
                // different climate res, soil res 1
                //cout << "Phase 2 step 3\t" << simulation_config->getSoilFile().c_str() << endl;
                sps = phase2SoilParametersFromFile(simulation_config->getSoilFile(), gps, centralParameterProvider, ph, project_id);
            }

        }
  }

  //cout << "Groundwater min:\t" << centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepth << endl;
  //cout << "Groundwater max:\t" << centralParameterProvider.userEnvironmentParameters.p_MaxGroundwaterDepth << endl;
  //cout << "Groundwater month:\t" << centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepthMonth << endl;

  // climate data
  Climate::DataAccessor climateData = climateDataFromMacsurFiles(simulation_config->getClimateFile(), centralParameterProvider, latitude, simulation_config);
  //cout << "Return from climateDataFromMacsurFiles" << endl;

  // fruchtfolge
  std::string file = input_path + crop_rotation_file;
  vector<ProductionProcess> ff = cropRotationFromHermesFile(file);
  //cout << "Return from cropRotationFromHermesFile" << endl;
  // fertilisation
  file = input_path + fertilisation_file;
  attachFertiliserApplicationsToCropRotation(ff, file);
  //cout << "Return from attachFertiliserApplicationsToCropRotation" << endl;
//  BOOST_FOREACH(const ProductionProcess& pv, ff){
//    cout << "pv: " << pv.toString() << endl;
//  }


  //build up the monica environment
  Env env(sps, centralParameterProvider);
  env.general = gps;
  env.pathToOutputDir = output_path;
  env.setMode(Env::MODE_MACSUR_SCALING);
  env.site = siteParams;

  env.da = climateData;
  env.cropRotation = ff;

  if (use_automatic_irrigation) {
    env.useAutomaticIrrigation = true;
    env.autoIrrigationParams = AutomaticIrrigationParameters(amount, treshold, nitrate, sulfate);
  }
  //cout << env.toString() << endl;
  //cout << "Before runMonica" << endl;
  const Monica::Result& res = runMonica(env);
  //cout << "After runMonica" << endl;
}

/**
 * Read soil information from specialised soil file
 */
const SoilPMs* Monica::soilParametersFromFile(const string pathToFile, const GeneralParameters& gps, double soil_ph)
{

  int lt = int(gps.ps_LayerThickness.front() * 100); //cm
  int maxDepth = int(gps.ps_ProfileDepth) * 100; //cm
  int maxNoOfLayers = int(double(maxDepth) / double(lt));


  vector<SoilParameters>* sps = new vector<SoilParameters> ;

  static L lockable;
  L::Lock lock(lockable);

  ifstream ifs(pathToFile.c_str(), ios::binary);
  if (! ifs.good()) {
      cerr << "Could not open file " << pathToFile.c_str() << " . Aborting now!" << endl;
      exit(1);
  }

  string s;

  //skip first line(s)
  getline(ifs, s);
  getline(ifs, s);

  int currenth = 1;
  int hu = 0, ho = 0;
  while (getline(ifs, s)) {
      if (trim(s) == "end")
        break;

      std::string name;
      double hor_thickness, wilting_point, field_cap, saturation;
      double air_capacity, SOC, soil_nitrogen, ph, clay, silt, sand, stone;
      double bulk_dens, cn, raw_dens, soil_ammonium, soil_nitrate;

      istringstream ss(s);
      //cout << s << endl;
      ss >> name >> hor_thickness >> wilting_point >> field_cap >> saturation >>
            air_capacity >> SOC >> soil_nitrogen >> ph >> clay >> silt >> sand >> stone >> bulk_dens >>
            cn >> raw_dens >> soil_nitrate >> soil_ammonium;


      hu += int(hor_thickness*100.0);

      double vs_SoilSpecificMaxRootingDepth = 1.5; // [m]

      int hsize = hu - ho;


      int subhcount = int(Tools::round(double(hsize / 10.0)));

      SoilParameters p;
      p.set_vs_SoilOrganicCarbon(SOC/100.0);
      p.set_vs_SoilRawDensity(raw_dens);
      p.vs_SoilSandContent = sand/100.0;
      p.vs_SoilClayContent = clay/100.0;
      p.vs_SoilStoneContent = stone / 100.0;

      p.vs_Lambda = texture2lambda(p.vs_SoilSandContent, p.vs_SoilClayContent);

      p.vs_SoilTexture = "";
      p.vs_SoilpH = ph;

      p.vs_FieldCapacity = field_cap;
      p.vs_Saturation = saturation;
      p.vs_PermanentWiltingPoint = wilting_point;

      // TODO Umrechnung von Claas prüfen lassen
      p.vs_SoilAmmonium = (soil_ammonium / subhcount) / (0.1 * 10000); // kg ha-1 --> kg m-3
      p.vs_SoilNitrate = (soil_nitrate / subhcount)  / (0.1 * 10000);  // kg ha-1 --> kg m-3

      bool valid_soil_params = p.isValid();
      if (!valid_soil_params) {
          cout << "Error in soil parameters. Aborting now simulation";
          exit(-1);
      }

      for (int i = 0; i < subhcount; i++) {
        if (sps->size()<maxNoOfLayers) {
          sps->push_back(p);
          //cout << "Layer " << sps->size() << endl << p.toString().c_str() << endl;
        }
      }

      currenth++;

      ho = hu;

  } // while


  return sps;
}


/**
 * Read soil information from specialised soil file of phase 2, step 1
 */
const SoilPMs* Monica::phase2SoilParametersFromFile(const string pathToFile, const GeneralParameters& gps, CentralParameterProvider &cpp, double soil_ph, std::string project_id)
{

   //cout << "pathToFile: " << pathToFile << endl;

  int lt = int(gps.ps_LayerThickness.front() * 100); //cm
  int maxDepth = int(gps.ps_ProfileDepth) * 100; //cm
  int maxNoOfLayers = int(double(maxDepth) / double(lt));

  //cout << "phase2SoilParametersFromFile\t"  << maxDepth << "\t" << maxNoOfLayers<< endl;

  vector<SoilParameters>* sps = new vector<SoilParameters> ;

  static L lockable;

  L::Lock lock(lockable);

  ifstream ifs(pathToFile.c_str(), ios::binary);
  if (! ifs.good()) {
      cerr << "Could not open file " << pathToFile.c_str() << " . Aborting now!" << endl;
      exit(1);
  }

  string s;

  //skip first line(s)
  getline(ifs, s);

  int currenth = 1;
  int hu = 0, ho = 0;

  SoilParameters last_soil_parameters;
  double soil_nitrate_percentage = 0.9;
  double soil_nitrogen = 0.0;

  while (getline(ifs, s)) {
      
      if (trim(s) == "end")
        break;



      std::string pid, tmp;
      int row, col, nr_horizon, layer_nr;
      double depth, hor_thickness, wilting_point, field_cap, saturation;
      double air_capacity, SOC, soil_nitrogen, ph, clay, silt, sand, stone;
      double bulk_dens, cn, raw_dens;
      double water_table_min, water_table_max;

      istringstream ss(s);
      //cout << s << endl;
      ss >> tmp >> pid;
      //cout << "project_id:\t" << project_id << "\t\tpid:\t" << pid << endl;

      if (pid == project_id) {
      
        ss >> tmp >> col >> row >> tmp >> tmp >>tmp >>tmp >>tmp; //9 grid_id	COLUMN	ROW	Rowid_	COUNT	AREA	MAJORITY	NAME
        ss >> tmp >> tmp >>tmp >>tmp; // 13 ART ART_TEXT TYP_TEXT_N soil_OID
        ss >> nr_horizon >> layer_nr >> depth >> hor_thickness; // 17 NumberLaye LayerNumbe Depth Thickness
        ss >> tmp >> wilting_point >> field_cap >> saturation >> air_capacity >> tmp >> tmp; // 24 WCAD WCWP WCFC WCST AirCapacit AWC AWCcum
        ss >> tmp >> water_table_min >> water_table_max >> clay >> silt >> sand >> stone; // HydGroup WTmin WTmax Clay Silt Sand Gravel
        ss >> tmp >> bulk_dens >> tmp >> SOC >> cn >> ph; // Gravel_Vol BD_Fe BD_tot Corg CN pH_1
        //ss >> tmp >> tmp >> tmp>> tmp>> tmp>> tmp>> tmp;  // gkx, gky, oid_f, id_f, salb, CaCO3, WCAD_2
        //ss >> wilting_point >> field_cap >> saturation >> air_capacity




        // ignore stone content because we use already corrected values for FC, SAT and PWP that
        // have been provided by the macsur organisation
        stone = 0.0;

        if (stone>=70.0) stone=70.0;
        if (bulk_dens<=0.57) bulk_dens=0.57;

        if (water_table_min==0.0) water_table_min = 20.0;
        if (water_table_max==0.0) water_table_min = 20.0;

        cpp.userEnvironmentParameters.p_MinGroundwaterDepth = water_table_min;
        cpp.userEnvironmentParameters.p_MaxGroundwaterDepth = water_table_max;
        cpp.userEnvironmentParameters.p_MinGroundwaterDepthMonth = 3;

        raw_dens = bulk_dens- (0.009 * clay);


//        cout << "SOC:\t" << SOC << endl;
//        cout << "raw_dens:\t" << raw_dens << endl;
//        cout << "sand:\t" << sand << endl;
//        cout << "clay:\t" << clay << endl;
//        cout << "stone:\t" << stone << endl;
//        cout << "ph:\t" << ph << endl;
//        cout << "field_cap:\t" << field_cap << endl;
//        cout << "saturation:\t" << saturation << endl;
//        cout << "cn:\t" << cn << endl;
//        cout << "soil_nitrogen:\t" << soil_nitrogen << endl;
//
//        cout << "water_table_min:\t" << water_table_min << endl;
//        cout << "water_table_max:\t" << water_table_max << endl;

        hu += int(hor_thickness*100.0);

        double vs_SoilSpecificMaxRootingDepth = 1.5; // [m]

        int hsize = hu - ho;


        int subhcount = int(Tools::round(double(hsize / 10.0)));
        //cout << "subhcount:\t" << subhcount << endl;
        SoilParameters p;


        p.set_vs_SoilOrganicCarbon(SOC/100.0);
        p.set_vs_SoilRawDensity(raw_dens);
        p.vs_SoilSandContent = sand/100.0;
        p.vs_SoilClayContent = clay/100.0;
        p.vs_SoilStoneContent = stone / 100.0;

        p.vs_Lambda = texture2lambda(p.vs_SoilSandContent, p.vs_SoilClayContent);

        p.vs_SoilTexture = texture2KA5(p.vs_SoilSandContent, p.vs_SoilClayContent);
        //cout << "SoilTexture:\t" << p.vs_SoilTexture << endl;
        p.vs_SoilpH = ph;

        p.vs_FieldCapacity = field_cap;
        p.vs_Saturation = saturation;
        p.vs_PermanentWiltingPoint = wilting_point;

        // TODO Umrechnung von Claas prüfen lassen


        bool valid_soil_params = p.isValid();
        if (!valid_soil_params) {
          cout << "Error in soil parameters. Aborting now simulation";
          exit(-1);
        }




        for (int i = 0; i < subhcount; i++) {
            if (sps->size()<maxNoOfLayers) {
              int soil_layers = sps->size();
              if (soil_layers<3) {
                soil_nitrogen = 30.0 / 3.0;  // see inital conditions (section 4) of macsur technical information

              } else if (soil_layers<12) {
                soil_nitrogen = 20.0 / 9.0;  // see inital conditions (section 4) of macsur technical information

              } else {
                soil_nitrogen = 5.0 / 8.0;  // see inital conditions (section 4) of macsur technical information
              }
              p.vs_SoilAmmonium = (soil_nitrogen * (1.0-soil_nitrate_percentage) ) / (0.1 * 10000); // kg ha-1 --> kg m-3
              p.vs_SoilNitrate = (soil_nitrogen * soil_nitrate_percentage ) / (0.1 * 10000); // kg ha-1 --> kg m-3
              last_soil_parameters = p;
              sps->push_back(p);
              //cout << "Layer " << sps->size() << endl << p.toString().c_str() << endl;
            }
        }

        currenth++;

        ho = hu;



      } // if

     
  } // while
  while (sps->size()<maxNoOfLayers) {
      int soil_layers = sps->size();
      if (soil_layers<3) {
        soil_nitrogen = 30.0 / 3.0;  // see inital conditions (section 4) of macsur technical information

      } else if (soil_layers<12) {
        soil_nitrogen = 20.0 / 9.0;  // see inital conditions (section 4) of macsur technical information

      } else {
        soil_nitrogen = 5.0 / 8.0;  // see inital conditions (section 4) of macsur technical information
      }
      last_soil_parameters.vs_SoilAmmonium = (soil_nitrogen * (1.0-soil_nitrate_percentage) ) / (0.1 * 10000); // kg ha-1 --> kg m-3
      last_soil_parameters.vs_SoilNitrate = (soil_nitrogen * soil_nitrate_percentage ) / (0.1 * 10000); // kg ha-1 --> kg m-3
      
      sps->push_back(last_soil_parameters);
  }
  return sps;
}


/**
 * Read climate information from macsur file
 */
Climate::DataAccessor
Monica::climateDataFromMacsurFiles(const std::string pathToFile, const CentralParameterProvider& cpp, double latitude, const MacsurScalingConfiguration *simulation_config)
{
  //cout << "climateDataFromMacsurFiles: " << pathToFile << endl;
  DataAccessor da(simulation_config->getStartDate(),simulation_config->getEndDate());

  int day_count = simulation_config->getEndDate()-simulation_config->getStartDate()+1;
  //cout << "start_date: " << simulation_config->getStartDate().toString() << endl;
  //cout << "end_date: " << simulation_config->getEndDate().toString() << endl;

  vector<double> _tmin;
  vector<double> _tavg;
  vector<double> _tmax;
  vector<double> _globrad;
  vector<double> _wind;
  vector<double> _precip;
  vector<double> _relhumid;


  Date date = simulation_config->getStartDate();

  ostringstream yss;
  ifstream ifs(pathToFile.c_str(), ios::binary);
  if (! ifs.good()) {
     cerr << "Could not open file " << pathToFile.c_str() << " . Aborting now!" << endl;
     exit(1);
   }


  string s;

  //skip first line(s)
  getline(ifs, s);

  int days_added = 0;
  while (getline(ifs, s)) {


    //cout << s.size() << "\t" << s.c_str() << endl;

    if (s.size() == 1) {
        // skip empty lines in climate file
        continue;
    }

    //Precipitation TempMin TempMean  TempMax Radiation Windspeed RefET Gridcell
    std::string date;
    double dstr, precip, tmin, tmean, tmax, globrad, windspeed, refet, relhumid;

    istringstream ss(s);
    

    if (simulation_config->getPhase() == 1) {

        // phase 1 climate information
        ss >> dstr >> dstr >>dstr >>precip >> tmin >> tmean >> tmax >> globrad >> windspeed;
        //cout << precip << "\t" << tmin << "\t" << tmean << "\t" << tmax << "\t" << globrad/1000.0 << "\t" << windspeed << endl;

    } else if (simulation_config->getPhase() == 2) {

        // phase 2 climate information
        
        ss >> dstr >> dstr >>dstr >>precip >> tmin >> tmean >> tmax >> globrad >> windspeed >> refet >> relhumid;
        //cout << precip << "\t" << tmin << "\t" << tmean << "\t" << tmax << "\t" << globrad/1000.0 << "\t" << windspeed << "\t" << relhumid << endl;

        _relhumid.push_back(relhumid);
    }




    // HERMES weather files deliver global radiation as [kJ m-2 d-1]
    // Here, we push back [MJ m-2 d-1]
    _globrad.push_back(globrad / 1000.0);

    // precipitation correction by Richter values
    //precip=cpp.getPrecipCorrectionValue(date.month()-1);
    _precip.push_back(precip);

    _tavg.push_back(tmean);
    _tmin.push_back(tmin);
    _tmax.push_back(tmax);
    _wind.push_back(windspeed);

    days_added++;

  } // while


    //cout << "days_added\t" << days_added << endl;
  if (days_added != day_count) {
    cout << "Wrong number of days in " << pathToFile.c_str() << " ." << " Found " << days_added << " days but should have been "
    << day_count << " days. Aborting." << endl;
    exit(1);
  }

  da.addClimateData(tmin, _tmin);
  da.addClimateData(tmax, _tmax);
  da.addClimateData(tavg, _tavg);
  da.addClimateData(globrad, _globrad);
  da.addClimateData(wind, _wind);
  da.addClimateData(precip, _precip);

  if (_relhumid.size()>0) {
     da.addClimateData(relhumid, _relhumid);
  }

  return da;
}
