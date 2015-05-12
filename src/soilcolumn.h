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
  class FertilizerTriggerThunk;
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
    AOM_Properties();

    double vo_AOM_Slow; /**< C content in slowly decomposing added organic matter pool [kgC m-3] */
    double vo_AOM_Fast; /**< C content in rapidly decomposing added organic matter pool [kgC m-3] */

		double vo_AOM_SlowDecRate_to_SMB_Slow; /**< Rate for slow AOM consumed by SMB Slow is calculated. */
		double vo_AOM_SlowDecRate_to_SMB_Fast; /**< Rate for slow AOM consumed by SMB Fast is calculated. */
		double vo_AOM_FastDecRate_to_SMB_Slow; /**< Rate for fast AOM consumed by SMB Slow is calculated. */
		double vo_AOM_FastDecRate_to_SMB_Fast; /**< Rate for fast AOM consumed by SMB Fast is calculated. */

    double vo_AOM_SlowDecCoeff; /**< Is dependent on environment */
    double vo_AOM_FastDecCoeff; /**< Is dependent on environment */

    double vo_AOM_SlowDecCoeffStandard; /**< Decomposition rate coefficient for slow AOM pool at standard conditions */
    double vo_AOM_FastDecCoeffStandard; /**< Decomposition rate coefficient for fast AOM pool at standard conditions */

    double vo_PartAOM_Slow_to_SMB_Slow; /**< Partial transformation from AOM to SMB (soil microbiological biomass) for slow AOMs. */
    double vo_PartAOM_Slow_to_SMB_Fast; /**< Partial transformation from AOM to SMB (soil microbiological biomass) for fast AOMs.*/

    double vo_CN_Ratio_AOM_Slow; /**< Used for calculation N-value if only C-value is known. Usually a constant value.*/
    double vo_CN_Ratio_AOM_Fast; /**< C-N-Ratio is dependent on the nutritional condition of the plant. */

    int vo_DaysAfterApplication; /**< Fertilization parameter */
    double vo_AOM_DryMatterContent; /**< Fertilization parameter */
    double vo_AOM_NH4Content; /**< Fertilization parameter */

    double vo_AOM_SlowDelta; /**< Difference of AOM slow between to timesteps */
    double vo_AOM_FastDelta; /**< Difference of AOM fast between to timesteps */

    bool incorporation;  /**< True if organic fertilizer is added with a subsequent incorporation. */
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
    SoilLayer();
    SoilLayer(const SoilLayer&);
    SoilLayer(const CentralParameterProvider& cpp);
    SoilLayer(double vs_LayerThickness, const Soil::SoilParameters& soilParams, const CentralParameterProvider& cpp);

    void calc_vs_SoilMoisture_pF();

    // calculations with Van Genuchten parameters
    double get_Saturation();
    double get_FieldCapacity();
    double get_PermanentWiltingPoint();

    /**
     * Sets value of soil organic matter parameter.
     * @param som New value for soil organic matter parameter
     */
    void set_SoilOrganicMatter(double som) { _vs_SoilOrganicMatter = som; }

    /**
     * Sets value for soil organic carbon.
     * @param soc New value for soil organic carbon.
     */
    void set_SoilOrganicCarbon(double soc) { _vs_SoilOrganicCarbon = soc; }

    /**
     * Returns bulk density of soil layer [kg m-3]
     * @return bulk density of soil layer [kg m-3]
     */
    double vs_SoilBulkDensity() const { return _vs_SoilBulkDensity; }

		/**
		 * Returns pH value of soil layer
		 * @return pH value of soil layer [ ]
		 */
    double get_SoilpH() const { return vs_SoilpH; }

    /**
     * Returns soil water pressure head as common logarithm pF.
     * @return soil water pressure head [pF]
     */
    double vs_SoilMoisture_pF()  {
      calc_vs_SoilMoisture_pF();
      return _vs_SoilMoisture_pF;
    }

    /**
     * Returns soil ammonium content.
     * @return soil ammonium content [kg N m-3]
     */
    double get_SoilNH4() const {
      return vs_SoilNH4;
    }

    /**
     * Returns soil nitrite content.
     * @return soil nitrite content [kg N m-3]
     */
    double get_SoilNO2() const {
      return vs_SoilNO2;
    }

    /**
     * Returns soil nitrate content.
     * @return soil nitrate content [kg N m-3]
     */
    double get_SoilNO3() const {
      return vs_SoilNO3;
    }

		/**
		 * Returns soil carbamide content.
		 * @return soil carbamide content [kg m-3]
		 */
		double get_SoilCarbamid() const {
			return vs_SoilCarbamid;
		}

    /**
     * Returns soil mineral N content.
     * @return soil mineral N content [kg m-3]
     */
    double get_SoilNmin() const {
      return vs_SoilNO3 + vs_SoilNO2 + vs_SoilNH4;
    }

    double get_Vs_SoilMoisture_m3() {
      // Sensitivity analysis case
      if (centralParameterProvider.sensitivityAnalysisParameters.vs_SoilMoisture != UNDEFINED) {
        return centralParameterProvider.sensitivityAnalysisParameters.vs_SoilMoisture;
      }
      return vs_SoilMoisture_m3;
    }

    void set_Vs_SoilMoisture_m3(double ms)
    {
      vs_SoilMoisture_m3 = ms;

      // Sensitivity analysis case
      if (centralParameterProvider.sensitivityAnalysisParameters.vs_SoilMoisture != UNDEFINED) {
        vs_SoilMoisture_m3 = centralParameterProvider.sensitivityAnalysisParameters.vs_SoilMoisture;
        calc_vs_SoilMoisture_pF();
      }

    }

    double get_Vs_SoilTemperature()
    {
      if (centralParameterProvider.sensitivityAnalysisParameters.vs_SoilTemperature != UNDEFINED) {
        return centralParameterProvider.sensitivityAnalysisParameters.vs_SoilTemperature;
      }
      return vs_SoilTemperature;

    }

    void set_Vs_SoilTemperature(double st)
    {
      vs_SoilTemperature = st;

      // Sensitivity analysis case
      if (centralParameterProvider.sensitivityAnalysisParameters.vs_SoilTemperature != UNDEFINED) {
        vs_SoilTemperature = centralParameterProvider.sensitivityAnalysisParameters.vs_SoilTemperature;
      }

    }

    // members ------------------------------------------------------------

    double vs_LayerThickness; /**< Soil layer's vertical extension [m] */
    double vs_SoilSandContent; /**< Soil layer's sand content [kg kg-1] */
    double vs_SoilClayContent; /**< Soil layer's clay content [kg kg-1] (Ton) */
    double vs_SoilStoneContent; /**< Soil layer's stone content in soil [kg kg-1] */
    double vs_SoilSiltContent() const; /**< Soil layer's silt content [kg kg-1] (Schluff) */
    std::string vs_SoilTexture;

    double vs_SoilpH; /**< Soil pH value [] */
    double vs_SoilOrganicCarbon() const; /**< Soil layer's organic carbon content [kg C kg-1] */
    double vs_SoilOrganicMatter() const; /**< Soil layer's organic matter content [kg OM kg-1] */

    double vs_SoilMoistureOld_m3; /**< Soil layer's moisture content of previous day [m3 m-3] */
    double vs_SoilWaterFlux; /**< Water flux at the upper boundary of the soil layer [l m-2] */
    double vs_Lambda; /**< Soil water conductivity coefficient [] */
    double vs_FieldCapacity;
    double vs_Saturation;
    double vs_PermanentWiltingPoint;

    std::vector<AOM_Properties> vo_AOM_Pool; /**< List of different added organic matter pools in soil layer */

    double vs_SOM_Slow; /**< C content of soil organic matter slow pool [kg C m-3] */
    double vs_SOM_Fast; /**< C content of soil organic matter fast pool size [kg C m-3] */
    double vs_SMB_Slow; /**< C content of soil microbial biomass slow pool size [kg C m-3] */
    double vs_SMB_Fast; /**< C content of soil microbial biomass fast pool size [kg C m-3] */

    // anorganische Stickstoff-Formen
    double vs_SoilCarbamid; /**< Soil layer's carbamide-N content [kg Carbamide-N m-3] */
    double vs_SoilNH4; /**< Soil layer's NH4-N content [kg NH4-N m-3] */
    double vs_SoilNO2; /**< Soil layer's NO2-N content [kg NO2-N m-3] */
    double vs_SoilNO3; /**< Soil layer's NO3-N content [kg NO3-N m-3] */
    bool vs_SoilFrozen;

    CentralParameterProvider centralParameterProvider;

  private:
    //! only one of the two is being initialized and used for calculations
    double _vs_SoilOrganicCarbon; /**< Soil organic carbon content [kg C kg-1] */
    double _vs_SoilOrganicMatter; /**< Soil organic matter content [kg OM kg-1] */

    double _vs_SoilBulkDensity; /**< Bulk density of soil [kg m-3] */
    double _vs_SoilMoisture_pF; /**< Soil water pressure head as common logarithm [pF]*/

    double vs_SoilMoisture_m3; /**< Soil layer's moisture content [m3 m-3] */
    double vs_SoilTemperature; /**< Soil layer's temperature [°C] */
  }; // class soil layer

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
  class SoilColumn
  {
  public:
    SoilColumn(const GeneralParameters& generalParams,
               const Soil::SoilPMs& soilParams, const CentralParameterProvider& cpp);

    void applyMineralFertiliser(MineralFertiliserParameters fertiliserPartition,
                                double amount);

    //! apply left over fertiliser after delay time
    double applyPossibleTopDressing();

    //! apply delayed fertiliser application again (yielding possible top dressing)
    double applyPossibleDelayedFerilizer();

    double applyMineralFertiliserViaNMinMethod
    (MineralFertiliserParameters fertiliserPartition,
     double vf_SamplingDepth, double vf_CropNTargetValue,
     double vf_CropNTargetValue30, double vf_FertiliserMaxApplication,
     double vf_FertiliserMinApplication, int vf_TopDressingDelay
     );

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
    inline std::size_t vs_NumberOfLayers() const { return vs_SoilLayers.size(); }
		void applyTillage(double depth);

    /**
     * Returns number of organic layers. Usually the number
     * of layers in the first 30 cm depth of soil.
     * @return Number of organic layers
     */
    inline std::size_t vs_NumberOfOrganicLayers() const { return _vs_NumberOfOrganicLayers; }

    /**
     * Overloaded operator for a fortran-similar access to a C-array.
     * @return SoilLayer at given Index.
     */
    SoilLayer& operator[](size_t i_Layer) {
      return vs_SoilLayers[i_Layer];
    }

    /**
     * Overloaded operator for a fortran-similar access to a C-array.
     * @return SoilLayer at given Index.
     */
    const SoilLayer & operator[](size_t i_Layer) const {
      return vs_SoilLayers.at(i_Layer);
    }

    /**
     * Returns a soil layer at given Index.
     * @return Reference to a soil layer
     */
    SoilLayer& soilLayer(size_t i_Layer) {
      return vs_SoilLayers[i_Layer];
    }

    /**
     * Returns a soil layer at given Index.
     * @return Reference to a soil layer
     */
    const SoilLayer& soilLayer(size_t i_Layer) const {
      return vs_SoilLayers.at(i_Layer);
    }

    /**
     * Returns the thickness of a layer.
     * Right now by definition all layers have the same size,
     * therefor only the thickness of first layer is returned.
     *
     * @return Size of a layer
     *
     * @todo Need to be changed if different layer sizes are used.
     */
    double vs_LayerThickness() const {
      return vs_SoilLayers[0].vs_LayerThickness;
    }


    /**
     * @brief Returns daily crop N uptake [kg N ha-1 d-1]
     * @return Daily crop N uptake
     */
    double get_DailyCropNUptake() const {
      return vq_CropNUptake * 10000.0;
    }



    std::size_t getLayerNumberForDepth(double depth);

    void put_Crop(CropGrowth* crop);

    void remove_Crop();

    double sumSoilTemperature(int layers);

    std::vector<SoilLayer> vs_SoilLayers; /**< Vector of all layers in column. */

    double vs_SurfaceWaterStorage; /**< Content of above-ground water storage [mm] */
    double vs_InterceptionStorage; /**< Amount of intercepted water on crop surface [mm] */
    int vm_GroundwaterTable; /**< Layer of current groundwater table */
    double vs_FluxAtLowerBoundary; /** Water flux out of bottom layer */
    double vq_CropNUptake; /** Daily amount of N taken up by the crop [kg m-2] */
		double vt_SoilSurfaceTemperature;
		double vm_SnowDepth;

  private:

    void set_vs_NumberOfOrganicLayers();

    const GeneralParameters& generalParams; /**< */
    const Soil::SoilPMs& soilParams; /**< Vector of soil parameter*/

    std::size_t _vs_NumberOfOrganicLayers; /**< Number of organic layers. */
    double _vf_TopDressing;
    MineralFertiliserParameters _vf_TopDressingPartition;
    int _vf_TopDressingDelay;

    CropGrowth* cropGrowth;

    std::list<std::function<double()> > _delayedNMinApplications;

    const CentralParameterProvider& centralParameterProvider;

  }; // class soil column

} /* namespace monica */

#endif
