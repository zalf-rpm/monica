/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
<one line to give the program's name and a brief idea of what it does.>
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

#ifndef __SOIL_ORGANIC_H
#define __SOIL_ORGANIC_H

/**
 * @file soilorganic.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <map>
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <utility>
#include <list>

#include "monica-parameters.h"

namespace Monica {

// forward declaration
class SoilColumn;
class CropGrowth;

/**
 * @brief Soil carbon and nitrogen part of model
 *
 * <img src="../images/soil_organic_schema.png">
 *
 * <img src="../images/nitrification_denitrification.png">
 *
 * @author Michael Berg, Claas Nendel, Xenia Specka
 */
class SoilOrganic {
  public:
		SoilOrganic(SoilColumn& soilColumn,
								const GeneralParameters& gps,
							 // const OrganicConstants& ocs,
								const SiteParameters& sps,
								const CentralParameterProvider& cpp);

    ~SoilOrganic();
    void step(double vw_Precipitation, double vw_MeanAirTemperature, double vw_WindSpeed);

		void addOrganicMatter(const OrganicMatterParameters* addedOrganicMatter,
													double amount, double nConcentration = 0);

    void addIrrigationWater(double amount);

    /**
     * @brief TRUE, if organic fertilizer is added with a following corporation.
     *
		 * Because such a corporation only affects the first layer, no tillage
     * is done for incorporation.
     *
     * @param incorporation TRUE/FALSE
     */
    void setIncorporation(bool incorp) { this->incorporation = incorp; }
    void put_Crop(CropGrowth* crop);
    void remove_Crop();

    double get_SoilOrganicC(int i_Layer) const;
    double get_AOM_FastSum(int i_Layer) const;
    double get_AOM_SlowSum(int i_Layer) const;
    double get_SMB_Fast(int i_Layer) const;
    double get_SMB_Slow(int i_Layer) const;
    double get_SOM_Fast(int i_Layer) const;
    double get_SOM_Slow(int i_Layer) const;
    double get_CBalance(int i_Layer) const;
    double get_SMB_CO2EvolutionRate(int i_layer) const;
    double get_ActDenitrificationRate(int i_Layer) const;
    double get_NetNMineralisationRate(int i_Layer) const;
    double get_NH3_Volatilised() const;
    double get_SumNH3_Volatilised() const;
		double get_N2O_Produced() const;
		double get_SumN2O_Produced() const;
    double get_NetNMineralisation() const;
    double get_SumDenitrification() const;
    double get_Denitrification() const;
    double get_DecomposerRespiration() const;
    double get_NetEcosystemProduction() const;
    double get_NetEcosystemExchange() const;


private:
    //void fo_OM_Input(bool vo_AOM_Addition);
    void fo_Urea(double vo_RainIrrigation);
    void fo_MIT();
    void fo_Volatilisation(bool vo_AOM_Addition, double vw_MeanAirTemperature, double vw_WindSpeed);
    void fo_Nitrification();
    void fo_Denitrification();
		void fo_N2OProduction();
    void fo_PoolUpdate();
    double fo_NetEcosystemProduction(double vc_NetPrimaryProduction, double vo_DecomposerRespiration);
    double fo_NetEcosystemExchange();
    double fo_ClayOnDecompostion(double d_SoilClayContent, double d_LimitClayEffect);
    double fo_TempOnDecompostion(double d_SoilTemperature);
    double fo_MoistOnDecompostion(double d_SoilMoisture_pF);
    double fo_MoistOnHydrolysis(double d_SoilMoisture_pF);
    double fo_TempOnNitrification(double d_SoilTemperature);
    double fo_MoistOnNitrification(double d_SoilMoisture_pF);
    double fo_MoistOnDenitrification(double d_SoilMoisture_m3, double d_Saturation);
		double fo_NH3onNitriteOxidation (double d_SoilNH4, double d_SoilpH);
    SoilColumn & soilColumn;
    const GeneralParameters & generalParams;
    const SiteParameters & siteParams;
    const CentralParameterProvider& centralParameterProvider;

    int                 vs_NumberOfLayers;
    int                 vs_NumberOfOrganicLayers;
    bool                addedOrganicMatter;
    double              irrigationAmount;
    std::vector<double> vo_ActDenitrificationRate;
    std::vector<double> vo_AOM_FastDeltaSum;
    double              vo_AOM_FastInput; //AOMfast pool change by direct input [kg C m-3]
    std::vector<double> vo_AOM_FastSum;
    std::vector<double> vo_AOM_SlowDeltaSum;
    double              vo_AOM_SlowInput; //AOMslow pool change by direct input [kg C m-3]
    std::vector<double> vo_AOM_SlowSum;
    std::vector<double> vo_CBalance;
    double              vo_DecomposerRespiration;
    std::string         vo_ErrorMessage;
    std::vector<double> vo_InertSoilOrganicC;
		double              vo_N2O_Produced;
    double              vo_NetEcosystemExchange;
    double              vo_NetEcosystemProduction;
    double              vo_NetNMineralisation;
    std::vector<double> vo_NetNMineralisationRate;
    double              vo_Total_NH3_Volatilised;
    double              vo_NH3_Volatilised;
		std::vector<double> vo_SMB_CO2EvolutionRate;
    std::vector<double> vo_SMB_FastDelta;
    std::vector<double> vo_SMB_SlowDelta;
    std::vector<double> vs_SoilMineralNContent;
    std::vector<double> vo_SoilOrganicC;
    std::vector<double> vo_SOM_FastDelta;
    double              vo_SOM_FastInput;// SOMfast pool change by direct input [kg C m-3]
    std::vector<double> vo_SOM_SlowDelta;
    double              vo_SumDenitrification;
		double              vo_SumN2O_Produced;
    double              vo_SumNH3_Volatilised;
    double              vo_TotalDenitrification;


    /*
		struct AddedOMParams {
			double vo_AddedOrganicCarbonAmount;
			double vo_AddedOrganicMatterAmount;
			double vo_AOM_DryMatterContent;
			double vo_AOM_NH4Content;
			double vo_AOM_NO3Content;
			double vo_PartAOM_to_AOM_Slow;
			double vo_PartAOM_to_AOM_Fast;
		};

		std::vector<AddedOMParams> newOM;

		 */


    //! True, if organic fertilizer has been added with a following incorporation.
    //! Parameter is automatically set to false, if carbamid amount is falling below 0.001.
    bool incorporation;
    CropGrowth* crop;
};

} // namespace Monica

#endif

