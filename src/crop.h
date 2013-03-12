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

#ifndef __CROP_H
#define __CROP_H

/**
 * @file crop.h
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

#include "monica-parameters.h"
#include "soilcolumn.h"

namespace Monica
{

  enum {
    ROOT=0,
    LEAF=1,
    SHOOT=2,
    STORAGE_ORGAN=3
  };
  /**
 * @brief  Crop part of model
 *
 * The crop is divided into several organs e.g. root, Sprossachse, leaf and Speicherorgan.
 * In the source code, the organs are mapped to numbers:<br>
 * 0 - Root<br>
 * 1 - Leaf<br>
 * 2 - Shoot<br>
 * 3 - Storage organ<br>
 *
 * @author Michael Berg
 */
  class CropGrowth
  {
  public:
    CropGrowth(SoilColumn& soilColumn, const GeneralParameters& generalParams,
               const CropParameters& cropParams, const SiteParameters& siteParams,
               const CentralParameterProvider& cpp, int eva2_usage = NUTZUNG_UNDEFINED);
    ~CropGrowth();

    void applyCutting();

    void step(double vw_MeanAirTemperature, double vw_MaxAirTemperature,
              double vw_MinAirTemperature, double vw_GlobalRadiation,
              double vw_SunshineHours, int vs_JulianDay,
              double vw_RelativeHumidity,
              double vw_WindSpeed, double vw_WindSpeedHeight,
              double vw_AtmosphericCO2Concentration,
              double vw_GrossPrecipitation);

    void get_CropIdentity();
    void get_CropParameters();

    void fc_Radiation(double vs_JulianDay, double vs_Latitude,
                      double vw_GlobalRadiation, double vw_SunshineHours);
    double fc_DaylengthFactor(double d_DaylengthRequirement,
															double vc_EffectiveDayLength,
															double vc_PhotoperiodicDaylength,
                              double d_BaseDaylength);

    std::pair<double, double>
	      fc_VernalisationFactor(double vw_MeanAirTemperature, double vc_TimeStep,
                               double pc_VernalisationRequirement,
                               double vc_VernalisationDays);

    double fc_OxygenDeficiency(double pc_CriticalOxygenContent);

    void fc_CropDevelopmentalStage(double vw_MeanAirTemperature,
                                   std::vector<double> pc_BaseTemperature,
																	 std::vector<double> pc_OptimumTemperature,
																	 std::vector<double> pc_StageTemperatureSum,
                                   double vc_TimeStep,
                                   double d_SoilMoisture_m3,
                                   double d_FieldCapacity,
                                   double d_PermanentWiltingPoint,
                                   int pc_NumberOfDevelopmentalStages,
                                   double vc_VernalisationFactor,
                                   double vc_DaylengthFactor,
                                   double vc_CropNRedux);

    double fc_KcFactor(int vc_DevelopmentalStage, double d_StageTemperatureSum,
                       double d_CurrentTemperatureSum,
                       double pc_InitialKcFactor, double d_StageKcFactor,
                       double d_EarlierStageKcFactor);

    void fc_CropSize(double pc_MaxCropHeight,
                     double pc_MaxCropDiameter,
                     double pc_StageAtMaxHeight,
                     double pc_StageAtMaxDiameter,
                     std::vector <double> pc_StageTemperatureSum,
                     double vc_CurrentTotalTemperatureSum,
                     double pc_CropHeightP1,
                     double pc_CropHeightP2);

    void fc_CropGreenArea(double d_LeafBiomassIncrement,
                          double d_LeafBiomassDecrement,
                          double vc_CropHeiht,
                          double vc_CropDiameter,
                          double d_SpecificLeafAreaStart,
                          double pc_SpecificLeafAreaEnd,
                          double pc_SpecificLeafAreaEarly,
                          double d_StageTemperatureSum,
                          double d_CurrentTemperatureSum,
                          double pc_PlantDensity,
                          double vc_TimeStep);

    double fc_SoilCoverage(double vc_LeafAreaIndex);

    void fc_CropPhotosynthesis(double vw_MeanAirTemperature,
                               double vw_MaxAirTemperature,
                               double vw_MinAirTemperature,
                               double vw_GlobalRadiation,
                               double vw_AtmosphericCO2Concentration,
                               double vs_Latitude, double vc_LeafAreaIndex,
                               double pc_DefaultRadiationUseEfficiency,
                               double pc_MaxAssimilationRate,
                               double pc_MinimumTemperatureForAssimilation,
                               double vc_AstronomicDayLenght, double vc_Declination,
                               double vc_ClearDayRadiation,
															 double vc_EffectiveDayLength,
                               double vc_OvercastDayRadiation);

    void fc_HeatStressImpact(double vw_MeanAirTemperature,
		         double vw_MaxAirTemperature,
		         double vc_CurrentTotalTemperatureSum);

		void fc_DroughtImpactOnFertility(double vc_TranspirationDeficit);

    void fc_CropNitrogen();

    void fc_CropDryMatter(int vs_NumberOfLayers,
                          double vs_LayerThickness,
                          int vc_DevelopmentalStage,
                          double vc_GrossPhotosynthesis,
                          double vc_NetMaintenanceRespiration,
                          double pc_CropSpecificMaxRootingDepth,
                          double vs_SoilSpecificMaxRootingDepth,
                          double vw_MeanAirTemperature);

    double fc_ReferenceEvapotranspiration(
	      double vs_HeightNN,
	      double vw_MaxAirTemperature,
	      double vw_MinAirTemperature,
	      double vw_RelativeHumidity,
	      double vw_MeanAirTemperature,
	      double vw_WindSpeed,
	      double vw_WindSpeedHeight,
	      double vw_GlobalRadiation,
	      double vw_AtmosphericCO2Concentration,
	      double vc_GrossPhotosynthesisReference_mol);

    void fc_CropWaterUptake(int vs_NumberOfLayers,
                            double vs_LayerThickness,
                            double vc_SoilCoverage,
                            int vc_RootingDepth,
                            int vm_GroundwaterTable,
                            double vc_ReferenceEvapotranspiration,
                            double vw_GrossPrecipitation,
                            double vc_CurrentTotalTemperatureSum,
                            double vc_TotalTemperatureSum);

    void fc_CropNUptake(int vs_NumberOfLayers,
                        double vs_LayerThickness,
                        int vc_RootingDepth,
                        int vm_GroundwaterTable,
                        double vc_CurrentTotalTemperatureSum,
                        double vc_TotalTemperatureSum);

    double fc_GrossPrimaryProduction(double vc_Assimilates);

    double fc_NetPrimaryProduction(double vc_GrossPrimaryProduction,
                                   double vc_TotalRespired);


    double get_ReferenceEvapotranspiration() const;
    double get_RemainingEvapotranspiration() const;
    double get_EvaporatedFromIntercept() const;
    double get_NetPrecipitation() const;
    double get_GrossPhotosynthesisRate() const;
    double get_GrossPhotosynthesisHaRate() const;
    double get_AssimilationRate() const;
    double get_Assimilates() const;
    double get_NetMaintenanceRespiration() const;
    double get_MaintenanceRespirationAS() const;
    double get_VernalisationFactor() const;
    double get_DaylengthFactor() const;
    double get_OrganGrowthIncrement(int i_Organ) const;
    double get_NetPhotosynthesis() const;
    double get_LeafAreaIndex() const;
    double get_CropHeight() const;
    int get_RootingDepth() const;
    double get_SoilCoverage() const;
    double get_KcFactor() const;
    double get_StomataResistance() const;
    double get_Transpiration(int i_Layer) const;
    double get_TranspirationDeficit() const;
    double get_CropNRedux() const;
    double get_HeatStressRedux() const;
    double get_PotentialTranspiration() const;
    double get_ActualTranspiration() const;
    double get_OxygenDeficit() const;
    double get_CurrentTemperatureSum() const;
    int get_DevelopmentalStage() const;
    double get_RelativeTotalDevelopment() const;
    double get_OrganBiomass(int i_Organ) const;
    double get_AbovegroundBiomass() const;
    double get_AbovegroundBiomassNContent() const;
    double get_HeatSumIrrigationStart() const;
    double get_HeatSumIrrigationEnd() const;
    double get_NUptakeFromLayer(int i_Layer) const;
    double get_TotalBiomassNContent() const;
    double get_RootNConcentration() const;
    double get_TargetNConcentration() const;
    double get_CriticalNConcentration() const;
    double get_AbovegroundBiomassNConcentration() const;
    double get_PrimaryCropYield() const;
    double get_SecondaryCropYield() const;
    double get_FreshPrimaryCropYield() const;
    double get_FreshSecondaryCropYield() const;
    double get_ResidueBiomass(bool useSecondaryCropYields = true) const;
    double get_ResiduesNConcentration() const;
    double get_PrimaryYieldNConcentration() const;
    double get_ResiduesNContent(bool useSecondaryCropYields = true) const;
    double get_PrimaryYieldNContent() const;
    double get_RawProteinConcentration() const;
    double get_SecondaryYieldNContent() const;
    double get_SumTotalNUptake() const;
    double get_ActNUptake() const;
    double get_PotNUptake() const;
    double get_AccumulatedETa() const;
    double get_GrossPrimaryProduction() const;
    double get_NetPrimaryProduction() const;
    
    double get_VcRespiration() const;
    double get_OrganSpecificTotalRespired(int organ) const;
    double get_OrganSpecificNPP(int organ) const;
    int get_NumberOfOrgans() const { return pc_NumberOfOrgans; }

    inline void accumulateEvapotranspiration(double ETa) { vc_accumulatedETa += ETa;}

    /**
	 * @brief Getter for total biomass.
	 * @return total biomass
	 */
    inline double totalBiomass() const { return vc_TotalBiomass; }

    /**
     * Returns state of plant
     */
    inline bool isDying() const { return this->dyingOut; }

  private:
    //methods
    void calculateCropGrowthStep(double vw_MeanAirTemperature,
                                 double vw_MaxAirTemperature,
                                 double vw_MinAirTemperature,
                                 double vw_GlobalRadiation,
                                 double vw_SunshineHours, int vs_JulianDay,
                                 double vw_RelativeHumidity,
                                 double vw_WindSpeed,
                                 double vw_WindSpeedHeight,
                                 double vw_AtmosphericCO2Concentration,
                                 double vw_GrossPrecipitation);

    int pc_NumberOfAbovegroundOrgans() const;


    // members
    SoilColumn& soilColumn;
    GeneralParameters generalParams;
    const CropParameters& cropParams;
    const CentralParameterProvider& centralParameterProvider;

    //! old N
    int vs_NumberOfLayers;
    static const double vw_AtmosphericCO2Concentration;
    double vw_MeanAirTemperature;
    double vw_GlobalRadiation;
    double vw_SunshineHours;
    double vs_Latitude;
    double vs_JulianDay;
    double vc_AbovegroundBiomass;//! old OBMAS
    double vc_AbovegroundBiomassOld; //! old OBALT
    const std::vector<int>& pc_AbovegroundOrgan;	//! old KOMP
    double vc_ActualTranspiration;
    const std::vector<std::vector<double> >& pc_AssimilatePartitioningCoeff; //! old PRO
    double vc_Assimilates;
    double vc_AssimilationRate; //! old AMAX
    double vc_AstronomicDayLenght;	//! old DL
    const std::vector<double>& pc_BaseDaylength;	//! old DLBAS
    const std::vector<double>& pc_BaseTemperature;	//! old BAS
    double pc_BeginSensitivePhaseHeatStress;
    double vc_BelowgroundBiomass;
    double vc_BelowgroundBiomassOld;
    int pc_CarboxylationPathway;		//! old TEMPTYP
    double vc_ClearDayRadiation;		//! old DRC
    int pc_CO2Method;
    double vc_CriticalNConcentration; //! old GEHMIN
    const std::vector<double>& pc_CriticalOxygenContent; //! old LUKRIT
    double pc_CriticalTemperatureHeatStress;
    double vc_CropDiameter;
    double vc_CropHeatRedux;
    double vc_CropHeight;
    double pc_CropHeightP1;
    double pc_CropHeightP2;
    std::string pc_CropName;			//! old FRUCHT$(AKF)
    double vc_CropNDemand; //! old DTGESN
    double vc_CropNRedux;							//! old REDUK
    double pc_CropSpecificMaxRootingDepth;			//! old WUMAXPF [m]
    std::vector<double> vc_CropWaterUptake; //! old TP
    std::vector<double> vc_CurrentTemperatureSum;	//! old SUM
    double vc_CurrentTotalTemperatureSum;			//! old FP
    double vc_CurrentTotalTemperatureSumRoot;
    double vc_DaylengthFactor;						//! old DAYL
    const std::vector<double>& pc_DaylengthRequirement;		//! old DEC
    int vc_DaysAfterBeginFlowering;
    double vc_Declination;							//! old EFF0
    double pc_DefaultRadiationUseEfficiency;
    int vm_DepthGroundwaterTable;		//! old GRW
    int pc_DevelopmentAccelerationByNitrogenStress;
    int vc_DevelopmentalStage;			//! old INTWICK
    double vc_DroughtImpactOnFertility;
    double pc_DroughtImpactOnFertilityFactor;
    const std::vector<double>& pc_DroughtStressThreshold;	//! old DRYswell
    double pc_EndSensitivePhaseHeatStress;
    double vc_EffectiveDayLength;		//! old DLE
    bool vc_ErrorStatus;
    std::string vc_ErrorMessage;
    double vc_EvaporatedFromIntercept;
    double vc_ExtraterrestrialRadiation;
    int vc_FinalDevelopmentalStage;
    double vc_FixedN;
    int pc_FixingN;
    std::vector<double> vo_FreshSoilOrganicMatter;	//! old NFOS
    double vc_GlobalRadiation;
    double vc_GreenAreaIndex;
    double vc_GrossAssimilates;
    double vc_GrossPhotosynthesis;					//! old GPHOT
    double vc_GrossPhotosynthesis_mol;
    double vc_GrossPhotosynthesisReference_mol;
    double vc_GrossPrimaryProduction;
    double vs_HeightNN;
    double pc_InitialKcFactor;						//! old Kcini
    const std::vector<double>& pc_InitialOrganBiomass;
    double pc_InitialRootingDepth;
    double vc_InterceptionStorage;
    double vc_KcFactor;			//! old FKc
    double vc_LeafAreaIndex;	//! old LAI
    double pc_LimitingTemperatureHeatStress;
    double pc_LuxuryNCoeff;
    double vc_MaintenanceRespirationAS;
    double pc_MaxAssimilationRate;	//! old MAXAMAX
    double pc_MaxCropDiameter;
    double pc_MaxCropHeight;
    double vc_MaxNUptake; //! old MAXUP
    double pc_MaxNUptakeParam;
    double vc_MaxRootingDepth; //! old WURM
    double pc_MinimumNConcentration;
    double pc_MinimumTemperatureForAssimilation;	//! old MINTMP
    double pc_MinimumTemperatureRootGrowth;
    double vc_NetMaintenanceRespiration;			//! old MAINT
    double vc_NetPhotosynthesis; //! old GTW
    double vc_NetPrecipitation;
    double vc_NetPrimaryProduction;
    double pc_NConcentrationAbovegroundBiomass; //! initial value of old GEHOB
    double vc_NConcentrationAbovegroundBiomass; //! old GEHOB
    double vc_NConcentrationAbovegroundBiomassOld; //! old GEHALT
    double pc_NConcentrationB0;
    double vc_NContentDeficit;
    double pc_NConcentrationPN;
    double pc_NConcentrationRoot;	        //! initial value to WUGEH
    double vc_NConcentrationRoot;		//! old WUGEH
    double vc_NConcentrationRootOld;		//! old
    int pc_NumberOfDevelopmentalStages;
    int pc_NumberOfOrgans;							//! old NRKOM
    std::vector<double> vc_NUptakeFromLayer; //! old PE
    const std::vector<double>& pc_OptimumTemperature;
    std::vector<double> vc_OrganBiomass;	//! old WORG
    std::vector<double> vc_OrganDeadBiomass;	//! old WDORG
    std::vector<double> vc_OrganGreenBiomass;
    std::vector<double> vc_OrganGrowthIncrement;			//! old GORG
    const std::vector<double>& pc_OrganGrowthRespiration;	//! old MAIRT
    const std::vector<double>& pc_OrganMaintenanceRespiration;	//! old MAIRT
    std::vector<double> vc_OrganSenescenceIncrement; //! old DGORG
    const std::vector<std::vector<double> >& pc_OrganSenescenceRate;	//! old DEAD
    double vc_OvercastDayRadiation;					//! old DRO
    double vc_OxygenDeficit;					//! old LURED
    double vc_PhotoperiodicDaylength;				//! old DLP
    double vc_PhotActRadiationMean;					//! old RDN
    double pc_PlantDensity;
    double vc_PotentialTranspiration;
    double vc_ReferenceEvapotranspiration;
    double pc_ResidueNRatio;
    double vc_RelativeTotalDevelopment;
    double vc_RemainingEvapotranspiration;
    double vc_ReserveAssimilatePool;				//! old ASPOO
    double vc_RootBiomass;							//! old WUMAS
    double vc_RootBiomassOld;						//! old WUMALT
    std::vector<double> vc_RootDensity;				//! old WUDICH
    std::vector<double> vc_RootDiameter;				//! old WRAD
    double pc_RootDistributionParam;
    std::vector<double> vc_RootEffectivity; //! old WUEFF
    double pc_RootFormFactor;
    double pc_RootGrowthLag;
    int vc_RootingDepth;                                            //! old WURZ
    double vc_RootingDepth_m;
    int vc_RootingZone;
    double pc_RootPenetrationRate;
    double vm_SaturationDeficit;
    double vc_SoilCoverage;
    std::vector<double> vs_SoilMineralNContent;		//! old C1
    double vc_SoilSpecificMaxRootingDepth;				//! old WURZMAX [m]
    double vs_SoilSpecificMaxRootingDepth;
		const std::vector<double>& pc_SpecificLeafArea;		//! old LAIFKT [ha kg-1]
    double pc_SpecificRootLength;
    double pc_StageAtMaxDiameter;
    double pc_StageAtMaxHeight;
    const std::vector<double>& pc_StageMaxRootNConcentration;	//! old WGMAX
    const std::vector<double>& pc_StageKcFactor;		//! old Kc
    const std::vector<double>& pc_StageTemperatureSum;	//! old TSUM
    double vc_StomataResistance;					//! old RSTOM
    std::vector<int> pc_StorageOrgan;
    int vc_StorageOrgan;
    double vc_TargetNConcentration; //! old GEHMAX
    double vc_TimeStep; //! old dt
    int vc_TimeUnderAnoxia;
    double vs_Tortuosity;							//! old AD
    double vc_TotalBiomass;
    double vc_TotalBiomassNContent; //! old PESUM
    double vc_TotalCropHeatImpact;
    double vc_TotalNUptake; 			//! old SUMPE
    double vc_TotalRespired;
    double vc_Respiration;
    double vc_SumTotalNUptake;    //! summation of all calculated NUptake; needed for sensitivity analysis
    double vc_TotalRootLength;						//! old WULAEN
    double vc_TotalTemperatureSum;
    std::vector<double> vc_Transpiration;			//! old TP
    std::vector<double> vc_TranspirationRedux;   //! old TRRED
    double vc_TranspirationDeficit;					//! old TRREL
    double vc_VernalisationDays; //
    double vc_VernalisationFactor;					//! old FV
    const std::vector<double>& pc_VernalisationRequirement;	//! old VSCHWELL

    int eva2_usage;
    std::vector<YieldComponent> eva2_primaryYieldComponents;
    std::vector<YieldComponent> eva2_secondaryYieldComponents;

    bool dyingOut;
    double vc_accumulatedETa;

    int cutting_delay_days;
  };
} // namespace Monica

#endif


