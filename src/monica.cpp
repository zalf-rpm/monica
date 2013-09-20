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

#include "boost/foreach.hpp"
#include "tools/use-stl-algo-boost-lambda.h"

#include "debug.h"
#include "monica.h"
#include "climate/climate-common.h"
#include "db/abstract-db-connections.h"

#define LOKI_OBJECT_LEVEL_THREADING
#include "loki/Threads.h"

using namespace Monica;
using namespace std;
using namespace Climate;
using namespace Tools;

namespace
{
	struct L : public Loki::ObjectLevelLockable<L> {};

	//! simple functor for use in fertiliser trigger
  struct AddFertiliserAmountsCallback
  {
		double& _sf;
		double& _dsf;
		AddFertiliserAmountsCallback(double& sf, double& dsf) : _sf(sf), _dsf(dsf) { }
    void operator()(double v)
    {
            // cout << "!!!AddFertiliserAmountsCallback: daily_sum_fert += " << v << endl;
			_sf += v;
			_dsf += v;
		}
	};

}

//------------------------------------------------------------------------------

Env::Env(const SoilPMs* sps, const CentralParameterProvider cpp)
  : soilParams(sps),
  centralParameterProvider(cpp)
{

  UserEnvironmentParameters& user_env = centralParameterProvider.userEnvironmentParameters;
  windSpeedHeight = user_env.p_WindSpeedHeight;
  atmosphericCO2 = user_env.p_AthmosphericCO2;
  albedo = user_env.p_Albedo;

  noOfLayers = user_env.p_NumberOfLayers;
  layerThickness = user_env.p_LayerThickness;
  useNMinMineralFertilisingMethod = user_env.p_UseNMinMineralFertilisingMethod;
  useAutomaticIrrigation = user_env.p_UseAutomaticIrrigation;
  useSecondaryYields = user_env.p_UseSecondaryYields;
}

string Env::toString() const
{
  ostringstream s;
  s << "soilParams: " << endl;
  BOOST_FOREACH(const SoilParameters& sps, *soilParams)
    s << sps.toString() << endl;
  s << " noOfLayers: " << noOfLayers << " layerThickness: " << layerThickness
    << endl;
  s << "ClimateData: from: " << da.startDate().toString()
    << " to: " << da.endDate().toString() << endl;
  s << "Fruchtfolge: " << endl;
  BOOST_FOREACH(const ProductionProcess& pv, cropRotation)
    s << pv.toString() << endl;
  s << "gridPoint: " << gridPoint.toString();
  return s.str();
}

/**
 * @brief Copy constructor
 * @param env
 * @return
 */
Env::Env(const Env& env)
{
  debug() << "Copy constructor: Env" << "\tsoil param size: " << env.soilParams->size() << endl;
  soilParams = env.soilParams;
  noOfLayers = env.noOfLayers;
  layerThickness = env.layerThickness;
  useNMinMineralFertilisingMethod = env.useNMinMineralFertilisingMethod;
  useAutomaticIrrigation = env.useAutomaticIrrigation;
  useSecondaryYields = env.useSecondaryYields;

  windSpeedHeight = env.windSpeedHeight;
  atmosphericCO2 = env.atmosphericCO2;
  albedo = env.albedo;

  da = env.da;
  cropRotation = env.cropRotation;
//  std::vector<ProductionProcess>::const_iterator it = env.cropRotation.begin();
//  for (; it!=env.cropRotation.end(); it++) {
//    cropRotation.push_back(*it);
//  }

  gridPoint = env.gridPoint;

  site = env.site;
  general = env.general;
  organic = env.organic;

  nMinFertiliserPartition = env.nMinFertiliserPartition;
  nMinUserParams = env.nMinUserParams;
  autoIrrigationParams = env.autoIrrigationParams;
  centralParameterProvider = env.centralParameterProvider;

  pathToOutputDir = env.pathToOutputDir;
  mode = env.mode;

}

Env::~Env()
{
}

/**
 * Set execution mode of Monica.
 * Disables debug outputs for some modes.
 *
 * @param mode
 */
void Env::setMode(int mode)
{
  this->mode = mode;
}

/**
 * Interface method for python wrapping. Simply returns number
 * of possible simulation steps according to avaible climate data.
 *
 * @return Number of steps
 */
int
Env::numberOfPossibleSteps()
{
  return da.noOfStepsPossible();
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
  int acd=0;
  if (name == "tmin")
    acd = tmin;
  else if (name == "tmax")
    acd = tmax;
  else if (name == "tavg")
    acd = tavg;
  else if (name == "precip")
    acd = precip;
  else if (name == "globrad")
    acd = globrad;
  else if (name == "wind")
    acd = wind;
  else if (name == "sunhours")
    acd = sunhours;
  else if (name == "relhumid")
    acd = relhumid;

  da.addOrReplaceClimateData((AvailableClimateData)acd, data);
}
//------------------------------------------------------------------------------

/**
 * @brief Constructor
 * @param env Environment
 * @param da Data accessor
 *
 * Parameter initialization
 */
MonicaModel::MonicaModel(const Env& env, const Climate::DataAccessor& da) :
_env(env),
_soilColumn(_env.general, *_env.soilParams, _env.centralParameterProvider),
_soilTemperature(_soilColumn, *this, _env.centralParameterProvider),
_soilMoisture(_soilColumn, _env.site, *this, _env.centralParameterProvider),
_soilOrganic(_soilColumn, _env.general, _env.site,_env.centralParameterProvider),
_soilTransport(_soilColumn, _env.site, _env.centralParameterProvider),
_currentCropGrowth(NULL),
_sumFertiliser(0),
_dailySumFertiliser(0),
_dailySumIrrigationWater(0),
_dataAccessor(da),
centralParameterProvider(_env.centralParameterProvider),
p_daysWithCrop(0),
p_accuNStress(0.0),
p_accuWaterStress(0.0),
p_accuHeatStress(0.0),
p_accuOxygenStress(0.0)
{
}

/**
 * @brief Simulation of crop seed.
 * @param crop to be planted
 */
void MonicaModel::seedCrop(CropPtr crop)
{
  debug() << "seedCrop" << endl;
  delete _currentCropGrowth;
  p_daysWithCrop = 0;
  p_accuNStress = 0.0;
  p_accuWaterStress = 0.0;
  p_accuHeatStress = 0.0;
  p_accuOxygenStress = 0.0;

  _currentCropGrowth = NULL;
  _currentCrop = crop;
  if(_currentCrop->isValid())
  {
		const CropParameters* cps = _currentCrop->cropParameters();
    _currentCropGrowth = new CropGrowth(_soilColumn, _env.general,
                                        *cps, _env.site, _env.centralParameterProvider, crop->getEva2TypeUsage());
    _soilTransport.put_Crop(_currentCropGrowth);
    _soilColumn.put_Crop(_currentCropGrowth);
    _soilMoisture.put_Crop(_currentCropGrowth);
	_soilOrganic.put_Crop(_currentCropGrowth);

    debug() << "seedDate: "<< _currentCrop->seedDate().toString()
        << " harvestDate: " << _currentCrop->harvestDate().toString() << endl;

		if(_env.useNMinMineralFertilisingMethod
       && _currentCrop->seedDate().dayOfYear() <=
       _currentCrop->harvestDate().dayOfYear())
    {
      debug() << "nMin fertilising summer crop" << endl;
			double fert_amount = applyMineralFertiliserViaNMinMethod
          (_env.nMinFertiliserPartition,
           NMinCropParameters(cps->pc_SamplingDepth,
                              cps->pc_TargetNSamplingDepth,
                              cps->pc_TargetN30));
			addDailySumFertiliser(fert_amount);
		}

    if (this->writeOutputFiles()) {
        _currentCrop->writeCropParameters(_env.pathToOutputDir);
    }
  }
}

/**
 * @brief Simulating harvest of crop.
 *
 * Deletes the current crop.
 */
void MonicaModel::harvestCurrentCrop()
{
	//could be just a fallow, so there might be no CropGrowth object
  if(_currentCrop && _currentCrop->isValid())
  {
		//prepare to add root and crop residues to soilorganic (AOMs)
		double rootBiomass = _currentCropGrowth->get_OrganBiomass(0);
		double rootNConcentration = _currentCropGrowth->get_RootNConcentration();
		debug() << "adding organic matter from root to soilOrganic" << endl;
    debug() << "root biomass: " << rootBiomass
        << " Root N concentration: " << rootNConcentration << endl;

    _soilOrganic.addOrganicMatter(_currentCrop->residueParameters(),
                                  rootBiomass, rootNConcentration);

    double residueBiomass =
        _currentCropGrowth->get_ResidueBiomass(_env.useSecondaryYields);
		//!@todo Claas: das hier noch berechnen
		double residueNConcentration = _currentCropGrowth->get_ResiduesNConcentration();
		debug() << "adding organic matter from residues to soilOrganic" << endl;
    debug() << "residue biomass: " << residueBiomass
        << " Residue N concentration: " << residueNConcentration << endl;
    debug() << "primary yield biomass: " << _currentCropGrowth->get_PrimaryCropYield()
        << " Primary yield N concentration: " << _currentCropGrowth->get_PrimaryYieldNConcentration()<< endl;
    debug() << "secondary yield biomass: " << _currentCropGrowth->get_SecondaryCropYield()
        << " Secondary yield N concentration: " << _currentCropGrowth->get_PrimaryYieldNConcentration()<< endl;
    debug() << "Residues N content: " << _currentCropGrowth->get_ResiduesNContent()
        << " Primary yield N content: " << _currentCropGrowth->get_PrimaryYieldNContent()
        << " Secondary yield N content: " << _currentCropGrowth->get_SecondaryYieldNContent() << endl;

    _soilOrganic.addOrganicMatter(_currentCrop->residueParameters(),
                                  residueBiomass, residueNConcentration);
	}

  delete _currentCropGrowth;
  _currentCropGrowth = NULL;
  _currentCrop = CropPtr();
  _soilTransport.remove_Crop();
  _soilColumn.remove_Crop();
  _soilMoisture.remove_Crop();
}


/**
 * @brief Simulating plowing or incorporating of total crop.
 *
 * Deletes the current crop.
 */
void MonicaModel::incorporateCurrentCrop()
{
  //could be just a fallow, so there might be no CropGrowth object
  if(_currentCrop && _currentCrop->isValid())
  {
    //prepare to add root and crop residues to soilorganic (AOMs)
    double total_biomass = _currentCropGrowth->totalBiomass();
    double totalNConcentration = _currentCropGrowth->get_AbovegroundBiomassNConcentration() + _currentCropGrowth->get_RootNConcentration();

    debug() << "Adding organic matter from total biomass of crop to soilOrganic" << endl;
    debug() << "Total biomass: " << total_biomass << endl
        << " Total N concentration: " << totalNConcentration << endl;

    _soilOrganic.addOrganicMatter(_currentCrop->residueParameters(),
                                  total_biomass, totalNConcentration);
  }

  delete _currentCropGrowth;
  _currentCropGrowth = NULL;
  _currentCrop = CropPtr();
  _soilTransport.remove_Crop();
  _soilColumn.remove_Crop();
  _soilMoisture.remove_Crop();
}



/**
 * @brief Simulating cutting of crop.
 *
 * Only removes some biomass of crop but not harvesting the crop.
 */
/*
void MonicaModel::cutCurrentCrop()
{
  //could be just a fallow, so there might be no CropGrowth object
  if(_currentCrop->isValid())
  {

    debug() << "primary yield biomass: " << _currentCropGrowth->get_PrimaryCropYield()
        << " Primary yield N concentration: " << _currentCropGrowth->get_PrimaryYieldNConcentration()<< endl;
    debug() << "secondary yield biomass: " << _currentCropGrowth->get_SecondaryCropYield()
        << " Secondary yield N concentration: " << _currentCropGrowth->get_PrimaryYieldNConcentration()<< endl;
    debug() << "Residues N content: " << _currentCropGrowth->get_ResiduesNContent()
        << " Primary yield N content: " << _currentCropGrowth->get_PrimaryYieldNContent()
        << " Secondary yield N content: " << _currentCropGrowth->get_SecondaryYieldNContent() << endl;

    //_currentCrop->applyCutting();
  }

}
*/

/**
 * @brief Applying of fertilizer.
 *
 * @todo Nothing implemented yet.
 */
void MonicaModel::applyMineralFertiliser(MineralFertiliserParameters partition,
                                         double amount)
{
  if(!_env.useNMinMineralFertilisingMethod)
  {
		_soilColumn.applyMineralFertiliser(partition, amount);
		addDailySumFertiliser(amount);
	}
}

void MonicaModel::applyOrganicFertiliser(const OrganicMatterParameters* params,
																				 double amount, bool incorporation)
{
  debug() << "MONICA model: applyOrganicFertiliser:\t" << amount << "\t" << params->vo_NConcentration << endl;
  _soilOrganic.setIncorporation(incorporation);
  _soilOrganic.addOrganicMatter(params, amount, params->vo_NConcentration);
  addDailySumFertiliser(amount * params->vo_NConcentration);
}

double MonicaModel::
    applyMineralFertiliserViaNMinMethod(MineralFertiliserParameters partition,
        NMinCropParameters cps)
{
  //AddFertiliserAmountsCallback x(_sumFertiliser, _dailySumFertiliser);

  const NMinUserParameters& ups = _env.nMinUserParams;

  double fert_amount = _soilColumn.applyMineralFertiliserViaNMinMethod
      (partition, cps.samplingDepth, cps.nTarget, cps.nTarget30,
       ups.min, ups.max, ups.delayInDays);
  return fert_amount;

  //ref(_sumFertiliser) += _1);
}

void MonicaModel::applyIrrigation(double amount, double nitrateConcentration,
          double /*sulfateConcentration*/)
{
  //if the production process has still some defined manual irrigation dates
  if(!_env.useAutomaticIrrigation)
  {
    _soilOrganic.addIrrigationWater(amount);
    _soilColumn.applyIrrigation(amount, nitrateConcentration);
    if(_currentCrop)
    {
      _currentCrop->addAppliedIrrigationWater(amount);
      addDailySumIrrigationWater(amount);
    }
  }
}

/**
 * Applies tillage for a given soil depth. Tillage means in MONICA,
 * that for all effected soil layer the parameters are averaged.
 * @param depth Depth in meters
 */
void MonicaModel::applyTillage(double depth)
{
	_soilColumn.applyTillage(depth);
}

/**
 * @brief Simulating the soil processes for one time step.
 * @param stepNo Number of current processed step
 */
void MonicaModel::generalStep(unsigned int stepNo)
{

  Date startDate = _dataAccessor.startDate();
  Date currentDate = startDate + stepNo;
  unsigned int julday = _dataAccessor.julianDayForStep(stepNo);
  unsigned int year = currentDate.year();
  bool leapYear = currentDate.isLeapYear();
  double tmin = _dataAccessor.dataForTimestep(Climate::tmin, stepNo);
  double tavg = _dataAccessor.dataForTimestep(Climate::tavg, stepNo);
  double tmax = _dataAccessor.dataForTimestep(Climate::tmax, stepNo);
  double precip = _dataAccessor.dataForTimestep(Climate::precip, stepNo);
  double wind = _dataAccessor.dataForTimestep(Climate::wind, stepNo);
  double globrad = _dataAccessor.dataForTimestep(Climate::globrad, stepNo);

  // test if data for relhumid are available; if not, value is set to -1.0
  double relhumid = _dataAccessor.hasAvailableClimateData(Climate::relhumid) ?
       _dataAccessor.dataForTimestep(Climate::relhumid, stepNo) : -1.0;


//  cout << "tmin:\t" << tmin << endl;
//  cout << "tavg:\t" << tavg << endl;
//  cout << "tmax:\t" << tmax << endl;
//  cout << "precip:\t" << precip << endl;
//  cout << "wind:\t" << wind << endl;
//  cout << "globrad:\t" << globrad << endl;
//  cout << "relhumid:\t" << relhumid << endl;
  // write climate data into separate file with hermes format
//  std::ofstream climate_file((_env.pathToOutputDir+"climate.csv").c_str(), ios_base::app);
//  if (stepNo==0) {
//      climate_file << "Tp_av\t"<<"Tpmin\t"<<"Tpmax\t"<<"T_s10\t"<<"T_s20\t"<<"vappd\t"<<"wind\t"<<"sundu\t"<<"radia\t"<<"prec\t"<<"tagesnummer\t"<<"RF\t"<< endl;
//      climate_file << "C_deg\t"<<"C_deg\t"<<"C_deg\t"<<"C_deg\t"<<"C_deg\t"<<"mm_Hg\t"<<"m/se\t"<<"hours\t"<<"J/cm²\t"<<"mm\t"<<"jday\t"<<"%\t"<<endl;
//      climate_file << "-----------------------------------" << endl;
//  }
//
//  climate_file << tavg <<"\t" << tmin <<"\t" << tmax <<"\t" << 0.0 <<"\t" << 0.0 <<"\t"
//      << 0.0 <<"\t" << wind <<"\t" << sunhours <<"\t" << globrad <<"\t" << precip <<"\t" << stepNo <<"\t" << relhumid << endl;
//  climate_file.close();


  const UserEnvironmentParameters &user_env = centralParameterProvider.userEnvironmentParameters;
  vw_AtmosphericCO2Concentration = _env.atmosphericCO2 == -1
                                   ? user_env.p_AthmosphericCO2
                                     : _env.atmosphericCO2;

//  cout << "vs_GroundwaterDepth:\t" << user_env.p_MinGroundwaterDepth << "\t" << user_env.p_MaxGroundwaterDepth << endl;
  vs_GroundwaterDepth = GroundwaterDepthForDate(user_env.p_MaxGroundwaterDepth,
				        user_env.p_MinGroundwaterDepth,
				        user_env.p_MinGroundwaterDepthMonth,
				        julday,
				        leapYear);
  if (stepNo<=1) {
    //    : << "Monica: tmin: " << tmin << endl;
    //    cout << "Monica: tmax: " << tmax << endl;
    //    cout << "Monica: globrad: " << globrad << endl;
    //    cout << "Monica: precip: " << precip << endl;
  }


  if (int(vw_AtmosphericCO2Concentration) == 0)
    vw_AtmosphericCO2Concentration = CO2ForDate(year, julday, leapYear);

  //  debug << "step: " << stepNo << " p: " << precip << " gr: " << globrad << endl;

  //31 + 28 + 15
  unsigned int pc_JulianDayAutomaticFertilising =
      user_env.p_JulianDayAutomaticFertilising;

  _soilColumn.deleteAOMPool();

  _soilColumn.applyPossibleDelayedFerilizer();
  double delayed_fert_amount = _soilColumn.applyPossibleTopDressing();
  addDailySumFertiliser(delayed_fert_amount);

  if(_currentCrop && _currentCrop->isValid() &&
     _env.useNMinMineralFertilisingMethod
		 && _currentCrop->seedDate().dayOfYear() > _currentCrop->harvestDate().dayOfYear()
    && _dataAccessor.julianDayForStep(stepNo) == pc_JulianDayAutomaticFertilising)
    {
    debug() << "nMin fertilising winter crop" << endl;
		const CropParameters* cps = _currentCrop->cropParameters();
		double fert_amount = applyMineralFertiliserViaNMinMethod
        (_env.nMinFertiliserPartition,
         NMinCropParameters(cps->pc_SamplingDepth,
                            cps->pc_TargetNSamplingDepth,
                            cps->pc_TargetN30));
		addDailySumFertiliser(fert_amount);

	}

  _soilTemperature.step(tmin, tmax, globrad);
  _soilMoisture.step(vs_GroundwaterDepth,
                     precip, tmax, tmin,
                     (relhumid / 100.0), tavg, wind,
                     _env.windSpeedHeight,
		 globrad, julday);

  _soilOrganic.step(tavg, precip, wind);
  _soilTransport.step();
}

/**
 * @brief Simulating crop growth for one time step.
 */
void MonicaModel::cropStep(unsigned int stepNo)
{
  // do nothing if there is no crop
  if(!_currentCropGrowth)
    return;

  p_daysWithCrop++;

  unsigned int julday = _dataAccessor.julianDayForStep(stepNo);

  double tavg = _dataAccessor.dataForTimestep(Climate::tavg, stepNo);
  double tmax = _dataAccessor.dataForTimestep(Climate::tmax, stepNo);
  double tmin = _dataAccessor.dataForTimestep(Climate::tmin, stepNo);
  double globrad = _dataAccessor.dataForTimestep(Climate::globrad, stepNo);

  // test if data for sunhours are available; if not, value is set to -1.0
  double sunhours = _dataAccessor.hasAvailableClimateData(Climate::sunhours) ?
	  _dataAccessor.dataForTimestep(Climate::sunhours, stepNo) : -1.0;		

  // test if data for relhumid are available; if not, value is set to -1.0
  double relhumid = _dataAccessor.hasAvailableClimateData(Climate::relhumid) ?
      _dataAccessor.dataForTimestep(Climate::relhumid, stepNo) : -1.0;

  double wind =  _dataAccessor.dataForTimestep(Climate::wind, stepNo);
  double precip =  _dataAccessor.dataForTimestep(Climate::precip, stepNo);

  double vw_WindSpeedHeight =
      centralParameterProvider.userEnvironmentParameters.p_WindSpeedHeight;

  _currentCropGrowth->step(tavg, tmax, tmin, globrad, sunhours, julday,
                           (relhumid / 100.0), wind, vw_WindSpeedHeight,
                           vw_AtmosphericCO2Concentration, precip);
  if(_env.useAutomaticIrrigation)
  {
    const AutomaticIrrigationParameters& aips = _env.autoIrrigationParams;
    if(_soilColumn.applyIrrigationViaTrigger(aips.treshold, aips.amount,
                                             aips.nitrateConcentration))
    {
      _soilOrganic.addIrrigationWater(aips.amount);
      _currentCrop->addAppliedIrrigationWater(aips.amount);
      _dailySumIrrigationWater += aips.amount;
    }
  }

  p_accuNStress += _currentCropGrowth->get_CropNRedux();
  p_accuWaterStress += _currentCropGrowth->get_TranspirationDeficit();
  p_accuHeatStress += _currentCropGrowth->get_HeatStressRedux();
  p_accuOxygenStress += _currentCropGrowth->get_OxygenDeficit();
}

/**
* @brief Returns atmospheric CO2 concentration for date [ppm]
*
* @param co2
*
* @return
*/
double MonicaModel::CO2ForDate(double year, double julianday, bool leapYear)
{
  double co2;
  double decimalDate;

  if (leapYear)
    decimalDate = year + (julianday/366.0);
  else
    decimalDate = year + (julianday/365.0);

  co2 = 222.0 + exp(0.0119 * (decimalDate - 1580.0)) + 2.5 * sin((decimalDate - 0.5) / 0.1592);
  return co2;
}

/**
* @brief Returns groundwater table for date [m]
*
* @param pm_MaxGroundwaterTable; pm_MinGroundwaterTable; pm_MaxGroundwaterTableMonth
*
* @return
*/
double MonicaModel::GroundwaterDepthForDate(double maxGroundwaterDepth,
				    double minGroundwaterDepth,
				    int minGroundwaterDepthMonth,
				    double julianday,
				    bool leapYear)
{
  double groundwaterDepth;
  double days;
  double meanGroundwaterDepth;
  double groundwaterAmplitude;

  if (leapYear)
    days = 366.0;
  else
    days = 365.0;

  meanGroundwaterDepth = (maxGroundwaterDepth + minGroundwaterDepth) / 2.0;
  groundwaterAmplitude = (maxGroundwaterDepth - minGroundwaterDepth) / 2.0;

  double sinus = sin(((julianday / days * 360.0) - 90.0 -
	       ((double(minGroundwaterDepthMonth) * 30.0) - 15.0)) *
	       3.14159265358979 / 180.0);

  groundwaterDepth = meanGroundwaterDepth + (sinus * groundwaterAmplitude);

  if (groundwaterDepth < 0.0) groundwaterDepth = 20.0;

  return groundwaterDepth;
}

//----------------------------------------------------------------------------

/**
 * @brief Returns mean soil organic C.
 * @param depth_m
 */
//Kohlenstoffgehalt 0-depth [% kg C / kg Boden]
double MonicaModel::avgCorg(double depth_m) const
{
  double lsum = 0, sum = 0;
  int count = 0;

  for(int i = 0, nols = _env.noOfLayers; i < nols; i++)
  {
    count++;
    sum +=_soilColumn[i].vs_SoilOrganicCarbon(); //[kg C / kg Boden]
    lsum += _soilColumn[i].vs_LayerThickness;
    if(lsum >= depth_m)
      break;
  }

  return sum / double(count) * 100.0;
}

/**
 * @brief Returns the soil moisture up to 90 cm depth
 * @return water content
 */
//Bodenwassergehalt 0-90cm [%nFK]
double MonicaModel::mean90cmWaterContent() const
{
  return _soilMoisture.meanWaterContent(0.9);
}

double MonicaModel::meanWaterContent(int layer, int number_of_layers) const
{
  return _soilMoisture.meanWaterContent(layer, number_of_layers);
}

/**
 * @brief Returns the N content up to given depth.
 *
 *@return N content
 *
 */
//Boden-Nmin-Gehalt 0-90cm am 31.03. [kg N/ha]
double MonicaModel::sumNmin(double depth_m) const
{
  double lsum = 0, sum = 0;
  int count = 0;

  for(int i = 0, nols = _env.noOfLayers; i < nols; i++)
  {
    count++;
    sum += _soilColumn[i].get_SoilNmin(); //[kg N m-3]
    lsum += _soilColumn[i].vs_LayerThickness;
    if(lsum >= depth_m)
      break;
  }

  return sum / double(count) * lsum * 10000;
}

/**
 * Returns accumulation of soil nitrate for 90cm soil at 31.03.
 * @param depth Depth of soil
 * @return Accumulated nitrate
 */
double
MonicaModel::sumNO3AtDay(double depth_m)
{
  double lsum = 0, sum = 0;
  int count = 0;

  for(int i = 0, nols = _env.noOfLayers; i < nols; i++)
  {
    count++;
    sum += _soilColumn[i].get_SoilNO3(); //[kg m-3]
    lsum += _soilColumn[i].vs_LayerThickness;
    if(lsum >= depth_m)
      break;
  }

  return sum;
}

//Grundwasserneubildung[mm Wasser]
double MonicaModel::groundWaterRecharge() const
{
  return _soilMoisture.get_GroundwaterRecharge();
}

//N-Auswaschung[kg N/ha]
double MonicaModel::nLeaching() const {
  return _soilTransport.get_NLeaching();//[kg N ha-1]
}

/**
 * Returns sum of soiltemperature in given number of soil layers
 * @param layers Number of layers that should be added.
 * @return Soil temperature sum [°C]
 */
double MonicaModel::sumSoilTemperature(int layers)
{
  return _soilColumn.sumSoilTemperature(layers);
}

/**
 * Returns maximal snow depth during simulation
 * @return
 */
double
MonicaModel::maxSnowDepth() const
{
  return _soilMoisture.getMaxSnowDepth();
}

/**
 * Returns sum of all snowdepth during whole simulation
 * @return
 */
double MonicaModel::accumulatedSnowDepth() const
{
  return _soilMoisture.accumulatedSnowDepth();
}

/**
 * Returns sum of frost depth during whole simulation.
 * @return Accumulated frost depth
 */
double MonicaModel::accumulatedFrostDepth() const
{
  return _soilMoisture.getAccumulatedFrostDepth();
}

/**
 * Returns average soil temperature of first 30cm soil.
 * @return Average soil temperature of organic layers.
 */
double MonicaModel::avg30cmSoilTemperature()
{
  double nols = 3;
  double accu_temp = 0.0;
  for (int layer=0; layer<nols; layer++)
    accu_temp+=_soilColumn.soilLayer(layer).get_Vs_SoilTemperature();

  return accu_temp / nols;
}

/**
 * Returns average soil moisture concentration in soil in a defined layer.
 * Layer is specified by start end end of soil layer.
 *
 * @param start_layer
 * @param end_layer
 * @return Average soil moisture concentation
 */
double MonicaModel::avgSoilMoisture(int start_layer, int end_layer)
{
  int num=0;
  double accu = 0.0;
  for (int i=start_layer; i<end_layer; i++)
  {
    accu+=_soilColumn.soilLayer(i).get_Vs_SoilMoisture_m3();
    num++;
  }
  return accu/num;
}

/**
 * Returns mean of capillary rise in a set of layers.
 * @param start_layer First layer to be included
 * @param end_layer Last layer, is not included;
 * @return Average capillary rise [mm]
 */
double MonicaModel::avgCapillaryRise(int start_layer, int end_layer)
{
  int num=0;
  double accu = 0.0;
  for (int i=start_layer; i<end_layer; i++)
  {
    accu+=_soilMoisture.get_CapillaryRise(i);
    num++;
  }
  return accu/num;
}

/**
 * @brief Returns mean percolation rate
 * @param start_layer
 * @param end_layer
 * @return Mean percolation rate [mm]
 */
double MonicaModel::avgPercolationRate(int start_layer, int end_layer)
{
  int num=0;
  double accu = 0.0;
  for (int i=start_layer; i<end_layer; i++)
  {
    accu+=_soilMoisture.get_PercolationRate(i);
    num++;
  }
  return accu/num;
}

/**
 * Returns sum of all surface run offs at this point in simulation time.
 * @return Sum of surface run off in [mm]
 */
double MonicaModel::sumSurfaceRunOff()
{
  return _soilMoisture.get_SumSurfaceRunOff();
}

/**
 * Returns surface runoff of current day [mm].
 */
double MonicaModel::surfaceRunoff()
{
  return _soilMoisture.get_SurfaceRunOff();
}

/**
 * Returns evapotranspiration [mm]
 * @return
 */
double MonicaModel::getEvapotranspiration()
{
  if (_currentCropGrowth!=0)
    return _currentCropGrowth->get_RemainingEvapotranspiration();

  return 0.0;
}

/**
 * Returns actual transpiration
 * @return
 */
double MonicaModel::getTranspiration()
{
  if (_currentCropGrowth!=0)
    return _currentCropGrowth->get_ActualTranspiration();

  return 0.0;
}

/**
 * Returns actual evaporation
 * @return
 */
double MonicaModel::getEvaporation()
{
  if (_currentCropGrowth!=0)
    return _currentCropGrowth->get_EvaporatedFromIntercept();

  return 0.0;
}

double MonicaModel::getETa()
{
  return _soilMoisture.get_Evapotranspiration();
}

/**
 * Returns sum of evolution rate in first three layers.
 * @return
 */
double MonicaModel::get_sum30cmSMB_CO2EvolutionRate()
{
  double sum = 0.0;
  for (int layer=0; layer<3; layer++) {
    sum+=_soilOrganic.get_SMB_CO2EvolutionRate(layer);
  }

  return sum;
}

/**
 * Returns volatilised NH3
 * @return
 */
double MonicaModel::getNH3Volatilised()
{
  return _soilOrganic.get_NH3_Volatilised();
}

/**
 * Returns accumulated sum of all volatilised NH3 in simulation time.
 * @return
 */
double MonicaModel::getSumNH3Volatilised()
{
//  cout << "NH4Vol: " << _soilOrganic.get_sumNH3_Volatilised() << endl;
  return _soilOrganic.get_SumNH3_Volatilised();
}

/**
 * Returns sum of denitrification rate in first 30cm soil.
 * @return Denitrification rate [kg N m-3 d-1]
 */
double MonicaModel::getsum30cmActDenitrificationRate()
{
  double sum=0.0;
  for (int layer=0; layer<3; layer++) {
//    cout << "DENIT: " << _soilOrganic.get_ActDenitrificationRate(layer) << endl;
    sum+=_soilOrganic.get_ActDenitrificationRate(layer);
  }

  return sum;
}

//------------------------------------------------------------------------------



/**
 * @brief Static method for starting calculation
 * @param env
 */
Result Monica::runMonica(Env env)
{

  Result res;
  res.gp = env.gridPoint;

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
  env.centralParameterProvider.writeOutputFiles = write_output_files;

  debug() << "-----" << endl;

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
    debug() << "Outputpath: " << (env.pathToOutputDir+"/rmout.dat").c_str() << endl;
    fout.open((env.pathToOutputDir+"/rmout.dat").c_str());
    if (fout.fail())
    {
      debug() << "Error while opening output file \"" << (env.pathToOutputDir+"/rmout.dat").c_str() << "\"" << endl;
      return res;
    }

    // open smout.dat
    gout.open((env.pathToOutputDir+"/smout.dat").c_str());
    if (gout.fail())
    {
      debug() << "Error while opening output file \"" << (env.pathToOutputDir+"/smout.dat").c_str() << "\"" << endl;
      return res;
    }

    // writes the header line to output files
    initializeFoutHeader(fout);
    initializeGoutHeader(gout);

    dumpMonicaParametersIntoFile(env.pathToOutputDir, env.centralParameterProvider);
  }

  //debug() << "MonicaModel" << endl;
  //debug() << env.toString().c_str();
  MonicaModel monica(env, env.da);
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
  for(unsigned int d = 0; d < nods; ++d, ++currentDate, ++dim)
  {
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
        if(!env.useSecondaryYields)
          r.pvResults[secondaryYield] = 0;
        r.pvResults[sumFertiliser] = monica.sumFertiliser();
        r.pvResults[daysWithCrop] = monica.daysWithCrop();
        r.pvResults[NStress] = monica.getAccumulatedNStress();
        r.pvResults[WaterStress] = monica.getAccumulatedWaterStress();
        r.pvResults[HeatStress] = monica.getAccumulatedHeatStress();
        r.pvResults[OxygenStress] = monica.getAccumulatedOxygenStress();



        res.pvrs.push_back(r);
        debug() << "py: " << r.pvResults[primaryYield]
            << " sy: " << r.pvResults[secondaryYield]
            << " iw: " << r.pvResults[sumIrrigation]
            << " sf: " << monica.sumFertiliser()
            << endl;

				//to count the applied fertiliser for the next production process
				monica.resetFertiliserCounter();

        //resets crop values for use in next year
        currentPP.crop()->reset();

        ppci++;
        //start anew if we reached the end of the crop rotation
        if(ppci == env.cropRotation.end()) {
          ppci = env.cropRotation.begin();
        }

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
    {
      monica.cropStep(d);
    }
    // writes crop results to output file
    if (write_output_files)
    {
      writeCropResults(monica.cropGrowth(), fout, gout, monica.isCropPlanted());
    }
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
      res.generalResults[sumSnowDepth].push_back(monica.accumulatedSnowDepth());
      res.generalResults[sumFrostDepth].push_back(monica.accumulatedFrostDepth());
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
    if ((currentDate.year() != (currentDate-1).year()) && (currentDate.year()!= env.da.startDate().year())) {
        res.generalResults[yearlySumGroundWaterRecharge].push_back(yearly_groundwater);
//        cout << "#######################################################" << endl;
//        cout << "Push back yearly_nleaching: " << currentDate.year()  << "\t" << yearly_nleaching << endl;
//        cout << "#######################################################" << endl;
        res.generalResults[yearlySumNLeaching].push_back(yearly_nleaching);
        yearly_groundwater = 0.0;
        yearly_nleaching = 0.0;
    } else {
        yearly_groundwater += monica.groundWaterRecharge();
        yearly_nleaching += monica.nLeaching();
    }

    if (monica.isCropPlanted()) {
      //cout << "monica.cropGrowth()->get_GrossPrimaryProduction()\t" << monica.cropGrowth()->get_GrossPrimaryProduction() << endl;

      res.generalResults[dev_stage].push_back(monica.cropGrowth()->get_DevelopmentalStage()+1);


    } else {
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

std::string
Result::toString()
{
    ostringstream s;
    map<ResultId,std::vector<double> >::iterator it;
    // show content:
    for ( it=generalResults.begin() ; it != generalResults.end(); it++ ) {
        ResultId id = (*it).first;
        std::vector<double> data = (*it).second;
        s << resultIdInfo(id).shortName.c_str() << ":\t" <<  data.at(data.size()-1) << endl;
    }

    return s.str();
}

//------------------------------------------------------------------------------


/**
 * Write header line to fout Output file
 * @param fout File pointer to rmout.dat
 */
void
Monica::initializeFoutHeader(ofstream &fout)
{
  int outLayers = 20;
  fout << "Datum     ";
  fout << "\tCrop";
  fout << "\tTraDef";
  fout << "\tTra";
  fout << "\tNDef";
  fout << "\tHeatRed";
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
  fout << "\tRoot";
  fout << "\tLeaf";
  fout << "\tShoot";
  fout << "\tFruit";
  fout << "\tYield";

  fout << "\tGroPhot";
  fout << "\tNetPhot";
  fout << "\tMaintR";
  fout << "\tGrowthR";
  fout << "\tStomRes";
  fout << "\tHeight";
  fout << "\tLAI";
  fout << "\tRootDep";
  fout << "\tAbBiom";

  fout << "\tNBiom";
  fout << "\tSumNUp";
  fout << "\tActNup";
  fout << "\tPotNup";
  fout << "\tTarget";

  fout << "\tCritN";
  fout << "\tAbBiomN";

  fout << "\tNPP";
  fout << "\tNPPRoot";
  fout << "\tNPPLeaf";
  fout << "\tNPPShoot";
  fout << "\tNPPFruit";

  fout << "\tGPP";
  fout << "\tRa";
  fout << "\tRaRoot";
  fout << "\tRaLeaf";
  fout << "\tRaShoot";
  fout << "\tRaFruit";

  for (int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << "\tMois" << i_Layer;
  }
  fout << "\tPrecip";
  fout << "\tIrrig";
  fout << "\tInfilt";
  fout << "\tSurface";
  fout << "\tRunOff";
  fout << "\tSnowD";
  fout << "\tFrostD";
  fout << "\tThawD";
  for (int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << "\tPASW-" << i_Layer;
  }
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

  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << "\tNO3-" << i_Layer;
  }
  fout << "\tCarb";
  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << "\tNH4-" << i_Layer;
  }
	for(int i_Layer = 0; i_Layer < 4; i_Layer++) {
		fout << "\tNO2-" << i_Layer;
	}
  for(int i_Layer = 0; i_Layer < 6; i_Layer++) {
    fout << "\tSOC-" << i_Layer;
  }

  fout << "\tSOC-0-30";
  fout << "\tSOC-0-200";

  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\tAOMf-" << i_Layer;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\tAOMs-" << i_Layer;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\tSMBf-" << i_Layer;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\tSMBs-" << i_Layer;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\tSOMf-" << i_Layer;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\tSOMs-" << i_Layer;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\tCBal-" << i_Layer;
  }
  for(int i_Layer = 0; i_Layer < 3; i_Layer++) {
    fout << "\tNmin-" << i_Layer;
  }

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
  fout << "\t[0;1]";    // OxygenDeficit

  fout << "\t[ ]";      // DevelopmentalStage
  fout << "\t[°Cd]";    // CurrentTemperatureSum
  fout << "\t[0;1]";    // VernalisationFactor
  fout << "\t[0;1]";    // DaylengthFactor
  fout << "\t[kg/ha]";  // OrganGrowthIncrement root
  fout << "\t[kg/ha]";  // OrganGrowthIncrement leaf
  fout << "\t[kg/ha]";  // OrganGrowthIncrement shoot
  fout << "\t[kg/ha]";  // OrganGrowthIncrement fruit

  fout << "\t[0;1]";        // RelativeTotalDevelopment

  fout << "\t[kgDM/ha]";    // get_OrganBiomass(0)
  fout << "\t[kgDM/ha]";    // get_OrganBiomass(1)
  fout << "\t[kgDM/ha]";    // get_OrganBiomass(2)
  fout << "\t[kgDM/ha]";    // get_OrganBiomass(3)
  fout << "\t[kgDM/ha]";    // get_PrimaryCropYield(3)

  fout << "\t[kgCH2O/ha]";  // GrossPhotosynthesisHaRate
  fout << "\t[kgCH2O/ha]";  // NetPhotosynthesis
  fout << "\t[kgCH2O/ha]";  // MaintenanceRespirationAS
  fout << "\t[kgCH2O/ha]";  // GrowthRespirationAS
  fout << "\t[s/m]";        // StomataResistance
  fout << "\t[m]";          // CropHeight
  fout << "\t[m2/m2]";      // LeafAreaIndex
  fout << "\t[layer]";      // RootingDepth
  fout << "\t[kg/ha]";       // AbovegroundBiomass

  fout << "\t[kgN/ha]";     // TotalBiomassNContent
  fout << "\t[kgN/ha]";     // SumTotalNUptake
  fout << "\t[kgN/ha]";     // ActNUptake
  fout << "\t[kgN/ha]";     // PotNUptake
  fout << "\t[kgN/kg]";     // TargetNConcentration

  fout << "\t[kgN/kg]";     // CriticalNConcentration
  fout << "\t[kgN/kg]";     // AbovegroundBiomassNConcentration

  fout << "\t[kg C ha-1]";   // NPP
  fout << "\t[kg C ha-1]";   // NPP root
  fout << "\t[kg C ha-1]";   // NPP leaf
  fout << "\t[kg C ha-1]";   // NPP shoot
  fout << "\t[kg C ha-1]";   // NPP fruit

  fout << "\t[kg C ha-1]";   // GPP
  fout << "\t[kg C ha-1]";   // Ra
  fout << "\t[kg C ha-1]";   // Ra root
  fout << "\t[kg C ha-1]";   // Ra leaf
  fout << "\t[kg C ha-1]";   // Ra shoot
  fout << "\t[kg C ha-1]";   // Ra fruit


  for (int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << "\t[m3/m3]"; // Soil moisture content
  }
  fout << "\t[mm]"; // Precipitation
  fout << "\t[mm]"; // Irrigation
  fout << "\t[mm]"; // Infiltration
  fout << "\t[mm]"; // Surface water storage
  fout << "\t[mm]"; // Surface water runoff
  fout << "\t[mm]"; // Snow depth
  fout << "\t[m]"; // Frost front depth in soil
  fout << "\t[m]"; // Thaw front depth in soil
  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
  fout << "\t[m3/m3]"; //PASW
  }

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
  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << "\t[kgN/m3]";
  }

  fout << "\t[kgN/m3]";  // Soil Carbamid

  // NH4
  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << "\t[kgN/m3]";
  }

	// NO2
	for(int i_Layer = 0; i_Layer < 4; i_Layer++) {
		fout << "\t[kgN/m3]";
	}

  // get_SoilOrganicC
  for(int i_Layer = 0; i_Layer < 6; i_Layer++) {
    fout << "\t[kgC/kg]";
  }

  fout << "\t[gC m-2]";   // SOC-0-30
  fout << "\t[gC m-2]";   // SOC-0-200

  // get_AOM_FastSum
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\t[kgC/m3]";
  }
  // get_AOM_SlowSum
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\t[kgC/m3]";
  }

  // get_SMB_Fast
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\t[kgC/m3]";
  }
  // get_SMB_Slow
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\t[kgC/m3]";
  }

  // get_SOM_Fast
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\t[kgC/m3]";
  }
  // get_SOM_Slow
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\t[kgC/m3]";
  }

  // get_CBalance
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << "\t[kgC/m3]";
  }

  // NetNMineralisationRate
  for(int i_Layer = 0; i_Layer < 3; i_Layer++) {
    fout << "\t[kgN/ha]";
  }

  fout << "\t[kgN/ha]";  // NetNmin
  fout << "\t[kgN/ha]";  // Denit
	fout << "\t[kgN/ha]";  // N2O
	fout << "\t[ ]";       // SoilpH
  fout << "\t[kgC/ha]";  // NEP
  fout << "\t[kgC/ha]";  // NEE
  fout << "\t[kgC/ha]"; // Rh

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
void
Monica::initializeGoutHeader(ofstream &gout)
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
void
Monica::writeCropResults(const CropGrowth *mcg, ofstream &fout, ofstream &gout, bool crop_is_planted)
{
  if(crop_is_planted) {
    fout << "\t" << mcg->get_CropName();
    fout << fixed << setprecision(2) << "\t" << mcg->get_TranspirationDeficit();// [0;1]
    fout << fixed << setprecision(2) << "\t" << mcg->get_ActualTranspiration();
    fout << fixed << setprecision(2) << "\t" << mcg->get_CropNRedux();// [0;1]
    fout << fixed << setprecision(2) << "\t" << mcg->get_HeatStressRedux();// [0;1]
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
    fout << fixed << setprecision(1) << "\t" << mcg->get_OrganBiomass(0);
    fout << "\t" << mcg->get_OrganBiomass(1);
    fout << "\t" << mcg->get_OrganBiomass(2);
    fout << "\t" << mcg->get_OrganBiomass(3);
	fout << fixed << setprecision(1) << "\t" << mcg->get_PrimaryCropYield();

    fout << fixed << setprecision(4) << "\t" << mcg->get_GrossPhotosynthesisHaRate(); // [kg CH2O ha-1 d-1]
	fout << fixed << setprecision(2) << "\t" << mcg->get_NetPhotosynthesis();  // [kg CH2O ha-1 d-1]
    fout << fixed << setprecision(4) << "\t" << mcg->get_MaintenanceRespirationAS();// [kg CH2O ha-1]
	fout << fixed << setprecision(4) << "\t" << mcg->get_GrowthRespirationAS();// [kg CH2O ha-1]

    fout << fixed << setprecision(2) << "\t" << mcg->get_StomataResistance();// [s m-1]

    fout << fixed << setprecision(2) << "\t" << mcg->get_CropHeight();// [m]
    fout << fixed << setprecision(2) << "\t" << mcg->get_LeafAreaIndex(); //[m2 m-2]
    fout << fixed << setprecision(0) << "\t" << mcg->get_RootingDepth(); //[layer]
    fout << fixed << setprecision(1) << "\t" << mcg->get_AbovegroundBiomass(); //[kg ha-1]

    fout << fixed << setprecision(1) << "\t" << mcg->get_TotalBiomassNContent();
    fout << fixed << setprecision(2) << "\t" << mcg->get_SumTotalNUptake();
    fout << fixed << setprecision(2) << "\t" << mcg->get_ActNUptake(); // [kg N ha-1]
    fout << fixed << setprecision(2) << "\t" << mcg->get_PotNUptake(); // [kg N ha-1]
    fout << fixed << setprecision(3) << "\t" << mcg->get_TargetNConcentration();//[kg N kg-1]

    fout << fixed << setprecision(3) << "\t" << mcg->get_CriticalNConcentration();//[kg N kg-1]
    fout << fixed << setprecision(3) << "\t" << mcg->get_AbovegroundBiomassNConcentration();//[kg N kg-1]

    fout << fixed << setprecision(5) << "\t" << mcg->get_NetPrimaryProduction(); // NPP, [kg C ha-1]
    for (int i=0; i<mcg->get_NumberOfOrgans(); i++) {
        fout << fixed << setprecision(7) << "\t" << mcg->get_OrganSpecificNPP(i); // NPP organs, [kg C ha-1]
    }
    // if there less than 4 organs we have to fill the column that
    // was added in the output header of rmout; in this header there
    // are statically 4 columns initialised for the organ NPP
    for (int i=mcg->get_NumberOfOrgans(); i<4; i++) {
        fout << fixed << setprecision(2) << "\t0.0"; // NPP organs, [kg C ha-1]
    }

    fout << fixed << setprecision(5) << "\t" << mcg->get_GrossPrimaryProduction(); // GPP, [kg C ha-1]

    fout << fixed << setprecision(5) << "\t" << mcg->get_AutotrophicRespiration(); // Ra, [kg C ha-1]
    for (int i=0; i<mcg->get_NumberOfOrgans(); i++) {
      fout << fixed << setprecision(7) << "\t" << mcg->get_OrganSpecificTotalRespired(i); // Ra organs, [kg C ha-1]
    }
    // if there less than 4 organs we have to fill the column that
    // was added in the output header of rmout; in this header there
    // are statically 4 columns initialised for the organ RA
    for (int i=mcg->get_NumberOfOrgans(); i<4; i++) {
        fout << fixed << setprecision(2) << "\t0.0";
    }

	gout << "\t" << mcg->get_CropName();
    gout << fixed << setprecision(0) << "\t" << mcg->get_DevelopmentalStage()  + 1;
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
    gout << "\t0"; //! @todo
    gout << fixed << setprecision(3) << "\t" << mcg->get_RawProteinConcentration();

  } else { // crop is not planted

    fout << "\t"; // Crop Name
	fout << "\t1.00"; // TranspirationDeficit
    fout << "\t0.00"; // ActualTranspiration
    fout << "\t1.00"; // CropNRedux
    fout << "\t1.00"; // HeatStressRedux
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

    fout << "\t0.0";    // get_OrganBiomass(0)
    fout << "\t0.0";    // get_OrganBiomass(1)
    fout << "\t0.0";    // get_OrganBiomass(2)
    fout << "\t0.0";    // get_OrganBiomass(3)
	fout << "\t0.0";    // get_PrimaryCropYield(3)

    fout << "\t0.000";  // GrossPhotosynthesisHaRate
    fout << "\t0.00";   // NetPhotosynthesis
	fout << "\t0.000";  // MaintenanceRespirationAS
	fout << "\t0.000";  // GrowthRespirationAS
    fout << "\t0.00";   // StomataResistance
    fout << "\t0.00";   // CropHeight
    fout << "\t0.00";   // LeafAreaIndex
    fout << "\t0";      // RootingDepth
    fout << "\t0.0";    // AbovegroundBiomass

    fout << "\t0.0";    // TotalBiomassNContent
    fout << "\t0.00";   // SumTotalNUptake
    fout << "\t0.00";   // ActNUptake
    fout << "\t0.00";   // PotNUptake
    fout << "\t0.000";  // TargetNConcentration

    fout << "\t0.000";  // CriticalNConcentration
    fout << "\t0.000";  // AbovegroundBiomassNConcentration
    fout << "\t0.0"; // NetPrimaryProduction

    fout << "\t0.0"; // NPP root
    fout << "\t0.0"; // NPP leaf
    fout << "\t0.0"; // NPP shoot
    fout << "\t0.0"; // NPP fruit


    fout << "\t0.0"; // GrossPrimaryProduction
    fout << "\t0.0"; // Ra - VcRespiration
    fout << "\t0.0"; // Ra root - OrganSpecificTotalRespired
    fout << "\t0.0"; // Ra leaf - OrganSpecificTotalRespired
    fout << "\t0.0"; // Ra shoot - OrganSpecificTotalRespired
    fout << "\t0.0"; // Ra fruit - OrganSpecificTotalRespired

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
void
Monica::writeGeneralResults(ofstream &fout, ofstream &gout, Env &env, MonicaModel &monica, int d)
{
  const SoilTemperature& mst = monica.soilTemperature();
  const SoilMoisture& msm = monica.soilMoisture();
  const SoilOrganic& mso = monica.soilOrganic();
  const SoilColumn& msc = monica.soilColumn();

  //! TODO: schmutziger work-around. Hier muss was eleganteres hin!
  SoilColumn& msa = monica.soilColumnNC();
  const SoilTransport& msq = monica.soilTransport();

  int outLayers = 20;
  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << fixed << setprecision(3) << "\t" << msm.get_SoilMoisture(i_Layer);
  }
  fout << fixed << setprecision(2) << "\t" << env.da.dataForTimestep(Climate::precip, d);
  fout << fixed << setprecision(1) << "\t" << monica.dailySumIrrigationWater();
  fout << "\t" << msm.get_Infiltration(); // {mm]
  fout << "\t" << msm.get_SurfaceWaterStorage();// {mm]
  fout << "\t" << msm.get_SurfaceRunOff();// {mm]
  fout << "\t" << msm.get_SnowDepth(); // [mm]
  fout << "\t" << msm.get_FrostDepth();
  fout << "\t" << msm.get_ThawDepth();
  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << fixed << setprecision(3) << "\t" << msm.get_SoilMoisture(i_Layer) - msa[i_Layer].get_PermanentWiltingPoint();
  }
  fout << fixed << setprecision(1) << "\t" << mst.get_SoilSurfaceTemperature();


  for(int i_Layer = 0; i_Layer < 5; i_Layer++) {
    fout << fixed << setprecision(1) << "\t" << mst.get_SoilTemperature(i_Layer);// [°C]
  }

  fout << "\t" << msm.get_ActualEvaporation();// [mm]
  fout << "\t" << msm.get_Evapotranspiration();// [mm]
  fout << "\t" << msm.get_ET0();// [mm]
  fout << "\t" << msm.get_KcFactor();
  fout << "\t" << monica.get_AtmosphericCO2Concentration();// [ppm]
  fout << fixed << setprecision(2) << "\t" << monica.get_GroundwaterDepth();// [m]
  fout << fixed << setprecision(1) << "\t" << msm.get_GroundwaterRecharge();// [mm]
  fout << "\t" << msq.get_NLeaching(); // [kg N ha-1]


  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << fixed << setprecision(3) << "\t" << msc.soilLayer(i_Layer).get_SoilNO3();// [kg N m-3]
//    cout << "msc.soilLayer(i_Layer).get_SoilNO3():\t" << msc.soilLayer(i_Layer).get_SoilNO3() << endl;
  }

  fout << fixed << setprecision(4) << "\t" << msc.soilLayer(0).get_SoilCarbamid();

  for(int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << msc.soilLayer(i_Layer).get_SoilNH4();
  }
	for(int i_Layer = 0; i_Layer < 4; i_Layer++) {
		fout << fixed << setprecision(4) << "\t" << msc.soilLayer(i_Layer).get_SoilNO2();
	}
	for(int i_Layer = 0; i_Layer < 6; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << msc.soilLayer(i_Layer).vs_SoilOrganicCarbon();
  }

	// SOC-0-30 [g C m-2]
  double soc_30_accumulator = 0.0;
  for (int i_Layer = 0; i_Layer < 3; i_Layer++) {
      // kg C / kg --> g C / m2
      soc_30_accumulator += msc.soilLayer(i_Layer).vs_SoilOrganicCarbon() * msc.soilLayer(i_Layer).vs_SoilBulkDensity() * msc.soilLayer(i_Layer).vs_LayerThickness * 1000;
  }
  fout << fixed << setprecision(4) << "\t" << soc_30_accumulator ;


  // SOC-0-200   [g C m-2]
	double soc_200_accumulator = 0.0;
	for (int i_Layer = 0; i_Layer < outLayers; i_Layer++) {
	    // kg C / kg --> g C / m2
	    soc_200_accumulator += msc.soilLayer(i_Layer).vs_SoilOrganicCarbon() * msc.soilLayer(i_Layer).vs_SoilBulkDensity() * msc.soilLayer(i_Layer).vs_LayerThickness * 1000;
	}
	fout << fixed << setprecision(4) << "\t" << soc_200_accumulator ;

  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << mso.get_AOM_FastSum(i_Layer) ;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << mso.get_AOM_SlowSum(i_Layer) ;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << mso.get_SMB_Fast(i_Layer) ;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << mso.get_SMB_Slow(i_Layer) ;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << mso.get_SOM_Fast(i_Layer) ;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << mso.get_SOM_Slow(i_Layer) ;
  }
  for(int i_Layer = 0; i_Layer < 1; i_Layer++) {
    fout << fixed << setprecision(4) << "\t" << mso.get_CBalance(i_Layer) ;
  }
  for(int i_Layer = 0; i_Layer < 3; i_Layer++) {
    fout << fixed << setprecision(6) << "\t" << mso.get_NetNMineralisationRate(i_Layer) ; // [kg N ha-1]
  }

  fout << fixed << setprecision(5) << "\t" << mso.get_NetNMineralisation(); // [kg N ha-1]
  fout << fixed << setprecision(5) << "\t" << mso.get_Denitrification(); // [kg N ha-1]
  fout << fixed << setprecision(5) << "\t" << mso.get_N2O_Produced(); // [kg N ha-1]
  fout << fixed << setprecision(1) << "\t" << msc.soilLayer(0).get_SoilpH(); // [ ]
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

  for(int i_Layer = 0; i_Layer < 9; i_Layer++) {
    gout << fixed << setprecision(3) << "\t" << msm.get_SoilMoisture(i_Layer); // [m3 m-3]
  }

  gout << fixed << setprecision(2) << "\t" << (msm.get_SoilMoisture(0) + msm.get_SoilMoisture(1) + msm.get_SoilMoisture(2)) / 3.0; //[m3 m-3]
  gout << fixed << setprecision(2) << "\t" << (msm.get_SoilMoisture(3) + msm.get_SoilMoisture(4) + msm.get_SoilMoisture(5)) / 3.0; //[m3 m-3]
  gout << fixed << setprecision(3) << "\t" << (msm.get_SoilMoisture(6) + msm.get_SoilMoisture(7) + msm.get_SoilMoisture(8)) / 3.0; //[m3 m-3]

  double M0_60 = 0.0;
  for(int i_Layer = 0; i_Layer < 6; i_Layer++) {
    M0_60 += msm.get_SoilMoisture(i_Layer);
  }
  gout << fixed << setprecision(3) << "\t" << (M0_60 / 6.0); // [m3 m-3]

  double M0_90 = 0.0;
  for(int i_Layer = 0; i_Layer < 9; i_Layer++) {
    M0_90 += msm.get_SoilMoisture(i_Layer);
  }
  gout << fixed << setprecision(3) << "\t" << (M0_90 / 9.0); // [m3 m-3]

  double PAW0_200 = 0.0;
  for(int i_Layer = 0; i_Layer < 20; i_Layer++) {
      PAW0_200 += (msm.get_SoilMoisture(i_Layer) - msa[i_Layer].get_PermanentWiltingPoint()) ;
  }
  gout << fixed << setprecision(1) << "\t" << (PAW0_200 * 0.1 * 1000.0); // [mm]

  double PAW0_130 = 0.0;
  for(int i_Layer = 0; i_Layer < 13; i_Layer++) {
      PAW0_130 += (msm.get_SoilMoisture(i_Layer) - msa[i_Layer].get_PermanentWiltingPoint()) ;
  }
  gout << fixed << setprecision(1) << "\t" << (PAW0_130 * 0.1 * 1000.0); // [mm]

    double PAW0_150 = 0.0;
    for(int i_Layer = 0; i_Layer < 15; i_Layer++) {
            PAW0_150 += (msm.get_SoilMoisture(i_Layer) - msa[i_Layer].get_PermanentWiltingPoint()) ;
  }
    gout << fixed << setprecision(1) << "\t" << (PAW0_150 * 0.1 * 1000.0); // [mm]

  gout << fixed << setprecision(2) << "\t" << (msc.soilLayer(0).get_SoilNmin() + msc.soilLayer(1).get_SoilNmin() + msc.soilLayer(2).get_SoilNmin()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
  gout << fixed << setprecision(2) << "\t" << (msc.soilLayer(3).get_SoilNmin() + msc.soilLayer(4).get_SoilNmin() + msc.soilLayer(5).get_SoilNmin()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
  gout << fixed << setprecision(2) << "\t" << (msc.soilLayer(6).get_SoilNmin() + msc.soilLayer(7).get_SoilNmin() + msc.soilLayer(8).get_SoilNmin()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
  gout << fixed << setprecision(2) << "\t" << (msc.soilLayer(9).get_SoilNmin() + msc.soilLayer(10).get_SoilNmin() + msc.soilLayer(11).get_SoilNmin()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]

  double N0_60 = 0.0;
  for(int i_Layer = 0; i_Layer < 6; i_Layer++) {
    N0_60 += msc.soilLayer(i_Layer).get_SoilNmin();
  }
  gout << fixed << setprecision(2) << "\t" << (N0_60 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

  double N0_90 = 0.0;
  for(int i_Layer = 0; i_Layer < 9; i_Layer++) {
    N0_90 += msc.soilLayer(i_Layer).get_SoilNmin();
  }
  gout << fixed << setprecision(2) << "\t" << (N0_90 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

  double N0_200 = 0.0;
  for(int i_Layer = 0; i_Layer < 20; i_Layer++) {
    N0_200 += msc.soilLayer(i_Layer).get_SoilNmin();
  }
  gout << fixed << setprecision(2) << "\t" << (N0_200 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

  double N0_130 = 0.0;
  for(int i_Layer = 0; i_Layer < 13; i_Layer++) {
    N0_130 += msc.soilLayer(i_Layer).get_SoilNmin();
  }
  gout << fixed << setprecision(2) << "\t" << (N0_130 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

  double N0_150 = 0.0;
  for(int i_Layer = 0; i_Layer < 15; i_Layer++) {
    N0_150 += msc.soilLayer(i_Layer).get_SoilNmin();
  }
  gout << fixed << setprecision(2) << "\t" << (N0_150 * 0.1 * 10000);  // [kg m-3] -> [kg ha-1]

  gout << fixed << setprecision(2) << "\t" << (msc.soilLayer(0).get_SoilNH4() + msc.soilLayer(1).get_SoilNH4() + msc.soilLayer(2).get_SoilNH4()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
  gout << fixed << setprecision(2) << "\t" << (msc.soilLayer(3).get_SoilNH4() + msc.soilLayer(4).get_SoilNH4() + msc.soilLayer(5).get_SoilNH4()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
  gout << fixed << setprecision(2) << "\t" << (msc.soilLayer(6).get_SoilNH4() + msc.soilLayer(7).get_SoilNH4() + msc.soilLayer(8).get_SoilNH4()) / 3.0 * 0.3 * 10000; // [kg m-3] -> [kg ha-1]
  gout << fixed << setprecision(2) << "\t" << mso.get_SoilOrganicC(0) * 0.1 * 10000;// [kg m-3] -> [kg ha-1]
  gout << fixed << setprecision(2) << "\t" << ((mso.get_SoilOrganicC(0) + mso.get_SoilOrganicC(1) + mso.get_SoilOrganicC(2)) / 3.0 * 0.3 * 10000); // [kg m-3] -> [kg ha-1]
  gout << fixed << setprecision(1) << "\t" << mst.get_SoilTemperature(0);
  gout << fixed << setprecision(1) << "\t" << mst.get_SoilTemperature(2);
  gout << fixed << setprecision(1) << "\t" << mst.get_SoilTemperature(5);
  gout << fixed << setprecision(2) << "\t" << mso.get_DecomposerRespiration()* 10000.0; // Rh, [kg C ha-1 d-1]

  gout << fixed << setprecision(3) << "\t" << mso.get_NH3_Volatilised() * 10000; // [kg N ha-1]
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
  parameter_output_file.open((path + "/monica_parameters.txt").c_str());
  if (parameter_output_file.fail()){
      debug() << "Could not write file\"" << (path + "/monica_parameters.txt").c_str() << "\"" << endl;
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


