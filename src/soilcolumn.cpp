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

#include <cmath>
#include <algorithm>
#include <assert.h>

/**
 * @file soilcolumn.cpp
 *
 * @brief This file contains the definition of classes AOM_Properties,  SoilLayer, SoilColumn
 * and FertilizerTriggerThunk.
 *
 * @see Monica::AOM_Properties
 * @see Monica::SoilLayer
 * @see Monica::SoilColumn
 * @see Monica::FertilizerTriggerThunk
 */

#include "crop.h"
#include "soilcolumn.h"
#include "tools/debug.h"
#include "soil/constants.h"

using namespace Monica;
using namespace std;
using namespace Soil;
using namespace Tools;

/**
 * Constructor with default parameter initialization
 */
AOM_Properties::AOM_Properties()
: vo_AOM_Slow(0.0),
vo_AOM_Fast(0.0),
vo_AOM_SlowDecRate_to_SMB_Slow(0.0),
vo_AOM_SlowDecRate_to_SMB_Fast(0.0),
vo_AOM_FastDecRate_to_SMB_Slow(0.0),
vo_AOM_FastDecRate_to_SMB_Fast(0.0),
vo_AOM_SlowDecCoeff(0.0),
vo_AOM_FastDecCoeff(0.0),
vo_AOM_SlowDecCoeffStandard(1.0),
vo_AOM_FastDecCoeffStandard(1.0),
vo_PartAOM_Slow_to_SMB_Slow(0.0),
vo_PartAOM_Slow_to_SMB_Fast(0.0),
vo_CN_Ratio_AOM_Slow(1.0),
vo_CN_Ratio_AOM_Fast(1.0),
vo_DaysAfterApplication(0),
vo_AOM_DryMatterContent(0.0),
vo_AOM_NH4Content(0.0),
vo_AOM_SlowDelta(0.0),
vo_AOM_FastDelta(0.0) {
 }

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/**
 * Default constructor with parameter initialization.
 */
SoilLayer::SoilLayer()
: vs_SoilSandContent(0.90),
vs_SoilClayContent(0.05),
vs_SoilStoneContent(0),
vs_SoilTexture("Ss"),
vs_SoilpH(7),
vs_SoilMoistureOld_m3(0.25),
vs_SoilWaterFlux(0),
vs_Lambda(0.5),
vs_FieldCapacity(0.21),
vs_Saturation(0.43),
vs_PermanentWiltingPoint(0.08),
vs_SOM_Slow(0),
vs_SOM_Fast(0),
vs_SMB_Slow(0),
vs_SMB_Fast(0),
vs_SoilCarbamid(0),
vs_SoilNH4(0.0001),
vs_SoilNO2(0.001),
vs_SoilNO3(0.001),
vs_SoilFrozen(false),
_vs_SoilOrganicCarbon(-1.0),
_vs_SoilOrganicMatter(-1.0),
_vs_SoilBulkDensity(0),
_vs_SoilMoisture_pF(-1),
vs_SoilMoisture_m3(0.25),
vs_SoilTemperature(0)
{

  vs_SoilMoisture_m3 = vs_FieldCapacity * centralParameterProvider.userInitValues.p_initPercentageFC;
	vs_SoilMoistureOld_m3 = vs_FieldCapacity * centralParameterProvider.userInitValues.p_initPercentageFC;
	vs_SoilNO3 = centralParameterProvider.userInitValues.p_initSoilNitrate;
	vs_SoilNH4 = centralParameterProvider.userInitValues.p_initSoilAmmonium;

//	cout  << "Constructor 1" << endl;
//  cout << "vs_SoilMoisture_m3\t" << vs_SoilMoisture_m3  << "\t" << centralParameterProvider.userInitValues.p_initPercentageFC << endl;
//  cout << "vs_SoilNO3\t" << vs_SoilNO3 << endl;
//  cout << "vs_SoilNH4\t" << vs_SoilNH4 << endl;
}

/**
 * Default constructor with parameter initialization.
 */
SoilLayer::SoilLayer(const CentralParameterProvider& cpp)
: vs_SoilSandContent(0.90),
vs_SoilClayContent(0.05),
vs_SoilStoneContent(0),
vs_SoilTexture("Ss"),
vs_SoilpH(7),
vs_SoilMoistureOld_m3(0.25),
vs_SoilWaterFlux(0),
vs_Lambda(0.5),
vs_FieldCapacity(0.21),
vs_Saturation(0.43),
vs_PermanentWiltingPoint(0.08),
vs_SOM_Slow(0),
vs_SOM_Fast(0),
vs_SMB_Slow(0),
vs_SMB_Fast(0),
vs_SoilCarbamid(0),
vs_SoilNH4(0.0001),
vs_SoilNO2(0.001),
vs_SoilNO3(0.0001),
vs_SoilFrozen(false),
centralParameterProvider(cpp),
_vs_SoilOrganicCarbon(-1.0),
_vs_SoilOrganicMatter(-1.0),
_vs_SoilBulkDensity(0),
_vs_SoilMoisture_pF(-1),
vs_SoilMoisture_m3(0.25),
vs_SoilTemperature(0)
{
  vs_SoilMoisture_m3 = vs_FieldCapacity * centralParameterProvider.userInitValues.p_initPercentageFC;
  vs_SoilMoistureOld_m3 = vs_FieldCapacity * centralParameterProvider.userInitValues.p_initPercentageFC;
  vs_SoilNO3 = centralParameterProvider.userInitValues.p_initSoilNitrate;
  vs_SoilNH4 = centralParameterProvider.userInitValues.p_initSoilAmmonium;

//  cout  << "Constructor 2" << endl;
//  cout << "vs_SoilMoisture_m3\t" << vs_SoilMoisture_m3  << "\t" << centralParameterProvider.userInitValues.p_initPercentageFC << endl;
//  cout << "vs_SoilNO3\t" << vs_SoilNO3 << endl;
//  cout << "vs_SoilNH4\t" << vs_SoilNH4 << endl;
}

/**
 * Constructor
 * @param vs_LayerThickness Vertical expansion
 * @param sps Soil parameters
 */
SoilLayer::SoilLayer(double vs_LayerThickness, const SoilParameters& sps, const CentralParameterProvider& cpp)
: vs_LayerThickness(vs_LayerThickness),
vs_SoilSandContent(sps.vs_SoilSandContent),
vs_SoilClayContent(sps.vs_SoilClayContent),
vs_SoilStoneContent(sps.vs_SoilStoneContent),
vs_SoilTexture(sps.vs_SoilTexture),
vs_SoilpH(sps.vs_SoilpH),
vs_SoilMoistureOld_m3(0.25), // QUESTION - Warum wird hier mit 0.25 initialisiert?
vs_SoilWaterFlux(0),
vs_Lambda(sps.vs_Lambda),
vs_FieldCapacity(sps.vs_FieldCapacity),
vs_Saturation(sps.vs_Saturation),
vs_PermanentWiltingPoint(sps.vs_PermanentWiltingPoint),
vs_SOM_Slow(0),
vs_SOM_Fast(0),
vs_SMB_Slow(0),
vs_SMB_Fast(0),
vs_SoilCarbamid(0),
vs_SoilNH4(0.0001),
vs_SoilNO2(0.001),
vs_SoilNO3(0.005),
vs_SoilFrozen(false),
centralParameterProvider(cpp),
_vs_SoilOrganicCarbon(sps.vs_SoilOrganicCarbon()),
_vs_SoilOrganicMatter(sps.vs_SoilOrganicMatter()),
_vs_SoilBulkDensity(sps.vs_SoilBulkDensity()),
_vs_SoilMoisture_pF(0),
vs_SoilMoisture_m3(0.25), // QUESTION - Warum wird hier mit 0.25 initialisiert?
vs_SoilTemperature(0)
{
  assert((_vs_SoilOrganicCarbon - (_vs_SoilOrganicMatter * OrganicConstants::po_SOM_to_C)) < 0.00001);

  vs_SoilMoisture_m3 = vs_FieldCapacity * cpp.userInitValues.p_initPercentageFC;
  vs_SoilMoistureOld_m3 = vs_FieldCapacity * cpp.userInitValues.p_initPercentageFC;

  if (sps.vs_SoilAmmonium < 0.0) {
      vs_SoilNH4 = cpp.userInitValues.p_initSoilAmmonium;
  } else {
      vs_SoilNH4 = sps.vs_SoilAmmonium; // kg m-3
  }
  if (sps.vs_SoilNitrate < 0.0) {
      vs_SoilNO3 = cpp.userInitValues.p_initSoilNitrate;
  } else {
      vs_SoilNO3 = sps.vs_SoilNitrate;  // kg m-3
  }


  //cout  << "Constructor 3" << endl;
  //cout << "vs_SoilMoisture_m3\t" << vs_SoilMoisture_m3  << "\t" << cpp.userInitValues.p_initPercentageFC << endl;
  //cout << "vs_SoilNO3\t" << vs_SoilNO3 << endl;
  //cout << "vs_SoilNH4\t" << vs_SoilNH4 << endl;
 }

/**
 * @brief Copyconstructor
 * @param sl
 * @return
 */
SoilLayer::SoilLayer(const SoilLayer &sl)
{
  vs_LayerThickness = sl.vs_LayerThickness;
  vs_SoilSandContent = sl.vs_SoilSandContent;
  vs_SoilClayContent = sl.vs_SoilClayContent;
  vs_SoilStoneContent = sl.vs_SoilStoneContent;
  vs_SoilTexture = sl.vs_SoilTexture;

  vs_SoilpH = sl.vs_SoilpH;

  vs_SoilTemperature = sl.vs_SoilTemperature;
  vs_SoilMoisture_m3 = sl.vs_SoilMoisture_m3;
  vs_SoilMoistureOld_m3 = sl.vs_SoilMoistureOld_m3;
  vs_SoilWaterFlux = sl.vs_SoilWaterFlux;
  vs_Lambda = sl.vs_Lambda;
  vs_FieldCapacity = sl.vs_FieldCapacity;
  vs_Saturation = sl.vs_Saturation;
  vs_PermanentWiltingPoint = sl.vs_PermanentWiltingPoint;

  vo_AOM_Pool = sl.vo_AOM_Pool;

  vs_SOM_Slow = sl.vs_SOM_Slow;
  vs_SOM_Fast = sl.vs_SOM_Fast;
  vs_SMB_Slow = sl.vs_SMB_Slow;
  vs_SMB_Fast = sl.vs_SMB_Fast;

  vs_SoilCarbamid = sl.vs_SoilCarbamid;
  vs_SoilNH4 = sl.vs_SoilNH4;
  vs_SoilNO2 = sl.vs_SoilNO2;
  vs_SoilNO3 = sl.vs_SoilNO3;
  vs_SoilFrozen = sl.vs_SoilFrozen;

  centralParameterProvider = sl.centralParameterProvider;

  _vs_SoilOrganicCarbon = sl.vs_SoilOrganicCarbon();
  _vs_SoilOrganicMatter = sl.vs_SoilOrganicMatter();

  _vs_SoilBulkDensity = sl._vs_SoilBulkDensity;
  _vs_SoilMoisture_pF = sl._vs_SoilMoisture_pF;

}

/**
 * @brief Returns value for soil organic carbon.
 *
 * If value for soil organic matter is not defined, because DB does not
 * contain the according value, than the store value for organic carbon
 * is returned. If the soil organic matter parameter is defined,
 * than the value for soil organic carbon is calculated depending on
 * the soil organic matter.
 *
 * @return Value for soil organic carbon
 */
double SoilLayer::vs_SoilOrganicCarbon() const {
  // if soil organic carbon is not defined, than calculate from soil organic
  // matter value [kg C kg-1]
  if(_vs_SoilOrganicCarbon >= 0.0) {
    return _vs_SoilOrganicCarbon;
  }


  // calculate soil organic carbon with soil organic matter parameter
	return _vs_SoilOrganicMatter * OrganicConstants::po_SOM_to_C;
}

/**
 * @brief Returns value for soil organic matter.
 *
 * If the value for soil organic carbon is not defined, because the DB does
 * not contain any value, than the stored value for organic matter
 * is returned. If the soil organic carbon parameter is defined,
 * than the value for soil organic matter is calculated depending on
 * the soil organic carbon.
 *
 * @return Value for soil organic matter
 * */
double SoilLayer::vs_SoilOrganicMatter() const {
  // if soil organic matter is not defined, calculate from soil organic C
  if(_vs_SoilOrganicMatter >= 0.0) {
    return _vs_SoilOrganicMatter;
  }

  // ansonsten berechne den Wert aus dem C-Gehalt
	return (_vs_SoilOrganicCarbon / OrganicConstants::po_SOM_to_C); //[kg C kg-1]
}

/**
 * @brief Returns fraction of silt content of the layer.
 *
 * Calculates the silt particle size fraction in the layer in dependence
 * of its sand and clay content.
 *
 * @return Fraction of silt in the layer.
 */
double SoilLayer::vs_SoilSiltContent() const {
  return (1 - vs_SoilSandContent - vs_SoilClayContent);
}

/**
 * Soil layer's moisture content, expressed as logarithm of
 * pressure head in cm water column. Algorithm of Van Genuchten is used.
 * Conversion of water saturation into soil-moisture tension.
 *
 * @todo Einheiten prüfen
 */
void SoilLayer::calc_vs_SoilMoisture_pF() {
  /** Derivation of Van Genuchten parameters (Vereecken at al. 1989) */
  //TODO Einheiten prüfen
  double vs_ThetaR;
  double vs_ThetaS;

  if (vs_PermanentWiltingPoint > 0.0){
    vs_ThetaR = vs_PermanentWiltingPoint;
  } else {
    vs_ThetaR = get_PermanentWiltingPoint();
  }

  if (vs_Saturation > 0.0){
    vs_ThetaS = vs_Saturation;
  } else {
    vs_ThetaS = get_Saturation();
  }

  double vs_VanGenuchtenAlpha = exp(-2.486 + (2.5 * vs_SoilSandContent)
                                    - (35.1 * vs_SoilOrganicCarbon())
                                    - (2.617 * (vs_SoilBulkDensity() / 1000.0))
			      - (2.3 * vs_SoilClayContent));

  double vs_VanGenuchtenM = 1.0;

  double vs_VanGenuchtenN = exp(0.053
                                - (0.9 * vs_SoilSandContent)
                                - (1.3 * vs_SoilClayContent)
																+ (1.5 * (pow(vs_SoilSandContent, 2.0)))); 

  /** Van Genuchten retention curve */
  double vs_MatricHead;

  if(get_Vs_SoilMoisture_m3() <= vs_ThetaR) {
    vs_MatricHead = 5.0E+7;
    //else  d_MatricHead = (1.0 / vo_VanGenuchtenAlpha) * (pow(((1 / (pow(((d_SoilMoisture_m3 - d_ThetaR) /
     //                     (d_ThetaS - d_ThetaR)), (1 / vo_VanGenuchtenM)))) - 1), (1 / vo_VanGenuchtenN)));
  }   else {
    vs_MatricHead = (1.0 / vs_VanGenuchtenAlpha)
      * (pow(
          (
              (pow(
                    (
                      (vs_ThetaS - vs_ThetaR) / (get_Vs_SoilMoisture_m3() - vs_ThetaR)
                    ),
                    (
                       1 / vs_VanGenuchtenM
                    )
                  )
              )
              - 1
           ),
           (1 / vs_VanGenuchtenN)
           )
      );
  }

	//  debug() << "get_Vs_SoilMoisture_m3: " << get_Vs_SoilMoisture_m3() << std::endl;
	//  debug() << "vs_SoilSandContent: " << vs_SoilSandContent << std::endl;
	//  debug() << "vs_SoilOrganicCarbon: " << vs_SoilOrganicCarbon() << std::endl;
	//  debug() << "vs_SoilBulkDensity: " << vs_SoilBulkDensity() << std::endl;
	//  debug() << "that.vs_SoilClayContent: " << vs_SoilClayContent << std::endl;
	//  debug() << "vs_ThetaR: " << vs_ThetaR << std::endl;
	//  debug() << "vs_ThetaS: " << vs_ThetaS << std::endl;
	//  debug() << "vs_VanGenuchtenAlpha: " << vs_VanGenuchtenAlpha << std::endl;
	//  debug() << "vs_VanGenuchtenM: " << vs_VanGenuchtenM << std::endl;
	//  debug() << "vs_VanGenuchtenN: " << vs_VanGenuchtenN << std::endl;
	//  debug() << "vs_MatricHead: " << vs_MatricHead << std::endl;

  _vs_SoilMoisture_pF = log10(vs_MatricHead);

	/* JV! set _vs_SoilMoisture_pF to "small" number in case of vs_Theta "close" to vs_ThetaS (vs_Psi < 1 -> log(vs_Psi) < 0) */
	_vs_SoilMoisture_pF = (_vs_SoilMoisture_pF < 0.0) ? 5.0E-7 : _vs_SoilMoisture_pF;
	//  debug() << "_vs_SoilMoisture_pF: " << _vs_SoilMoisture_pF << std::endl;
}

/**
 * Soil layer's water content at field capacity (1.8 < pF < 2.1) [m3 m-3]
 *
 * This method applies only in the case when soil charcteristics have not
 * been set before.
 *
 * In german: "Maximaler Wassergehalt, der gegen die Wirkung der
 * Schwerkraft zurückgehalten wird"
 *
 * @todo Einheiten prüfen
 */
double SoilLayer::get_FieldCapacity()
{
  // Sensitivity analysis case
  if (centralParameterProvider.sensitivityAnalysisParameters.vs_FieldCapacity != UNDEFINED) {
    return centralParameterProvider.sensitivityAnalysisParameters.vs_FieldCapacity;
  }

  //***** Derivation of Van Genuchten parameters (Vereecken at al. 1989) *****
  if (vs_SoilTexture == "") {
//    cout << "Field capacity is calculated from van Genuchten parameters" << endl;
    double vs_ThetaR;
    double vs_ThetaS;

    if (vs_PermanentWiltingPoint > 0.0){
      vs_ThetaR = vs_PermanentWiltingPoint;
    } else {
      vs_ThetaR = get_PermanentWiltingPoint();
    }

    if (vs_Saturation > 0.0){
      vs_ThetaS = vs_Saturation;
    } else {
      vs_ThetaS = get_Saturation();
    }

    double vs_VanGenuchtenAlpha = exp(-2.486
			        + 2.5 * vs_SoilSandContent
			        - 35.1 * vs_SoilOrganicCarbon()
			        - 2.617 * (vs_SoilBulkDensity() / 1000.0)
			        - 2.3 * vs_SoilClayContent);

    double vs_VanGenuchtenM = 1.0;

    double vs_VanGenuchtenN = exp(0.053
			    - 0.9 * vs_SoilSandContent
			    - 1.3 * vs_SoilClayContent
					+ 1.5 * (pow(vs_SoilSandContent, 2.0))); 

    //***** Van Genuchten retention curve to calculate volumetric water content at
    //***** moisture equivalent (Field capacity definition KA5)

    double vs_FieldCapacity_pF = 2.1;
    if ((vs_SoilSandContent > 0.48) && (vs_SoilSandContent <= 0.9) && (vs_SoilClayContent <= 0.12))
      vs_FieldCapacity_pF = 2.1 - (0.476 * (vs_SoilSandContent - 0.48));
    else if ((vs_SoilSandContent > 0.9) && (vs_SoilClayContent <= 0.05))
      vs_FieldCapacity_pF = 1.9;
    else if (vs_SoilClayContent > 0.45)
      vs_FieldCapacity_pF = 2.5;
    else if ((vs_SoilClayContent > 0.30) && (vs_SoilSandContent < 0.2))
      vs_FieldCapacity_pF = 2.4;
    else if (vs_SoilClayContent > 0.35)
      vs_FieldCapacity_pF = 2.3;
    else if ((vs_SoilClayContent > 0.25) && (vs_SoilSandContent < 0.1))
      vs_FieldCapacity_pF = 2.3;
    else if ((vs_SoilClayContent > 0.17) && (vs_SoilSandContent > 0.68))
      vs_FieldCapacity_pF = 2.2;
    else if ((vs_SoilClayContent > 0.17) && (vs_SoilSandContent < 0.33))
      vs_FieldCapacity_pF = 2.2;
    else if ((vs_SoilClayContent > 0.08) && (vs_SoilSandContent < 0.27))
      vs_FieldCapacity_pF = 2.2;
    else if ((vs_SoilClayContent > 0.25) && (vs_SoilSandContent < 0.25))
      vs_FieldCapacity_pF = 2.2;

    double vs_MatricHead = pow(10, vs_FieldCapacity_pF);

    vs_FieldCapacity = vs_ThetaR + ((vs_ThetaS - vs_ThetaR) /
			      (pow((1.0 + pow((vs_VanGenuchtenAlpha * vs_MatricHead),
					  vs_VanGenuchtenN)), vs_VanGenuchtenM)));

    vs_FieldCapacity *= (1.0 - vs_SoilStoneContent);
  }

  return vs_FieldCapacity;

}

/**
 * Soil layer's water content at full saturation (pF=0.0) [m3 m-3].
 * Uses empiric calculation of Van Genuchten. *
 *
 * In german:  Wassergehalt bei maximaler Füllung des Poren-Raums
 *
 * @return Water content at full saturation
 */
double SoilLayer::get_Saturation()
{
  // Sensitivity analysis case
  if (centralParameterProvider.sensitivityAnalysisParameters.vs_Saturation != UNDEFINED) {
    return centralParameterProvider.sensitivityAnalysisParameters.vs_Saturation;
  }

  if (vs_SoilTexture == "") {
//    cout << "Pore Volume is calculated from van Genuchten parameters" << endl;
    vs_Saturation = 0.81 - 0.283 * (vs_SoilBulkDensity() / 1000.0) + 0.1 * vs_SoilClayContent;

    vs_Saturation *= (1.0 - vs_SoilStoneContent);
  }
  return vs_Saturation;
}

/**
 * Soil layer's water content at permanent wilting point (pF=4.2) [m3 m-3].
 * Uses empiric calculation of Van Genuchten.
 *
 * In german: Wassergehalt des Bodens am permanenten Welkepunkt.
 *
 * @return Water content at permanent wilting point
 */
double SoilLayer::get_PermanentWiltingPoint()
{
  // Sensitivity analysis case
  if (centralParameterProvider.sensitivityAnalysisParameters.vs_PermanentWiltingPoint != UNDEFINED) {
    return centralParameterProvider.sensitivityAnalysisParameters.vs_PermanentWiltingPoint;
  }

  if (vs_SoilTexture == "") {
//    cout << "Permanent Wilting Point is calculated from van Genuchten parameters" << endl;
    vs_PermanentWiltingPoint = 0.015 + 0.5 * vs_SoilClayContent + 1.4 * vs_SoilOrganicCarbon();

    vs_PermanentWiltingPoint *= (1.0 - vs_SoilStoneContent);
  }

  return vs_PermanentWiltingPoint;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/**
 * @brief Constructor
 *
 * Constructor with parameter initialization. Initializes every layer
 * in vector with the layer-thickness and special soil parameter in this layer.
 *
 * @param gps General Parameters
 * @param soilParams Soil Parameter
 */
SoilColumn::SoilColumn(const GeneralParameters& gps,
                       const SoilPMs& soilParams, const CentralParameterProvider& cpp)
:
vs_SurfaceWaterStorage(0.0),
vs_InterceptionStorage(0.0),
vm_GroundwaterTable(0),
vs_FluxAtLowerBoundary(0.0),
vq_CropNUptake(0.0),
vt_SoilSurfaceTemperature(0.0),
vm_SnowDepth(0.0),
generalParams(gps),
soilParams(soilParams),
_vf_TopDressing(0.0),
_vf_TopDressingDelay(0),
cropGrowth(NULL),
centralParameterProvider(cpp)
{
  debug() << "Constructor: SoilColumn "  << soilParams.size() << endl;
  for(unsigned int i = 0; i < soilParams.size(); i++) {
    vs_SoilLayers.push_back(SoilLayer(gps.ps_LayerThickness.front(), soilParams.at(i), cpp));
  }

  set_vs_NumberOfOrganicLayers();
}



/**
 * @brief Calculates number of organic layers.
 *
 * Calculates number of organic layers in in in dependency on
 * the layer depth and the ps_MaxMineralisationDepth. Result is saved
 * in private member variable _vs_NumberOfOrganicLayers.
 */
void SoilColumn::set_vs_NumberOfOrganicLayers() {
	//std::cout << "--------- set_vs_NumberOfOrganicLayers -----------" << std::endl;
  double lsum = 0;
  int count = 0;
  for(int i = 0; i < vs_NumberOfLayers(); i++) {
		//std::cout << vs_SoilLayers[i].vs_LayerThickness << std::endl;
    count++;
    lsum += vs_SoilLayers[i].vs_LayerThickness;

    if(lsum >= generalParams.ps_MaxMineralisationDepth)
      break;
  }
  _vs_NumberOfOrganicLayers = count;

	//std::cout << vs_NumberOfLayers() << std::endl;
	//std::cout << generalParams.ps_MaxMineralisationDepth << std::endl;
	//std::cout << _vs_NumberOfOrganicLayers << std::endl;
}

/**
 * Method for calculating fertilizer demand from crop demand and soil mineral
 * status (Nmin method).
 *
 * @param fp
 * @param vf_SamplingDepth
 * @param vf_CropNTarget N availability required by the crop down to rooting depth
 * @param vf_CropNTarget30 N availability required by the crop down to 30 cm
 * @param vf_FertiliserMaxApplication Maximal value of N that can be applied until the crop will be damaged
 * @param vf_FertiliserMinApplication Threshold value for economically reasonable fertilizer application
 * @param vf_TopDressingDelay Number of days for which the application of surplus fertilizer is delayed
 */
double SoilColumn::
applyMineralFertiliserViaNMinMethod(MineralFertiliserParameters fp,
                                    double vf_SamplingDepth,
                                    double vf_CropNTarget,
                                    double vf_CropNTarget30,
                                    double vf_FertiliserMinApplication,
			      double vf_FertiliserMaxApplication,
			      int vf_TopDressingDelay ) {

  // Wassergehalt > Feldkapazität
  if(soilLayer(0).get_Vs_SoilMoisture_m3() > soilLayer(0).get_FieldCapacity())
  {
    _delayedNMinApplications.push_back
        ([=](){ return this->applyMineralFertiliserViaNMinMethod(fp, vf_SamplingDepth, vf_CropNTarget,
                                                                 vf_CropNTarget30, vf_FertiliserMinApplication,
                                                                 vf_FertiliserMaxApplication, vf_TopDressingDelay); });

    //cerr << "Soil too wet for fertilisation. "
    //  "Fertiliser event adjourned to next day." << endl;
    return 0.0;
  }

  double vf_SoilNO3Sum = 0.0;
  double vf_SoilNO3Sum30 = 0.0;
  double vf_SoilNH4Sum = 0.0;
  double vf_SoilNH4Sum30 = 0.0;
  int vf_Layer30cm = getLayerNumberForDepth(0.3);

  for(int i_Layer = 0;
      i_Layer < (ceil(vf_SamplingDepth / soilLayer(i_Layer).vs_LayerThickness));
      i_Layer++) {
    //vf_TargetLayer is in cm. We want number of layers
    vf_SoilNO3Sum += soilLayer(i_Layer).vs_SoilNO3; //! [kg N m-3]
    vf_SoilNH4Sum += soilLayer(i_Layer).vs_SoilNH4; //! [kg N m-3]
  }

  // Same calculation for a depth of 30 cm
  /** @todo Must be adapted when using variable layer depth. */
  for(int i_Layer = 0; i_Layer < vf_Layer30cm; i_Layer++) {
    vf_SoilNO3Sum30 += soilLayer(i_Layer).vs_SoilNO3; //! [kg N m-3]
    vf_SoilNH4Sum30 += soilLayer(i_Layer).vs_SoilNH4; //! [kg N m-3]
  }

  // Converts [kg N ha-1] to [kg N m-3]
  double vf_CropNTargetValue;
  vf_CropNTargetValue = vf_CropNTarget / 10000.0 / soilLayer(0).vs_LayerThickness;

  // Converts [kg N ha-1] to [kg N m-3]
  double vf_CropNTargetValue30;
  vf_CropNTargetValue30 = vf_CropNTarget30 / 10000.0 / soilLayer(0).vs_LayerThickness;

  double vf_FertiliserDemandVol = vf_CropNTargetValue - (vf_SoilNO3Sum + vf_SoilNH4Sum);
  double vf_FertiliserDemandVol30 = vf_CropNTargetValue30 - (vf_SoilNO3Sum30 + vf_SoilNH4Sum30);

  // Converts fertiliser demand back from [kg N m-3] to [kg N ha-1]
  double vf_FertiliserDemand = vf_FertiliserDemandVol * 10000.0 * soilLayer(0).vs_LayerThickness;
  double vf_FertiliserDemand30 = vf_FertiliserDemandVol30 * 10000.0 * soilLayer(0).vs_LayerThickness;

  double vf_FertiliserRecommendation = max(vf_FertiliserDemand, vf_FertiliserDemand30);

  if(vf_FertiliserRecommendation < vf_FertiliserMinApplication) {

    // If the N demand of the crop is smaller than the user defined
    // minimum fertilisation then no need to fertilise
    vf_FertiliserRecommendation = 0.0;
    //cerr << "Fertiliser demand below minimum application value. No fertiliser applied." << endl;
  }

  if(vf_FertiliserRecommendation > vf_FertiliserMaxApplication) {

    // If the N demand of the crop is greater than the user defined
    // maximum fertilisation then need to split so surplus fertilizer can
    // be applied after a delay time
    _vf_TopDressing = vf_FertiliserRecommendation - vf_FertiliserMaxApplication;
    _vf_TopDressingPartition = fp;
    _vf_TopDressingDelay = vf_TopDressingDelay;
    vf_FertiliserRecommendation = vf_FertiliserMaxApplication;

    //cerr << "Fertiliser demand above maximum application value. "
    //  "A top dressing of " << _vf_TopDressing <<
    //  " will be applied from now on day" << vf_TopDressingDelay << "." << endl;
  }

  //Apply fertiliser
  applyMineralFertiliser(fp, vf_FertiliserRecommendation);

  debug() << "SoilColumn::applyMineralFertiliserViaNMinMethod:\t" << vf_FertiliserRecommendation << endl;

  //apply the callback to all of the fertiliser, even though some if it
  //(the top-dressing) will only be applied later
  //we simply assume it really will be applied, in the worst case
  //the delay is so long, that the crop is already harvested until
  //the top-dressing will be applied
   return vf_FertiliserRecommendation;// + _vf_TopDressing);
}

// prüft ob top-dressing angewendet werden sollte, ansonsten wird
// zeitspanne nur reduziert

/**
 * Tests for every calculation step if a delayed fertilising should be applied.
 * If not, the delay time will be decremented. Otherwise the surplus fertiliser
 * stored in _vf_TopDressing is applied.
 *
 * @see ApplyFertiliser
 */
double SoilColumn::applyPossibleTopDressing() {
  // do nothing if there is no active delay time
  if(_vf_TopDressingDelay > 0) {

    // if there is a delay time, decrement this value for this time step
    _vf_TopDressingDelay--;

    // test if now is the correct time for applying top dressing
    if(_vf_TopDressingDelay == 0) {
      double amount = _vf_TopDressing;
      applyMineralFertiliser(_vf_TopDressingPartition, amount);
      _vf_TopDressing = 0;
      return amount;

    }
  }
  return 0.0;
}

/**
 * Calls function for applying delayed fertilizer and
 * then removes the first fertilizer item in list.
 */
double SoilColumn::applyPossibleDelayedFerilizer() {
	list<std::function<double()> > delayedApps = _delayedNMinApplications;
	double n_amount = 0.0;
	while(!delayedApps.empty()) {
    n_amount += delayedApps.front()();
    delayedApps.pop_front();
		_delayedNMinApplications.pop_front();
  }
	return n_amount;
}

/**
 * @brief Applies mineral fertiliser
 *
 * @author: Claas Nendel
 */
void SoilColumn::applyMineralFertiliser(MineralFertiliserParameters fp,
                                        double amount) {
	debug() << "SoilColumn::applyMineralFertilser: params: " << fp.toString()
	<< " amount: " << amount << endl;
  // [kg N ha-1 -> kg m-3]
  soilLayer(0).vs_SoilNO3 += amount * fp.getNO3() / 10000.0 / soilLayer(0).vs_LayerThickness;
  soilLayer(0).vs_SoilNH4 += amount * fp.getNH4() / 10000.0 / soilLayer(0).vs_LayerThickness;
  soilLayer(0).vs_SoilCarbamid += amount * fp.getCarbamid() / 10000.0 / soilLayer(0).vs_LayerThickness;
}


/**
 * @brief Checks and deletes AOM pool
 *
 * This method checks the content of each AOM Pool. In case the sum over all
 * layers of a respective pool is very low the pool will be deleted from the
 * list.
 *
 * @author: Claas Nendel
 */
void SoilColumn::deleteAOMPool() {

  for(unsigned int i_AOMPool = 0; i_AOMPool < soilLayer(0).vo_AOM_Pool.size();){

    double vo_SumAOM_Slow = 0.0;
    double vo_SumAOM_Fast = 0.0;

    for(int i_Layer = 0; i_Layer < _vs_NumberOfOrganicLayers; i_Layer++){

      vo_SumAOM_Slow += soilLayer(i_Layer).vo_AOM_Pool.at(i_AOMPool).vo_AOM_Slow;
      vo_SumAOM_Fast += soilLayer(i_Layer).vo_AOM_Pool.at(i_AOMPool).vo_AOM_Fast;

    }

    //cout << "Pool " << i_AOMPool << " -> Slow: " << vo_SumAOM_Slow << "; Fast: " << vo_SumAOM_Fast << endl;

    if ((vo_SumAOM_Slow + vo_SumAOM_Fast) < 0.00001){
      for(int i_Layer = 0; i_Layer < _vs_NumberOfOrganicLayers; i_Layer++){
        vector<AOM_Properties>::iterator it_AOMPool = soilLayer(i_Layer).vo_AOM_Pool.begin();
        it_AOMPool += i_AOMPool;

        soilLayer(i_Layer).vo_AOM_Pool.erase(it_AOMPool);
      }
      //cout << "Habe Pool " << i_AOMPool << " gelöscht" << endl;
    } else {
			i_AOMPool++;
		}
  }
}

/**
 * Method for calculating irrigation demand from soil moisture status.
 * The trigger will be activated and deactivated according to crop parameters
 * (temperature sum)
 *
 * @param vi_IrrigationThreshold
 * @return could irrigation be applied
 */
bool SoilColumn::
applyIrrigationViaTrigger(double vi_IrrigationThreshold,
                          double vi_IrrigationAmount,
                          double vi_IrrigationNConcentration) {


  //is actually only called from cropStep and thus there should always
  //be a crop
  assert(cropGrowth != NULL);

  double s = cropGrowth->get_HeatSumIrrigationStart();
  double e = cropGrowth->get_HeatSumIrrigationEnd();
  double cts = cropGrowth->get_CurrentTemperatureSum();

  if (cts < s || cts > e) return false;

  double vi_CriticalMoistureDepth = centralParameterProvider.userSoilMoistureParameters.pm_CriticalMoistureDepth;

  // Initialisation
  double vi_ActualPlantAvailableWater = 0.0;
  double vi_MaxPlantAvailableWater = 0.0;
  double vi_PlantAvailableWaterFraction = 0.0;

  int vi_CriticalMoistureLayer = int(ceil(vi_CriticalMoistureDepth /
			   soilLayer(0).vs_LayerThickness));
  for (int i_Layer = 0; i_Layer < vi_CriticalMoistureLayer; i_Layer++){
    vi_ActualPlantAvailableWater += (soilLayer(i_Layer).get_Vs_SoilMoisture_m3()
                                 - soilLayer(i_Layer).get_PermanentWiltingPoint())
                                 * vs_LayerThickness() * 1000.0; // [mm]
    vi_MaxPlantAvailableWater += (soilLayer(i_Layer).get_FieldCapacity()
                                 - soilLayer(i_Layer).get_PermanentWiltingPoint())
                                 * vs_LayerThickness() * 1000.0; // [mm]
    vi_PlantAvailableWaterFraction = vi_ActualPlantAvailableWater
                                       / vi_MaxPlantAvailableWater; // []
  }
  if (vi_PlantAvailableWaterFraction <= vi_IrrigationThreshold){
    applyIrrigation(vi_IrrigationAmount, vi_IrrigationNConcentration);

    debug() << "applying automatic irrigation treshold: " << vi_IrrigationThreshold
    << " amount: " << vi_IrrigationAmount
    << " N concentration: " << vi_IrrigationNConcentration << endl;

    return true;
  }

  return false;
}


/**
 * @brief Applies irrigation
 *
 * @author: Claas Nendel
 */
void SoilColumn::applyIrrigation(double vi_IrrigationAmount,
                                 double vi_IrrigationNConcentration) {

  double vi_NAddedViaIrrigation = 0.0; //[kg m-3]

  // Adding irrigation water amount to surface water storage
  vs_SurfaceWaterStorage += vi_IrrigationAmount; // [mm]

  vi_NAddedViaIrrigation = vi_IrrigationNConcentration * // [mg dm-3]
		       vi_IrrigationAmount / //[dm3 m-2]
		       soilLayer(0).vs_LayerThickness / 1000000.0; // [m]
		       // [-> kg m-3]

  // Adding N from irrigation water to top soil nitrate pool
  soilLayer(0).vs_SoilNO3 += vi_NAddedViaIrrigation;
}


/**
 * Applies tillage to effected layers. Parameters for effected soil layers
 * are averaged.
 * @param depth Depth of affected soil.
 */
void SoilColumn::applyTillage(double depth)
{
  size_t layer_index = getLayerNumberForDepth(depth)+1;

  double soil_organic_carbon = 0.0;
  double soil_organic_matter = 0.0;
  double soil_temperature = 0.0;
  double soil_moisture = 0.0;
  double soil_moistureOld = 0.0;
  double som_slow = 0.0;
  double som_fast = 0.0;
  double smb_slow = 0.0;
  double smb_fast = 0.0;
  double carbamid = 0.0;
  double nh4 = 0.0;
  double no2 = 0.0;
  double no3 = 0.0;

  // add up all parameters that are affected by tillage
  for (int i=0; i<layer_index; i++) {
    soil_organic_carbon += this->soilLayer(i).vs_SoilOrganicCarbon();
    soil_organic_matter += this->soilLayer(i).vs_SoilOrganicMatter();
    soil_temperature += this->soilLayer(i).get_Vs_SoilTemperature();
    soil_moisture += this->soilLayer(i).get_Vs_SoilMoisture_m3();
    soil_moistureOld += this->soilLayer(i).vs_SoilMoistureOld_m3;
    som_slow += this->soilLayer(i).vs_SOM_Slow;
    som_fast += this->soilLayer(i).vs_SOM_Fast;
    smb_slow += this->soilLayer(i).vs_SMB_Slow;
    smb_fast += this->soilLayer(i).vs_SMB_Fast;
    carbamid += this->soilLayer(i).vs_SoilCarbamid;
    nh4 += this->soilLayer(i).vs_SoilNH4;
    no2 += this->soilLayer(i).vs_SoilNO2;
    no3 += this->soilLayer(i).vs_SoilNO3;
  }

  // calculate mean value of accumulated soil paramters
  soil_organic_carbon = double(soil_organic_carbon/double(layer_index));
  soil_organic_matter = double(soil_organic_matter/double(layer_index));
  soil_temperature = double(soil_temperature/double(layer_index));
  soil_moisture = double(soil_moisture/double(layer_index));
  soil_moistureOld = double(soil_moistureOld/double(layer_index));
  som_slow = double(som_slow/double(layer_index));
  som_fast = double(som_fast/double(layer_index));
  smb_slow = double(smb_slow/double(layer_index));
  smb_fast = double(smb_fast/double(layer_index));
  carbamid = double(carbamid/double(layer_index));
  nh4 = double(nh4/double(layer_index));
  no2 = double(no2/double(layer_index));
  no3 = double(no3/double(layer_index));

  // use calculated mean values for all affected layers
  for (int i=0; i < layer_index; i++) {
		//assert((soil_organic_carbon - (soil_organic_matter * OrganicConstants::po_SOM_to_C)) < 0.00001);
    this->soilLayer(i).set_SoilOrganicCarbon(soil_organic_carbon);
    this->soilLayer(i).set_SoilOrganicMatter(soil_organic_matter);
    this->soilLayer(i).set_Vs_SoilTemperature(soil_temperature);
    this->soilLayer(i).set_Vs_SoilMoisture_m3(soil_moisture);
    this->soilLayer(i).vs_SoilMoistureOld_m3 = soil_moistureOld;
    this->soilLayer(i).vs_SOM_Slow = som_slow;
    this->soilLayer(i).vs_SOM_Fast = som_fast;
    this->soilLayer(i).vs_SMB_Slow = smb_slow;
    this->soilLayer(i).vs_SMB_Fast = smb_fast;
    this->soilLayer(i).vs_SoilCarbamid = carbamid;
    this->soilLayer(i).vs_SoilNH4 = nh4;
    this->soilLayer(i).vs_SoilNO2 = no2;
    this->soilLayer(i).vs_SoilNO3 = no3;
  }

  // merge aom pool
  unsigned int aom_pool_count = soilLayer(0).vo_AOM_Pool.size();

  if (aom_pool_count>0) {
    vector<double> aom_slow(aom_pool_count);
    vector<double> aom_fast(aom_pool_count);

    // initialization of aom pool accumulator
    for (unsigned int pool_index=0; pool_index<aom_pool_count; pool_index++) {
      aom_slow[pool_index] = 0.0;
      aom_fast[pool_index] = 0.0;
    }

    layer_index = min(layer_index, vs_NumberOfOrganicLayers());

    //cout << "Soil parameters before applying tillage for the first "<< layer_index+1 << " layers: " << endl;

    // add up pools for affected layer with same index
    for (size_t j=0; j<layer_index; j++) {
      //cout << "Layer " << j << endl << endl;

      SoilLayer &layer = soilLayer(j);
      unsigned int pool_index = 0;
      for(vector<AOM_Properties>::iterator it_AOM_Pool = layer.vo_AOM_Pool.begin();
              it_AOM_Pool != layer.vo_AOM_Pool.end();
              it_AOM_Pool++) {

        aom_slow[pool_index] += it_AOM_Pool->vo_AOM_Slow;
        aom_fast[pool_index] += it_AOM_Pool->vo_AOM_Fast;

        //cout << "AOMPool " << pool_index << endl;
        //cout << "vo_AOM_Slow:\t"<< it_AOM_Pool->vo_AOM_Slow << endl;
        //cout << "vo_AOM_Fast:\t"<< it_AOM_Pool->vo_AOM_Fast << endl;

        pool_index++;
      }
    }

    //
    for (unsigned int pool_index=0; pool_index<aom_pool_count; pool_index++) {
      aom_slow[pool_index] = aom_slow[pool_index] / double(layer_index);
      aom_fast[pool_index] = aom_fast[pool_index] / double(layer_index);
    }

    //cout << "Soil parameters after applying tillage for the first "<< layer_index+1 << " layers: " << endl;

    // rewrite parameters of aom pool with mean values
    for (int j=0; j<layer_index; j++) {
      SoilLayer &layer = soilLayer(j);
      //cout << "Layer " << j << endl << endl;
      unsigned int pool_index = 0;
      for(vector<AOM_Properties>::iterator it_AOM_Pool = layer.vo_AOM_Pool.begin();
              it_AOM_Pool != layer.vo_AOM_Pool.end();
              it_AOM_Pool++) {

        it_AOM_Pool->vo_AOM_Slow = aom_slow[pool_index];
        it_AOM_Pool->vo_AOM_Fast = aom_fast[pool_index];

        //cout << "AOMPool " << pool_index << endl;
        //cout << "vo_AOM_Slow:\t"<< it_AOM_Pool->vo_AOM_Slow << endl;
        //cout << "vo_AOM_Fast:\t"<< it_AOM_Pool->vo_AOM_Fast << endl;

        pool_index++;
      }
    }
  }

  //cout << "soil_organic_carbon: " << soil_organic_carbon << endl;
  //cout << "soil_organic_matter: " << soil_organic_matter << endl;
  //cout << "soil_temperature: " << soil_temperature << endl;
  //cout << "soil_moisture: " << soil_moisture << endl;
  //cout << "soil_moistureOld: " << soil_moistureOld << endl;
  //cout << "som_slow: " << som_slow << endl;
  //cout << "som_fast: " << som_fast << endl;
  //cout << "smb_slow: " << smb_slow << endl;
  //cout << "smb_fast: " << smb_fast << endl;
  //cout << "carbamid: " << carbamid << endl;
  //cout << "nh4: " << nh4 << endl;
  //cout << "no3: " << no3 << endl << endl;

}

/**
 * @brief Returns index of layer that lays in the given depth.
 * @param depth Depth in meters
 * @return Index of layer
 */
size_t SoilColumn::getLayerNumberForDepth(double depth)
{
  int layer=0;
  int size= vs_SoilLayers.size();
  double accu_depth=0;
  double layer_thickness=this->soilLayer(0).vs_LayerThickness;

  // find number of layer that lay between the given depth
  for (int i=0; i<size; i++) {

    accu_depth+=layer_thickness;
    if (depth <= accu_depth) {
      break;
    }
    layer++;
  }

  return layer;
}

/**
 * @brief Makes crop information available when needed.
 *
 * @return crop object
 */
void SoilColumn::put_Crop(CropGrowth* c){
    cropGrowth = c;
}

/**
 * @brief Deletes crop object when not needed anymore.
 *
 * @return crop object is NULL
 */
void SoilColumn::remove_Crop(){
    cropGrowth = NULL;
}

/**
 * Returns sum of soiltemperature for several soil layers.
 * @param layers Number of layers that are of interest
 * @return Temperature sum
 */
double
SoilColumn::sumSoilTemperature(int layers)
{
  double accu = 0.0;
  for (int i=0; i<layers; i++)
    accu+=soilLayer(i).get_Vs_SoilTemperature();
  return accu;
}

//------------------------------------------------------------------------------

/**
 * @brief Constructor
 *
 * Parameter initialization.
 */
/*
FertilizerTriggerThunk::FertilizerTriggerThunk(
MineralFertiliserPartition fp,
SoilColumn* sc,
double vf_SamplingDepth,
double vf_CropNTargetValue,
double vf_CropNTargetValue30,
double vf_FertiliserMaxApplication,
double vf_FertiliserMinApplication,
int vf_TopDressingDelay)
: _fp(fp), _sc(sc),
_sd(vf_SamplingDepth),
_cntv(vf_CropNTargetValue),
_cntv30(vf_CropNTargetValue30),
_fmaxa(vf_FertiliserMaxApplication),
_fmina(vf_FertiliserMinApplication),
_tdd(vf_TopDressingDelay)
{

}
 */

/**
 * Starts applying of fertilizer in the soil column according
 * to specified type (nmin or manual).
 * @todo Frage an Micha: Warum wurd operator überladen anstatt eine methode zu implementieren?
 * @todo Micha: Das haut so nicht hin. Beide Fertiliser events haben unterschiedliche Parameter-Anforderungen!
 */
/*
void FertilizerTriggerThunk::operator()() const {
  _sc->nMinFertiliserTrigger(_fp, _sd, _cntv, _cntv30, _fmaxa, _fmina, _tdd);
}
 */

