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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include "boost/foreach.hpp"

#include "tools/use-stl-algo-boost-lambda.h"
#include "db/db.h"
#include "db/abstract-db-connections.h"
#include "tools/date.h"
#include "tools/algorithms.h"
#include "tools/helper.h"

#include "debug.h"
#include "monica.h"
#include "monica-eom.h"
#include "monica-parameters.h"
#include "simulation.h"

using namespace std;
using namespace Db;
using namespace Tools;
using namespace Monica;

#ifdef RUN_LANDCARE_DSS

#include "eom/src/eom.h"
using namespace Eom;

struct E
{
  E() :
      year(2025),
      station("PRENZLAU"),
      simulation("star2"),
      scenario("2K"),
      realization("1"),
      dontRotateCropRotation(true),
      heightNN(50),
      slope(0.01),
      region("uecker"),
      weisseritzBk50Id(-1),
      ueckerSTR("Al2a03"),
      farmName("ImpAss"),
      cropRotationName("WR"),
      useNMinMethod(true),
      useAutoIrrigation(true),
      noOfLayers(20),
      layerThickness(0.1),
      co2(-1),
      activateDebugOutput(true),
      showGeneralResultsOutput(false),
      activateOutputFiles(true),
      pathToOutputDir("C:/Users/nendel/development/nendel-data/ImpAss/")
  {}

  int year;
  string station;
  string simulation;
  string scenario;
  string realization;
  bool dontRotateCropRotation;
  double heightNN;
  int slope;
  string region;
  int weisseritzBk50Id;
  string ueckerSTR;
  string farmName;
  string cropRotationName;
  bool useNMinMethod;
  bool useAutoIrrigation;
  int noOfLayers;
  double layerThickness;
  double co2;
  bool activateDebugOutput;
  bool showGeneralResultsOutput;
  bool activateOutputFiles;
  string pathToOutputDir;
};

//! test monica in the LandcareDSS environment
void runLandcareDSSMonica(E e)
{
  activateDebug = e.activateDebugOutput;

        //input data
  CentralParameterProvider cpp =
      readUserParameterFromDatabase(Monica::Env::MODE_LC_DSS);


  AvailableClimateData acds[] = {tmin, tavg, tmax, globrad, relhumid, wind,
                                 precip, sunhours};

  //one year in front for monica or EOM vorjahr and the 14 years for the statistics
  Date start(1, 1, e.year - 14 - 1);
  //noOfPVs - 1 years for the following PVs in the fruchtfolge + 15 years statistics
  Date end(31, 12, e.year + 15);// + noOfPVs - 1);
  debug() << "start: " << start.toString() << " end: " << end.toString() << endl;

  ClimateSimulation* s = NULL;
  if(e.simulation == "wettreg")
    s = newDDWettReg2006();
  else if(e.simulation == "star")
    s = new StarSimulation(toMysqlDB(newConnection("star")));
  else if(e.simulation == "star2")
    s = new Star2Simulation(toMysqlDB(newConnection("star2")));
  if(!s)
  {
    cout << "There is no simulation:" << e.simulation << " supported right now!" << endl;
    return;
  }
  ClimateScenario* scen = s->scenario(e.scenario);
  if(!scen)
  {
    if(s->scenarios().size() == 1)
    {
      scen = s->scenarios().front();
      cout << "There is no scenario: " << e.scenario
          << " but there is only one anyway, so choosing: "
          << scen->name() << " instead of ending." << endl;
    }
    else
    {
      cout << "There is no scenario: " << e.scenario << "." << endl;
      return;
    }
  }

  LatLngCoord llc = s->climateStation2geoCoord(e.station);


  typedef map<string, DataAccessor> DAS;
  DAS das;
  BOOST_FOREACH(ClimateRealization* r, scen->realizations())
  {
    if(e.realization == "all" ||
       r->name() == e.realization ||
       scen->realizations().size() == 1)
      das[r->name()] = r->dataAccessorFor(vector<ACD > (acds, acds + 8), llc,
                                          start, end);
  }

	//the betrieb to use
  Betrieb* farm = NULL;
  BOOST_FOREACH(Betrieb* f, Betrieb::all())
  {
		//if(f->name.find(e.farmName) == 0)
		if(f->name == e.farmName)
    {
      farm = f;
			break;
    }
	}
  if(farm)
    debug() << "Found farm: " << e.farmName << endl;
  else
  {
    cerr << "Didn't find farm: " << e.farmName << endl;
    exit(1);
  }

  Fruchtfolge* cr = NULL;
  BOOST_FOREACH(Fruchtfolge* ff, farm->ffs)
  {
		//if(ff->name.find(e.cropRotationName) == 0)
		if(ff->name == e.cropRotationName)
    {
      cr = ff;
      break;
    }
  }
  if(cr)
    debug() << "Found crop rotation: " << e.cropRotationName << endl;
  else
  {
    cerr << "Didn't find cropRotation: " << e.cropRotationName;
    return;
  }

  //build up the monica environment
  GeneralParameters genps(e.layerThickness);

  const SoilPMs* sps = NULL;
  if(e.region == "weisseritz")
    sps = weisseritzSoilParameters(e.weisseritzBk50Id, genps, true);
  else if(e.region == "uecker")
    sps = ueckerSoilParameters(e.ueckerSTR, genps, true);
  else
  {
    cerr << "Wrong region: " << e.region << endl;
    return;
  }
  if(sps->empty())
  {
    cerr << "No soil parameters available for region: " << e.region
        << " and weisseritzBk50Id: " << e.weisseritzBk50Id
        << " or ueckerSTR: " << e.ueckerSTR << endl;
    return;
  }

  Monica::Env env(sps, cpp);
  env.noOfLayers = e.noOfLayers;
  env.layerThickness = e.layerThickness; //[m]
  env.site.vs_Slope = double(e.slope) / 100.0;
  env.site.vs_HeightNN = e.heightNN;
  env.site.vs_Latitude = llc.lat;
  env.general = genps;
  env.setMode(Monica::Env::MODE_LC_DSS);
  env.pathToOutputDir = e.pathToOutputDir;
  if(env.pathToOutputDir.at(env.pathToOutputDir.length()-1) != '/')
    env.pathToOutputDir.append("/");
  if(e.activateOutputFiles)
    env.setMode(Monica::Env::MODE_ACTIVATE_OUTPUT_FILES);

  env.atmosphericCO2 = e.co2;

  if((env.useNMinMineralFertilisingMethod = e.useNMinMethod))
  {
    env.nMinUserParams.min = 10;
    env.nMinUserParams.max = 100;
    env.nMinUserParams.delayInDays = 30;

    Fertilizer* f = Fertilizer::f4id(1); //Kalkammonsalpeter (KAS)
    env.nMinFertiliserPartition =
        MineralFertiliserParameters(f->name, f->amidn / f->n,
                                    f->no3n / f->n, f->no4n / f->n);
  }

  if((env.useAutomaticIrrigation = e.useAutoIrrigation))
  {
    env.useAutomaticIrrigation = true;
    env.autoIrrigationParams.amount = 15;
  }

	SeedHarvestDates shDates = seedHarvestDates(cr->pvs);
  FertiliserApplicationData mineralFertilisingData =
    extractFertiliserData(cr, mineralFertiliserIds());
  FertiliserApplicationData organicFertilisingData =
    extractFertiliserData(cr, organicFertiliserIds());

  BOOST_FOREACH(Produktionsverfahren* pv, cr->pvs)
  {
    PVPId pvpId = pv->base->id;
    const EomPVPInfo& i = eomPVPId2cropId(pvpId);
    const CropParameters* cps = getCropParametersFromMonicaDB(i.cropId);
    const OrganicMatterParameters* rps =
        getResidueParametersFromMonicaDB(i.cropId);
    CropPtr crop(new Crop(i.cropId, cps->pc_CropName,
                          shDates[pv].first, shDates[pv].second, cps, rps,
                          i.crossCropAdaptionFactor));
    ProductionProcess pp(crop->name(), crop);
    if(!env.useNMinMineralFertilisingMethod)
    {
      BOOST_FOREACH(FertiliserData mfd, mineralFertilisingData[pv])
      {
        Fertilizer* f = mfd.fertiliser;
        MineralFertiliserParameters mfps(f->name, f->amidn / f->n,
                                         f->no3n / f->n, f->no4n / f->n);
        pp.addApplication(MineralFertiliserApplication
                          (mfd.at, mfps, mfd.amountN / mfd.fraction));
      }

      BOOST_FOREACH(FertiliserData ofd, organicFertilisingData[pv])
      {
        Fertilizer* f = ofd.fertiliser;

        int monicaId = eomOrganicFertilizerId2monicaOrganicFertilizerId(f->id);
        const OrganicMatterParameters* ofps =
            getOrganicFertiliserParametersFromMonicaDB(monicaId);

        pp.addApplication(OrganicFertiliserApplication
                          (ofd.at, ofps, ofd.amountN / ofd.fraction));
      }
    }

    if(pv->irrigate() && !env.useAutomaticIrrigation)
    {
      env.useAutomaticIrrigation = true;
      env.autoIrrigationParams.amount = 20;
    }

    env.cropRotation.push_back(pp);
  }

  typedef int Count;
  typedef map<Monica::ResultId, vector<double> > CR;
  typedef map<CropId, CR> CropResults;
  typedef map<Year, CropResults> YearlyCropResults;
  YearlyCropResults avgYearlyCropResults;
  CropResults avgCropResults;

  typedef map<Monica::ResultId, vector<double> > GeneralResults;
  typedef map<Year, GeneralResults> YearlyGeneralResults;
  YearlyGeneralResults avgYearlyGeneralResults;
  GeneralResults avgGeneralResults;

  //initialize with current env
  BOOST_FOREACH(DAS::value_type p, das)
  {
    string realizationName = p.first;
    DataAccessor da = p.second;

    //cycle through the produktionsverfahren to mitigate possible problems
    //due to a specific (random weather) starting year
    //so at least every PV has started under the same conditions in the FF
    for(int i = 0, size = env.cropRotation.size(); i < size; i++)
    {
      if(i > 0)
        rotate(env.cropRotation.begin(), env.cropRotation.begin()+i,
               env.cropRotation.end());
      env.da = da;

      Monica::Result res = runMonica(env);

      cout << "realization: " << realizationName << " cropRotation: ";
      for_each(env.cropRotation.begin(), env.cropRotation.end(),
							 cout << boost::lambda::bind(&ProductionProcess::name, _1) << " | ");
      cout << endl;
      cout << "----------------------------------------------------" << endl;

      int year = start.year() + 1;
      cout << "noys: " << res.pvrs.size() << endl;
      //show crop results
      BOOST_FOREACH(Monica::PVResult pvr, res.pvrs)
      {
        cout << "year: " << year++ << " cropId: " << pvr.id << endl;
        cout << "---------------------------" << endl;
        typedef map<Monica::ResultId, double> M;
        BOOST_FOREACH(M::value_type p, pvr.pvResults)
        {
          Monica::ResultId rid = p.first;
          ResultIdInfo info = resultIdInfo(rid);
          cout << rid << " " << info.name << " [" << info.unit << "]: " << p.second << endl;

          avgYearlyCropResults[year][pvr.id][rid].push_back(p.second);
          avgCropResults[pvr.id][rid].push_back(p.second);
        }
        cout << "---------------------------" << endl;
      }

      if(e.showGeneralResultsOutput)
      {
        cout << "general results (monthly and yearly values)" << endl;
        //show general results
        cout << "---------------------------" << endl;
        BOOST_FOREACH(Monica::Result::RId2Vector::value_type p, res.generalResults)
        {
          Monica::ResultId rid = p.first;
          ResultIdInfo info = resultIdInfo(rid);
          cout << rid << " " << info.name << " [" << info.unit << "]:" << endl;

          for_each(p.second.begin(), p.second.end(), cout << _1 << " ");
          cout << endl;
          cout << "---------------------------" << endl;

          avgYearlyGeneralResults[year][rid].insert
              (avgYearlyGeneralResults[year][rid].end(),
               p.second.begin(), p.second.end());

          avgGeneralResults[rid].insert(avgGeneralResults[rid].end(),
                                        p.second.begin(), p.second.end());
        }
        cout << "----------------------------------------------------" << endl;
      }

      //just run once
      if(e.dontRotateCropRotation)
        goto leave;
    }
    leave: ;
  }

  cout << endl;
  cout << "-------------------------------------------------------" << endl
      << "averaged over realizations:" << endl
      << "-------------------------------------------------------" << endl;

  cout << "crop results:" << endl
      << "---------------" << endl;
  BOOST_FOREACH(YearlyCropResults::value_type p, avgYearlyCropResults)
  {
    Year year = p.first;
    cout << "year: " << year << endl;

    BOOST_FOREACH(CropResults::value_type p2, p.second)
    {
      CropId cropId = p2.first;
      cout << "cropId: " << cropId << endl;

      BOOST_FOREACH(CR::value_type p3, p2.second)
      {
        Monica::ResultId rid = p3.first;
        pair<double, double> p4 = standardDeviationAndAvg(p3.second);

        ResultIdInfo info = resultIdInfo(rid);
        cout << rid << " " << info.name << " [ " << info.unit << "] avgValue: "
            << p4.second << " sigma: " << p4.first << endl;
      }
    }
    cout << "---------------------------" << endl;
  }
  cout << "-----------------------------------------" << endl;

  if(e.showGeneralResultsOutput)
  {
    cout << "general results:" << endl
        << "---------------" << endl;
    BOOST_FOREACH(YearlyGeneralResults::value_type p, avgYearlyGeneralResults)
    {
      Year year = p.first;
      cout << "year: " << year << endl;

      BOOST_FOREACH(GeneralResults::value_type p2, p.second)
      {
        Monica::ResultId rid = p2.first;
        pair<double, double> p3 = standardDeviationAndAvg(p2.second);

        ResultIdInfo info = resultIdInfo(rid);
        cout << rid << " " << info.name << " [ " << info.unit << "] avgValue: "
            << p3.second << " sigma: " << p3.first << endl;
      }
      cout << "---------------------------" << endl;
    }
    cout << "-----------------------------------------" << endl;
  }

  cout << endl;
  cout << "-------------------------------------------------------" << endl
      << "averaged over realizations and years:" << endl
      << "-------------------------------------------------------" << endl;

  cout << "crop results:" << endl
      << "---------------" << endl;

  BOOST_FOREACH(CropResults::value_type p, avgCropResults)
  {
    CropId cropId = p.first;
    cout << "cropId: " << cropId << endl;

    BOOST_FOREACH(CR::value_type p2, p.second)
    {
      Monica::ResultId rid = p2.first;
      pair<double, double> p3 = standardDeviationAndAvg(p2.second);

      ResultIdInfo info = resultIdInfo(rid);
      cout << rid << " " << info.name << " [ " << info.unit << "] avgValue: "
          << p3.second << " sigma: " << p3.first << endl;
    }
  }
  cout << "-----------------------------------------" << endl;

  if(e.showGeneralResultsOutput)
  {
    cout << "general results:" << endl
        << "---------------" << endl;

    BOOST_FOREACH(GeneralResults::value_type p, avgGeneralResults)
    {
      Monica::ResultId rid = p.first;
      pair<double, double> p2 = standardDeviationAndAvg(p.second);

      ResultIdInfo info = resultIdInfo(rid);
      cout << rid << " " << info.name << " [ " << info.unit << "] avgValue: "
          << p2.second << " sigma: " << p2.first << endl;
    }
    cout << "-----------------------------------------" << endl;
  }

  cout << "testLandcareDSS executed" << endl;
}

void testLandcareDSS(int argc, char** argv)
{
  E e;

  if((argc == 2 && ((argv[1][0] == '-' && argv[1][1] == 'h') ||
                    argv[1][0] == 'h')) ||
     (argc > 1 && (argc-1) % 2 == 1))
  {
    cout << "usage: run-monica" << endl
        << "\t[" << endl
        << "\t| year: " << e.year << endl
        << "\t| station: " << e.station << endl
        << "\t| simulation: " << e.simulation << " [wettreg | star]" << endl
        << "\t| scenario: " << e.scenario << " [A1B | B1] for wettreg or [---] for star" << endl
        << "\t| realization: " << e.realization << " [tro_a | nor_a | feu_a | all] for wettreg or [1] for star" << endl
        << "\t| dontRotate: " << (e.dontRotateCropRotation ? "true" : "false") << " [true | false]" << endl
        << "\t| heightNN: " << e.heightNN << endl
        << "\t| slope: " << e.slope << endl
        << "\t| region: " << e.region << " [weisseritz | uecker]" << endl
        << "\t| weisseritzBk50Id: " << e.weisseritzBk50Id << " (if region: weisseritz)" << endl
        << "\t| ueckerSTR: " << e.ueckerSTR << " (if region: uecker)" << endl
        << "\t| farmName: " << e.farmName << " (start of name is enough)" << endl
        << "\t| cropRotationName: " << e.cropRotationName << " (start of name is enough)" << endl
        << "\t| useNMinMethod: " << (e.useNMinMethod ? "true" : "false") << " [true | false]" << endl
        << "\t| useAutomaticIrrigation: " << (e.useAutoIrrigation ? "true" : "false") << " [true | false]" << endl
        << "\t| noOfLayers: " << e.noOfLayers << endl
        << "\t| layerThickness: " << e.layerThickness << " in meters" << endl
        << "\t| co2: " << e.co2 << " in ppm" << " [-1 means use Monica internal CO2-algorithm]" << endl
        << "\t| activateDebugOutput: " << (e.activateDebugOutput ? "true" : "false") << " [true | false]" << endl
        << "\t| showGeneralResultsOutput: " << (e.showGeneralResultsOutput ? "true" : "false") << " [true | false]" << endl
        << "\t| activateOutputFiles: " << (e.activateOutputFiles ? "true" : "false") << " [true | false]" << endl
        << "\t| pathToOutputDir: " << e.pathToOutputDir << " [path to dir for output files]" << endl
        << "\t]*" << endl;
    return;
  }
  else
  {
    for(int i = 1; i < argc; i+=2)
    {
      if(string(argv[i]) == trim("year:"))
        e.year = atoi(argv[i+1]);
      else if(string(argv[i]) == trim("station:"))
        e.station = trim(argv[i+1]);
      else if(string(argv[i]) == trim("simulation:"))
        e.simulation = trim(argv[i+1]);
      else if(string(argv[i]) == trim("scenario:"))
        e.scenario = trim(argv[i+1]);
      else if(string(argv[i]) == trim("realization:"))
        e.realization = trim(argv[i+1]);
      else if(string(argv[i]) == trim("dontRotate:"))
        e.dontRotateCropRotation = string(argv[i+1]) == "true";
      else if(string(argv[i]) == trim("heightNN:"))
        e.heightNN = atof(argv[i+1]);
      else if(string(argv[i]) == trim("slope:"))
        e.slope = atoi(argv[i+1]);
      else if(string(argv[i]) == trim("region:"))
        e.region = trim(argv[i+1]);
      else if(string(argv[i]) == trim("weisseritzBk50Id:"))
        e.weisseritzBk50Id = atoi(argv[i+1]);
      else if(string(argv[i]) == trim("ueckerSTR:"))
        e.ueckerSTR = argv[i+1];
      else if(string(argv[i]) == trim("farmName:"))
        e.farmName = trim(argv[i+1]);
      else if(string(argv[i]) == trim("cropRotationName:"))
        e.cropRotationName = trim(argv[i+1]);
      else if(string(argv[i]) == trim("useNMinMethod:"))
        e.useNMinMethod = string(argv[i+1]) == "true";
      else if(string(argv[i]) == trim("useAutoIrrigation:"))
        e.useAutoIrrigation = string(argv[i+1]) == "true";
      else if(string(argv[i]) == trim("noOfLayers:"))
        e.noOfLayers = atoi(argv[i+1]);
      else if(string(argv[i]) == trim("layerThickness:"))
        e.layerThickness = atof(argv[i+1]);
      else if(string(argv[i]) == trim("co2:"))
        e.co2 = atof(argv[i+1]);
      else if(string(argv[i]) == trim("activateDebugOutput:"))
        e.activateDebugOutput = string(argv[i+1]) == "true";
      else if(string(argv[i]) == trim("showGeneralResultsOutput:"))
        e.showGeneralResultsOutput = string(argv[i+1]) == "true";
      else if(string(argv[i]) == trim("activateOutputFiles:"))
        e.activateOutputFiles = string(argv[i+1]) == "true";
      else if(string(argv[i]) == trim("pathToOutputDir:"))
        e.pathToOutputDir = trim(argv[i+1]);
    }

    cout << "running monica with:" << endl
        << "\t| year: " << e.year << endl
        << "\t| station: " << e.station << endl
        << "\t| simulation: " << e.simulation << endl
        << "\t| scenario: " << e.scenario << endl
        << "\t| realization: " << e.realization << endl
        << "\t| dontRotate: " << (e.dontRotateCropRotation ? "true" : "false") << endl
        << "\t| heightNN: " << e.heightNN << endl
        << "\t| slope: " << e.slope << endl
        << "\t| region: " << e.region << endl
        << "\t| weisseritzBk50Id: " << e.weisseritzBk50Id << endl
        << "\t| ueckerSTR: " << e.ueckerSTR << endl
        << "\t| farmName: " << e.farmName << endl
        << "\t| cropRotationName: " << e.cropRotationName << endl
        << "\t| useNMinMethod: " << (e.useNMinMethod ? "true" : "false") << endl
        << "\t| useAutomaticIrrigation: " << (e.useAutoIrrigation ? "true" : "false") << endl
        << "\t| noOfLayers: " << e.noOfLayers << endl
        << "\t| layerThickness: " << e.layerThickness << " [m]" << endl
        << "\t| co2: " << e.co2 << endl
        << "\t| activateDebugOutput: " << (e.activateDebugOutput ? "true" : "false") << endl
        << "\t| showGeneralResultsOutput: " << (e.showGeneralResultsOutput ? "true" : "false") << endl
        << "\t| activateOutputFiles: " << (e.activateOutputFiles ? "true" : "false") << endl
        << "\t| pathToOutputDir: " << e.pathToOutputDir << endl;

    runLandcareDSSMonica(e);
  }

}

#endif //LANDCARE_DSS

/**
 * Main routine of stand alone model.
 * @param argc Number of program's arguments
 * @param argv Pointer of program's arguments
 */
int main(int argc, char** argv)
{
#if WIN32
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");

    //use the non-default db-conections-core.ini
		dbConnectionParameters("db-connections.ini");//-win32.ini");
#endif

#ifdef RUN_LANDCARE_DSS
  testLandcareDSS(argc, argv);
#endif

#ifdef RUN_HERMES
  runWithHermesData(argc == 2 ? string(argv[1])+"/" : "");
#endif

#ifdef RUN_EVA2
  runEVA2Simulation(0);
#endif

#ifdef RUN_CC_GERMANY
  CCGermanySimulationConfiguration *config = new CCGermanySimulationConfiguration();
  config->setBuekId(23);
  config->setJulianSowingDate(284.1);
  config->setGroundwaterDepth(-9999.9);
  config->setStatId(1757);
  config->setStartDate("1996-01-01");
  config->setEndDate("2025-12-31");
  runCCGermanySimulation(config);
  delete config;
#endif

#ifdef RUN_GIS
  GISSimulationConfiguration *config = new GISSimulationConfiguration();
  config->setJulianSowingDate(277);
  config->setRow(956);
  config->setCol(993);
  config->setStartDate("1996-01-01");
  config->setEndDate("2025-12-31");
  config->setOutputPath("python/gis_simulation");
  runGISSimulation(config);

#endif

  //cout << debug.str();

}
