/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
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
#include <tuple>

#include "run-monica.h"
#include "tools/debug.h"
#include "climate/climate-common.h"
#include "db/abstract-db-connections.h"
#include "../core/monica-typedefs.h"
#include "tools/json11-helper.h"

using namespace Monica;
using namespace std;
using namespace Climate;
using namespace Tools;
using namespace Soil;
using namespace json11;

Env::Env(CentralParameterProvider cpp)
	: params(cpp)
{}

Env::Env(json11::Json j)
{
	merge(j);
}

void Env::merge(json11::Json j)
{
	params.merge(j["params"]);

	cropRotation.clear();
	for(Json cmj : j["cropRotation"].array_items())
		cropRotation.push_back(cmj);

	set_bool_value(debugMode, j, "debugMode");
}

json11::Json Env::to_json() const
{
	J11Array cr;
	for(const auto& c : cropRotation)
		cr.push_back(c.to_json());

	return json11::Json::object{
		{"type", "Env"},
		{"params", params.to_json()},
		{"cropRotation", cr},
		{"da", da.to_json()},
		{"debugMode", debugMode}
	};
}

string Env::toString() const
{
	ostringstream s;
	s << " noOfLayers: " << params.simulationParameters.p_NumberOfLayers
		<< " layerThickness: " << params.simulationParameters.p_LayerThickness
		<< endl;
	s << "ClimateData: from: " << da.startDate().toString()
		<< " to: " << da.endDate().toString() << endl;
	s << "Fruchtfolge: " << endl;
	for(const CultivationMethod& cm : cropRotation)
		s << cm.toString() << endl;
	s << "customId: " << customId;
	return s.str();
}

/**
* Interface method for python wrapping, so climate module
* does not need to be wrapped by python.
*
* @param acd
* @param data
*/
void
Env::addOrReplaceClimateData(std::string name, const std::vector<double>& data)
{
	int acd = 0;
	if(name == "tmin")
		acd = tmin;
	else if(name == "tmax")
		acd = tmax;
	else if(name == "tavg")
		acd = tavg;
	else if(name == "precip")
		acd = precip;
	else if(name == "globrad")
		acd = globrad;
	else if(name == "wind")
		acd = wind;
	else if(name == "sunhours")
		acd = sunhours;
	else if(name == "relhumid")
		acd = relhumid;

	da.addOrReplaceClimateData(AvailableClimateData(acd), data);
}







/**
* Returns the result vector of a special output. Python swig is
* not able to wrap stl-maps correctly. Stl-vectors are working
* properly so this function has been implemented to use the results
* in wrapped python code.
*
* @param id ResultId of output
* @return Vector of result values
*/
std::vector<double> Result::getResultsById(int id)
{
	// test if crop results are requested
	if(id == primaryYield || id == secondaryYield || id == sumIrrigation ||
		 id == sumFertiliser || id == biomassNContent || id == sumTotalNUptake ||
		 id == cropHeight || id == cropname || id == sumETaPerCrop || sumTraPerCrop ||
		 id == primaryYieldTM || id == secondaryYieldTM || id == daysWithCrop || id == aboveBiomassNContent ||
		 id == NStress || id == WaterStress || id == HeatStress || id == OxygenStress || id == aboveGroundBiomass ||
		 id == anthesisDay || id == maturityDay || id == harvestDay ||
		 id == soilMoist0_90cmAtHarvest || id == corg0_30cmAtHarvest || id == nmin0_90cmAtHarvest)
	{
		vector<double> result_vector;
		int size = pvrs.size();
		for(int i = 0; i < size; i++)
		{
			CMResult crop_result = pvrs.at(i);
			result_vector.push_back(crop_result.results[(ResultId)id]);
		}
		return result_vector;
	}

	return generalResults[(ResultId)id];
}

std::string Result::toString()
{
	ostringstream s;
	map<ResultId, std::vector<double> >::iterator it;
	// show content:
	for(it = generalResults.begin(); it != generalResults.end(); it++)
	{
		ResultId id = (*it).first;
		std::vector<double> data = (*it).second;
		s << resultIdInfo(id).shortName.c_str() << ":\t" << data.at(data.size() - 1) << endl;
	}

	return s.str();
}

//--------------------------------------------------------------------------------------

pair<Date, map<Climate::ACD, double>>
Monica::climateDataForStep(const Climate::DataAccessor& da, size_t stepNo)
{
	Date startDate = da.startDate();
	Date currentDate = startDate + stepNo;
	double tmin = da.dataForTimestep(Climate::tmin, stepNo);
	double tavg = da.dataForTimestep(Climate::tavg, stepNo);
	double tmax = da.dataForTimestep(Climate::tmax, stepNo);
	double precip = da.dataForTimestep(Climate::precip, stepNo);
	double wind = da.dataForTimestep(Climate::wind, stepNo);
	double globrad = da.dataForTimestep(Climate::globrad, stepNo);

	// test if data for relhumid are available; if not, value is set to -1.0
	double relhumid = da.hasAvailableClimateData(Climate::relhumid)
		? da.dataForTimestep(Climate::relhumid, stepNo)
		: -1.0;

	map<Climate::ACD, double> m{
		{ Climate::tmin, tmin },
	{ Climate::tavg, tavg },
	{ Climate::tmax, tmax },
	{ Climate::precip, precip },
	{ Climate::wind, wind },
	{ Climate::globrad, globrad },
	{ Climate::relhumid, relhumid }};
	return make_pair(currentDate, m);
}

void writeDebugInputs(const Env& env, string fileName = "inputs.json")
{
	ofstream pout;
	pout.open(ensureDirExists(env.params.pathToOutputDir()) + fileName);
	if(pout.fail())
	{
		cerr << "Error couldn't open file: '" << env.params.pathToOutputDir() + "/" + fileName << "'." << endl;
		return;
	}
	pout << "{" << endl;
	auto cropPs = env.params.userCropParameters.to_json().dump();
	pout << "\"userCropParameters\":" << endl << cropPs << "," << endl;
	auto simPs = env.params.simulationParameters.to_json().dump();
	pout << "\"simulationParameters\":" << endl << simPs << "," << endl;
	auto envPs = env.params.userEnvironmentParameters.to_json().dump();
	pout << "\"userEnvironmentParameters\":" << endl << envPs << "," << endl;
	auto smPs = env.params.userSoilMoistureParameters.to_json().dump();
	pout << "\"userSoilMoistureParameters\":" << endl << smPs << "," << endl;
	auto soPs = env.params.userSoilOrganicParameters.to_json().dump();
	pout << "\"userSoilOrganicParameters\":" << endl << soPs << "," << endl;
	auto stempPs = env.params.userSoilTemperatureParameters.to_json().dump();
	pout << "\"userSoilTemperatureParameters\":" << endl << stempPs << "," << endl;
	auto stransPs = env.params.userSoilTransportParameters.to_json().dump();
	pout << "\"userSoilTransportParameters\":" << endl << stransPs << "," << endl;
	auto sitePs = env.params.siteParameters.to_json().dump();
	pout << "\"siteParameters\":" << endl << sitePs << "," << endl;
	pout << "\"crop-rotation\": {" << endl;
	bool isFirstItem = true;
	for(auto cm : env.cropRotation)
	{
		pout
			<< (isFirstItem ? "  " : ", ")
			<< "\"" << cm.crop()->id() << "\":" << cm.to_json().dump() << endl;
		isFirstItem = !isFirstItem && isFirstItem;
	}
	pout << "}," << endl;
	pout << "\"climate-data\": [" << endl;
	isFirstItem = true;
	for(size_t i = 0, nosp = env.da.noOfStepsPossible(); i < nosp; i++)
	{
		pout
			<< (isFirstItem ? "  " : ", ")
			<< "[\"" << env.da.dateForStep(i).toIsoDateString()
			<< "\", " << env.da.dataForTimestep(tavg, i)
			<< ", " << env.da.dataForTimestep(precip, i)
			<< ", " << env.da.dataForTimestep(globrad, i)
			<< "]" << endl;
		isFirstItem = !isFirstItem && isFirstItem;
	}
	pout << "]" << endl;
	pout << "}" << endl;
	pout.flush();
	pout.close();
}

Result Monica::runMonica(Env env)
{
	if(activateDebug)
	{
		writeDebugInputs(env, "inputs.json");
	}

	Result res;
	res.customId = env.customId;

	if(env.cropRotation.empty())
	{
		debug() << "Error: Crop rotation is empty!" << endl;
		return res;
	}

	debug() << "starting Monica" << endl;

	ostream& fout = *env.fout;
	ostream& gout = *env.gout;

	debug() << "-----" << endl;

	MonicaModel monica(env.params);

	if(env.params.writeOutputFiles())
	{
		// open rmout.dat
		//debug() << "Outputpath: " << (env.params.pathToOutputDir() + pathSeparator() + "rmout.csv") << endl;
		//fout.open(ensureDirExists(env.params.pathToOutputDir() + pathSeparator()) + "rmout.csv");
		//if(fout.fail())
		//{
		//	cerr << "Error while opening output file \"" << (env.params.pathToOutputDir() + pathSeparator() + "rmout.csv") << "\"" << endl;
		//	return res;
		//}

		// open smout.dat
		//gout.open(ensureDirExists(env.params.pathToOutputDir() + pathSeparator()) + "smout.csv");
		//if(gout.fail())
		//{
		//	cerr << "Error while opening output file \"" << (env.params.pathToOutputDir() + pathSeparator() + "smout.csv").c_str() << "\"" << endl;
		//	return res;
		//}

		// writes the header line to output files
		initializeFoutHeader(fout);
		initializeGoutHeader(gout);

		dumpMonicaParametersIntoFile(env.params.pathToOutputDir(), env.params);
	}

	debug() << "currentDate" << endl;
	Date currentDate = env.da.startDate();
	size_t nods = env.da.noOfStepsPossible();
	debug() << "nods: " << nods << endl;

	size_t currentMonth = currentDate.month();
	unsigned int dim = 0; //day in current month

	double avg10corg = 0, avg30corg = 0, watercontent = 0,
		groundwater = 0, nLeaching = 0, yearly_groundwater = 0,
		yearly_nleaching = 0, monthSurfaceRunoff = 0.0;
	double monthPrecip = 0.0;
	double monthETa = 0.0;

	//iterator through the production processes
	vector<CultivationMethod>::const_iterator cmci = env.cropRotation.begin();
	//direct handle to current process
	CultivationMethod currentCM = *cmci;
	//are the dates in the production process relative dates
	//or are they absolute as produced by the hermes inputs
	bool useRelativeDates = currentCM.startDate().isRelativeDate();
	//the next application date, either a relative or an absolute date
	//to get the correct applications out of the production processes
	Date nextCMApplicationDate = currentCM.startDate();
	//a definitely absolute next application date to keep track where
	//we are in the list of climate data
	Date nextAbsoluteCMApplicationDate = useRelativeDates
		? nextCMApplicationDate.toAbsoluteDate(currentDate.year())// + 1) // + 1 probably due to use in DSS and have one year to init monica 
		: nextCMApplicationDate;
	debug() << "next app-date: " << nextCMApplicationDate.toString()
		<< " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;

	//if for some reason there are no applications (no nothing) in the
	//production process: quit
	if(!nextAbsoluteCMApplicationDate.isValid())
	{
		debug() << "start of production-process: " << currentCM.toString()
			<< " is not valid" << endl;
		return res;
	}

	//beware: !!!! if there are absolute days used, then there is basically
	//no rotation if the last crop in the crop rotation has changed
	//the loop starts anew but the first crops date has already passed
	//so the crop won't be seeded again or any work applied
	//thus for absolute dates the crop rotation has to be as long as there
	//are climate data !!!!!

	for(unsigned int d = 0; d < nods; ++d, ++currentDate, ++dim)
	{
		debug() << "currentDate: " << currentDate.toString() << endl;
		monica.resetDailyCounter();

		//    if (currentDate.year() == 2012) {
		//        cout << "Reaching problem year :-)" << endl;
		//    }
				// test if monica's crop has been dying in previous step
				// if yes, it will be incorporated into soil
		if(monica.cropGrowth() && monica.cropGrowth()->isDying())
			monica.incorporateCurrentCrop();

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
		if(monica.cropGrowth() && currentCM.crop()->useAutomaticHarvestTrigger())
		{
			// Test if crop should be harvested at maturity
			if(currentCM.crop()->getAutomaticHarvestParams().getHarvestTime() == AutomaticHarvestParameters::maturity)
			{

				if(monica.cropGrowth()->maturityReached()
					 || currentCM.crop()->getAutomaticHarvestParams().getLatestHarvestDOY() == currentDate.julianDay())
				{
					debug() << "####################################################" << endl;
					debug() << "AUTOMATIC HARVEST TRIGGER EVENT" << endl;
					debug() << "####################################################" << endl;

					//auto harvestApplication = make_unique<Harvest>(currentDate, currentPP.crop(), currentPP.cropResultPtr());
					auto harvestApplication =
						unique_ptr<Harvest>(new Harvest(currentDate,
																						currentCM.crop(),
																						currentCM.cropResultPtr()));
					harvestApplication->apply(&monica);
				}
			}
		}

		//there's something to at this day
		if(nextAbsoluteCMApplicationDate == currentDate)
		{
			debug() << "applying at: " << nextCMApplicationDate.toString()
				<< " absolute-at: " << nextAbsoluteCMApplicationDate.toString() << endl;
			//apply everything to do at current day
			//cout << currentPP.toString().c_str() << endl;
			currentCM.apply(nextCMApplicationDate, &monica);

			//get the next application date to wait for (either absolute or relative)
			Date prevPPApplicationDate = nextCMApplicationDate;

			nextCMApplicationDate = currentCM.nextDate(nextCMApplicationDate);

			nextAbsoluteCMApplicationDate = useRelativeDates
				? nextCMApplicationDate.toAbsoluteDate
				(currentDate.year()
				 + (nextCMApplicationDate.dayOfYear() > prevPPApplicationDate.dayOfYear()
						? 0
						: 1),
				 true)
				: nextCMApplicationDate;

			debug() << "next app-date: " << nextCMApplicationDate.toString()
				<< " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
			//if application date was not valid, we're (probably) at the end
			//of the application list of this production process
			//-> go to the next one in the crop rotation


			if(!nextAbsoluteCMApplicationDate.isValid())
			{
				//get yieldresults for crop
				CMResult r = currentCM.cropResult();
				r.customId = currentCM.customId();
				r.date = currentDate;

				if(!env.params.simulationParameters.p_UseSecondaryYields)
					r.results[secondaryYield] = 0;
				r.results[sumFertiliser] = monica.sumFertiliser();
				r.results[daysWithCrop] = monica.daysWithCrop();
				r.results[NStress] = monica.getAccumulatedNStress();
				r.results[WaterStress] = monica.getAccumulatedWaterStress();
				r.results[HeatStress] = monica.getAccumulatedHeatStress();
				r.results[OxygenStress] = monica.getAccumulatedOxygenStress();

				res.pvrs.push_back(r);
				//        debug() << "py: " << r.pvResults[primaryYield] << endl;
				//            << " sy: " << r.pvResults[secondaryYield]
				//            << " iw: " << r.pvResults[sumIrrigation]
				//            << " sf: " << monica.sumFertiliser()
				//            << endl;

				//to count the applied fertiliser for the next production process
				monica.resetFertiliserCounter();

				//resets crop values for use in next year
				currentCM.crop()->reset();

				cmci++;
				//start anew if we reached the end of the crop rotation
				if(cmci == env.cropRotation.end())
					cmci = env.cropRotation.begin();

				currentCM = *cmci;
				nextCMApplicationDate = currentCM.startDate();
				nextAbsoluteCMApplicationDate =
					useRelativeDates ? nextCMApplicationDate.toAbsoluteDate
					(currentDate.year() + (nextCMApplicationDate.dayOfYear() > prevPPApplicationDate.dayOfYear() ? 0 : 1),
					 true) : nextCMApplicationDate;
				debug() << "new valid next app-date: " << nextCMApplicationDate.toString()
					<< " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
			}
			//if we got our next date relative it might be possible that
			//the actual relative date belongs into the next year
			//this is the case if we're already (dayOfYear) past the next dayOfYear
			if(useRelativeDates && currentDate > nextAbsoluteCMApplicationDate)
				nextAbsoluteCMApplicationDate.addYears(1);
		}
		// write simulation date to file
		if(env.params.writeOutputFiles())
		{
			fout << currentDate.toString("/");
			gout << currentDate.toString("/");
		}


		const auto& dateAndClimateDataP = climateDataForStep(env.da, d);

		// run crop step
		if(monica.isCropPlanted())
			monica.cropStep(currentDate, dateAndClimateDataP.second);

		// writes crop results to output file
		if(env.params.writeOutputFiles())
			writeCropResults(monica.cropGrowth(), fout, gout, monica.isCropPlanted());

		monica.generalStep(currentDate, dateAndClimateDataP.second);

		// write special outputs at 31.03.
		if(currentDate.day() == 31 && currentDate.month() == 3)
		{
			res.generalResults[sum90cmYearlyNatDay].push_back(monica.sumNmin(0.9));
			//      debug << "N at: " << monica.sumNmin(0.9) << endl;
			res.generalResults[sum30cmSoilTemperature].push_back(monica.sumSoilTemperature(3));
			res.generalResults[sum90cmYearlyNO3AtDay].push_back(monica.sumNO3AtDay(0.9));
			res.generalResults[avg30cmSoilTemperature].push_back(monica.avg30cmSoilTemperature());
			//cout << "MONICA_TEMP:\t" << monica.avg30cmSoilTemperature() << endl;
			res.generalResults[avg0_30cmSoilMoisture].push_back(monica.avgSoilMoisture(0, 3));
			res.generalResults[avg30_60cmSoilMoisture].push_back(monica.avgSoilMoisture(3, 6));
			res.generalResults[avg60_90cmSoilMoisture].push_back(monica.avgSoilMoisture(6, 9));
			res.generalResults[avg0_90cmSoilMoisture].push_back(monica.avgSoilMoisture(0, 9));
			res.generalResults[waterFluxAtLowerBoundary].push_back(monica.groundWaterRecharge());
			res.generalResults[avg0_30cmCapillaryRise].push_back(monica.avgCapillaryRise(0, 3));
			res.generalResults[avg30_60cmCapillaryRise].push_back(monica.avgCapillaryRise(3, 6));
			res.generalResults[avg60_90cmCapillaryRise].push_back(monica.avgCapillaryRise(6, 9));
			res.generalResults[avg0_30cmPercolationRate].push_back(monica.avgPercolationRate(0, 3));
			res.generalResults[avg30_60cmPercolationRate].push_back(monica.avgPercolationRate(3, 6));
			res.generalResults[avg60_90cmPercolationRate].push_back(monica.avgPercolationRate(6, 9));
			res.generalResults[evapotranspiration].push_back(monica.getEvapotranspiration());
			res.generalResults[transpiration].push_back(monica.getTranspiration());
			res.generalResults[evaporation].push_back(monica.getEvaporation());
			res.generalResults[sum30cmSMB_CO2EvolutionRate].push_back(monica.get_sum30cmSMB_CO2EvolutionRate());
			res.generalResults[NH3Volatilised].push_back(monica.getNH3Volatilised());
			res.generalResults[sum30cmActDenitrificationRate].push_back(monica.getsum30cmActDenitrificationRate());
			res.generalResults[leachingNAtBoundary].push_back(monica.nLeaching());
		}

		if((currentDate.month() != currentMonth) || d == nods - 1)
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
			res.generalResults[monthlySoilMoistureL0].push_back(monica.avgSoilMoisture(0, 1) * 100.0);
			res.generalResults[monthlySoilMoistureL1].push_back(monica.avgSoilMoisture(1, 2) * 100.0);
			res.generalResults[monthlySoilMoistureL2].push_back(monica.avgSoilMoisture(2, 3) * 100.0);
			res.generalResults[monthlySoilMoistureL3].push_back(monica.avgSoilMoisture(3, 4) * 100.0);
			res.generalResults[monthlySoilMoistureL4].push_back(monica.avgSoilMoisture(4, 5) * 100.0);
			res.generalResults[monthlySoilMoistureL5].push_back(monica.avgSoilMoisture(5, 6) * 100.0);
			res.generalResults[monthlySoilMoistureL6].push_back(monica.avgSoilMoisture(6, 7) * 100.0);
			res.generalResults[monthlySoilMoistureL7].push_back(monica.avgSoilMoisture(7, 8) * 100.0);
			res.generalResults[monthlySoilMoistureL8].push_back(monica.avgSoilMoisture(8, 9) * 100.0);
			res.generalResults[monthlySoilMoistureL9].push_back(monica.avgSoilMoisture(9, 10) * 100.0);
			res.generalResults[monthlySoilMoistureL10].push_back(monica.avgSoilMoisture(10, 11) * 100.0);
			res.generalResults[monthlySoilMoistureL11].push_back(monica.avgSoilMoisture(11, 12) * 100.0);
			res.generalResults[monthlySoilMoistureL12].push_back(monica.avgSoilMoisture(12, 13) * 100.0);
			res.generalResults[monthlySoilMoistureL13].push_back(monica.avgSoilMoisture(13, 14) * 100.0);
			res.generalResults[monthlySoilMoistureL14].push_back(monica.avgSoilMoisture(14, 15) * 100.0);
			res.generalResults[monthlySoilMoistureL15].push_back(monica.avgSoilMoisture(15, 16) * 100.0);
			res.generalResults[monthlySoilMoistureL16].push_back(monica.avgSoilMoisture(16, 17) * 100.0);
			res.generalResults[monthlySoilMoistureL17].push_back(monica.avgSoilMoisture(17, 18) * 100.0);
			res.generalResults[monthlySoilMoistureL18].push_back(monica.avgSoilMoisture(18, 19) * 100.0);


			//      cout << "c10: " << (avg10corg / double(dim))
			//          << " c30: " << (avg30corg / double(dim))
			//          << " wc: " << (watercontent / double(dim))
			//          << " gwrc: " << groundwater
			//          << " nl: " << nLeaching
			//          << endl;

			avg10corg = avg30corg = watercontent = groundwater = nLeaching = monthSurfaceRunoff = 0.0;
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
		if((currentDate.year() != (currentDate - 1).year()) && (currentDate.year() != env.da.startDate().year()))
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

		if(monica.isCropPlanted())
		{
			//cout << "monica.cropGrowth()->get_GrossPrimaryProduction()\t" << monica.cropGrowth()->get_GrossPrimaryProduction() << endl;

			res.generalResults[dev_stage].push_back(monica.cropGrowth()->get_DevelopmentalStage() + 1);


		}
		else
		{
			res.generalResults[dev_stage].push_back(0.0);
		}

		res.dates.push_back(currentDate.toMysqlString());

		if(env.params.writeOutputFiles())
		{
			writeGeneralResults(fout, gout, env, monica, d);
		}
	}
	if(env.params.writeOutputFiles())
	{
		//fout.close();
		//gout.close();
	}

	//cout << res.dates.size() << endl;
//  cout << res.toString().c_str();
	debug() << "returning from runMonica" << endl;

	return res;
}

/**
* Write header line to fout Output file
* @param fout File pointer to rmout.dat
*/
void Monica::initializeFoutHeader(ostream &fout)
{
	int outLayers = 20;
	int numberOfOrgans = 5;
	fout << "Datum     ";
	fout << "\tCrop";
	fout << "\tTraDef";
	fout << "\tTra";
	fout << "\tNDef";
	fout << "\tHeatRed";
	fout << "\tFrostRed";
	fout << "\tOxRed";

	fout << "\tStage";
	fout << "\tTempSum";
	fout << "\tVernF";
	fout << "\tDaylF";
	fout << "\tIncRoot";
	fout << "\tIncLeaf";
	fout << "\tIncShoot";
	fout << "\tIncFruit";

	fout << "\tRelDev";
	fout << "\tLT50";
	fout << "\tAbBiom";

	fout << "\tRoot";
	fout << "\tLeaf";
	fout << "\tShoot";
	fout << "\tFruit";
	fout << "\tStruct";
	fout << "\tSugar";

	fout << "\tYield";
	fout << "\tSumYield";

	fout << "\tGroPhot";
	fout << "\tNetPhot";
	fout << "\tMaintR";
	fout << "\tGrowthR";
	fout << "\tStomRes";
	fout << "\tHeight";
	fout << "\tLAI";
	fout << "\tRootDep";
	fout << "\tEffRootDep";

	fout << "\tTotBiomN";
	fout << "\tAbBiomN";
	fout << "\tSumNUp";
	fout << "\tActNup";
	fout << "\tPotNup";
	fout << "\tNFixed";
	fout << "\tTarget";

	fout << "\tCritN";
	fout << "\tAbBiomNc";
	fout << "\tYieldNc";
	fout << "\tProtein";

	fout << "\tNPP";
	fout << "\tNPPRoot";
	fout << "\tNPPLeaf";
	fout << "\tNPPShoot";
	fout << "\tNPPFruit";
	fout << "\tNPPStruct";
	fout << "\tNPPSugar";

	fout << "\tGPP";
	fout << "\tRa";
	fout << "\tRaRoot";
	fout << "\tRaLeaf";
	fout << "\tRaShoot";
	fout << "\tRaFruit";
	fout << "\tRaStruct";
	fout << "\tRaSugar";

	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		fout << "\tMois" << i_Layer;

	fout << "\tPrecip";
	fout << "\tIrrig";
	fout << "\tInfilt";
	fout << "\tSurface";
	fout << "\tRunOff";
	fout << "\tSnowD";
	fout << "\tFrostD";
	fout << "\tThawD";

	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		fout << "\tPASW-" << i_Layer;

	fout << "\tSurfTemp";
	fout << "\tSTemp0";
	fout << "\tSTemp1";
	fout << "\tSTemp2";
	fout << "\tSTemp3";
	fout << "\tSTemp4";
	fout << "\tact_Ev";
	fout << "\tact_ET";
	fout << "\tET0";
	fout << "\tKc";
	fout << "\tatmCO2";
	fout << "\tGroundw";
	fout << "\tRecharge";
	fout << "\tNLeach";

	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		fout << "\tNO3-" << i_Layer;

	fout << "\tCarb";
	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		fout << "\tNH4-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 4; i_Layer++)
		fout << "\tNO2-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 6; i_Layer++)
		fout << "\tSOC-" << i_Layer;

	fout << "\tSOC-0-30";
	fout << "\tSOC-0-200";

	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\tAOMf-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\tAOMs-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\tSMBf-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\tSMBs-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\tSOMf-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\tSOMs-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\tCBal-" << i_Layer;

	for(int i_Layer = 0; i_Layer < 3; i_Layer++)
		fout << "\tNmin-" << i_Layer;

	fout << "\tNetNmin";
	fout << "\tDenit";
	fout << "\tN2O";
	fout << "\tSoilpH";
	fout << "\tNEP";
	fout << "\tNEE";
	fout << "\tRh";

	fout << "\ttmin";
	fout << "\ttavg";
	fout << "\ttmax";
	fout << "\twind";
	fout << "\tglobrad";
	fout << "\trelhumid";
	fout << "\tsunhours";
	fout << endl;

	//**** Second header line ***
	fout << "TTMMYYY";	// Date
	fout << "\t[ ]";		// Crop name
	fout << "\t[0;1]";    // TranspirationDeficit
	fout << "\t[mm]";     // ActualTranspiration
	fout << "\t[0;1]";    // CropNRedux
	fout << "\t[0;1]";    // HeatStressRedux
	fout << "\t[0;1]";    // FrostStressRedux
	fout << "\t[0;1]";    // OxygenDeficit

	fout << "\t[ ]";      // DevelopmentalStage
	fout << "\t[°Cd]";    // CurrentTemperatureSum
	fout << "\t[0;1]";    // VernalisationFactor
	fout << "\t[0;1]";    // DaylengthFactor
	fout << "\t[kg/ha]";  // OrganGrowthIncrement root
	fout << "\t[kg/ha]";  // OrganGrowthIncrement leaf
	fout << "\t[kg/ha]";  // OrganGrowthIncrement shoot
	fout << "\t[kg/ha]";  // OrganGrowthIncrement fruit

	fout << "\t[0;1]";    // RelativeTotalDevelopment
	fout << "\t[°C]";     // LT50
	fout << "\t[kg/ha]";  // AbovegroundBiomass

	for(int i = 0; i < 6; i++)
		fout << "\t[kgDM/ha]"; // get_OrganBiomass(i)

	fout << "\t[kgDM/ha]";    // get_PrimaryCropYield(3)
	fout << "\t[kgDM/ha]";    // get_AccumulatedPrimaryCropYield(3)

	fout << "\t[kgCH2O/ha]";  // GrossPhotosynthesisHaRate
	fout << "\t[kgCH2O/ha]";  // NetPhotosynthesis
	fout << "\t[kgCH2O/ha]";  // MaintenanceRespirationAS
	fout << "\t[kgCH2O/ha]";  // GrowthRespirationAS
	fout << "\t[s/m]";        // StomataResistance
	fout << "\t[m]";          // CropHeight
	fout << "\t[m2/m2]";      // LeafAreaIndex
	fout << "\t[layer]";      // RootingDepth
	fout << "\t[m]";          // Effective RootingDepth

	fout << "\t[kgN/ha]";     // TotalBiomassNContent
	fout << "\t[kgN/ha]";     // AbovegroundBiomassNContent
	fout << "\t[kgN/ha]";     // SumTotalNUptake
	fout << "\t[kgN/ha]";     // ActNUptake
	fout << "\t[kgN/ha]";     // PotNUptake
	fout << "\t[kgN/ha]";     // NFixed
	fout << "\t[kgN/kg]";     // TargetNConcentration
	fout << "\t[kgN/kg]";     // CriticalNConcentration
	fout << "\t[kgN/kg]";     // AbovegroundBiomassNConcentration
	fout << "\t[kgN/kg]";     // PrimaryYieldNConcentration
	fout << "\t[kg/kg]";      // RawProteinConcentration

	fout << "\t[kg C ha-1]";   // NPP
	fout << "\t[kg C ha-1]";   // NPP root
	fout << "\t[kg C ha-1]";   // NPP leaf
	fout << "\t[kg C ha-1]";   // NPP shoot
	fout << "\t[kg C ha-1]";   // NPP fruit
	fout << "\t[kg C ha-1]";   // NPP struct
	fout << "\t[kg C ha-1]";   // NPP sugar

	fout << "\t[kg C ha-1]";   // GPP
	fout << "\t[kg C ha-1]";   // Ra
	fout << "\t[kg C ha-1]";   // Ra root
	fout << "\t[kg C ha-1]";   // Ra leaf
	fout << "\t[kg C ha-1]";   // Ra shoot
	fout << "\t[kg C ha-1]";   // Ra fruit
	fout << "\t[kg C ha-1]";   // Ra struct
	fout << "\t[kg C ha-1]";   // Ra sugar

	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		fout << "\t[m3/m3]"; // Soil moisture content

	fout << "\t[mm]"; // Precipitation
	fout << "\t[mm]"; // Irrigation
	fout << "\t[mm]"; // Infiltration
	fout << "\t[mm]"; // Surface water storage
	fout << "\t[mm]"; // Surface water runoff
	fout << "\t[mm]"; // Snow depth
	fout << "\t[m]"; // Frost front depth in soil
	fout << "\t[m]"; // Thaw front depth in soil

	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		fout << "\t[m3/m3]"; //PASW

	fout << "\t[°C]"; //
	fout << "\t[°C]";
	fout << "\t[°C]";
	fout << "\t[°C]";
	fout << "\t[°C]";
	fout << "\t[°C]";
	fout << "\t[mm]";
	fout << "\t[mm]";
	fout << "\t[mm]";
	fout << "\t[ ]";
	fout << "\t[ppm]";
	fout << "\t[m]";
	fout << "\t[mm]";
	fout << "\t[kgN/ha]";

	// NO3
	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		fout << "\t[kgN/m3]";

	fout << "\t[kgN/m3]";  // Soil Carbamid
												 // NH4
	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		fout << "\t[kgN/m3]";

	// NO2
	for(int i_Layer = 0; i_Layer < 4; i_Layer++)
		fout << "\t[kgN/m3]";

	// get_SoilOrganicC
	for(int i_Layer = 0; i_Layer < 6; i_Layer++)
		fout << "\t[kgC/kg]";

	fout << "\t[gC m-2]";   // SOC-0-30
	fout << "\t[gC m-2]";   // SOC-0-200
													// get_AOM_FastSum
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\t[kgC/m3]";

	// get_AOM_SlowSum
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\t[kgC/m3]";

	// get_SMB_Fast
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\t[kgC/m3]";

	// get_SMB_Slow
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\t[kgC/m3]";

	// get_SOM_Fast
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\t[kgC/m3]";

	// get_SOM_Slow
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\t[kgC/m3]";

	// get_CBalance
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
		fout << "\t[kgC/m3]";

	// NetNMineralisationRate
	for(int i_Layer = 0; i_Layer < 3; i_Layer++)
		fout << "\t[kgN/ha]";

	fout << "\t[kgN/ha]";  // NetNmin
	fout << "\t[kgN/ha]";  // Denit
	fout << "\t[kgN/ha]";  // N2O
	fout << "\t[ ]";       // SoilpH
	fout << "\t[kgC/ha]";  // NEP
	fout << "\t[kgC/ha]";  // NEE
	fout << "\t[kgC/ha]";  // Rh

	fout << "\t[°C]";     // tmin
	fout << "\t[°C]";     // tavg
	fout << "\t[°C]";     // tmax
	fout << "\t[m/s]";    // wind
	fout << "\tglobrad";  // globrad
	fout << "\t[m3/m3]";  // relhumid
	fout << "\t[h]";      // sunhours
	fout << endl;
}

//------------------------------------------------------------------------------

/**
* Writes header line to gout-Outputfile
* @param gout File pointer to smout.dat
*/
void Monica::initializeGoutHeader(ostream &gout)
{
	gout << "Datum     ";
	gout << "\tCrop";
	gout << "\tStage";
	gout << "\tHeight";
	gout << "\tRoot";
	gout << "\tRoot10";
	gout << "\tLeaf";
	gout << "\tShoot";
	gout << "\tFruit";
	gout << "\tAbBiom";
	gout << "\tAbGBiom";
	gout << "\tYield";
	gout << "\tEarNo";
	gout << "\tGrainNo";

	gout << "\tLAI";
	gout << "\tAbBiomNc";
	gout << "\tYieldNc";
	gout << "\tAbBiomN";
	gout << "\tYieldN";

	gout << "\tTotNup";
	gout << "\tNGrain";
	gout << "\tProtein";

	gout << "\tBedGrad";
	gout << "\tM0-10";
	gout << "\tM10-20";
	gout << "\tM20-30";
	gout << "\tM30-40";
	gout << "\tM40-50";
	gout << "\tM50-60";
	gout << "\tM60-70";
	gout << "\tM70-80";
	gout << "\tM80-90";
	gout << "\tM0-30";
	gout << "\tM30-60";
	gout << "\tM60-90";
	gout << "\tM0-60";
	gout << "\tM0-90";
	gout << "\tPAW0-200";
	gout << "\tPAW0-130";
	gout << "\tPAW0-150";
	gout << "\tN0-30";
	gout << "\tN30-60";
	gout << "\tN60-90";
	gout << "\tN90-120";
	gout << "\tN0-60";
	gout << "\tN0-90";
	gout << "\tN0-200";
	gout << "\tN0-130";
	gout << "\tN0-150";
	gout << "\tNH430";
	gout << "\tNH460";
	gout << "\tNH490";
	gout << "\tCo0-10";
	gout << "\tCo0-30";
	gout << "\tT0-10";
	gout << "\tT20-30";
	gout << "\tT50-60";
	gout << "\tCO2";
	gout << "\tNH3";
	gout << "\tN2O";
	gout << "\tN2";
	gout << "\tNgas";
	gout << "\tNFert";
	gout << "\tIrrig";
	gout << endl;

	// **** Second header line ****

	gout << "TTMMYYYY";
	gout << "\t[ ]";
	gout << "\t[ ]";
	gout << "\t[m]";
	gout << "\t[kgDM/ha]";
	gout << "\t[kgDM/ha]";
	gout << "\t[kgDM/ha]";
	gout << "\t[kgDM/ha]";
	gout << "\t[kgDM/ha]";
	gout << "\t[kgDM/ha]";
	gout << "\t[kgDM/ha]";
	gout << "\t[kgDM/ha]";
	gout << "\t[ ]";
	gout << "\t[ ]";
	gout << "\t[m2/m2]";
	gout << "\t[kgN/kgDM";
	gout << "\t[kgN/kgDM]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[-]";
	gout << "\t[kg/kgDM]";

	gout << "\t[0;1]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[m3/m3]";
	gout << "\t[mm]";
	gout << "\t[mm]";
	gout << "\t[mm]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[kgC/ha]";
	gout << "\t[kgC/ha]";
	gout << "\t[°C]";
	gout << "\t[°C]";
	gout << "\t[°C]";
	gout << "\t[kgC/ha]";
	gout << "\t[kgN/ha]";
	gout << "\t[-]";
	gout << "\t[-]";
	gout << "\t[-]";
	gout << "\t[kgN/ha]";
	gout << "\t[mm]";
	gout << endl;
}

//------------------------------------------------------------------------------

/**
* Write crop results to file; if no crop is planted, fields are filled out with zeros;
* @param mcg CropGrowth modul that contains information about crop
* @param fout File pointer to rmout.dat
* @param gout File pointer to smout.dat
*/
void Monica::writeCropResults(const CropGrowth* mcg,
															ostream& fout,
															ostream& gout,
															bool crop_is_planted)
{
	if(crop_is_planted)
	{
		fout << "\t" << mcg->get_CropName();
		fout << fixed << setprecision(2) << "\t" << mcg->get_TranspirationDeficit();// [0;1]
		fout << fixed << setprecision(2) << "\t" << mcg->get_ActualTranspiration();
		fout << fixed << setprecision(2) << "\t" << mcg->get_CropNRedux();// [0;1]
		fout << fixed << setprecision(2) << "\t" << mcg->get_HeatStressRedux();// [0;1]
		fout << fixed << setprecision(2) << "\t" << mcg->get_FrostStressRedux();// [0;1]
		fout << fixed << setprecision(2) << "\t" << mcg->get_OxygenDeficit();// [0;1]

		fout << fixed << setprecision(0) << "\t" << mcg->get_DevelopmentalStage() + 1;
		fout << fixed << setprecision(1) << "\t" << mcg->get_CurrentTemperatureSum();
		fout << fixed << setprecision(2) << "\t" << mcg->get_VernalisationFactor();
		fout << fixed << setprecision(2) << "\t" << mcg->get_DaylengthFactor();
		fout << fixed << setprecision(2) << "\t" << mcg->get_OrganGrowthIncrement(0);
		fout << fixed << setprecision(2) << "\t" << mcg->get_OrganGrowthIncrement(1);
		fout << fixed << setprecision(2) << "\t" << mcg->get_OrganGrowthIncrement(2);
		fout << fixed << setprecision(2) << "\t" << mcg->get_OrganGrowthIncrement(3);

		fout << fixed << setprecision(2) << "\t" << mcg->get_RelativeTotalDevelopment();
		fout << fixed << setprecision(1) << "\t" << mcg->get_LT50(); //  [°C] 
		fout << fixed << setprecision(1) << "\t" << mcg->get_AbovegroundBiomass(); //[kg ha-1]
		for(int i = 0; i < mcg->get_NumberOfOrgans(); i++)
		{
			fout << fixed << setprecision(1) << "\t" << mcg->get_OrganBiomass(i); // biomass organs, [kg C ha-1]
		}

		for(int i = 0; i < (6 - mcg->get_NumberOfOrgans()); i++)
		{
			fout << fixed << setprecision(1) << "\t" << 0.0; // adding zero fill if biomass organs < 6,
		}

		fout << fixed << setprecision(1) << "\t" << mcg->get_PrimaryCropYield();
		fout << fixed << setprecision(1) << "\t" << mcg->get_AccumulatedPrimaryCropYield();

		fout << fixed << setprecision(4) << "\t" << mcg->get_GrossPhotosynthesisHaRate(); // [kg CH2O ha-1 d-1]
		fout << fixed << setprecision(2) << "\t" << mcg->get_NetPhotosynthesis();  // [kg CH2O ha-1 d-1]
		fout << fixed << setprecision(4) << "\t" << mcg->get_MaintenanceRespirationAS();// [kg CH2O ha-1]
		fout << fixed << setprecision(4) << "\t" << mcg->get_GrowthRespirationAS();// [kg CH2O ha-1]

		fout << fixed << setprecision(2) << "\t" << mcg->get_StomataResistance();// [s m-1]

		fout << fixed << setprecision(2) << "\t" << mcg->get_CropHeight();// [m]
		fout << fixed << setprecision(2) << "\t" << mcg->get_LeafAreaIndex(); //[m2 m-2]
		fout << fixed << setprecision(0) << "\t" << mcg->get_RootingDepth(); //[layer]
		fout << fixed << setprecision(2) << "\t" << mcg->getEffectiveRootingDepth(); //[m]

		fout << fixed << setprecision(1) << "\t" << mcg->get_TotalBiomassNContent();
		fout << fixed << setprecision(1) << "\t" << mcg->get_AbovegroundBiomassNContent();
		fout << fixed << setprecision(2) << "\t" << mcg->get_SumTotalNUptake();
		fout << fixed << setprecision(2) << "\t" << mcg->get_ActNUptake(); // [kg N ha-1]
		fout << fixed << setprecision(2) << "\t" << mcg->get_PotNUptake(); // [kg N ha-1]
		fout << fixed << setprecision(2) << "\t" << mcg->get_BiologicalNFixation(); // [kg N ha-1]
		fout << fixed << setprecision(3) << "\t" << mcg->get_TargetNConcentration();//[kg N kg-1]

		fout << fixed << setprecision(3) << "\t" << mcg->get_CriticalNConcentration();//[kg N kg-1]
		fout << fixed << setprecision(3) << "\t" << mcg->get_AbovegroundBiomassNConcentration();//[kg N kg-1]
		fout << fixed << setprecision(3) << "\t" << mcg->get_PrimaryYieldNConcentration();//[kg N kg-1]
		fout << fixed << setprecision(3) << "\t" << mcg->get_RawProteinConcentration();//[kg kg-1]
		fout << fixed << setprecision(5) << "\t" << mcg->get_NetPrimaryProduction(); // NPP, [kg C ha-1]
		for(int i = 0; i < mcg->get_NumberOfOrgans(); i++)
		{
			fout << fixed << setprecision(4) << "\t" << mcg->get_OrganSpecificNPP(i); // NPP organs, [kg C ha-1]
		}
		// if there less than 6 organs we have to fill the column that
		// was added in the output header of rmout; in this header there
		// are statically 6 columns initialised for the organ NPP
		for(int i = mcg->get_NumberOfOrgans(); i < 6; i++)
		{
			fout << fixed << setprecision(2) << "\t0.0"; // NPP organs, [kg C ha-1]
		}

		fout << fixed << setprecision(5) << "\t" << mcg->get_GrossPrimaryProduction(); // GPP, [kg C ha-1]

		fout << fixed << setprecision(5) << "\t" << mcg->get_AutotrophicRespiration(); // Ra, [kg C ha-1]
		for(int i = 0; i < mcg->get_NumberOfOrgans(); i++)
		{
			fout << fixed << setprecision(4) << "\t" << mcg->get_OrganSpecificTotalRespired(i); // Ra organs, [kg C ha-1]
		}
		// if there less than 6 organs we have to fill the column that
		// was added in the output header of rmout; in this header there
		// are statically 6 columns initialised for the organ RA
		for(int i = mcg->get_NumberOfOrgans(); i < 6; i++)
		{
			fout << fixed << setprecision(2) << "\t0.0";
		}

		gout << "\t" << mcg->get_CropName();
		gout << fixed << setprecision(0) << "\t" << mcg->get_DevelopmentalStage() + 1;
		gout << fixed << setprecision(2) << "\t" << mcg->get_CropHeight();
		gout << fixed << setprecision(1) << "\t" << mcg->get_OrganBiomass(0);
		gout << fixed << setprecision(1) << "\t" << mcg->get_OrganBiomass(0); //! @todo
		gout << fixed << setprecision(1) << "\t" << mcg->get_OrganBiomass(1);
		gout << fixed << setprecision(1) << "\t" << mcg->get_OrganBiomass(2);
		gout << fixed << setprecision(1) << "\t" << mcg->get_OrganBiomass(3);
		gout << fixed << setprecision(1) << "\t" << mcg->get_AbovegroundBiomass();
		gout << fixed << setprecision(1) << "\t" << mcg->get_AbovegroundBiomass(); //! @todo
		gout << fixed << setprecision(1) << "\t" << mcg->get_PrimaryCropYield();
		gout << "\t0"; //! @todo
		gout << "\t0"; //! @todo
		gout << fixed << setprecision(2) << "\t" << mcg->get_LeafAreaIndex();
		gout << fixed << setprecision(4) << "\t" << mcg->get_AbovegroundBiomassNConcentration();
		gout << fixed << setprecision(3) << "\t" << mcg->get_PrimaryYieldNConcentration();
		gout << fixed << setprecision(1) << "\t" << mcg->get_AbovegroundBiomassNContent();
		gout << fixed << setprecision(1) << "\t" << mcg->get_PrimaryYieldNContent();
		gout << fixed << setprecision(1) << "\t" << mcg->get_TotalBiomassNContent();
		gout << fixed << setprecision(3) << "\t" << mcg->get_PrimaryYieldNConcentration();
		gout << fixed << setprecision(3) << "\t" << mcg->get_RawProteinConcentration();

	}
	else
	{ // crop is not planted

		fout << "\t"; // Crop Name
		fout << "\t1.00"; // TranspirationDeficit
		fout << "\t0.00"; // ActualTranspiration
		fout << "\t1.00"; // CropNRedux
		fout << "\t1.00"; // HeatStressRedux
		fout << "\t1.00"; // FrostStressRedux
		fout << "\t1.00"; // OxygenDeficit

		fout << "\t0";      // DevelopmentalStage
		fout << "\t0.0";    // CurrentTemperatureSum
		fout << "\t0.00";   // VernalisationFactor
		fout << "\t0.00";   // DaylengthFactor

		fout << "\t0.00";   // OrganGrowthIncrement root
		fout << "\t0.00";   // OrganGrowthIncrement leaf
		fout << "\t0.00";   // OrganGrowthIncrement shoot
		fout << "\t0.00";   // OrganGrowthIncrement fruit
		fout << "\t0.00";   // RelativeTotalDevelopment
		fout << "\t0.0";   // LT50

		fout << "\t0.0";    // AbovegroundBiomass
		fout << "\t0.0";    // get_OrganBiomass(0)
		fout << "\t0.0";    // get_OrganBiomass(1)
		fout << "\t0.0";    // get_OrganBiomass(2)
		fout << "\t0.0";    // get_OrganBiomass(3)
		fout << "\t0.0";    // get_OrganBiomass(4)
		fout << "\t0.0";    // get_OrganBiomass(5)
		fout << "\t0.0";    // get_PrimaryCropYield(3)
		fout << "\t0.0";    // get_AccumulatedPrimaryCropYield(3)

		fout << "\t0.000";  // GrossPhotosynthesisHaRate
		fout << "\t0.00";   // NetPhotosynthesis
		fout << "\t0.000";  // MaintenanceRespirationAS
		fout << "\t0.000";  // GrowthRespirationAS
		fout << "\t0.00";   // StomataResistance
		fout << "\t0.00";   // CropHeight
		fout << "\t0.00";   // LeafAreaIndex
		fout << "\t0";      // RootingDepth
		fout << "\t0.0";    // EffectiveRootingDepth

		fout << "\t0.0";    // TotalBiomassNContent
		fout << "\t0.0";    // AbovegroundBiomassNContent
		fout << "\t0.00";   // SumTotalNUptake
		fout << "\t0.00";   // ActNUptake
		fout << "\t0.00";   // PotNUptake
		fout << "\t0.00";   // NFixed
		fout << "\t0.000";  // TargetNConcentration
		fout << "\t0.000";  // CriticalNConcentration
		fout << "\t0.000";  // AbovegroundBiomassNConcentration
		fout << "\t0.000";  // PrimaryYieldNConcentration
		fout << "\t0.000";  // CrudeProteinConcentration

		fout << "\t0.0";    // NetPrimaryProduction
		fout << "\t0.0"; // NPP root
		fout << "\t0.0"; // NPP leaf
		fout << "\t0.0"; // NPP shoot
		fout << "\t0.0"; // NPP fruit
		fout << "\t0.0"; // NPP struct
		fout << "\t0.0"; // NPP sugar

		fout << "\t0.0"; // GrossPrimaryProduction
		fout << "\t0.0"; // Ra - VcRespiration
		fout << "\t0.0"; // Ra root - OrganSpecificTotalRespired
		fout << "\t0.0"; // Ra leaf - OrganSpecificTotalRespired
		fout << "\t0.0"; // Ra shoot - OrganSpecificTotalRespired
		fout << "\t0.0"; // Ra fruit - OrganSpecificTotalRespired
		fout << "\t0.0"; // Ra struct - OrganSpecificTotalRespired
		fout << "\t0.0"; // Ra sugar - OrganSpecificTotalRespired

		gout << "\t";       // Crop Name
		gout << "\t0";      // DevelopmentalStage
		gout << "\t0.00";   // CropHeight
		gout << "\t0.0";    // OrganBiomass(0)
		gout << "\t0.0";    // OrganBiomass(0)
		gout << "\t0.0";    // OrganBiomass(1)

		gout << "\t0.0";    // OrganBiomass(2)
		gout << "\t0.0";    // OrganBiomass(3)
		gout << "\t0.0";    // AbovegroundBiomass
		gout << "\t0.0";    // AbovegroundBiomass
		gout << "\t0.0";    // PrimaryCropYield

		gout << "\t0";
		gout << "\t0";

		gout << "\t0.00";   // LeafAreaIndex
		gout << "\t0.000";  // AbovegroundBiomassNConcentration
		gout << "\t0.0";    // PrimaryYieldNConcentration
		gout << "\t0.00";   // AbovegroundBiomassNContent
		gout << "\t0.0";    // PrimaryYieldNContent

		gout << "\t0.0";    // TotalBiomassNContent
		gout << "\t0";
		gout << "\t0.00";   // RawProteinConcentration
	}
}

//------------------------------------------------------------------------------

/**
* Writing general results from MONICA simulation to output files
* @param fout File pointer to rmout.dat
* @param gout File pointer to smout.dat
* @param env Environment object
* @param monica MONICA model that contains pointer to all submodels
* @param d Day of simulation
*/
void Monica::writeGeneralResults(ostream& fout,
																 ostream& gout,
																 Env& env,
																 MonicaModel& monica,
																 int d)
{
	const SoilTemperature& mst = monica.soilTemperature();
	const SoilMoisture& msm = monica.soilMoisture();
	const SoilOrganic& mso = monica.soilOrganic();
	const SoilColumn& msc = monica.soilColumn();

	//! TODO: schmutziger work-around. Hier muss was eleganteres hin!
	SoilColumn& msa = monica.soilColumnNC();
	const SoilTransport& msq = monica.soilTransport();

	int outLayers = 20;
	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
	{
		fout << fixed << setprecision(3) << "\t" << msm.get_SoilMoisture(i_Layer);
	}
	fout << fixed << setprecision(2) << "\t" << env.da.dataForTimestep(Climate::precip, d);
	fout << fixed << setprecision(1) << "\t" << monica.dailySumIrrigationWater();
	fout << fixed << setprecision(1) << "\t" << msm.get_Infiltration(); // {mm]
	fout << fixed << setprecision(1) << "\t" << msm.get_SurfaceWaterStorage();// {mm]
	fout << fixed << setprecision(1) << "\t" << msm.get_SurfaceRunOff();// {mm]
	fout << fixed << setprecision(1) << "\t" << msm.get_SnowDepth(); // [mm]
	fout << fixed << setprecision(1) << "\t" << msm.get_FrostDepth();
	fout << fixed << setprecision(1) << "\t" << msm.get_ThawDepth();
	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
	{
		fout << fixed << setprecision(3) << "\t"
			<< msm.get_SoilMoisture(i_Layer) - msa.at(i_Layer).vs_PermanentWiltingPoint();
	}
	fout << fixed << setprecision(1) << "\t" << mst.get_SoilSurfaceTemperature();


	for(int i_Layer = 0; i_Layer < 5; i_Layer++)
	{
		fout << fixed << setprecision(1) << "\t" << mst.get_SoilTemperature(i_Layer);// [°C]
	}
	//  for(int i_Layer = 0; i_Layer < 20; i_Layer++) {
	//    cout << mst.get_SoilTemperature(i_Layer) << "\t";
	//  }
	//  cout << endl;

	fout << "\t" << msm.get_ActualEvaporation();// [mm]
	fout << "\t" << msm.get_Evapotranspiration();// [mm]
	fout << "\t" << msm.get_ET0();// [mm]
	fout << "\t" << msm.get_KcFactor();
	fout << "\t" << monica.get_AtmosphericCO2Concentration();// [ppm]
	fout << fixed << setprecision(2) << "\t" << monica.get_GroundwaterDepth();// [m]
	fout << fixed << setprecision(3) << "\t" << msm.get_GroundwaterRecharge();// [mm]
	fout << fixed << setprecision(3) << "\t" << msq.get_NLeaching(); // [kg N ha-1]

	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
	{
		fout << fixed << setprecision(3) << "\t" << msc.at(i_Layer).get_SoilNO3();// [kg N m-3]
	 // cout << "msc.soilLayer(i_Layer).get_SoilNO3():\t" << msc.soilLayer(i_Layer).get_SoilNO3() << endl;
	}

	fout << fixed << setprecision(4) << "\t" << msc.at(0).get_SoilCarbamid();

	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << msc.at(i_Layer).get_SoilNH4();
	}
	for(int i_Layer = 0; i_Layer < 4; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << msc.at(i_Layer).get_SoilNO2();
	}
	for(int i_Layer = 0; i_Layer < 6; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << msc.at(i_Layer).vs_SoilOrganicCarbon(); // [kg C kg-1]
	}

	// SOC-0-30 [g C m-2]
	double soc_30_accumulator = 0.0;
	for(int i_Layer = 0; i_Layer < 3; i_Layer++)
	{
		// kg C / kg --> g C / m2
		soc_30_accumulator +=
			msc.at(i_Layer).vs_SoilOrganicCarbon()
			* msc.at(i_Layer).vs_SoilBulkDensity()
			* msc.at(i_Layer).vs_LayerThickness
			* 1000;
	}
	fout << fixed << setprecision(4) << "\t" << soc_30_accumulator;

	// SOC-0-200   [g C m-2]
	double soc_200_accumulator = 0.0;
	for(int i_Layer = 0; i_Layer < outLayers; i_Layer++)
	{
		// kg C / kg --> g C / m2
		soc_200_accumulator +=
			msc.at(i_Layer).vs_SoilOrganicCarbon()
			* msc.at(i_Layer).vs_SoilBulkDensity()
			* msc.at(i_Layer).vs_LayerThickness
			* 1000;
	}
	fout << fixed << setprecision(4) << "\t" << soc_200_accumulator;

	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << mso.get_AOM_FastSum(i_Layer);
	}
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << mso.get_AOM_SlowSum(i_Layer);
	}
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << mso.get_SMB_Fast(i_Layer);
	}
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << mso.get_SMB_Slow(i_Layer);
	}
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << mso.get_SOM_Fast(i_Layer);
	}
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << mso.get_SOM_Slow(i_Layer);
	}
	for(int i_Layer = 0; i_Layer < 1; i_Layer++)
	{
		fout << fixed << setprecision(4) << "\t" << mso.get_CBalance(i_Layer);
	}
	for(int i_Layer = 0; i_Layer < 3; i_Layer++)
	{
		fout << fixed << setprecision(6) << "\t" << mso.get_NetNMineralisationRate(i_Layer); // [kg N ha-1]
	}

	fout << fixed << setprecision(5) << "\t" << mso.get_NetNMineralisation(); // [kg N ha-1]
	fout << fixed << setprecision(5) << "\t" << mso.get_Denitrification(); // [kg N ha-1]
	fout << fixed << setprecision(5) << "\t" << mso.get_N2O_Produced(); // [kg N ha-1]
	fout << fixed << setprecision(1) << "\t" << msc.at(0).get_SoilpH(); // [ ]
	fout << fixed << setprecision(5) << "\t" << mso.get_NetEcosystemProduction(); // [kg C ha-1]
	fout << fixed << setprecision(5) << "\t" << mso.get_NetEcosystemExchange(); // [kg C ha-1]
	fout << fixed << setprecision(5) << "\t" << mso.get_DecomposerRespiration(); // Rh, [kg C ha-1 d-1]


	fout << fixed << setprecision(4) << "\t" << env.da.dataForTimestep(Climate::tmin, d);
	fout << fixed << setprecision(4) << "\t" << env.da.dataForTimestep(Climate::tavg, d);
	fout << fixed << setprecision(4) << "\t" << env.da.dataForTimestep(Climate::tmax, d);
	fout << fixed << setprecision(4) << "\t" << env.da.dataForTimestep(Climate::wind, d);
	fout << fixed << setprecision(4) << "\t" << env.da.dataForTimestep(Climate::globrad, d);
	fout << fixed << setprecision(4) << "\t" << env.da.dataForTimestep(Climate::relhumid, d);
	fout << fixed << setprecision(4) << "\t" << env.da.dataForTimestep(Climate::sunhours, d);
	fout << endl;

	// smout
	gout << fixed << setprecision(3) << "\t" << msm.get_PercentageSoilCoverage();

	for(int i_Layer = 0; i_Layer < 9; i_Layer++)
	{
		gout << fixed << setprecision(3) << "\t" << msm.get_SoilMoisture(i_Layer); // [m3 m-3]
	}

	gout << fixed << setprecision(2) << "\t" << (msm.get_SoilMoisture(0) + msm.get_SoilMoisture(1) + msm.get_SoilMoisture(2)) / 3.0; //[m3 m-3]
	gout << fixed << setprecision(2) << "\t" << (msm.get_SoilMoisture(3) + msm.get_SoilMoisture(4) + msm.get_SoilMoisture(5)) / 3.0; //[m3 m-3]
	gout << fixed << setprecision(3) << "\t" << (msm.get_SoilMoisture(6) + msm.get_SoilMoisture(7) + msm.get_SoilMoisture(8)) / 3.0; //[m3 m-3]

	double M0_60 = 0.0;
	for(int i_Layer = 0; i_Layer < 6; i_Layer++)
	{
		M0_60 += msm.get_SoilMoisture(i_Layer);
	}
	gout << fixed << setprecision(3) << "\t" << (M0_60 / 6.0); // [m3 m-3]

	double M0_90 = 0.0;
	for(int i_Layer = 0; i_Layer < 9; i_Layer++)
	{
		M0_90 += msm.get_SoilMoisture(i_Layer);
	}
	gout << fixed << setprecision(3) << "\t" << (M0_90 / 9.0); // [m3 m-3]

	double PAW0_200 = 0.0;
	for(int i_Layer = 0; i_Layer < 20; i_Layer++)
	{
		PAW0_200 += (msm.get_SoilMoisture(i_Layer) - msa.at(i_Layer).vs_PermanentWiltingPoint());
	}
	gout << fixed << setprecision(1) << "\t" << (PAW0_200 * 0.1 * 1000.0); // [mm]

	double PAW0_130 = 0.0;
	for(int i_Layer = 0; i_Layer < 13; i_Layer++)
	{
		PAW0_130 += (msm.get_SoilMoisture(i_Layer) - msa.at(i_Layer).vs_PermanentWiltingPoint());
	}
	gout << fixed << setprecision(1) << "\t" << (PAW0_130 * 0.1 * 1000.0); // [mm]

	double PAW0_150 = 0.0;
	for(int i_Layer = 0; i_Layer < 15; i_Layer++)
	{
		PAW0_150 += (msm.get_SoilMoisture(i_Layer) - msa.at(i_Layer).vs_PermanentWiltingPoint());
	}
	gout << fixed << setprecision(1) << "\t" << (PAW0_150 * 0.1 * 1000.0); // [mm]

	gout << fixed << setprecision(2) << "\t" << (msc.at(0).get_SoilNmin() + msc.at(1).get_SoilNmin() + msc.at(2).get_SoilNmin()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
	gout << fixed << setprecision(2) << "\t" << (msc.at(3).get_SoilNmin() + msc.at(4).get_SoilNmin() + msc.at(5).get_SoilNmin()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
	gout << fixed << setprecision(2) << "\t" << (msc.at(6).get_SoilNmin() + msc.at(7).get_SoilNmin() + msc.at(8).get_SoilNmin()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
	gout << fixed << setprecision(2) << "\t" << (msc.at(9).get_SoilNmin() + msc.at(10).get_SoilNmin() + msc.at(11).get_SoilNmin()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]

	double N0_60 = 0.0;
	for(int i_Layer = 0; i_Layer < 6; i_Layer++)
	{
		N0_60 += msc.at(i_Layer).get_SoilNmin();
	}
	gout << fixed << setprecision(2) << "\t" << (N0_60 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

	double N0_90 = 0.0;
	for(int i_Layer = 0; i_Layer < 9; i_Layer++)
	{
		N0_90 += msc.at(i_Layer).get_SoilNmin();
	}
	gout << fixed << setprecision(2) << "\t" << (N0_90 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

	double N0_200 = 0.0;
	for(int i_Layer = 0; i_Layer < 20; i_Layer++)
	{
		N0_200 += msc.at(i_Layer).get_SoilNmin();
	}
	gout << fixed << setprecision(2) << "\t" << (N0_200 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

	double N0_130 = 0.0;
	for(int i_Layer = 0; i_Layer < 13; i_Layer++)
	{
		N0_130 += msc.at(i_Layer).get_SoilNmin();
	}
	gout << fixed << setprecision(2) << "\t" << (N0_130 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

	double N0_150 = 0.0;
	for(int i_Layer = 0; i_Layer < 15; i_Layer++)
	{
		N0_150 += msc.at(i_Layer).get_SoilNmin();
	}
	gout << fixed << setprecision(2) << "\t" << (N0_150 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

	gout << fixed << setprecision(2) << "\t" << (msc.at(0).get_SoilNH4() + msc.at(1).get_SoilNH4() + msc.at(2).get_SoilNH4()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
	gout << fixed << setprecision(2) << "\t" << (msc.at(3).get_SoilNH4() + msc.at(4).get_SoilNH4() + msc.at(5).get_SoilNH4()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
	gout << fixed << setprecision(2) << "\t" << (msc.at(6).get_SoilNH4() + msc.at(7).get_SoilNH4() + msc.at(8).get_SoilNH4()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
	gout << fixed << setprecision(2) << "\t" << mso.get_SoilOrganicC(0) * 0.1 * 10000;// [kg m-3] -> [kg ha-1]
	gout << fixed << setprecision(2) << "\t" << ((mso.get_SoilOrganicC(0) + mso.get_SoilOrganicC(1) + mso.get_SoilOrganicC(2)) / 3.0 * 0.3 * 10000); // [kg m-3] -> [kg ha-1]
	gout << fixed << setprecision(1) << "\t" << mst.get_SoilTemperature(0);
	gout << fixed << setprecision(1) << "\t" << mst.get_SoilTemperature(2);
	gout << fixed << setprecision(1) << "\t" << mst.get_SoilTemperature(5);
	gout << fixed << setprecision(2) << "\t" << mso.get_DecomposerRespiration(); // Rh, [kg C ha-1 d-1]

	gout << fixed << setprecision(3) << "\t" << mso.get_NH3_Volatilised(); // [kg N ha-1]
	gout << "\t0"; //! @todo
	gout << "\t0"; //! @todo
	gout << "\t0"; //! @todo
	gout << fixed << setprecision(1) << "\t" << monica.dailySumFertiliser();
	gout << fixed << setprecision(1) << "\t" << monica.dailySumIrrigationWater();
	gout << endl;

}

void Monica::dumpMonicaParametersIntoFile(std::string path, CentralParameterProvider &cpp)
{
	ofstream parameter_output_file;
	parameter_output_file.open(ensureDirExists(path) + "/monica_parameters.txt");
	if(parameter_output_file.fail())
	{
		cerr << "Could not write file\"" << (path + "/monica_parameters.txt") << "\"" << endl;
		return;
	}
	//double po_AtmosphericResistance; //0.0025 [s m-1], from Sadeghi et al. 1988

	// userSoilOrganicParameters
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SOM_SlowDecCoeffStandard" << "\t" << cpp.userSoilOrganicParameters.po_SOM_SlowDecCoeffStandard << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SOM_FastDecCoeffStandard" << "\t" << cpp.userSoilOrganicParameters.po_SOM_FastDecCoeffStandard << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SMB_SlowMaintRateStandard" << "\t" << cpp.userSoilOrganicParameters.po_SMB_SlowMaintRateStandard << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SMB_FastMaintRateStandard" << "\t" << cpp.userSoilOrganicParameters.po_SMB_FastMaintRateStandard << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SMB_SlowDeathRateStandard" << "\t" << cpp.userSoilOrganicParameters.po_SMB_SlowDeathRateStandard << endl;

	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SMB_FastDeathRateStandard" << "\t" << cpp.userSoilOrganicParameters.po_SMB_FastDeathRateStandard << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SMB_UtilizationEfficiency" << "\t" << cpp.userSoilOrganicParameters.po_SMB_UtilizationEfficiency << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SOM_SlowUtilizationEfficiency" << "\t" << cpp.userSoilOrganicParameters.po_SOM_SlowUtilizationEfficiency << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SOM_FastUtilizationEfficiency" << "\t" << cpp.userSoilOrganicParameters.po_SOM_FastUtilizationEfficiency << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_AOM_SlowUtilizationEfficiency" << "\t" << cpp.userSoilOrganicParameters.po_AOM_SlowUtilizationEfficiency << endl;

	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_AOM_FastUtilizationEfficiency" << "\t" << cpp.userSoilOrganicParameters.po_AOM_FastUtilizationEfficiency << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_AOM_FastMaxC_to_N" << "\t" << cpp.userSoilOrganicParameters.po_AOM_FastMaxC_to_N << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_PartSOM_Fast_to_SOM_Slow" << "\t" << cpp.userSoilOrganicParameters.po_PartSOM_Fast_to_SOM_Slow << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_PartSMB_Slow_to_SOM_Fast" << "\t" << cpp.userSoilOrganicParameters.po_PartSMB_Slow_to_SOM_Fast << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_PartSMB_Fast_to_SOM_Fast" << "\t" << cpp.userSoilOrganicParameters.po_PartSMB_Fast_to_SOM_Fast << endl;

	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_PartSOM_to_SMB_Slow" << "\t" << cpp.userSoilOrganicParameters.po_PartSOM_to_SMB_Slow << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_PartSOM_to_SMB_Fast" << "\t" << cpp.userSoilOrganicParameters.po_PartSOM_to_SMB_Fast << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_CN_Ratio_SMB" << "\t" << cpp.userSoilOrganicParameters.po_CN_Ratio_SMB << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_LimitClayEffect" << "\t" << cpp.userSoilOrganicParameters.po_LimitClayEffect << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_AmmoniaOxidationRateCoeffStandard" << "\t" << cpp.userSoilOrganicParameters.po_AmmoniaOxidationRateCoeffStandard << endl;

	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_NitriteOxidationRateCoeffStandard" << "\t" << cpp.userSoilOrganicParameters.po_NitriteOxidationRateCoeffStandard << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_TransportRateCoeff" << "\t" << cpp.userSoilOrganicParameters.po_TransportRateCoeff << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_SpecAnaerobDenitrification" << "\t" << cpp.userSoilOrganicParameters.po_SpecAnaerobDenitrification << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_ImmobilisationRateCoeffNO3" << "\t" << cpp.userSoilOrganicParameters.po_ImmobilisationRateCoeffNO3 << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_ImmobilisationRateCoeffNH4" << "\t" << cpp.userSoilOrganicParameters.po_ImmobilisationRateCoeffNH4 << endl;

	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_Denit1" << "\t" << cpp.userSoilOrganicParameters.po_Denit1 << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_Denit2" << "\t" << cpp.userSoilOrganicParameters.po_Denit2 << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_Denit3" << "\t" << cpp.userSoilOrganicParameters.po_Denit3 << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_HydrolysisKM" << "\t" << cpp.userSoilOrganicParameters.po_HydrolysisKM << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_ActivationEnergy" << "\t" << cpp.userSoilOrganicParameters.po_ActivationEnergy << endl;

	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_HydrolysisP1" << "\t" << cpp.userSoilOrganicParameters.po_HydrolysisP1 << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_HydrolysisP2" << "\t" << cpp.userSoilOrganicParameters.po_HydrolysisP2 << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_AtmosphericResistance" << "\t" << cpp.userSoilOrganicParameters.po_AtmosphericResistance << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_N2OProductionRate" << "\t" << cpp.userSoilOrganicParameters.po_N2OProductionRate << endl;
	parameter_output_file << "userSoilOrganicParameters" << "\t" << "po_Inhibitor_NH3" << "\t" << cpp.userSoilOrganicParameters.po_Inhibitor_NH3 << endl;

	parameter_output_file << endl;


	parameter_output_file.close();
}



