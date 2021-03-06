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

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>
#include <cmath>
#include <exception>

#include "soiltemperature.h"
#include "soilcolumn.h"
#include "monica-model.h"
#include "tools/debug.h"
#include "tools/helper.h"
#include "monica-model.h"
#include "soilmoisture.h"
#include "crop-module.h"

using namespace std;
using namespace Climate;
using namespace Monica;
using namespace Tools;

//! Create soil column giving a the number of layers it consists of
SoilTemperature::SoilTemperature(MonicaModel& mm, const SoilTemperatureModuleParameters& params)
	: _soilColumn(mm.soilColumnNC())
	, monica(mm)
  , soilColumn(_soilColumn,
		_soilColumn_vt_GroundLayer,
		_soilColumn_vt_BottomLayer,
		_soilColumn.vs_NumberOfLayers())
	, _params(params)
	, vt_NumberOfLayers(_soilColumn.vs_NumberOfLayers() + 2)
	, vs_NumberOfLayers(_soilColumn.vs_NumberOfLayers())
	, vs_SoilMoisture_const(vt_NumberOfLayers)
	, vt_SoilTemperature(vt_NumberOfLayers)
	, vt_V(vt_NumberOfLayers)
	, vt_VolumeMatrix(vt_NumberOfLayers)
	, vt_VolumeMatrixOld(vt_NumberOfLayers)
	, vt_B(vt_NumberOfLayers)
	, vt_MatrixPrimaryDiagonal(vt_NumberOfLayers)
	, vt_MatrixSecundaryDiagonal(vt_NumberOfLayers + 1)
	, vt_HeatConductivity(vt_NumberOfLayers)
	, vt_HeatConductivityMean(vt_NumberOfLayers)
	, vt_HeatCapacity(vt_NumberOfLayers)
{
	debug() << "Constructor: SoilColumn" << endl;

	//initialize the two additional layers to the same values 
	//as the bottom most standard soil layer
	//even though this hadn't been done before it was ok, 
	//as these two layers seam to be used only for 
	//calculating heat capacity and conductivity, the layer sizes are set
	//manually below, nevertheless initializing them to some sensible values
	//shouldn't hurt
	if(!_soilColumn.empty())
	{
		_soilColumn_vt_GroundLayer = _soilColumn_vt_BottomLayer = _soilColumn.back();
	}
	
	double pt_BaseTemperature = _params.pt_BaseTemperature;  // temp für unterste Schicht (durch. Jahreslufttemp-)
	double pt_InitialSurfaceTemperature = _params.pt_InitialSurfaceTemperature; // Replace by Mean air temperature
	double pt_Ntau = _params.pt_NTau;
	double pt_TimeStep = monica.environmentParameters().p_timeStep;  // schon in soil_moisture in DB extrahiert
	double ps_QuartzRawDensity = _params.pt_QuartzRawDensity;
	double pt_SpecificHeatCapacityWater = _params.pt_SpecificHeatCapacityWater;   // [J kg-1 K-1]
	double pt_SpecificHeatCapacityQuartz = _params.pt_SpecificHeatCapacityQuartz; // [J kg-1 K-1]
	double pt_SpecificHeatCapacityAir = _params.pt_SpecificHeatCapacityAir;       // [J kg-1 K-1]
	double pt_SpecificHeatCapacityHumus = _params.pt_SpecificHeatCapacityHumus;   // [J kg-1 K-1]
	double pt_DensityWater = _params.pt_DensityWater;   // [kg m-3]
	double pt_DensityAir = _params.pt_DensityAir;       // [kg m-3]
	double pt_DensityHumus = _params.pt_DensityHumus;   // [kg m-3]


	// according to sensitivity tests, soil moisture has minor
	// influence to the temperature and thus can be set as constant
	// by xenia
	double ps_SoilMoisture_const = _params.pt_SoilMoisture;


	// Initialising the soil properties until a database feed is realised
	for(size_t i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++)
	{
		// Initialising the soil temperature
		vt_SoilTemperature[i_Layer] = ((1.0 - (double(i_Layer) / vs_NumberOfLayers))
																	 * pt_InitialSurfaceTemperature)
			+ ((double(i_Layer) / vs_NumberOfLayers) * pt_BaseTemperature);

		// Initialising the soil moisture content
		// Soil moisture content is held constant for numeric stability.
		// If dynamic soil moisture should be used, the energy balance
		// must be extended by latent heat flow.
		vs_SoilMoisture_const[i_Layer] = ps_SoilMoisture_const;
	}

	// Determination of the geometry parameters for soil temperature calculation
	// with Cholesky-Method
	auto& lay0 = soilColumn.at(0);
	vt_V[0] = lay0.vs_LayerThickness;
	vt_B[0] = 2.0 / lay0.vs_LayerThickness;

	auto vt_GroundLayer = vt_NumberOfLayers - 2;
	auto vt_BottomLayer = vt_NumberOfLayers - 1;

	soilColumn.at(vt_GroundLayer).vs_LayerThickness = 2.0 * soilColumn.at(vt_GroundLayer - 1).vs_LayerThickness;
	soilColumn.at(vt_BottomLayer).vs_LayerThickness = 1.0;
	vt_SoilTemperature[vt_GroundLayer] = (vt_SoilTemperature[vt_GroundLayer - 1] + pt_BaseTemperature) * 0.5;
	vt_SoilTemperature[vt_BottomLayer] = pt_BaseTemperature;

	double vt_h0 = lay0.vs_LayerThickness;

	for(size_t i = 1; i < vt_NumberOfLayers; i++)
	{
		double vt_h1 = soilColumn.at(i).vs_LayerThickness; // [m]
		vt_B[i] = 2.0 / (vt_h1 + vt_h0); // [m]
		vt_V[i] = vt_h1 * pt_Ntau; // [m3]
		vt_h0 = vt_h1;
	}
	// End determination of the geometry parameters for soil temperature calculation

	// initialising heat state variables
	//iterates only over the standard soil number of layers, the other two layers
	//will be assigned below that loop
	for(size_t i = 0; i < vs_NumberOfLayers; i++)
	{
		///////////////////////////////////////////////////////////////////////////////////////
		// Calculate heat conductivity following Neusypina 1979
		// Neusypina, T.A. (1979): Rascet teplovo rezima pocvi v modeli formirovanija urozaja.
		// Teoreticeskij osnovy i kolicestvennye metody programmirovanija urozaev. Leningrad,
		// 53 -62.
		// Note: in this original publication lambda is calculated in cal cm-1 s-1 K-1!
		///////////////////////////////////////////////////////////////////////////////////////
		const double sbdi = soilColumn.at(i).vs_SoilBulkDensity();
		const double smi = vs_SoilMoisture_const.at(i);

		vt_HeatConductivity[i] =
			((3.0 * (sbdi / 1000.0) - 1.7) * 0.001)
			/ (1.0 + (11.5 - 5.0 * (sbdi / 1000.0))
				 * exp((-50.0) * pow((smi / (sbdi / 1000.0)), 1.5)))
			* 86400.0
			* double(pt_TimeStep)  //  gives result in [days]
			* 100.0  //  gives result in [m]
			* 4.184; // gives result in [J]
					 // --> [J m-1 d-1 K-1]

		///////////////////////////////////////////////////////////////////////////////////////
		// Calculate specific heat capacity following DAISY
		// Abrahamsen, P, and S. Hansen (2000): DAISY - An open soil-crop-atmosphere model
		// system. Environmental Modelling and Software 15, 313-330
		///////////////////////////////////////////////////////////////////////////////////////

		const double cw = pt_SpecificHeatCapacityWater;
		const double cq = pt_SpecificHeatCapacityQuartz;
		const double ca = pt_SpecificHeatCapacityAir;
		const double ch = pt_SpecificHeatCapacityHumus;
		const double dw = pt_DensityWater;
		const double dq = ps_QuartzRawDensity;
		const double da = pt_DensityAir;
		const double dh = pt_DensityHumus;
		const double spv = soilColumn.at(i).vs_Saturation();
		const double som = soilColumn.at(i).vs_SoilOrganicMatter() / da * sbdi; // Converting [kg kg-1] to [m3 m-3]

		vt_HeatCapacity[i] =
			(smi * dw * cw)
			+ ((spv - smi) * da * ca)
			+ (som * dh * ch)
			+ ((1.0 - spv - som) * dq * cq);
		// --> [J m-3 K-1]
	}

	vt_HeatCapacity[vt_GroundLayer] = vt_HeatCapacity[vt_GroundLayer - 1];
	vt_HeatCapacity[vt_BottomLayer] = vt_HeatCapacity[vt_GroundLayer];
	vt_HeatConductivity[vt_GroundLayer] = vt_HeatConductivity[vt_GroundLayer - 1];
	vt_HeatConductivity[vt_BottomLayer] = vt_HeatConductivity[vt_GroundLayer];

	// Initialisation soil surface temperature
	vt_SoilSurfaceTemperature = pt_InitialSurfaceTemperature;

	///////////////////////////////////////////////////////////////////////////////////////
	// Initialising Numerical Solution
	// Suckow,F. (1985): A model serving the calculation of soil
	// temperatures. Zeitschrift für Meteorologie 35 (1), 66 -70.
	///////////////////////////////////////////////////////////////////////////////////////

	// Calculation of the mean heat conductivity per layer
	vt_HeatConductivityMean[0] = vt_HeatConductivity[0];

	for(size_t i = 1; i < vt_NumberOfLayers; i++)	{
		const double lti_1 = soilColumn.at(i - 1).vs_LayerThickness;
		const double lti = soilColumn.at(i).vs_LayerThickness;
		const double hci_1 = vt_HeatConductivity.at(i - 1);
		const double hci = vt_HeatConductivity.at(i);

		// @todo <b>Claas: </b>Formel nochmal durchgehen
		vt_HeatConductivityMean[i] = ((lti_1 * hci_1) + (lti * hci)) / (lti + lti_1);
	}

	// Determination of the volume matrix
	for(size_t i_Layer = 0; i_Layer < vt_NumberOfLayers; i_Layer++)
	{
		vt_VolumeMatrix[i_Layer] = vt_V[i_Layer] * vt_HeatCapacity[i_Layer]; // [J K-1]

		// If initial entry, rearrengement of volume matrix
		vt_VolumeMatrixOld[i_Layer] = vt_VolumeMatrix[i_Layer];

		// Determination of the matrix secundary diagonal
		vt_MatrixSecundaryDiagonal[i_Layer] = -vt_B[i_Layer] * vt_HeatConductivityMean[i_Layer]; //[J K-1]
	}

	vt_MatrixSecundaryDiagonal[vt_BottomLayer + 1] = 0.0;

	// Determination of the matrix primary diagonal
	for(size_t i_Layer = 0; i_Layer < vt_NumberOfLayers; i_Layer++)
	{
		vt_MatrixPrimaryDiagonal[i_Layer] =
			vt_VolumeMatrix[i_Layer]
			- vt_MatrixSecundaryDiagonal[i_Layer]
			- vt_MatrixSecundaryDiagonal[i_Layer + 1]; //[J K-1]
	}
}

SoilTemperature::SoilTemperature(MonicaModel& mm, mas::models::monica::SoilTemperatureModuleState::Reader reader)
  : _soilColumn(mm.soilColumnNC())
  , monica(mm)
  , soilColumn(_soilColumn,
    _soilColumn_vt_GroundLayer,
    _soilColumn_vt_BottomLayer,
    _soilColumn.vs_NumberOfLayers()) {
	deserialize(reader); 
}

void SoilTemperature::deserialize(mas::models::monica::SoilTemperatureModuleState::Reader reader) {
	vt_SoilSurfaceTemperature = reader.getSoilSurfaceTemperature();
	_soilColumn_vt_GroundLayer.deserialize(reader.getSoilColumnVtGroundLayer());
	_soilColumn_vt_BottomLayer.deserialize(reader.getSoilColumnVtBottomLayer());
	_params.deserialize(reader.getModuleParams());
	vt_NumberOfLayers = reader.getNumberOfLayers();
	vs_NumberOfLayers = reader.getVsNumberOfLayers();
	setFromCapnpList(vs_SoilMoisture_const, reader.getVsSoilMoistureConst());
	setFromCapnpList(vt_SoilTemperature, reader.getSoilTemperature());
	setFromCapnpList(vt_V, reader.getV());
	setFromCapnpList(vt_VolumeMatrix, reader.getVolumeMatrix());
	setFromCapnpList(vt_VolumeMatrixOld, reader.getVolumeMatrixOld());
	setFromCapnpList(vt_B, reader.getB());
	setFromCapnpList(vt_MatrixPrimaryDiagonal, reader.getMatrixPrimaryDiagonal());
	setFromCapnpList(vt_MatrixSecundaryDiagonal, reader.getMatrixSecundaryDiagonal());
	vt_HeatFlow = reader.getHeatFlow();
	setFromCapnpList(vt_HeatConductivity, reader.getHeatConductivity());
	setFromCapnpList(vt_HeatConductivityMean, reader.getHeatConductivityMean());
	setFromCapnpList(vt_HeatCapacity, reader.getHeatCapacity());
	_dampingFactor = reader.getDampingFactor();
}

void SoilTemperature::serialize(mas::models::monica::SoilTemperatureModuleState::Builder builder) const {
	builder.setSoilSurfaceTemperature(vt_SoilSurfaceTemperature);
	_soilColumn_vt_GroundLayer.serialize(builder.initSoilColumnVtGroundLayer());
	_soilColumn_vt_BottomLayer.serialize(builder.initSoilColumnVtBottomLayer());
	_params.serialize(builder.initModuleParams());
	builder.setNumberOfLayers((uint16_t)vt_NumberOfLayers);
	builder.setVsNumberOfLayers((uint16_t)vs_NumberOfLayers);
	setCapnpList(vs_SoilMoisture_const, builder.initVsSoilMoistureConst((uint)vs_SoilMoisture_const.size()));
	setCapnpList(vt_SoilTemperature, builder.initSoilTemperature((uint)vt_SoilTemperature.size()));
	setCapnpList(vt_V, builder.initV((uint)vt_V.size()));
	setCapnpList(vt_VolumeMatrix, builder.initVolumeMatrix((uint)vt_VolumeMatrix.size()));
	setCapnpList(vt_VolumeMatrixOld, builder.initVolumeMatrixOld((uint)vt_VolumeMatrixOld.size()));
	setCapnpList(vt_B, builder.initB((uint)vt_B.size()));
	setCapnpList(vt_MatrixPrimaryDiagonal, builder.initMatrixPrimaryDiagonal((uint)vt_MatrixPrimaryDiagonal.size()));
	setCapnpList(vt_MatrixSecundaryDiagonal, builder.initMatrixSecundaryDiagonal((uint)vt_MatrixSecundaryDiagonal.size()));
	builder.setHeatFlow(vt_HeatFlow);
	setCapnpList(vt_HeatConductivity, builder.initHeatConductivity((uint)vt_HeatConductivity.size()));
	setCapnpList(vt_HeatConductivityMean, builder.initHeatConductivityMean((uint)vt_HeatConductivityMean.size()));
	setCapnpList(vt_HeatCapacity, builder.initHeatCapacity((uint)vt_HeatCapacity.size()));
	builder.setDampingFactor(_dampingFactor);
}

//! Single calculation step
void SoilTemperature::step(double tmin, double tmax, double globrad)
{
	size_t vt_GroundLayer = vt_NumberOfLayers - 2;
	size_t vt_BottomLayer = vt_NumberOfLayers - 1;

	vector<double> vt_Solution(vt_NumberOfLayers);//                = new double [vt_NumberOfLayers];
	vector<double> vt_MatrixDiagonal(vt_NumberOfLayers);//          = new double [vt_NumberOfLayers];
	vector<double> vt_MatrixLowerTriangle(vt_NumberOfLayers);//     = new double [vt_NumberOfLayers];

	/////////////////////////////////////////////////////////////
	// Internal Subroutine Numerical Solution - Suckow,F. (1986)
	/////////////////////////////////////////////////////////////

	vt_HeatFlow = f_SoilSurfaceTemperature(tmin, tmax, globrad)
		* vt_B[0]
		* vt_HeatConductivityMean[0]; //[J]

	// Determination of the equation's right side
	vt_Solution[0] = 
		(vt_VolumeMatrixOld[0] 
		 + (vt_VolumeMatrix[0] - vt_VolumeMatrixOld[0])
		 / soilColumn.at(0).vs_LayerThickness)
		* vt_SoilTemperature[0] + vt_HeatFlow;

	for(size_t i = 1; i < vt_NumberOfLayers; i++)
	{
		vt_Solution[i] = 
			(vt_VolumeMatrixOld[i]
			 + (vt_VolumeMatrix[i] - vt_VolumeMatrixOld[i])
			 / soilColumn.at(i).vs_LayerThickness)
			* vt_SoilTemperature[i];
	}
	// end subroutine NumericalSolution

	/////////////////////////////////////////////////////////////
	// Internal Subroutine Cholesky Solution Method
	//
	// Solution of EX=Z with E tridiagonal and symmetric
	// according to CHOLESKY (E=LDL')
	/////////////////////////////////////////////////////////////

	// Determination of the lower matrix triangle L and the diagonal matrix D
	vt_MatrixDiagonal[0] = vt_MatrixPrimaryDiagonal[0];

	for(size_t i_Layer = 1; i_Layer < vt_NumberOfLayers; i_Layer++)
	{
		vt_MatrixLowerTriangle[i_Layer] = vt_MatrixSecundaryDiagonal[i_Layer]
			/ vt_MatrixDiagonal[i_Layer - 1];
		vt_MatrixDiagonal[i_Layer] = vt_MatrixPrimaryDiagonal[i_Layer]
			- (vt_MatrixLowerTriangle[i_Layer] * vt_MatrixSecundaryDiagonal[i_Layer]);
	}

	// Solution of LY=Z
	for(size_t i_Layer = 1; i_Layer < vt_NumberOfLayers; i_Layer++)
	{
		vt_Solution[i_Layer] = 
			vt_Solution[i_Layer]
			- (vt_MatrixLowerTriangle[i_Layer] * vt_Solution[i_Layer - 1]);
	}

	// Solution of L'X=D(-1)Y
	vt_Solution[vt_BottomLayer] = 
		vt_Solution[vt_BottomLayer] / vt_MatrixDiagonal[vt_BottomLayer];

	for(size_t i_Layer = 0; i_Layer < vt_BottomLayer; i_Layer++)
	{
		auto j_Layer = (vt_BottomLayer - 1) - i_Layer;
		auto j_Layer1 = j_Layer + 1;
		vt_Solution[j_Layer] = (vt_Solution[j_Layer] / vt_MatrixDiagonal[j_Layer])
			- (vt_MatrixLowerTriangle[j_Layer1] * vt_Solution[j_Layer1]);
	}

	// end subroutine CholeskyMethod

	// Internal Subroutine Rearrangement
	for(size_t i = 0; i < vt_NumberOfLayers; i++)
		vt_SoilTemperature[i] = vt_Solution[i];

	for(size_t i = 0; i < vs_NumberOfLayers; i++)
	{
		vt_VolumeMatrixOld[i] = vt_VolumeMatrix[i];
		soilColumn.at(i).set_Vs_SoilTemperature(vt_SoilTemperature[i]);
	}

	vt_VolumeMatrixOld[vt_GroundLayer] = vt_VolumeMatrix[vt_GroundLayer];
	vt_VolumeMatrixOld[vt_BottomLayer] = vt_VolumeMatrix[vt_BottomLayer];
}


/**
 * @brief  Soil surface temperature [B0C]
 *
 * Soil surface temperature caluclation following Williams 1984
 *
 * @param tmin
 * @param tmax
 * @param globrad
 */
double SoilTemperature::f_SoilSurfaceTemperature(double tmin, double tmax, double globrad)
{
	double shading_coefficient = _dampingFactor;

	double soil_coverage = 0.0;
	if(monica.cropGrowth())
	{
		soil_coverage = monica.cropGrowth()->get_SoilCoverage();
	}
	shading_coefficient = 0.1 + ((soil_coverage * _dampingFactor)
															 + ((1 - soil_coverage) * (1 - _dampingFactor)));

	// Soil surface temperature caluclation following Williams 1984
	double vt_SoilSurfaceTemperatureOld = vt_SoilSurfaceTemperature;

	// corrected for very low radiation in winter
	if(globrad < 8.33)
	{
		globrad = 8.33;
	}

	vt_SoilSurfaceTemperature = 
		(1.0 - shading_coefficient)
		* (tmin + ((tmax - tmin) * pow((0.03 * globrad), 0.5)))
		+ shading_coefficient * vt_SoilSurfaceTemperatureOld;

	// damping negative temperatures due to heat loss for freezing water
	if(vt_SoilSurfaceTemperature < 0.0)
	{
		vt_SoilSurfaceTemperature = vt_SoilSurfaceTemperature * 0.5;
	}

	double vt_SnowDepth = 0.0;
	vt_SnowDepth = monica.soilMoisture().get_SnowDepth();

	double vt_TemperatureUnderSnow = 0.0;
	vt_TemperatureUnderSnow = monica.soilMoisture().getTemperatureUnderSnow();

	if(vt_SnowDepth > 0.0)
	{
		vt_SoilSurfaceTemperature = vt_TemperatureUnderSnow;
	}

	_soilColumn.vt_SoilSurfaceTemperature = vt_SoilSurfaceTemperature;

	return vt_SoilSurfaceTemperature;
}

/**
 * @brief Returns soil surface temperature.
 * @param
 * @return Soil surface temperature
 */
double SoilTemperature::get_SoilSurfaceTemperature() const
{
	return vt_SoilSurfaceTemperature;
}

/**
 * @brief Returns soil temperature of a layer.
 * @param layer Index of layer
 * @return Soil temperature
 */
double SoilTemperature::get_SoilTemperature(int layer) const
{
	return soilColumn.at(layer).get_Vs_SoilTemperature();
}

/**
 * @brief Returns heat conductivity of a layer.
 * @param layer Index of layer
 * @return Soil heat conductivity
 */
double SoilTemperature::get_HeatConductivity(int layer) const
{
	return vt_HeatConductivity[layer];
}

/**
 * @brief Returns mean soil temperature.
 * @param sumLT
 * @return Temperature
 */
double SoilTemperature::get_AvgTopSoilTemperature(double sumLT) const
{
	double lsum = 0;
	double tempSum = 0;
	int count = 0;

	for(size_t i = 0; i < vs_NumberOfLayers; i++)
	{
		auto& layi = soilColumn.at(i);
		count++;
		tempSum += layi.get_Vs_SoilTemperature();
		lsum += layi.vs_LayerThickness;
		if(lsum >= sumLT)
		{
			break;
		}
	}

	return count < 1 ? 0 : tempSum / double(count);
}
