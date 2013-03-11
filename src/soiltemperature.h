#ifndef __SOIL_TEMPERATURE_H
#define __SOIL_TEMPERATURE_H

/**
 * @file soiltemperature.h
 *
 * @brief This file contains declarations of class SoilTemperature and SC,
 * a soil solumn wrapper.
 *
 * Soil element definition by Claas Nendel.
 * This file contains class definitions of SoilTemperature and the wrapper
 * class SC that simulates a soil column with two extra layers.
 *
 * @see SoilTemperature
 * @see SC
 */

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cfloat>
#include <cmath>
#include <map>
#include <string>
#include <iostream>
#include <vector>
#include <iomanip>


#include "climate/climate.h"
#include "soilcolumn.h"

namespace Monica {

// forward declaration
class SoilColumn;
class MonicaModel;

/**
 * @brief Calculation of soil temperature.
 *
 * The calculation of soil temperature is based on a model
 * developed by PIC.
 *
 * @author Michael Berg
 */
class SoilTemperature
{

public:

	SoilTemperature(SoilColumn& soilColumn, MonicaModel& monica, const CentralParameterProvider& cpp);
	~SoilTemperature();

	void step(double tmin, double tmax, double globrad);

	double f_SoilSurfaceTemperature(double tmin, double tmax, double globrad);
	double get_SoilSurfaceTemperature() const;
	double get_SoilTemperature(int layer) const;
	double get_HeatConductivity(int layer) const;
	double get_AvgTopSoilTemperature(double sumUpLayerThickness = 0.3) const;

	double getDampingFactor() const { return dampingFactor; }
  void setDampingFactor(double factor) { this->dampingFactor = factor; }

    double vt_SoilSurfaceTemperature;
private:
    SoilColumn & _soilColumn;
    MonicaModel & monica;
    SoilLayer _soilColumn_vt_GroundLayer;
    SoilLayer _soilColumn_vt_BottomLayer;
    const CentralParameterProvider& centralParameterProvider;

    struct SC
    {
        SoilColumn & sc;
        SoilLayer & gl;
        SoilLayer & bl;
        int vs_nols;
        SC(SoilColumn & sc, SoilLayer & gl, SoilLayer & bl, int vs_nols)
        :sc(sc), gl(gl), bl(bl), vs_nols(vs_nols)
        {
        }

        SoilLayer & operator [](int i) const
        {
            if(i < vs_nols){
                return sc[i];
            }else
                if(i < vs_nols + 1){
                    return gl;
                }

            return bl;
        }

        const SoilLayer & at(int i) const
        {
            return (*this)[i];
        }

    } soilColumn;
    const int vt_NumberOfLayers;
    const int vs_NumberOfLayers;
    std::vector<double> vs_SoilMoisture_const;
    std::vector<double> vt_SoilTemperature;
    std::vector<double> vt_V;
    std::vector<double> vt_VolumeMatrix;
    std::vector<double> vt_VolumeMatrixOld;
    std::vector<double> vt_B;
    std::vector<double> vt_MatrixPrimaryDiagonal;
    std::vector<double> vt_MatrixSecundaryDiagonal;
    double vt_HeatFlow;
    std::vector<double> vt_HeatConductivity;
    std::vector<double> vt_HeatConductivityMean;
    std::vector<double> vt_HeatCapacity;
    double dampingFactor;


};

} /* namespace Monica */

#endif

