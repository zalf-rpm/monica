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

#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <set>
#include <sstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <thread>

#include "json11/json11.hpp"

#include "run-monica.h"
#include "tools/debug.h"
#include "climate/climate-common.h"
#include "db/abstract-db-connections.h"

#ifdef MONICA_GUI
#include "../gui/workerconfiguration.h"
#else
#include "../io/configuration.h"
#endif

using namespace Monica;
using namespace std;
using namespace Climate;
using namespace Tools;
using namespace Soil;
using namespace json11;

/**
 * @brief Static method for starting calculation
 * @param env
 */
#ifndef	MONICA_GUI
Result Monica::runMonica(Env env)
#else
Result Monica::runMonica(Env env, Monica::Configuration* cfg)
#endif
{
  
  Result res;
	res.customId = env.customId;
//  res.gp = env.gridPoint;

  if(env.cropRotation.begin() == env.cropRotation.end())
  {
    debug() << "Error: Fruchtfolge is empty" << endl;
    return res;
  }

  debug() << "starting Monica" << endl;


  ofstream fout;
  ofstream gout;

  bool write_output_files = false;

  // activate writing to output files only in special modes
  if (env.getMode() == Env::MODE_HERMES ||
      env.getMode() == Env::MODE_EVA2 ||
      env.getMode() == Env::MODE_MACSUR_SCALING ||
      env.getMode() == Env::MODE_ACTIVATE_OUTPUT_FILES)
  {

    write_output_files = true;
    debug() << "write_output_files: " << write_output_files << endl;
  }

  if (env.getMode() == Env::MODE_SENSITIVITY_ANALYSIS ||
      env.getMode() == Env::MODE_MACSUR_SCALING_CALIBRATION ) 
  {
    write_output_files = false;
  }

	env.centralParameterProvider.writeOutputFiles = write_output_files;

	debug() << "-----" << endl;

	MonicaModel monica(env, env.da);

	if (write_output_files)
	{
		//    static int ___c = 1;
		//    ofstream fout("/home/nendel/devel/lsa/models/monica/rmout.dat");
		//    ostringstream fs;
		//    fs << "/home/michael/development/lsa/landcare-dss/rmout-" << ___c << ".dat";
		//    ostringstream gs;
		//    gs << "/home/michael/development/lsa/landcare-dss/smout-" << ___c << ".dat";
		//    env.pathToOutputDir = "/home/nendel/devel/git/models/monica/";
		//    ofstream fout(fs.str().c_str());//env.pathToOutputDir+"rmout.dat").c_str());
		//    ofstream gout(gs.str().c_str());//env.pathToOutputDir+"smout.dat").c_str());
		//    ___c++;

		// open rmout.dat
		debug() << "Outputpath: " << (env.pathToOutputDir+pathSeparator()+"rmout.dat").c_str() << endl;
		fout.open((env.pathToOutputDir + pathSeparator()+ "rmout.dat").c_str());
		if (fout.fail())
		{
			debug() << "Error while opening output file \"" << (env.pathToOutputDir + pathSeparator() + "rmout.dat").c_str() << "\"" << endl;
			return res;
		}

		// open smout.dat
		gout.open((env.pathToOutputDir + pathSeparator() + "smout.dat").c_str());
		if (gout.fail())
		{
			debug() << "Error while opening output file \"" << (env.pathToOutputDir + pathSeparator() + "smout.dat").c_str() << "\"" << endl;
			return res;
		}

		MonicaModel monica(env, env.da);
		// writes the header line to output files
		initializeFoutHeader(fout);
		initializeGoutHeader(gout);

		dumpMonicaParametersIntoFile(env.pathToOutputDir, env.centralParameterProvider);
	}

	//debug() << "MonicaModel" << endl;
	//debug() << env.toString().c_str();
	
	debug() << "currentDate" << endl;
	Date currentDate = env.da.startDate();
	unsigned int nods = env.da.noOfStepsPossible();
	debug() << "nods: " << nods << endl;

	unsigned int currentMonth = currentDate.month();
	unsigned int dim = 0; //day in current month

	double avg10corg = 0, avg30corg = 0, watercontent = 0,
			groundwater = 0,  nLeaching= 0, yearly_groundwater=0,
			yearly_nleaching=0, monthSurfaceRunoff = 0.0;
	double monthPrecip = 0.0;
	double monthETa = 0.0;

	//iterator through the production processes
	vector<ProductionProcess>::const_iterator ppci = env.cropRotation.begin();
	//direct handle to current process
	ProductionProcess currentPP = *ppci;
	//are the dates in the production process relative dates
	//or are they absolute as produced by the hermes inputs
	bool useRelativeDates =  currentPP.start().isRelativeDate();
	//the next application date, either a relative or an absolute date
	//to get the correct applications out of the production processes
	Date nextPPApplicationDate = currentPP.start();
	//a definitely absolute next application date to keep track where
	//we are in the list of climate data
	Date nextAbsolutePPApplicationDate =
			useRelativeDates ? nextPPApplicationDate.toAbsoluteDate
												 (currentDate.year() + 1) : nextPPApplicationDate;
	debug() << "next app-date: " << nextPPApplicationDate.toString()
					<< " next abs app-date: " << nextAbsolutePPApplicationDate.toString() << endl;

	//if for some reason there are no applications (no nothing) in the
	//production process: quit
	if(!nextAbsolutePPApplicationDate.isValid())
	{
		debug() << "start of production-process: " << currentPP.toString()
						<< " is not valid" << endl;
		return res;
	}

	//beware: !!!! if there are absolute days used, then there is basically
	//no rotation if the last crop in the crop rotation has changed
	//the loop starts anew but the first crops date has already passed
	//so the crop won't be seeded again or any work applied
	//thus for absolute dates the crop rotation has to be as long as there
	//are climate data !!!!!

	/* post progress of calculation */
#ifndef	MONICA_GUI
	float progress = 0.0;
#else
	WorkerConfiguration* wCfg = static_cast<WorkerConfiguration*>(cfg);
#endif

	for(unsigned int d = 0; d < nods; ++d, ++currentDate, ++dim)
	{
		/* little progress bar */
#ifndef MONICA_GUI
		//    if (d % int(nods / 100) == 0 && !activateDebug) {
		//      int barWidth = 70;
		//      std::cout << "[";
		//      int pos = barWidth * progress;
		//      for (int i = 0; i < barWidth; ++i) {
		//        if (i < pos) std::cout << "=";
		//        else if (i == pos) std::cout << ">";
		//        else std::cout << " ";
		//      }
		//      if (progress >= 1.0) /* some days missing due to rounding */
		//        std::cout << "] " << 100 << "% (" << nods << " of " << nods << " days)\r";
		//      else
		//        std::cout << "] " << int(progress * 100) << "% (" << d << " of " << nods << " days)\r";
		//      std::cout.flush();
		//      progress += 0.01;
		//    }
		//    else if (d == nods - 1)
		//      std::cout << std::endl;
#else
		wCfg->setProgress(double(d) / double(nods));
#endif
		
    debug() << "currentDate: " << currentDate.toString() << endl;
    monica.resetDailyCounter();

//    if (currentDate.year() == 2012) {
//        cout << "Reaching problem year :-)" << endl;
//    }
    // test if monica's crop has been dying in previous step
    // if yes, it will be incorporated into soil
    if (monica.cropGrowth() && monica.cropGrowth()->isDying()) {
        monica.incorporateCurrentCrop();
    }

    /////////////////////////////////////////////////////////////////
    // AUTOMATIC HARVEST TRIGGER
    /////////////////////////////////////////////////////////////////

    /**
  * @TODO Change passing of automatic trigger parameters when building crop rotation (specka).
  * The automatic harvest trigger is passed globally to the method that reads in crop rotation
  * via hermes files because it cannot be configured crop specific with the HERMES format.
  * The harvest trigger prevents the adding of a harvest application as done in the normal case
  * that uses hard coded harvest data configured via the rotation file.
  *
  * When using the Json format, for each crop individual settings can be specified. The automatic
  * harvest trigger should be one of those options. Don't forget to pass a crop-specific latest
  * harvest date via json parameters too, that is now specified in the sqlite database globally
  * for each crop.
  */

    // Test if automatic harvest trigger is used
    if (monica.cropGrowth() && currentPP.crop()->useAutomaticHarvestTrigger()) {

      // Test if crop should be harvested at maturity
      if (currentPP.crop()->getAutomaticHarvestParams().getHarvestTime() == AutomaticHarvestTime::maturity) {

        if (monica.cropGrowth()->maturityReached() || currentPP.crop()->getAutomaticHarvestParams().getLatestHarvestDOY() == currentDate.julianDay()) {

          debug() << "####################################################" << endl;
          debug() << "AUTOMATIC HARVEST TRIGGER EVENT" << endl;
          debug() << "####################################################" << endl;

          auto harvestApplication = make_unique<Harvest>(currentDate, currentPP.crop(), currentPP.cropResultPtr());
          harvestApplication->apply(&monica);

        }
      }
    }

    //there's something to at this day
    if(nextAbsolutePPApplicationDate == currentDate)
    {
      debug() << "applying at: " << nextPPApplicationDate.toString()
              << " absolute-at: " << nextAbsolutePPApplicationDate.toString() << endl;
      //apply everything to do at current day
      //cout << currentPP.toString().c_str() << endl;
      currentPP.apply(nextPPApplicationDate, &monica);

      //get the next application date to wait for (either absolute or relative)
      Date prevPPApplicationDate = nextPPApplicationDate;

      nextPPApplicationDate =  currentPP.nextDate(nextPPApplicationDate);

      nextAbsolutePPApplicationDate =  useRelativeDates ? nextPPApplicationDate.toAbsoluteDate
          (currentDate.year() + (nextPPApplicationDate.dayOfYear() > prevPPApplicationDate.dayOfYear() ? 0 : 1),
           true) : nextPPApplicationDate;


      debug() << "next app-date: " << nextPPApplicationDate.toString()
          << " next abs app-date: " << nextAbsolutePPApplicationDate.toString() << endl;
      //if application date was not valid, we're (probably) at the end
      //of the application list of this production process
      //-> go to the next one in the crop rotation


      if(!nextAbsolutePPApplicationDate.isValid())
      {
        //get yieldresults for crop
        PVResult r = currentPP.cropResult();
				r.customId = currentPP.customId();
        r.date = currentDate;

        if(!env.useSecondaryYields)
          r.pvResults[secondaryYield] = 0;
        r.pvResults[sumFertiliser] = monica.sumFertiliser();
        r.pvResults[daysWithCrop] = monica.daysWithCrop();
        r.pvResults[NStress] = monica.getAccumulatedNStress();
        r.pvResults[WaterStress] = monica.getAccumulatedWaterStress();
        r.pvResults[HeatStress] = monica.getAccumulatedHeatStress();
        r.pvResults[OxygenStress] = monica.getAccumulatedOxygenStress();

				res.pvrs.push_back(r);
				//        debug() << "py: " << r.pvResults[primaryYield] << endl;
				//            << " sy: " << r.pvResults[secondaryYield]
				//            << " iw: " << r.pvResults[sumIrrigation]
				//            << " sf: " << monica.sumFertiliser()
				//            << endl;

				//to count the applied fertiliser for the next production process
				monica.resetFertiliserCounter();

        //resets crop values for use in next year
        currentPP.crop()->reset();

        ppci++;
        //start anew if we reached the end of the crop rotation
				if(ppci == env.cropRotation.end())
					ppci = env.cropRotation.begin();

        currentPP = *ppci;
        nextPPApplicationDate = currentPP.start();
        nextAbsolutePPApplicationDate =
            useRelativeDates ? nextPPApplicationDate.toAbsoluteDate
            (currentDate.year() + (nextPPApplicationDate.dayOfYear() > prevPPApplicationDate.dayOfYear() ? 0 : 1),
             true) : nextPPApplicationDate;
        debug() << "new valid next app-date: " << nextPPApplicationDate.toString()
            << " next abs app-date: " << nextAbsolutePPApplicationDate.toString() << endl;
      }
      //if we got our next date relative it might be possible that
      //the actual relative date belongs into the next year
      //this is the case if we're already (dayOfYear) past the next dayOfYear
      if(useRelativeDates && currentDate > nextAbsolutePPApplicationDate)
        nextAbsolutePPApplicationDate.addYears(1);
    }
    // write simulation date to file
    if (write_output_files)
    {
      fout << currentDate.toString("/");
      gout << currentDate.toString("/");
    }

    // run crop step
    if(monica.isCropPlanted())
      monica.cropStep(d);

    // writes crop results to output file
    if (write_output_files)
      writeCropResults(monica.cropGrowth(), fout, gout, monica.isCropPlanted());

    monica.generalStep(d);

    // write special outputs at 31.03.
    if(currentDate.day() == 31 && currentDate.month() == 3)
    {
      res.generalResults[sum90cmYearlyNatDay].push_back(monica.sumNmin(0.9));
      //      debug << "N at: " << monica.sumNmin(0.9) << endl;
      res.generalResults[sum30cmSoilTemperature].push_back(monica.sumSoilTemperature(3));
      res.generalResults[sum90cmYearlyNO3AtDay].push_back(monica.sumNO3AtDay(0.9));
      res.generalResults[avg30cmSoilTemperature].push_back(monica.avg30cmSoilTemperature());
      //cout << "MONICA_TEMP:\t" << monica.avg30cmSoilTemperature() << endl;
      res.generalResults[avg0_30cmSoilMoisture].push_back(monica.avgSoilMoisture(0,3));
      res.generalResults[avg30_60cmSoilMoisture].push_back(monica.avgSoilMoisture(3,6));
      res.generalResults[avg60_90cmSoilMoisture].push_back(monica.avgSoilMoisture(6,9));
      res.generalResults[avg0_90cmSoilMoisture].push_back(monica.avgSoilMoisture(0,9));
      res.generalResults[waterFluxAtLowerBoundary].push_back(monica.groundWaterRecharge());
      res.generalResults[avg0_30cmCapillaryRise].push_back(monica.avgCapillaryRise(0,3));
      res.generalResults[avg30_60cmCapillaryRise].push_back(monica.avgCapillaryRise(3,6));
      res.generalResults[avg60_90cmCapillaryRise].push_back(monica.avgCapillaryRise(6,9));
      res.generalResults[avg0_30cmPercolationRate].push_back(monica.avgPercolationRate(0,3));
      res.generalResults[avg30_60cmPercolationRate].push_back(monica.avgPercolationRate(3,6));
      res.generalResults[avg60_90cmPercolationRate].push_back(monica.avgPercolationRate(6,9));
      res.generalResults[evapotranspiration].push_back(monica.getEvapotranspiration());
      res.generalResults[transpiration].push_back(monica.getTranspiration());
      res.generalResults[evaporation].push_back(monica.getEvaporation());
      res.generalResults[sum30cmSMB_CO2EvolutionRate].push_back(monica.get_sum30cmSMB_CO2EvolutionRate());
      res.generalResults[NH3Volatilised].push_back(monica.getNH3Volatilised());
      res.generalResults[sum30cmActDenitrificationRate].push_back(monica.getsum30cmActDenitrificationRate());
      res.generalResults[leachingNAtBoundary].push_back(monica.nLeaching());
    }

    if(( currentDate.month() != currentMonth )|| d == nods-1)
    {
      currentMonth = currentDate.month();

      res.generalResults[avg10cmMonthlyAvgCorg].push_back(avg10corg / double(dim));
      res.generalResults[avg30cmMonthlyAvgCorg].push_back(avg30corg / double(dim));
      res.generalResults[mean90cmMonthlyAvgWaterContent].push_back(monica.mean90cmWaterContent());
      res.generalResults[monthlySumGroundWaterRecharge].push_back(groundwater);
      res.generalResults[monthlySumNLeaching].push_back(nLeaching);
      res.generalResults[maxSnowDepth].push_back(monica.maxSnowDepth());
      res.generalResults[sumSnowDepth].push_back(monica.getAccumulatedSnowDepth());
      res.generalResults[sumFrostDepth].push_back(monica.getAccumulatedFrostDepth());
      res.generalResults[sumSurfaceRunOff].push_back(monica.sumSurfaceRunOff());
      res.generalResults[sumNH3Volatilised].push_back(monica.getSumNH3Volatilised());
      res.generalResults[monthlySurfaceRunoff].push_back(monthSurfaceRunoff);
      res.generalResults[monthlyPrecip].push_back(monthPrecip);
      res.generalResults[monthlyETa].push_back(monthETa);
      res.generalResults[monthlySoilMoistureL0].push_back(monica.avgSoilMoisture(0,1) * 100.0);
      res.generalResults[monthlySoilMoistureL1].push_back(monica.avgSoilMoisture(1,2) * 100.0);
      res.generalResults[monthlySoilMoistureL2].push_back(monica.avgSoilMoisture(2,3) * 100.0);
      res.generalResults[monthlySoilMoistureL3].push_back(monica.avgSoilMoisture(3,4) * 100.0);
      res.generalResults[monthlySoilMoistureL4].push_back(monica.avgSoilMoisture(4,5) * 100.0);
      res.generalResults[monthlySoilMoistureL5].push_back(monica.avgSoilMoisture(5,6) * 100.0);
      res.generalResults[monthlySoilMoistureL6].push_back(monica.avgSoilMoisture(6,7) * 100.0);
      res.generalResults[monthlySoilMoistureL7].push_back(monica.avgSoilMoisture(7,8) * 100.0);
      res.generalResults[monthlySoilMoistureL8].push_back(monica.avgSoilMoisture(8,9) * 100.0);
      res.generalResults[monthlySoilMoistureL9].push_back(monica.avgSoilMoisture(9,10) * 100.0);
      res.generalResults[monthlySoilMoistureL10].push_back(monica.avgSoilMoisture(10,11) * 100.0);
      res.generalResults[monthlySoilMoistureL11].push_back(monica.avgSoilMoisture(11,12) * 100.0);
      res.generalResults[monthlySoilMoistureL12].push_back(monica.avgSoilMoisture(12,13) * 100.0);
      res.generalResults[monthlySoilMoistureL13].push_back(monica.avgSoilMoisture(13,14) * 100.0);
      res.generalResults[monthlySoilMoistureL14].push_back(monica.avgSoilMoisture(14,15) * 100.0);
      res.generalResults[monthlySoilMoistureL15].push_back(monica.avgSoilMoisture(15,16) * 100.0);
      res.generalResults[monthlySoilMoistureL16].push_back(monica.avgSoilMoisture(16,17) * 100.0);
      res.generalResults[monthlySoilMoistureL17].push_back(monica.avgSoilMoisture(17,18) * 100.0);
      res.generalResults[monthlySoilMoistureL18].push_back(monica.avgSoilMoisture(18,19) * 100.0);


//      cout << "c10: " << (avg10corg / double(dim))
//          << " c30: " << (avg30corg / double(dim))
//          << " wc: " << (watercontent / double(dim))
//          << " gwrc: " << groundwater
//          << " nl: " << nLeaching
//          << endl;

      avg10corg = avg30corg = watercontent = groundwater = nLeaching =  monthSurfaceRunoff = 0.0;
      monthPrecip = 0.0;
      monthETa = 0.0;

      dim = 0;
      //cout << "stored monthly values for month: " << currentMonth  << endl;
    }
    else
    {
			avg10corg += monica.avgCorg(0.1);
			avg30corg += monica.avgCorg(0.3);
			watercontent += monica.mean90cmWaterContent();
			groundwater += monica.groundWaterRecharge();

			//cout << "groundwater-recharge at: " << currentDate.toString() << " value: " << monica.groundWaterRecharge() << " monthlySum: " << groundwater << endl;
			nLeaching += monica.nLeaching();
			monthSurfaceRunoff += monica.surfaceRunoff();
			monthPrecip += env.da.dataForTimestep(Climate::precip, d);
			monthETa += monica.getETa();
		}

		// Yearly accumulated values
		if ((currentDate.year() != (currentDate-1).year()) && (currentDate.year()!= env.da.startDate().year()))
		{
			res.generalResults[yearlySumGroundWaterRecharge].push_back(yearly_groundwater);
			//        cout << "#######################################################" << endl;
			//        cout << "Push back yearly_nleaching: " << currentDate.year()  << "\t" << yearly_nleaching << endl;
			//        cout << "#######################################################" << endl;
			res.generalResults[yearlySumNLeaching].push_back(yearly_nleaching);
			yearly_groundwater = 0.0;
			yearly_nleaching = 0.0;
		}
		else
		{
			yearly_groundwater += monica.groundWaterRecharge();
			yearly_nleaching += monica.nLeaching();
		}

		if (monica.isCropPlanted())
		{
			//cout << "monica.cropGrowth()->get_GrossPrimaryProduction()\t" << monica.cropGrowth()->get_GrossPrimaryProduction() << endl;

			res.generalResults[dev_stage].push_back(monica.cropGrowth()->get_DevelopmentalStage()+1);


		}
		else
		{
      res.generalResults[dev_stage].push_back(0.0);
		}

		res.dates.push_back(currentDate.toMysqlString());

		if (write_output_files)
		{
			writeGeneralResults(fout, gout, env, monica, d);
		}
	}
	if (write_output_files)
  {
    fout.close();
    gout.close();
  }

  //cout << res.dates.size() << endl;
//  cout << res.toString().c_str();
  debug() << "returning from runMonica" << endl;

  return res;
}

