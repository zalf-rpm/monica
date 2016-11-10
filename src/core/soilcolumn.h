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

#ifndef SOILCOLUMN_H_
#define SOILCOLUMN_H_

/**
 * @file soilcolumn.h
 *
 * @brief This file contains the declaration of classes AOM_Properties,  SoilLayer, SoilColumn
 * and FertilizerTriggerThunk.
 *
 * @see Monica::AOM_Properties
 * @see Monica::SoilLayer
 * @see Monica::SoilColumn
 * @see Monica::FertilizerTriggerThunk
 */

#include <vector>
#include <list>
#include <iostream>

#include "monica-parameters.h"

namespace Monica
{
  // forward declarations
  class SoilLayer;
  class SoilColumn;
  class CropGrowth;

  //std::vector<AOM_Properties> vo_AOM_Pool;

  /**
   * @author Claas Nendel, Michael Berg
   *
   * @brief Storage class for the transformation of organic substance.
   *
   * Class stores data and parameters used in the AOM (Added Organic Matter) circle,
   * that is displayed in the picture below. The AOM-Circle is a description for the
   * transformation of organic substance.
   *
   * <img src="../images/aom-diagramm.png" width="600" height="420">
   */
  struct AOM_Properties
  {
    double vo_AOM_Slow{0.0}; //!< C content in slowly decomposing added organic matter pool [kgC m-3]
    double vo_AOM_Fast{0.0}; //!< C content in rapidly decomposing added organic matter pool [kgC m-3]

    double vo_AOM_SlowDecRate_to_SMB_Slow{0.0}; //!< Rate for slow AOM consumed by SMB Slow is calculated.
    double vo_AOM_SlowDecRate_to_SMB_Fast{0.0}; //!< Rate for slow AOM consumed by SMB Fast is calculated.
    double vo_AOM_FastDecRate_to_SMB_Slow{0.0}; //!< Rate for fast AOM consumed by SMB Slow is calculated.
    double vo_AOM_FastDecRate_to_SMB_Fast{0.0}; //!< Rate for fast AOM consumed by SMB Fast is calculated.

    double vo_AOM_SlowDecCoeff{0.0}; //!< Is dependent on environment
    double vo_AOM_FastDecCoeff{0.0}; //!< Is dependent on environment

    double vo_AOM_SlowDecCoeffStandard{1.0}; //!< Decomposition rate coefficient for slow AOM pool at standard conditions
    double vo_AOM_FastDecCoeffStandard{1.0}; //!< Decomposition rate coefficient for fast AOM pool at standard conditions

    double vo_PartAOM_Slow_to_SMB_Slow{0.0}; //!< Partial transformation from AOM to SMB (soil microbiological biomass) for slow AOMs.
    double vo_PartAOM_Slow_to_SMB_Fast{0.0}; //!< Partial transformation from AOM to SMB (soil microbiological biomass) for fast AOMs.

    double vo_CN_Ratio_AOM_Slow{1.0}; //!< Used for calculation N-value if only C-value is known. Usually a constant value.
    double vo_CN_Ratio_AOM_Fast{1.0}; //!< C-N-Ratio is dependent on the nutritional condition of the plant.

    int vo_DaysAfterApplication{0}; //!< Fertilization parameter
    double vo_AOM_DryMatterContent{0.0}; //!< Fertilization parameter
    double vo_AOM_NH4Content{0.0}; //!< Fertilization parameter

    double vo_AOM_SlowDelta{0.0}; //!< Difference of AOM slow between to timesteps
    double vo_AOM_FastDelta{0.0}; //!< Difference of AOM fast between to timesteps

    bool incorporation{false};  //!< True if organic fertilizer is added with a subsequent incorporation.
  };

  //----------------------------------------------------------------------------

  /**
   * @author Claas Nendel, Michael Berg
   *
   * @brief class that stores information and properties about a soil layer.
   *
   * Storage class for soil layer properties e.g. saturation, field capacity, etc.
   * Right now all layers are expected to be from the same size, but this code
   * allows different sizes for a layer, too.
   *
   */
  class SoilLayer
  {
  public:
    SoilLayer(){}

//    SoilLayer(const UserInitialValues* initParams);

    SoilLayer(double vs_LayerThickness,
              const Soil::SoilParameters& soilParams);

		//!< Soil layer's organic matter content [kg OM kg-1]
		double vs_SoilOrganicMatter() const { return _sps.vs_SoilOrganicMatter(); } 

    //! Sets value of soil organic matter parameter.
    //void set_SoilOrganicMatter(double som) { _sps.set_vs_SoilOrganicMatter(som); }

		//!< Soil layer's organic carbon content [kg C kg-1]
		double vs_SoilOrganicCarbon() const { return _sps.vs_SoilOrganicCarbon(); } 

    //! Sets value for soil organic carbon.
    void set_SoilOrganicCarbon(double soc) { _sps.set_vs_SoilOrganicCarbon(soc); }

    //! Returns bulk density of soil layer [kg m-3]
    double vs_SoilBulkDensity() const { return _sps.vs_SoilBulkDensity(); }

    //! Returns pH value of soil layer
    double get_SoilpH() const { return _sps.vs_SoilpH; }

    //! Returns soil water pressure head as common logarithm pF.
    double vs_SoilMoisture_pF();

    //! soil ammonium content [kgN m-3]
    double get_SoilNH4() const { return vs_SoilNH4; }

    //! soil nitrite content [kgN m-3]
    double get_SoilNO2() const { return vs_SoilNO2; }

    //! soil nitrate content [kgN m-3]
    double get_SoilNO3() const { return vs_SoilNO3; }

    //! soil carbamide content [kgN m-3]
    double get_SoilCarbamid() const { return vs_SoilCarbamid; }

    //! soil mineral N content [kg m-3]
    double get_SoilNmin() const { return vs_SoilNO3 + vs_SoilNO2 + vs_SoilNH4; }

    double get_Vs_SoilMoisture_m3() const { return vs_SoilMoisture_m3; }
    void set_Vs_SoilMoisture_m3(double ms){ vs_SoilMoisture_m3 = ms; }

    double get_Vs_SoilTemperature() const { return vs_SoilTemperature; }
    void set_Vs_SoilTemperature(double st){ vs_SoilTemperature = st; }

    double vs_SoilSandContent() const { return _sps.vs_SoilSandContent; } //!< Soil layer's sand content [kg kg-1]
    double vs_SoilClayContent() const { return _sps.vs_SoilClayContent; } //!< Soil layer's clay content [kg kg-1] (Ton)
    double vs_SoilStoneContent() const { return _sps.vs_SoilStoneContent; } //!< Soil layer's stone content in soil [kg kg-1]
    double vs_SoilSiltContent() const { return _sps.vs_SoilSiltContent(); } //!< Soil layer's silt content [kg kg-1] (Schluff)
    std::string vs_SoilTexture() const { return _sps.vs_SoilTexture; }

    double vs_SoilpH() const { return _sps.vs_SoilpH; } //!< Soil pH value []
    
    double vs_Lambda() const { return _sps.vs_Lambda; }  //!< Soil water conductivity coefficient []
    double vs_FieldCapacity() const { return _sps.vs_FieldCapacity; }
    double vs_Saturation() const { return _sps.vs_Saturation; }
    double vs_PermanentWiltingPoint() const { return _sps.vs_PermanentWiltingPoint; }

    double vs_Soil_CN_Ratio() const { return _sps.vs_Soil_CN_Ratio; }

    // members ------------------------------------------------------------

    double vs_LayerThickness; //!< Soil layer's vertical extension [m]
    //double vs_SoilMoistureOld_m3{0.25}; //!< Soil layer's moisture content of previous day [m3 m-3]
    double vs_SoilWaterFlux{0.0}; //!< Water flux at the upper boundary of the soil layer [l m-2]

    std::vector<AOM_Properties> vo_AOM_Pool; //!< List of different added organic matter pools in soil layer

    double vs_SOM_Slow{0.0}; //!< C content of soil organic matter slow pool [kg C m-3]
    double vs_SOM_Fast{0.0}; //!< C content of soil organic matter fast pool size [kg C m-3]
    double vs_SMB_Slow{0.0}; //!< C content of soil microbial biomass slow pool size [kg C m-3]
    double vs_SMB_Fast{0.0}; //!< C content of soil microbial biomass fast pool size [kg C m-3]

    // anorganische Stickstoff-Formen
    double vs_SoilCarbamid{0.0}; //!< Soil layer's carbamide-N content [kg Carbamide-N m-3]
    double vs_SoilNH4{0.0001}; //!< Soil layer's NH4-N content [kg NH4-N m-3]
    double vs_SoilNO2{0.001}; //!< Soil layer's NO2-N content [kg NO2-N m-3]
    double vs_SoilNO3{0.0001}; //!< Soil layer's NO3-N content [kg NO3-N m-3]
    bool vs_SoilFrozen{false};

  private:
    Soil::SoilParameters _sps;

    double vs_SoilMoisture_m3{0.25}; //!< Soil layer's moisture content [m3 m-3]
    double vs_SoilTemperature{0.0}; //!< Soil layer's temperature [°C]
  };

  //----------------------------------------------------------------------------

  /**
   * @author Claas Nendel, Michael Berg
   *
   * @brief Description of a soil column that consists of a list of soil layers.

   * All layers are stored in a list and can be access by different kind of operators.
   * This code is based on a Fortran-Program so some operators are overloaded
   * to make the access of an array similar to the Fortran's way.
   *
   * @see Monica::SoilLayer
   *
   */
  class SoilColumn : public std::vector<SoilLayer>
  {
  public:
    SoilColumn(double ps_LayerThickness,
               double ps_MaxMineralisationDepth,
               const Soil::SoilPMsPtr soilParams,
               double pm_CriticalMoistureDepth);

    void applyMineralFertiliser(MineralFertiliserParameters fertiliserPartition,
                                double amount);

    //! apply left over fertiliser after delay time
    double applyPossibleTopDressing();

    //! apply delayed fertiliser application again (yielding possible top dressing)
    double applyPossibleDelayedFerilizer();

    double applyMineralFertiliserViaNMinMethod(MineralFertiliserParameters fertiliserPartition,
                                               double vf_SamplingDepth,
                                               double vf_CropNTargetValue,
                                               double vf_CropNTargetValue30,
                                               double vf_FertiliserMaxApplication,
                                               double vf_FertiliserMinApplication,
                                               int vf_TopDressingDelay);

    bool applyIrrigationViaTrigger(double vi_IrrigationThreshold,
                                   double vi_IrrigationAmount,
                                   double vi_IrrigationNConcentration);

    void applyIrrigation(double vi_IrrigationAmount,
                         double vi_IrrigationNConcentration);
    void deleteAOMPool();


    /**
     * Returns number of layers.
     * @return Number of layers.
     */
    inline std::size_t vs_NumberOfLayers() const { return size(); }
		void applyTillage(double depth);

    /**
     * Returns number of organic layers. Usually the number
     * of layers in the first 30 cm depth of soil.
     * @return Number of organic layers
     */
    inline std::size_t vs_NumberOfOrganicLayers() const { return _vs_NumberOfOrganicLayers; }

    //! Returns the thickness of a layer.
    //! Right now by definition all layers have the same size,
    //! therefor only the thickness of first layer is returned.
    double vs_LayerThickness() const { return at(0).vs_LayerThickness; } 

    //! Returns daily crop N uptake [kg N ha-1 d-1]
    double get_DailyCropNUptake() const { return vq_CropNUptake * 10000.0; }

    std::size_t getLayerNumberForDepth(double depth);

    void put_Crop(CropGrowth* crop);

    void remove_Crop();

    double sumSoilTemperature(int layers) const;
		    
    double vs_SurfaceWaterStorage{0.0}; //!< Content of above-ground water storage [mm]
    double vs_InterceptionStorage{0.0}; //!< Amount of intercepted water on crop surface [mm]
    int vm_GroundwaterTable{0}; //!< Layer of current groundwater table
    double vs_FluxAtLowerBoundary{0.0}; //!< Water flux out of bottom layer
    double vq_CropNUptake{0.0}; //!< Daily amount of N taken up by the crop [kg m-2]
    double vt_SoilSurfaceTemperature{0.0};
    double vm_SnowDepth{0.0};

		void clearTopDressingParams() { _vf_TopDressing = 0.0, _vf_TopDressingDelay = 0; }

  private:
    std::size_t calculateNumberOfOrganicLayers();

    double ps_MaxMineralisationDepth{0.4};

    std::size_t _vs_NumberOfOrganicLayers{0}; //!< Number of organic layers.
    double _vf_TopDressing{0.0};
    MineralFertiliserParameters _vf_TopDressingPartition;
    int _vf_TopDressingDelay{0};

    CropGrowth* cropGrowth{nullptr};

    std::list<std::function<double()>> _delayedNMinApplications;

    double pm_CriticalMoistureDepth;
  };
}

#endif
