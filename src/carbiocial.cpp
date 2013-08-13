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

#include "boost/foreach.hpp"

#include "db/abstract-db-connections.h"
#include "climate/climate-common.h"
#include "tools/use-stl-algo-boost-lambda.h"
#include "tools/helper.h"
#include "tools/algorithms.h"

#include "carbiocial.h"
#include "debug.h"
#include "monica-parameters.h"
#include "conversion.h"

#define LOKI_OBJECT_LEVEL_THREADING
#include "loki/Threads.h"

using namespace Db;
using namespace std;
using namespace Carbiocial;
using namespace Monica;
using namespace Tools;

namespace
{
	struct L: public Loki::ObjectLevelLockable<L> {};
}

CropData Carbiocial::cropDataFor(CropId cropId)
{
	CropData cd;
	switch(cropId)
	{
	case maize:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case sugarcane:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case soy:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case cotton:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case millet:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case fallow:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	default: ;
	}
	return cd;
}

Monica::ProductionProcess Carbiocial::productionProcessFrom(CropId cropId)
{
	Monica::CropPtr cp;
	CropData cd = cropDataFor(cropId);

	switch(cropId)
	{
	case maize: cp = Monica::CropPtr(new Monica::Crop(6, nameFor(cropId)));

	case sugarcane: cp = Monica::CropPtr(new Monica::Crop(nameFor(fallow)));

	case soy: cp = Monica::CropPtr(new Monica::Crop(28, nameFor(cropId)));

	case cotton: cp = Monica::CropPtr(new Monica::Crop(44, nameFor(cropId)));

	case millet: cp = Monica::CropPtr(new Monica::Crop(nameFor(fallow)));

	case fallow:
	default: cp = Monica::CropPtr(new Monica::Crop(nameFor(fallow)));
	};

	cp->setSeedAndHarvestDate(cd.sowing, cd.harvesting);
	cp->setCropParameters(getCropParametersFromMonicaDB(cp->id()));
	cp->setResidueParameters(getResidueParametersFromMonicaDB(cp->id()));

	ProductionProcess pp(cp->name(), cp);

	BOOST_FOREACH(Date tillage, cd.tillages)
	{
		pp.addApplication(TillageApplication(tillage, 0.3));
	}

	return pp;
}

string Carbiocial::nameFor(CropId cropId, Language l)
{
	switch(l)
	{
	case de:
		switch(cropId)
		{
		case maize: return "Mais";
		case sugarcane: return "Zuckerrohr";
		case soy: return "Soja";
		case cotton: return "Baumwolle";
		case millet: return "Hirse";
		case fallow: return "Brache";
		default: return "unbekannte Fruchtart";
		};
		break;
	case en:
		switch(cropId)
		{
		case maize: return "maize";
		case sugarcane: return "sugar cane";
		case soy: return "soy";
		case cotton: return "cotton";
		case millet: return "millet";
		case fallow: return "fallow";
		default: return "unknown crop";
		}
	case br:
		switch(cropId)
		{
		case maize: return "maize";
		case sugarcane: return "sugar cane";
		case soy: return "soy";
		case cotton: return "cotton";
		case millet: return "millet";
		case fallow: return "fallow";
		default: "unknown crop";
		}
	}
	return "";
}

//-------------------------------------------------------------------------------------------------------

Mpmas::Mpmas()
	: _mpmas(NULL),
		_noOfYears(0),
		_noOfSpinUpYears(0)
{
	char* argv[] =
	{
		"mpmas-lib",
		"-IBRA020" ,
		"-OBRA020",
		"-Ntf__",
		"-T82",
		"-Pgis/carbiocial_typical_farms/",
		"-BBRA020/input/dat/tf__InactiveSectors0.dat",
		"-T19",
		"-T93",
		"-T94"
	};

	_mpmas = new mpmas(10, argv);
	mpmasScope::mpmasPointer = _mpmas;

	//allocate memory
	mpmas.allocateMemoryForMonica(CropActivity::all().size());

			//get length of simulation horizon and number of spin-up rounds
	_noOfYears = _mpmas->getNumberOfYearsToSimulate();
	_noOfSpinUpYears = _mpmas->getNumberOfSpinUpRounds();
}

vector<CropActivity*> Mpmas::landuse(int year, Farm* farm,
																		const map<int, int>& soilClassId2areaPercent,
																		vector<ProductionPractice*> pps)
{
	int noOfCropActivitys = CropActivity::all().size();
	int noOfSoilClasses = soilClassId2areaPercent.size();
	int noOfProductionPractices = pps.size();

	map<int, ProductionPractice*> id2pp;
	for_each(pps.begin(), pps.end(), [&id2pp](ProductionPractice* pp)
	{
		id2pp[pp->id] = pp;
	});

	int* cropActivityIds = new int[noOfCropActivitys];
	double* cropAreas = new double[noOfCropActivitys];

	int* disabledCropActivities = new int[noOfCropActivitys - noOfSoilClasses*noOfProductionPractices];
	int disabledCAcount = 0;
	BOOST_FOREACH(CropActivity* ca, CropActivity::all())
	{
		SoilClass* sc = ca->soilClass;
		ProductionPractice* pp = ca->productionPractice;
		if(soilClassId2areaPercent.find(sc->id) != soilClassId2areaPercent.end() &&
			 id2pp.find(pp->id) != id2pp.end())
			continue; //user choose that combination
		else
			disabledCropActivities[disabledCAcount++] = ca->id;
	}

	cassert(disabledCAcount == noOfCropActivitys - noOfSoilClasses*noOfProductionPractices);

	//if needed, exclude crop activities from being grown
	_mpmas.disableCropActivities(disabledCAcount, disabledCropActivities);

	//is not any more needed because of the disabled sectors file being written
	int catchmentID = 0;
	int numDisabledSectors = 1;
	int disabledSectorID[1];
	disabledSectorID[0] = 0;

	//if needed, exclude specific sectors from simulation (their agents will be deleted)
	//mpmas.disableAgentsInSectors(catchmentID, numDisabledSectors, disabledSectorID);

				//export land-use maps
				int rtcode1 = mpmas.simulateOnePeriodExportingLandUse(year, numCropActs, cropActIDX, cropAreaX);


	return vector<CropActivity*>();
}

Result Mpmas::calculateEconomicStuff(int year, const Mpmas::CAId2MonicaYields& caId2mYields)
{
	return Result();
}


//-------------------------------------------------------------------------------------------------------

vector<vector<ProductionProcess> >
Carbiocial::cropRotationsFromUsedCrops(vector<CropActivity> cas)
{
	vector<vector<ProductionProcess> > crs;

	map<Period, vector<CropActivity> > period2cas;
	for_each(cas.begin(), cas.end(), [&period2cas](CropActivity cas)
	{
		period2cas[cas.period].push_back(cas);
	});

	BOOST_FOREACH(CropActivity safraCA, period2cas[safra])
	{
		ProductionProcess safraPP = productionProcessFrom(safraCA.cropId);

		BOOST_FOREACH(CropActivity safrinhaCA, period2cas[safrinha])
		{
			ProductionProcess safrinhaPP = productionProcessFrom(safrinhaCA.cropId);

			BOOST_FOREACH(CropActivity offseasonCA, period2cas[offseason])
			{
				ProductionProcess offseasonPP = productionProcessFrom(offseasonCA.cropId);

				vector<ProductionProcess> cr;
				cr.push_back(safraPP);
				cr.push_back(safrinhaPP);
				cr.push_back(offseasonPP);

				crs.push_back(cr);
			}
		}
	}

	return crs;
}

//-------------------------------------------------------------------------------------------------------

std::pair<const SoilPMs *, int> Carbiocial::carbiocialSoilParameters(int profileId, const GeneralParameters& gps)
{
	//cout << "getting soilparameters for STR: " << str << endl;
	int lt = int(gps.ps_LayerThickness.front() * 100); //cm
	int maxDepth = int(gps.ps_ProfileDepth) * 100; //cm
	int maxNoOfLayers = int(double(maxDepth) / double(lt));

	static L lockable;

	typedef map<Carbiocial::ProfileId, SoilPMsPtr> Map;
	typedef map<Carbiocial::ProfileId, Carbiocial::SoilClassId> PId2SCId;
	static bool initialized = false;
	static Map spss;
	static PId2SCId profileId2soilClassId;

	if (!initialized)
	{
		L::Lock lock(lockable);

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
				Carbiocial::ProfileId id = Carbiocial::ProfileId(satoi(row[0]));

				Carbiocial::SoilClassId soilClassId = Carbiocial::SoilClassId(satoi(row[2]));
				if(profileId2soilClassId.find(id) != profileId2soilClassId.end())
					profileId2soilClassId[id] = soilClassId;

				Map::iterator spsi = spss.find(id);
				SoilPMsPtr sps;

				if (spsi == spss.end())
					spss.insert(make_pair(id, sps = SoilPMsPtr(new SoilPMs)));
				else
					sps = spsi->second;

				int hcount = id2layerCount[int(id)];
				int currenth = satoi(row[1]);

				int ho = sps->size() * lt;
				int hu = satoi(row[4]) ? satoi(row[4]) : maxDepth;
				int hsize = hu - ho;
				int subhcount = int(Tools::round(double(hsize) / double(lt)));
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
				p.vs_Lambda = texture2lambda(p.vs_SoilSandContent, p.vs_SoilClayContent);

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

			//      BOOST_FOREACH(Map::value_type p, spss)
			//      {
			//        cout << "code: " << p.first << endl;
			//        BOOST_FOREACH(SoilParameters sp, *p.second)
			//        {
			//          cout << sp.toString();
			//          cout << "---------------------------------" << endl;
			//        }
			//      }
		}
	}

	static SoilPMs nothing;
	Map::const_iterator ci = spss.find(profileId);
	/*
	if(ci != spss.end())
	{
		cout << "code: " << str << endl;
		BOOST_FOREACH(SoilParameters sp, *ci->second)
		{
			cout << sp.toString();
			cout << "---------------------------------" << endl;
		}
	}
	*/
	return ci != spss.end() ? make_pair(ci->second.get(), profileId2soilClassId[profileId])
													: make_pair(&nothing, none);
}

pair<int, int> roundTo5(int value)
{
	auto v = div(value, 10);
	switch(v.rem)
	{
	case 0: case 1: case 2: return make_pair(v.quot*10, v.rem);
	case 3: case 4: case 5: case 6: case 7: return make_pair(v.quot*10 + 5, 5 - v.rem);
	case 8: case 9: return make_pair(v.quot*10 + 10, 10 - v.rem);
	default: ;
	}
}

#ifndef NO_GRIDS
map<int, int> Carbiocial::roundedSoilFrequency(const Grids::GridP* soilGrid, int roundToDigits)
{
	typedef int SoilId;

	auto roundValue = [=](double v){ return Tools::roundRT<int>(v, 0); };
	auto roundPercentage = [=](double v){ return Tools::roundRT<int>(v, roundToDigits); };
	auto rsf = soilGrid->frequency<int, int>(false, roundValue, roundPercentage);
	int sumRoundedPercentage = accumulate(rsf.begin(), rsf.end(), 0, 
		[](int acc, pair<SoilId, int> p){ return acc + p.second; });
				
	int roundError = sumRoundedPercentage - 100;
	if(roundError != 0)
	{
		int delta = roundError > 0
								? -1 //we got too much percent = over 100%
								: 1; //we got too few percent = below 100%

		map<SoilId, int>::iterator i = rsf.begin();
		while(roundError != 0)
		{
			if(i == rsf.end())
				i = rsf.begin();

			i->second += delta;
			roundError += delta;

			i++;
		}

		map<SoilId, int> rsf2;
		for_each(rsf.begin(), rsf.end(), [&](pair<SoilId, int> p)
		{
			if(p.second > 0)
				rsf2.insert(p);
		});

		return rsf2;
	}

	return rsf;
}
#endif

void Carbiocial::runMonicaCarbiocial()
{
	/*
	Monica::activateDebug = true;

  debug() << "Running hermes with configuration information from \"" << output_path.c_str() << "\"" << endl;

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
  hermes_config->setLattitude(ipm.valueAsDouble("site_parameters", "latitude", -1.0));
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

  // initial values
  hermes_config->setInitPercentageFC(ipm.valueAsDouble("init_values", "init_percentage_FC", -1.0));
  hermes_config->setInitSoilNitrate(ipm.valueAsDouble("init_values", "init_soil_nitrate", -1.0));
  hermes_config->setInitSoilAmmonium(ipm.valueAsDouble("init_values", "init_soil_ammonium", -1.0));


  const Monica::Result result = runWithHermesData(hermes_config);

  delete hermes_config;
  return result;
	*/


	/*
	PVPsList PVPflanze::disallowedFor(Betrieb* b)
	{
		static Private::L lockable;
		static bool initialized = false;
		typedef map<int, PVPsList> Int2List;
		static Int2List mfId2dpvs; //Marktfruchtbau
		static Int2List ffbId2dpvs; //Feldfutterbau
		static Int2List bewId2dpvs; //Bewirtschaftungsweise
		static Int2List zfId2dpvs; //Zwischenfrucht

		if(!initialized)
		{
			Private::L::Lock lock(lockable);

			if(!initialized)
			{
				const Collection& allPvs = PVPflanze::all();
				//make pvs fast accessible via their id
				map<int, PVPflanze*> id2pv;
				for(Collection::const_iterator i = allPvs.begin(); i != allPvs.end(); i++)
					id2pv.insert(make_pair((*i)->id, *i));

				DBPtr con(newConnection("eom"));
				DBRow row;

				con->select("select MFVarNr, PVPNr FROM AWPVPVarMF");
				while(!(row = con->getRow()).empty())
				{
					PVPsList& dpvs = mfId2dpvs[satoi(row[0])];
					dpvs.push_back(id2pv[satoi(row[1])]);
				}

				con->select("select FFBVarNr, PVPNr FROM AWPVPVarFFB");
				while(!(row = con->getRow()).empty())
				{
					PVPsList& dpvs = ffbId2dpvs[satoi(row[0])];
					dpvs.push_back(id2pv[satoi(row[1])]);
				}

				con->select("select BewNr, PVPNr FROM AWPVPBew");
				while(!(row = con->getRow()).empty())
				{
					PVPsList& dpvs = bewId2dpvs[satoi(row[0])];
					dpvs.push_back(id2pv[satoi(row[1])]);
				}

				con->select("select ZFVarNr, PVPNr FROM AWPVPZF");
				while(!(row = con->getRow()).empty())
				{
					PVPsList& dpvs = zfId2dpvs[satoi(row[0])];
					dpvs.push_back(id2pv[satoi(row[1])]);
				}

				initialized = true;
			}
		}

		PVPsSet result;
		Int2List::const_iterator it;

		if(b->marktfruchtbau &&
			 (it = mfId2dpvs.find(b->marktfruchtbau->id)) != mfId2dpvs.end())
		{
			result.insert(it->second.begin(), it->second.end());
		}

		if(b->feldfutterbau &&
			 (it = ffbId2dpvs.find(b->feldfutterbau->id)) != ffbId2dpvs.end())
		{
			result.insert(it->second.begin(), it->second.end());
		}

		if(b->bewirtschaftungsweise &&
			 (it = bewId2dpvs.find(b->bewirtschaftungsweise->id)) != bewId2dpvs.end())
		{
			result.insert(it->second.begin(), it->second.end());
		}

		if(b->zwischenfruchtanbau &&
			 (it = zfId2dpvs.find(b->zwischenfruchtanbau->id)) != zfId2dpvs.end())
		{
			result.insert(it->second.begin(), it->second.end());
		}

		return PVPsList(result.begin(), result.end());
	}
	*/


}
