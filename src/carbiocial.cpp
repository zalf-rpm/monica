/**
Authors: 
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

#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <cassert>
#include <mutex>

#include "db/abstract-db-connections.h"
#include "climate/climate-common.h"
#include "tools/helper.h"
#include "tools/algorithms.h"

#include "carbiocial.h"
#include "tools/debug.h"
#include "monica-parameters.h"
#include "soil/conversion.h"
#include "simulation.h"

using namespace Db;
using namespace std;
using namespace Carbiocial;
using namespace Monica;
using namespace Tools;
using namespace Soil;

std::pair<const SoilPMs *, int>
Carbiocial::carbiocialSoilParameters(int profileId, int layerThicknessCm,
int maxDepthCm, GeneralParameters gps, string output_path, CentralParameterProvider centralParameterProvider)
{
	//cout << "getting soilparameters for STR: " << str << endl;
	int maxNoOfLayers = int(double(maxDepthCm) / double(layerThicknessCm));

	static mutex lockable;

	typedef int ProfileId;
	typedef int SoilClassId;

	typedef map<ProfileId, SoilPMsPtr> Map;
	typedef map<ProfileId, SoilClassId> PId2SCId;
	static bool initialized = false;
	static Map spss;
	static PId2SCId profileId2soilClassId;

	if (!initialized)
	{
    lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
			DBPtr con(newConnection("carbiocial"));
			DBRow row;

			ostringstream s;
			s << "select id, count(horizon_id) "
				"from soil_profile_data "
				"where id not null "
				"group by id";
			con->select(s.str().c_str());

			map<int, int> id2layerCount;
			while (!(row = con->getRow()).empty())
				id2layerCount[satoi(row[0])] = satoi(row[1]);
			con->freeResultSet();

			ostringstream s2;
			s2 << "select id, horizon_id, soil_class_id, "
				"upper_horizon_cm, lower_horizon_cm, "
				"silt_percent, clay_percent, sand_percent, "
				"ph_kcl, c_org_percent, c_n, bulk_density_t_per_m3 "
				"from soil_profile_data "
				"where id not null "
				"order by id, horizon_id";
			con->select(s2.str().c_str());

			while (!(row = con->getRow()).empty())
			{
				ProfileId id = ProfileId(satoi(row[0]));

				//SoilClassId soilClassId = SoilClassId(satoi(row[2]));
				//if (profileId2soilClassId.find(id) == profileId2soilClassId.end())
				//	profileId2soilClassId[id] = soilClassId;

				Map::iterator spsi = spss.find(id);
				SoilPMsPtr sps;

				if (spsi == spss.end())
					spss.insert(make_pair(id, sps = SoilPMsPtr(new SoilPMs)));
				else
					sps = spsi->second;

				int hcount = id2layerCount[int(id)];
				int currenth = satoi(row[1]);

				int ho = sps->size()*layerThicknessCm;
				int hu = satoi(row[4]) ? satoi(row[4]) : maxDepthCm;
				int hsize = max(0, hu - ho);
				int subhcount = Tools::roundRT<int>(double(hsize) / double(layerThicknessCm), 0);
				if (currenth == hcount && (int(sps->size()) + subhcount) < maxNoOfLayers)
					subhcount += maxNoOfLayers - sps->size() - subhcount;

				SoilParameters p;
				if (!row[8].empty())
					p.vs_SoilpH = satof(row[8]);
				p.set_vs_SoilOrganicCarbon(row[9].empty() ? 0 : satof(row[9]) / 100.0);
				p.set_vs_SoilRawDensity(satof(row[11]));
				p.vs_SoilSandContent = satof(row[7]) / 100.0;
				p.vs_SoilClayContent = satof(row[6]) / 100.0;
				p.vs_SoilTexture = texture2KA5(p.vs_SoilSandContent, p.vs_SoilClayContent);
				p.vs_SoilStoneContent = 0.0;
        p.vs_Lambda = Soil::texture2lambda(p.vs_SoilSandContent, p.vs_SoilClayContent);

				// initialization of saturation, field capacity and perm. wilting point
				soilCharacteristicsKA5(p);

				bool valid_soil_params = p.isValid();
				if (!valid_soil_params)
				{
					cout << "Error in soil parameters. Aborting now simulation";
					exit(-1);
				}

				for (int i = 0; i < subhcount; i++)
					sps->push_back(p);
			}

			initialized = true;
		}
	}

	static SoilPMs nothing;
	Map::const_iterator ci = spss.find(profileId);

	if(activateDebug)
	{
		///////////////////////////////////////////////////////////
		// write soil information of each individual soil layer to file
		
		const SoilPMs* sps = ci->second.get();

		ostringstream soildata_file;
		soildata_file << profileId << ".txt";
		ofstream file;
		file.open((soildata_file.str()).c_str());
		if (file.fail()) {
			debug() << "Error while opening output file \"" << soildata_file.str().c_str() << "\"" << endl;
		}
		file << "Layer;Saturation [Vol-%];FC [Vol-%];PWP [Vol-%];BoArt;Sand;Clay;Dichte [kg m-3];LeachingDepth" << endl;
		
		int i = 0;
		for (auto p : *ci->second)
		{
			file << i++ << ";" << p.vs_Saturation * 100.0
					<< ";" << p.vs_FieldCapacity * 100.0
					<< ";" << p.vs_PermanentWiltingPoint * 100.0
					<< ";" << p.vs_SoilTexture.c_str()
					<< ";" << p.vs_SoilSandContent
					<< ";" << p.vs_SoilClayContent
					<< ";" << p.vs_SoilRawDensity()
					<< ";" << centralParameterProvider.userEnvironmentParameters.p_LeachingDepth
					<< endl;
		}
		file.close();
	}
	return ci != spss.end() ? make_pair(ci->second.get(), -1)//profileId2soilClassId[profileId])
		: make_pair(&nothing, -1);
}


/**
* Method for starting a simulation with coordinates from ascii grid.
* A configuration object is passed that stores all relevant
* information e.g. location, output path etc.
*
* @param simulation_config Configuration object that stores all simulation information
*/
std::map<int, double> Carbiocial::runCarbiocialSimulation(const CarbiocialConfiguration* simulation_config)
{
	//std::cout << "Start Carbiocial cluster Simulation" << std::endl;

	//  int phase = simulation_config->getPhase();
	//  int step = simulation_config->getStep();

	std::string input_path = simulation_config->getInputPath();
	std::string output_path = simulation_config->getOutputPath();

	// read in ini - file ------------------------------------------
	IniParameterMap ipm(input_path + simulation_config->getIniFile());

	std::string crop_rotation_file = ipm.value("files", "croprotation");
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
	//  double slope = ipm.valueAsDouble("site_parameters", "slope", -1.0);
	//  double heightNN = ipm.valueAsDouble("site_parameters", "heightNN", -1.0);
	double CN_ratio = ipm.valueAsDouble("site_parameters", "soilCNRatio", -1.0);
	double atmospheric_CO2 = ipm.valueAsDouble("site_parameters", "atmospheric_CO2", -1.0);
	double wind_speed_height = ipm.valueAsDouble("site_parameters", "wind_speed_height", -1.0);
	double leaching_depth = ipm.valueAsDouble("site_parameters", "leaching_depth", -1.0);
	//  double critical_moisture_depth = ipm.valueAsDouble("site_parameters", "critical_moisture_depth", -1.0);
	//double ph = ipm.valueAsDouble("site_parameters", "pH", -1.0);

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

	CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase();
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

	GeneralParameters gps = GeneralParameters(layer_thickness, profile_depth, max_mineralisation_depth, n_response, water_deficit_response, emergence_flooding_control, emergence_moisture_control);

	//soil data
	const SoilPMs* sps;

	//  std::string project_id = simulation_config->getProjectId();
	//  std::string lookup_project_id = simulation_config->getLookupProjectId();

	pair<const SoilPMs*, int> p =
		Carbiocial::carbiocialSoilParameters(simulation_config->getProfileId(),
		int(layer_thickness * 100), int(profile_depth * 100), 
		 gps, output_path, centralParameterProvider);
	sps = p.first;

	//no soil available, return no yields
	if (sps->empty())
		return std::map<int, double>();

	//	sps = soilParametersFromCarbiocialFile(soil_file, gps, ph);

	//cout << "Groundwater min:\t" << centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepth << endl;
	//cout << "Groundwater max:\t" << centralParameterProvider.userEnvironmentParameters.p_MaxGroundwaterDepth << endl;
	//cout << "Groundwater month:\t" << centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepthMonth << endl;

	// climate data
	Climate::DataAccessor climateData = 
		climateDataFromCarbiocialFiles(simulation_config->getClimateFile(), centralParameterProvider, latitude, simulation_config);
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
	env.setMode(Env::MODE_CARBIOCIAL_CLUSTER);
	env.site = siteParams;

	env.da = climateData;
	env.cropRotation = ff;

	if (use_automatic_irrigation) 
	{
		env.useAutomaticIrrigation = true;
		env.autoIrrigationParams = AutomaticIrrigationParameters(amount, treshold, nitrate, sulfate);
	}
	//cout << env.toString() << endl;
	//cout << "Before runMonica" << endl;
	Monica::Result res = runMonica(env);
	//cout << "After runMonica" << endl;

	std::map<int, double> year2yield;
	int ey = env.da.endDate().year();
	for (int i = 0, size = res.pvrs.size(); i < size; i++)
	{
		int year = ey - size + 1 + i;
		double yield = res.pvrs[i].pvResults[primaryYieldTM] / 10.0;
		debug() << "year: " << year << " yield: " << yield << " tTM" << endl;
		year2yield[year] = Tools::round(yield, 3);
	}

	return year2yield;
}

/**
* Read climate information from macsur file
*/
Climate::DataAccessor Carbiocial::climateDataFromCarbiocialFiles(const std::string& pathToFile,
	const CentralParameterProvider& cpp, double latitude, const CarbiocialConfiguration* simulationConfig)
{
	bool reorderData = simulationConfig->create2013To2040ClimateData;
	map<int, map<int, map<int, vector<int>>>> year2month2day2oldDMY;
	if (reorderData)
	{
		string pathToReorderingFile = simulationConfig->pathToClimateDataReorderingFile;
		
		cout << "pathToReorderingFile: " << pathToReorderingFile << endl;
		cout << "reorderData: " << reorderData << endl;

		ifstream ifs(pathToReorderingFile.c_str());
		if (!ifs.good()) {
			cerr << "Could not open file " << pathToReorderingFile << " . Aborting now!" << endl;
			exit(1);
		}

		string s;
		while (getline(ifs, s)) 
		{
			istringstream iss(s);
			string arrow;
			int ty(0), tm(0), td(0);
			vector<int> from(3);
			
			iss >> td >> tm >> ty >> arrow >> from[0] >> from[1] >> from[2];

			//cout << "arrow: " << arrow << " to[0]: " << to[0] << " to[1]: " << to[1] << " to[2]: " << to[2] << " fd: " << fd << " fm: " << fm << " fy: " << fy << endl;
			
			if(ty > 0 && tm > 0 && td > 0)
				year2month2day2oldDMY[ty][tm][td] = from;
		}
	}

	//cout << "climateDataFromMacsurFiles: " << pathToFile << endl;
	Climate::DataAccessor da(simulationConfig->getStartDate(), simulationConfig->getEndDate());

	Date startDate = simulationConfig->getStartDate();
	Date endDate = simulationConfig->getEndDate();
	int dayCount = endDate - startDate + 1;
//	cout << "startDate: " << startDate.toString() << endl;
//	cout << "endDate: " << endDate.toString() << endl;
	
	ifstream ifs(pathToFile.c_str(), ios::binary);
	if (!ifs.good()) {
		cerr << "Could not open file " << pathToFile.c_str() << " . Aborting now!" << endl;
		exit(1);
	}

	//if have to store all data before adding them to the DataAccessor
	//because the climate files may contain duplicate entries and might
	//not be fully ordered due to problems during processing
	typedef vector<double> Values;
	typedef map<int, Values> DailyValues;
	typedef map<int, DailyValues> MonthlyValues;
	typedef map<int, MonthlyValues> YearlyValues;
	YearlyValues data;
	
	string s;
	while (getline(ifs, s)) 
	{
		//skip (repeated) headers
		if (s.substr(0, 3) == "day")
			continue;

		vector<string> r = splitString(s, ",");

		unsigned int day = satoi(r.at(0));
		unsigned int month = satoi(r.at(1));
		unsigned int year = satoi(r.at(2));

		if (!reorderData)
		{
			if (year < startDate.year())
				continue;
			else if (month < startDate.month())
				continue;
			else if (day < startDate.day())
				continue;

			if (year > endDate.year())
				continue;
			else if (month > endDate.month())
				continue;
			else if (day > endDate.day())
				continue;
		}
		
		//double tmin, tavg, tmax ,precip, globrad, relhumid, windspeed;
		vector<double> d;
		d.push_back(satof(r.at(4)));
		d.push_back(satof(r.at(5)));
		d.push_back(satof(r.at(6)));
		d.push_back(satof(r.at(7)));
		d.push_back(satof(r.at(8)));
		d.push_back(satof(r.at(9)));
		d.push_back(satof(r.at(10)));

		//    cout << "[" << day << "." << month << "." << year << "] ";

		data[year][month][day] = d;
	}
	
	//  cout << endl;

	vector<double> tavgs, tmins, tmaxs, precips, globrads, relhumids, winds;

	int daysAdded = 0;
	for (Date d = startDate, ed = endDate; d <= ed; d++)
	{
		//cout << "date: " << d.toString() << endl;
		int year = d.year();
		int month = d.month();
		int day = d.day();
		
		if (reorderData && year >= 2013 && year <= 2040)
		{
			auto from = year2month2day2oldDMY[year][month][day];
			if (!from.empty())
			{
				year = from[2];
				month = from[1];
				day = from[0];
			}
		}
	
		auto v = data[year][month][day];

		tavgs.push_back(v.at(0));
		tmins.push_back(v.at(1));
		tmaxs.push_back(v.at(2));
		precips.push_back(v.at(3));
		globrads.push_back(v.at(4));
		relhumids.push_back(v.at(5));
		winds.push_back(v.at(6));

		daysAdded++;
	}

	//  cout << "daysAdded: " << daysAdded << endl;
	if (daysAdded != dayCount) {
		cout << "Wrong number of days in " << pathToFile.c_str() << " ." << " Found " << daysAdded << " days but should have been "
			<< dayCount << " days. Aborting." << endl;
		exit(1);
	}

	da.addClimateData(Climate::tmin, tmins);
	da.addClimateData(Climate::tmax, tmaxs);
	da.addClimateData(Climate::tavg, tavgs);
	da.addClimateData(Climate::precip, precips);
	da.addClimateData(Climate::globrad, globrads);
	da.addClimateData(Climate::relhumid, relhumids);
	da.addClimateData(Climate::wind, winds);

	return da;
}


