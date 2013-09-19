/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

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

#include "simulation.h"

#include "eva_methods.h"
#include "cc_germany_methods.h"

#include "climate/climate-common.h"

#include <iostream>

#include "soilcolumn.h"
#include "soiltemperature.h"
#include "soilmoisture.h"
#include "soilorganic.h"
#include "soiltransport.h"
#include "crop.h"
#include "debug.h"
#include "monica-parameters.h"
#include "tools/read-ini.h"

#if defined RUN_CC_GERMANY || defined RUN_GIS
#include "grid/grid.h"
#endif


#include <fstream>
#include <sys/stat.h>

#ifdef __unix__
  #include <sys/io.h>
#else
  #include <io.h>
#endif

#include <boost/foreach.hpp>

using namespace Monica;
using namespace std;
using namespace Tools;


#ifdef RUN_EVA

/**
 * Method for starting a simulation with data from eva2 database.
 * A configuration object is passed that stores all relevant
 * information e.g. location, output path etc.
 *
 * @param simulation_config Configuration object that stores all simulation information
 */
const Monica::Result
Monica::runEVA2Simulation(const Eva2SimulationConfiguration *simulation_config)
{
  cout<< "runEVA2Simulation" << endl;
  Monica::activateDebug = true;


  // eva2 controlling parameters
  int location;
  std::string locationName;
  int profil_nr;
  int classification;
  int variante;
  string fruchtfolge;
  std::vector<int> fruchtfolge_glied;
  std::vector<string> fruchtart;
  std::vector<string> fruchtfolgeYear;
  std::vector<int> anlagen;
  string output_path;
  Tools::Date start_date;
  Tools::Date end_date;
  bool pseudo;


  if (simulation_config == 0) {
      debug() << "Using hard coded information for eva2 simulation" << endl;
      location = 18;
      locationName = "guelzow";
      profil_nr = 73;
      classification = 1;
      variante = 1;
      pseudo=false;
      fruchtfolge = "01";

      fruchtfolge_glied.push_back(1);
      fruchtart.push_back("145");       // Sommergerste
      fruchtfolgeYear.push_back("2005");
      anlagen.push_back(1);

      fruchtfolge_glied.push_back(2);
      fruchtart.push_back("041");       // Ölrettich
      fruchtfolgeYear.push_back("2005");
      anlagen.push_back(1);

      fruchtfolge_glied.push_back(3);
      fruchtart.push_back("141");       // Silomais
      fruchtfolgeYear.push_back("2006");
      anlagen.push_back(1);

      fruchtfolge_glied.push_back(4);
      fruchtart.push_back("175");       // Wintertriticale
      fruchtfolgeYear.push_back("2007");
      anlagen.push_back(1);

      fruchtfolge_glied.push_back(5);
      fruchtart.push_back("180");       // Futterhirse
      fruchtfolgeYear.push_back("2007");
      anlagen.push_back(1);

      fruchtfolge_glied.push_back(6);
      fruchtart.push_back("176");       // Winterweizen
      fruchtfolgeYear.push_back("2008");
      anlagen.push_back(1);

      output_path = "./";
      start_date = Tools::Date(1,11,2005,true);
      end_date = Tools::Date(31,12,2008,true);



  } else {
      debug() << "Using extern configuration for eva2 simulation" << endl;

      // eva2 controlling parameters
      location = simulation_config->getLocation();
      locationName = simulation_config->getLocationName();
      profil_nr = simulation_config->getProfil_number();
      classification = simulation_config->getClassification();
      variante = simulation_config->getVariante();
      fruchtfolge = simulation_config->getFruchtFolge();
	  
	  
      output_path = simulation_config->getOutputPath();
      start_date = simulation_config->getStartDate();
      end_date = simulation_config->getEndDate();
	  fruchtart = simulation_config->getFruchtArt();
      fruchtfolge_glied = simulation_config->getFruchtfolgeGlied();
      fruchtfolgeYear = simulation_config->getFruchtfolgeYear();
      anlagen = simulation_config->getFfgAnlage();
	  
      // creating output directory if this does not exist
      
      if (access( output_path.c_str(), 0 ) != 0 ) {
          // creating output dir if this does not exist
          ostringstream command;
          command << "mkdir \"" << output_path << "\"";

		  system(command.str().c_str());
      }
	  debug() << "Test 5" << endl;
      pseudo = simulation_config->isPseudoSimulation();
  }

  debug() << "*******************************************************" << endl;
  debug() << "* Running eva2 simulation" << endl;
  debug() << "* Standort:\t" << location << endl;
  debug() << "* Profil:\t" << profil_nr << endl;
  debug() << "* Startdate:\t" << start_date.toString().c_str() << endl;
  debug() << "* Enddate:\t" << end_date.toString().c_str() << endl;

  unsigned int size = fruchtfolge_glied.size();
  cout << fruchtfolge_glied.size() << endl;
  for (unsigned int i=0; i<size; i++) {
      std::ostringstream id_string;
      int ffg_temp = fruchtfolge_glied.at(i);
      int ffg_anlage = anlagen.at(i);
      std::string ff_temp = fruchtart.at(i);
      std::string ff_year = fruchtfolgeYear.at(i);


      id_string << location << classification << ffg_anlage << fruchtfolge
          << ffg_temp << ff_temp << "_" << ff_year;
      debug() << "* ID:\t\t" << id_string.str().c_str() << endl;
  }

  debug() << "*******************************************************" << endl;


  // read user defined values from database
  CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase(Env::MODE_EVA2);

  readPrecipitationCorrectionValues(centralParameterProvider);
  readGroundwaterInfos(centralParameterProvider, location);

  // read parameters from user environment to initialize general parameters data structure
  double layer_thickness = centralParameterProvider.userEnvironmentParameters.p_LayerThickness;
  double profile_depth = layer_thickness * double(centralParameterProvider.userEnvironmentParameters.p_NumberOfLayers);
  GeneralParameters gps = GeneralParameters(layer_thickness, profile_depth);
  SiteParameters site_parameters = readSiteParametersForEva2(location, profil_nr);


  // read soil parameters for a special profile e.g. Dornburg
  const SoilPMs* sps = readSoilParametersForEva2(gps, profil_nr, location, variante);

  // set leaching depth to effictive rooting depth of soil profile
  double leaching_depth = 1.2; // getEffictiveRootingDepth(profil_nr); // 1.2
  centralParameterProvider.userEnvironmentParameters.p_LeachingDepth = leaching_depth;


  vector<ProductionProcess> ff;

  // read crop rotation for all crops in fruchtfolge
  size = fruchtfolge_glied.size();
  for (unsigned int i=0; i<size; i++) {
      std::ostringstream id_string;
      int ffg_temp = fruchtfolge_glied.at(i);
      int ffg_anlage = anlagen.at(i);
      std::string ff_temp = fruchtart.at(i);
      std::string ff_year = fruchtfolgeYear.at(i);

      id_string << location << classification << ffg_anlage << fruchtfolge
          << ffg_temp << ff_temp << "_" << ff_year;

      vector<ProductionProcess> ff_tmp= getCropManagementData(id_string.str(), ff_temp, location);
      ff_tmp.at(0).toString();
      ff.push_back(ff_tmp.at(0));
  }

  vector<ProductionProcess> ff_orig = ff;

  if (pseudo) {

      debug() << "Generate pseudo simulation" << endl;

      Tools::Date pseudo_start_date = simulation_config->getPseudoStartDate();

      int years_count = (end_date.year()-start_date.year())+1;
      int end_year = end_date.year();
      unsigned int current_year = start_date.year()-1;

      while (current_year>=pseudo_start_date.year()) {
          debug() << "Current year: " << current_year << endl;
          int years_diff = end_year - current_year;
          //vector<ProductionProcess>::iterator it = ff_orig.begin();
          int ff_index = 0;
          BOOST_FOREACH(ProductionProcess& pp, ff_orig){
            //for (; it!=ff_orig.end(); it++) {
            cout << "pp.start " << pp.start().year() << endl;
            cout << "pp.start().year()-years_diff: " << pp.start().year()-years_diff << endl;
            cout << "pseudo_start_date.year(): " << pseudo_start_date.year() << endl;
            if (pp.start().year()-years_diff>=pseudo_start_date.year() ) {

                debug() << "Old ff: " << endl << pp.toString() << endl << endl;

                std::multimap<Tools::Date, WSPtr> worksteps = pp.getWorksteps();

                ProductionProcess new_pp = pp.deepCloneAndClearWorksteps();
                new_pp.clearWorksteps();

                // run through all worksteps of this production process an change
                // date of worksteps
                std::multimap<Tools::Date, WSPtr>::iterator ws_it = worksteps.begin();
                for (; ws_it!=worksteps.end(); ws_it++) {

                    // get workstep pointer that is saved in a smart pointer
                    // and make a copy of it for adding this later as new application
                    WSPtr workstep_pointer = ws_it->second;
                    WorkStep *a = workstep_pointer->clone();

                    // change year of workstep
                    Tools::Date workstep_date = a->date();
                    int new_year = (int)((years_diff / years_count)*years_count);
                    workstep_date.setYear(workstep_date.year()-new_year);
                    a->setDate(workstep_date);

                    // add workstep with new date to new worksteps map
                    new_pp.addApplication(WSPtr(a));
                }

                ff.insert(ff.begin()+ff_index, new_pp);
                debug() << "New pp: " << endl <<  (new_pp).toString() << endl << endl;

                ff_index++;
            }
          }
          current_year-=years_count;

      }


      start_date = pseudo_start_date;
  }

  Climate::DataAccessor da = climateDataFromEva2DB(location, profil_nr, start_date, end_date, centralParameterProvider,site_parameters.vs_Latitude);

  Env env(sps,centralParameterProvider);
  env.setMode(Env::MODE_EVA2);
  env.general = gps;
  env.site = site_parameters;
  env.da=da;
  env.pathToOutputDir = output_path;
  env.cropRotation = ff;


  // write crop rotation data into file
  ostringstream ff_filestream;
  std::transform(locationName.begin(), locationName.end(), locationName.begin(), ::tolower);
  ff_filestream << output_path  << "/" <<  locationName << "_arbeitsschritte" << "_ff" + fruchtfolge << "_anlage"
      << variante << "_profil-" << profil_nr << ".txt";
  ofstream ff_file;
  ff_file.open((ff_filestream.str()).c_str());
  if (ff_file.fail()) debug() << "Error while opening output file \"" << ff_filestream.str().c_str() << "\"" << endl;
  BOOST_FOREACH(ProductionProcess& pp, ff) {
    ff_file << pp.toString().c_str() << endl << endl;
  }
  ff_file.close();

  // write soil information of each individual soil layer to file
  ostringstream soildata_file;
  soildata_file << output_path << "/"<< locationName << "_soildata" << "_ff" + fruchtfolge << "_anlage"
      << variante << "_profil-" << profil_nr << ".txt";
  ofstream file;
  file.open((soildata_file.str()).c_str());
  if (file.fail()) {
    debug() << "Error while opening output file \"" << soildata_file.str().c_str() << "\"" << endl;
  }
  file << "Layer;Saturation [Vol-%];FC [Vol-%];PWP [Vol-%];BoArt;Dichte [kg m-3];LeachingDepth" << endl;

  unsigned int soil_size = sps->size();

  for (unsigned i=0; i<soil_size; i++) {
    SoilParameters parameters = sps->at(i);
    file << i << ";" << parameters.vs_Saturation * 100.0
        << ";" << parameters.vs_FieldCapacity * 100.0
        << ";" << parameters.vs_PermanentWiltingPoint * 100.0
        << ";" << parameters.vs_SoilTexture.c_str()
        << ";" << parameters.vs_SoilRawDensity()
        << ";" << centralParameterProvider.userEnvironmentParameters.p_LeachingDepth
        << endl;


  }
  file.close();


  //env.autoIrrigationParams = AutomaticIrrigationParameters(20, 0.1, 0.01);
  //env.nMinUserParams = NMinUserParameters(10, 100, 30);
  //env.nMinFertiliserPartition = getMineralFertiliserParametersFromMonicaDB(1);

  /** @todo Do something useful with the result */
  // start calucation of model
  const Monica::Result result = runMonica(env);
  return result;
}

#endif /*#ifdef RUN_EVA*/

//------------------------------------------------------------------

#ifdef RUN_HERMES

/**
 * Method for compatibility issues. Hermes configuration is hard coded
 * and only output path can be configured outside.
 *
 * @param output_path Path to input and output files
 */
const Monica::Result
Monica::runWithHermesData(const std::string output_path)
{
  Monica::activateDebug = true;

  debug() << "Running hermes with configuration information from \"" << output_path.c_str() << "\"" << endl;

  HermesSimulationConfiguration *hermes_config = getHermesConfigFromIni(output_path);

  const Monica::Result result = runWithHermesData(hermes_config);

  delete hermes_config;
  return result;
}


Monica::HermesSimulationConfiguration *
Monica::getHermesConfigFromIni(std::string output_path)
{
  HermesSimulationConfiguration *hermes_config = new HermesSimulationConfiguration();

  hermes_config->setOutputPath(output_path);
  string ini_path;
  ini_path.append(output_path);
  ini_path.append("/monica.ini");

  IniParameterMap ipm(ini_path);

  // files input
  hermes_config->setSoilParametersFile(ipm.value("files", "soil"));
  hermes_config->setWeatherFile(ipm.value("files", "climate_prefix"));
  hermes_config->setRotationFile(ipm.value("files", "croprotation"));
  hermes_config->setFertiliserFile(ipm.value("files", "fertiliser"));
  hermes_config->setIrrigationFile(ipm.value("files", "irrigation"));

  // simulation time
  hermes_config->setStartYear(ipm.valueAsInt("simulation_time", "startyear"));
  hermes_config->setEndYear(ipm.valueAsInt("simulation_time", "endyear"));

  bool use_nmin_fertiliser = ipm.valueAsInt("nmin_fertiliser", "activated") == 1;
  if (use_nmin_fertiliser) {
      hermes_config->setOrganicFertiliserID(ipm.valueAsInt("nmin_fertiliser", "organic_fert_id"));
      hermes_config->setMineralFertiliserID(ipm.valueAsInt("nmin_fertiliser", "mineral_fert_id"));
      double min = ipm.valueAsDouble("nmin_fertiliser", "min", 10.0);
      double max = ipm.valueAsDouble("nmin_fertiliser", "max",100.0);
      int delay = ipm.valueAsInt("nmin_fertiliser", "delay_in_days",30);
      hermes_config->setNMinFertiliser(true);
      hermes_config->setNMinUserParameters(min, max, delay);
  }

  bool use_automatic_irrigation = ipm.valueAsInt("automatic_irrigation", "activated") == 1;
  if (use_automatic_irrigation) {
      double amount = ipm.valueAsDouble("automatic_irrigation", "amount", 0.0);
      double treshold = ipm.valueAsDouble("automatic_irrigation", "treshold", 0.15);
      double nitrate = ipm.valueAsDouble("automatic_irrigation", "nitrate", 0.0);
      double sulfate = ipm.valueAsDouble("automatic_irrigation", "sulfate", 0.0);
      hermes_config->setAutomaticIrrigation(true);
      hermes_config->setAutomaticIrrigationParameters(amount, treshold, nitrate, sulfate);
  }

  //site configuration
  hermes_config->setlatitude(ipm.valueAsDouble("site_parameters", "latitude", -1.0));
  hermes_config->setSlope(ipm.valueAsDouble("site_parameters", "slope", -1.0));
  hermes_config->setHeightNN(ipm.valueAsDouble("site_parameters", "heightNN", -1.0));
  hermes_config->setSoilCNRatio(ipm.valueAsDouble("site_parameters", "soilCNRatio", -1.0));
  hermes_config->setAtmosphericCO2(ipm.valueAsDouble("site_parameters", "atmospheric_CO2", -1.0));
  hermes_config->setWindSpeedHeight(ipm.valueAsDouble("site_parameters", "wind_speed_height", -1.0));
  hermes_config->setLeachingDepth(ipm.valueAsDouble("site_parameters", "leaching_depth", -1.0));
  hermes_config->setMinGWDepth(ipm.valueAsDouble("site_parameters", "groundwater_depth_min", -1.0));
  hermes_config->setMaxGWDepth(ipm.valueAsDouble("site_parameters", "groundwater_depth_max", -1.0));
  hermes_config->setMinGWDepthMonth(ipm.valueAsInt("site_parameters", "groundwater_depth_min_month", -1));

  hermes_config->setGroundwaterDischarge(ipm.valueAsDouble("site_parameters", "groundwater_discharge", -1.0));
  hermes_config->setLayerThickness(ipm.valueAsDouble("site_parameters", "layer_thickness", -1.0));
  hermes_config->setNumberOfLayers(ipm.valueAsDouble("site_parameters", "number_of_layers", -1.0));
  hermes_config->setCriticalMoistureDepth(ipm.valueAsDouble("site_parameters", "critical_moisture_depth", -1.0));
  hermes_config->setSurfaceRoughness(ipm.valueAsDouble("site_parameters", "surface_roughness", -1.0));
  hermes_config->setDispersionLength(ipm.valueAsDouble("site_parameters", "dispersion_length", -1.0));
  hermes_config->setMaxPercolationRate(ipm.valueAsDouble("site_parameters", "max_percolation_rate", -1.0));
  hermes_config->setPH(ipm.valueAsDouble("site_parameters", "pH", -1.0));
  hermes_config->setNDeposition(ipm.valueAsDouble("site_parameters", "N_deposition", -1.0));

  // general parameters
  hermes_config->setSecondaryYields(ipm.valueAsBool("general_parameters", "use_secondary_yields", true));
  hermes_config->setNitrogenResponseOn(ipm.valueAsBool("general_parameters", "nitrogen_response_on", true));
  hermes_config->setWaterDeficitResponseOn(ipm.valueAsBool("general_parameters", "water_deficit_response_on", true));
  hermes_config->setEmergenceMoistureControlOn(ipm.valueAsBool("general_parameters", "emergence_moisture_control_on", true));

  // initial values
  hermes_config->setInitPercentageFC(ipm.valueAsDouble("init_values", "init_percentage_FC", -1.0));
  hermes_config->setInitSoilNitrate(ipm.valueAsDouble("init_values", "init_soil_nitrate", -1.0));
  hermes_config->setInitSoilAmmonium(ipm.valueAsDouble("init_values", "init_soil_ammonium", -1.0));

  return hermes_config;
}

#endif /*#ifdef RUN_HERMES*/

//------------------------------------------------------------------

#ifdef RUN_HERMES

/**
 *
 * @param hermes_config
 */
const Monica::Result
Monica::runWithHermesData(HermesSimulationConfiguration *hermes_config)
{
  Monica::activateDebug = true;

  Env env = getHermesEnvFromConfiguration(hermes_config);

  /** @todo Do something useful with the result */
  // start calucation of model
  const Monica::Result& res = runMonica(env);

  debug() << "Monica with data from Hermes executed" << endl;

  return res;
}

Monica::Env
Monica::getHermesEnvFromConfiguration(HermesSimulationConfiguration *hermes_config)
{

  debug() << "Running hermes with configuration object" << endl;
  std::string outputPath = hermes_config->getOutputPath();
  CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase(Env::MODE_HERMES);

  SiteParameters siteParams;
  siteParams.vq_NDeposition = hermes_config->getNDeposition();

  if (hermes_config->getAtmosphericCO2()!=-1.0){
      centralParameterProvider.userEnvironmentParameters.p_AthmosphericCO2 = hermes_config->getAtmosphericCO2();
  }
  if (hermes_config->getlatitude()!=-1.0){
      siteParams.vs_Latitude = hermes_config->getlatitude();
  }

  if (hermes_config->getSlope()!=-1.0){
      siteParams.vs_Slope = hermes_config->getSlope();
  }

  if (hermes_config->getHeightNN()!=-1.0){
      siteParams.vs_HeightNN = hermes_config->getHeightNN();
  }

  if (hermes_config->getSoilCNRatio()!=-1.0){
      siteParams.vs_Soil_CN_Ratio = hermes_config->getSoilCNRatio();
  }

  if (hermes_config->getMinGWDepth()!=-1.0){
      centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepth = hermes_config->getMinGWDepth();
  }

  if (hermes_config->getMaxGWDepth()!=-1.0){
      centralParameterProvider.userEnvironmentParameters.p_MaxGroundwaterDepth = hermes_config->getMaxGWDepth();
  }

  if (hermes_config->getMinGWDepthMonth()!=-1.0){
      centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepthMonth = hermes_config->getMinGWDepthMonth();
  }

  if (hermes_config->getAtmosphericCO2()!=-1.0){
      centralParameterProvider.userEnvironmentParameters.p_AthmosphericCO2 = hermes_config->getAtmosphericCO2();
  }

  if (hermes_config->getWindSpeedHeight()!=-1.0){
      centralParameterProvider.userEnvironmentParameters.p_WindSpeedHeight = hermes_config->getWindSpeedHeight();
  }

  if (hermes_config->getLeachingDepth()!=-1.0){
      centralParameterProvider.userEnvironmentParameters.p_LeachingDepth = hermes_config->getLeachingDepth();
  }

  if (hermes_config->getNDeposition() != -1.0) {
      siteParams.vq_NDeposition = hermes_config->getNDeposition();
  }

  if (hermes_config->getInitPercentageFC() != -1.0) {
      centralParameterProvider.userInitValues.p_initPercentageFC = hermes_config->getInitPercentageFC();
  }

  if (hermes_config->getInitSoilNitrate() != -1.0) {
      centralParameterProvider.userInitValues.p_initSoilNitrate = hermes_config->getInitSoilNitrate();
  }

  if (hermes_config->getInitSoilAmmonium() != -1.0) {
      centralParameterProvider.userInitValues.p_initSoilAmmonium = hermes_config->getInitSoilAmmonium();
  }


  double layer_thickness = centralParameterProvider.userEnvironmentParameters.p_LayerThickness;
  double profile_depth = layer_thickness * double(centralParameterProvider.userEnvironmentParameters.p_NumberOfLayers);
  double max_mineralisation_depth = 0.4;
  bool nitrogen_response_on = hermes_config->getNitrogenResponseOn();
  bool water_deficit_response_on = hermes_config->getWaterDeficitResponseOn();
  bool emergence_moisture_control_on = hermes_config->getEmergenceMoistureControlOn(); 
  GeneralParameters gps = GeneralParameters(layer_thickness, profile_depth, max_mineralisation_depth, nitrogen_response_on, water_deficit_response_on, emergence_moisture_control_on);

  //soil data
  const SoilPMs* sps = soilParametersFromHermesFile(1, outputPath + hermes_config->getSoilParametersFile(), gps, hermes_config->getPH());

  //climate data
  std::string file = outputPath+hermes_config->getWeatherFile();
  Climate::DataAccessor climateData =
      climateDataFromHermesFiles(file, hermes_config->getStartYear(), hermes_config->getEndYear(), centralParameterProvider, true);


  // test if precipitation values should be manipulated; used for carboZALF simulation
  double precip_manipulation_value = hermes_config->precipManipulationValue();
  debug() << "precip_manipulation_value: " << precip_manipulation_value << endl;
  if (precip_manipulation_value!=1.0) {
    vector<double> _precip = climateData.dataAsVector(Climate::precip);
    vector<double> new_precip;
    for (vector<double>::iterator it= _precip.begin(); it!=_precip.end(); it++) {
//        cout << "Precip Manip: " << (*it) << "\t" << (*it)*precip_manipulation_value << endl;
        new_precip.push_back((*it)*precip_manipulation_value);
    }
    climateData.addOrReplaceClimateData(Climate::precip, new_precip);
  }

  debug() << "climate data from: " << climateData.startDate().toString()
      << " to: " << climateData.endDate().toString() << endl;
  debug() << "--------------------------" << endl;

  // testClimateData(climateData);

  debug() << "--------------------------" << endl;

  //fruchtfolge
  file = outputPath+hermes_config->getRotationFile();
  vector<ProductionProcess> ff = cropRotationFromHermesFile(file);

  // fertilisation
  file = outputPath+hermes_config->getFertiliserFile();
  attachFertiliserApplicationsToCropRotation(ff, file);

  // irrigation
  file = outputPath+hermes_config->getIrrigationFile();
  if (file != "") {
    attachIrrigationApplicationsToCropRotation(ff, file);
  }

  debug() << "------------------------------------" << endl;

  BOOST_FOREACH(const ProductionProcess& pv, ff){
    debug() << "pv: " << pv.toString() << endl;
  }

  // fertilisation parameter
  /** @todo fertiliser IDs are not set static, but must be read dynamically or specified by user. */
  //  int organ_fert_id = hermes_config->getOrganicFertiliserID();
//  int mineral_fert_id = hermes_config->getMineralFertiliserID();

  //  const OrganicMatterParameters* organic_fert_params = getOrganicFertiliserParametersFromMonicaDB(organ_fert_id);

  //build up the monica environment
  Env env(sps, centralParameterProvider);
  env.general = gps;
  env.pathToOutputDir = outputPath;
  env.setMode(Env::MODE_HERMES);


  env.site = siteParams;

  env.da = climateData;
  env.cropRotation = ff;



  if (hermes_config->useAutomaticIrrigation()) {
    env.useAutomaticIrrigation = true;
    env.autoIrrigationParams = hermes_config->getAutomaticIrrigationParameters();
  }

  if (hermes_config->useNMinFertiliser()) {
    env.useNMinMineralFertilisingMethod = true;
    env.nMinUserParams = hermes_config->getNMinUserParameters();
    env.nMinFertiliserPartition = getMineralFertiliserParametersFromMonicaDB(hermes_config->getMineralFertiliserID());
  }

  return env;
}

#endif /*#ifdef RUN_HERMES*/
//------------------------------------------------------------------

/**
 * Interface method for python swig for manipulating debug object.
 * @param status TRUE, if debug outputs are written to console.
 */
void
Monica::activateDebugOutput(bool status)
{
  Monica::activateDebug = status;
}


//------------------------------------------------------------------

#ifdef RUN_CC_GERMANY

/**
 * Method for starting a simulation with data from eva2 database.
 * A configuration object is passed that stores all relevant
 * information e.g. location, output path etc.
 *
 * @param simulation_config Configuration object that stores all simulation information
 */
const Monica::Result
Monica::runCCGermanySimulation(const CCGermanySimulationConfiguration *simulation_config)
{
  Monica::activateDebug = true;

  // eva2 controlling parameters
  int leg1000_id;
  double jul_sowing_date;
  double gw_depth;
  int stat_id;
  std::string realisierung = "nor_a";

  int crop_id = 1;  // 1 - Winterweizen

  Tools::Date start_date;
  Tools::Date end_date;

  if (simulation_config == 0) {
      debug() << "Using hard coded information for cc_germany simulation" << endl;
      leg1000_id = 51;
      jul_sowing_date = 294.5;
      gw_depth = -9999;
      stat_id=377;
      start_date = Tools::Date(1,1,1996,true);
      end_date = Tools::Date(31,12,2025,true);

  } else {

      leg1000_id = simulation_config->getBuekId();
      jul_sowing_date = simulation_config->getJulianSowingDate();
      gw_depth = simulation_config->getGroundwaterDepth();
      stat_id = simulation_config->getStatId();
      start_date = simulation_config->getStartDate();
      end_date = simulation_config->getEndDate();
      crop_id = 1;  // w - Winterweizen


  }

  cout << "--------------------------------------" << endl;
  cout << "STAT_ID:\t" << stat_id << endl;
  cout << "BUEK_ID:\t" << leg1000_id << endl;
  cout << "Sowing Day:\t" << jul_sowing_date << endl;
  cout << "Groundwater:\t" << gw_depth << endl;
  cout << "Period:\t\t" << start_date.toString().c_str() << " - " << end_date.toString().c_str() << endl;
  cout << "Crop Id:\t" << crop_id << endl;
  cout << "--------------------------------------" << endl;

  // read user defined values from database
  CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase(Env::MODE_HERMES);
  centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepth=gw_depth;
  centralParameterProvider.userEnvironmentParameters.p_MaxGroundwaterDepth=gw_depth;

  double leaching_depth = centralParameterProvider.userEnvironmentParameters.p_LeachingDepth;
  if (gw_depth<leaching_depth) {
      centralParameterProvider.userEnvironmentParameters.p_LeachingDepth = gw_depth-0.2;
  }



  // site parameters
  // TODO: Longitude & Latitude?
  SiteParameters site_parameters;
//  site_parameters.vs_GroundwaterDepth = gw_depth;
  double llc = getLatitudeOfStatId(stat_id);
  site_parameters.vs_Latitude = llc;

  // read parameters from user environment to initialize general parameters data structure
  double layer_thickness = centralParameterProvider.userEnvironmentParameters.p_LayerThickness;
  double profile_depth = layer_thickness * double(centralParameterProvider.userEnvironmentParameters.p_NumberOfLayers);
  GeneralParameters gps = GeneralParameters(layer_thickness, profile_depth);

  // read soil parameters for a special profile e.g. Dornburg
  const SoilPMs* sps = readBUEKDataFromMonicaDB(leg1000_id, gps);
  if (sps == NULL) {
      cout << "Error while reading soil data from BUEK database. Received NULL SoilParameters.\nAbortin simulation ..." << endl;
      return Monica::Result();
  }
  // crop rotation
  vector<ProductionProcess> ff = getCropManagementData(crop_id, start_date.toMysqlString(""), end_date.toMysqlString(""), jul_sowing_date);

  Climate::DataAccessor da = climateDataForCCGermany2(stat_id, start_date.toMysqlString(""), end_date.toMysqlString(""), realisierung, centralParameterProvider);

  Env env(sps, centralParameterProvider);

  env.general = gps;
  env.site = site_parameters;
  env.da=da;
  env.pathToOutputDir = "python/cc_germany/";
  env.setCropRotation(ff);
  env.useNMinMineralFertilisingMethod=true;
  env.setMode(Env::MODE_ACTIVATE_OUTPUT_FILES);
  env.nMinFertiliserPartition = getMineralFertiliserParametersFromMonicaDB(1);
  env.nMinUserParams = NMinUserParameters(10, 100, 30);
  env.nMinUserParams.min = 10;
  env.nMinUserParams.max = 100;
  env.nMinUserParams.delayInDays = 30;

  std::cout << env.toString().c_str() << endl;

  /** @todo Do something useful with the result */
  // start calucation of model
  const Monica::Result result = runMonica(env);
  return result;
}

#endif /*#ifdef RUN_CC_GERMANY*/


#ifdef RUN_GIS

/**
 * Method for starting a simulation with coordinates from ascii grid.
 * A configuration object is passed that stores all relevant
 * information e.g. location, output path etc.
 *
 * @param simulation_config Configuration object that stores all simulation information
 */
const Monica::Result
Monica::runGISSimulation(const GISSimulationConfiguration *simulation_config)
{
  Monica::activateDebug = true;

  double row;
  double col;
  int crop_id = 1;  // 1 - Winterweizen
  Tools::Date start_date;
  Tools::Date end_date;
  double julian_sowing_date;

  if (simulation_config == 0) {
      debug() << "Using hard coded information for GIS simulation" << endl;
      row = 0;
      col = 0;
      start_date = Tools::Date(1,1,1996,true);
      end_date = Tools::Date(31,12,2025,true);

  } else {

      julian_sowing_date = simulation_config->getJulianSowingDate();
      start_date = simulation_config->getStartDate();
      end_date = simulation_config->getEndDate();
      crop_id = 1;  // w - Winterweizen
      row = simulation_config->getRow();
      col = simulation_config->getCol();
  }

  Monica::Result result = createGISSimulation(row, col, start_date.toMysqlString(""), end_date.toMysqlString(""),
                                julian_sowing_date,
                                "python/gis_simulation/data/thue.h5",
                                "python/gis_simulation/data/voronoi_regions/TH5_ORG_KN.h5",
                                "gis_results", -1);


  return result;

}

#endif

