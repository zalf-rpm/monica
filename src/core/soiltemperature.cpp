/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg-mohnicke@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "soiltemperature.h"

#include "monica-model.h"
#include "tools/debug.h"
#include "tools/helper.h"

using namespace std;
using namespace monica;
using namespace Tools;

//! Create soil column giving a the number of layers it consists of
SoilTemperature::SoilTemperature(MonicaModel &mm, const SoilTemperatureModuleParameters &params)
    : _soilColumn(mm.soilColumnNC())
      , _monica(mm)
      , soilColumn(_soilColumn,
                   _soilColumnGroundLayer,
                   _soilColumnBottomLayer,
                   _soilColumn.vs_NumberOfLayers())
      , _params(params)
      , _noOfTempLayers(_soilColumn.vs_NumberOfLayers() + 2)
      , _noOfSoilLayers(_soilColumn.vs_NumberOfLayers())
//	, vs_SoilMoisture_const(_noOfTempLayers)
      , _soilTemperature(_noOfTempLayers)
      , _V(_noOfTempLayers)
      , _volumeMatrix(_noOfTempLayers)
      , _volumeMatrixOld(_noOfTempLayers)
      , _B(_noOfTempLayers)
      , _matrixPrimaryDiagonal(_noOfTempLayers)
      , _matrixSecondaryDiagonal(_noOfTempLayers + 1)
      , _heatConductivity(_noOfTempLayers)
      , _heatConductivityMean(_noOfTempLayers)
      , _heatCapacity(_noOfTempLayers)
      , _solution(_noOfTempLayers)
      , _matrixDiagonal(_noOfTempLayers)
      , _matrixLowerTriangle(_noOfTempLayers)
      , _heatFlow(_noOfTempLayers, 0.0) {
  debug() << "Constructor: SoilColumn" << endl;

#ifdef AMEI
  auto &st = _soilTempComp;
  st.settimeStep(_monica.environmentParameters().p_timeStep);
  st.setsoilMoistureConst(_params.pt_SoilMoisture);
  st.setbaseTemp(_params.pt_BaseTemperature);
  st.setinitialSurfaceTemp(_params.pt_InitialSurfaceTemperature);
  st.setdensityAir(_params.pt_DensityAir);
  st.setspecificHeatCapacityAir(_params.pt_SpecificHeatCapacityAir);
  st.setdensityHumus(_params.pt_DensityHumus);
  st.setspecificHeatCapacityHumus(_params.pt_SpecificHeatCapacityHumus);
  st.setdensityWater(_params.pt_DensityWater);
  st.setspecificHeatCapacityWater(_params.pt_SpecificHeatCapacityWater);
  st.setquartzRawDensity(_params.pt_QuartzRawDensity);
  st.setspecificHeatCapacityQuartz(_params.pt_SpecificHeatCapacityQuartz);
  st.setnTau(_params.pt_NTau);
  st.setnoOfTempLayers(int(_noOfTempLayers));
  st.setnoOfSoilLayers(int(_noOfSoilLayers));
  std::vector<double> lts;
  std::vector<double> bds;
  std::vector<double> sats;
  std::vector<double> oms;
  for (const auto &sc: soilColumn.sc) {
    lts.push_back(sc.vs_LayerThickness);
    bds.push_back(sc.vs_SoilBulkDensity());
    sats.push_back(sc.vs_Saturation());
    oms.push_back(sc.vs_SoilOrganicMatter());
  }
  lts.push_back(soilColumn.sc.back().vs_LayerThickness);
  lts.push_back(soilColumn.sc.back().vs_LayerThickness);
  st.setlayerThickness(lts);
  st.setsoilBulkDensity(bds);
  st.setsaturation(sats);
  st.setsoilOrganicMatter(oms);
  st.setdampingFactor(_dampingFactor);

  //init soil temp component
  _soilTempComp._SoilTemperature.Init(_soilTempState, _soilTempState1,
                                      _soilTempRate, _soilTempAux, _soilTempExo);
#endif

  //initialize the two additional layers to the same values
  //as the bottom most standard soil layer
  //even though this hadn't been done before it was ok, 
  //as these two layers seam to be used only for 
  //calculating heat capacity and conductivity, the layer sizes are set
  //manually below, nevertheless initializing them to some sensible values
  //shouldn't hurt
  if (!_soilColumn.empty()) _soilColumnGroundLayer = _soilColumnBottomLayer = _soilColumn.back();

  // according to sensitivity tests, soil moisture has minor
  // influence to the temperature and thus can be set as constant
  // by xenia
  const double soilMoistureConst = _params.pt_SoilMoisture;

  double baseTemp = _params.pt_BaseTemperature;  // temperature for lowest layer (avg yearly air temp)
  double initialSurfaceTemp = _params.pt_InitialSurfaceTemperature; // Replace by Mean air temperature
  auto soilNols = _noOfSoilLayers;

  // Initialising the soil properties 
  for (size_t i = 0; i < soilNols; i++) {
    // Initialising the soil temperature
    _soilTemperature[i] = ((1.0 - (double(i) / soilNols)) * initialSurfaceTemp)
                          + ((double(i) / soilNols) * baseTemp);

    // Initialising the soil moisture content
    // Soil moisture content is held constant for numeric stability.
    // If dynamic soil moisture should be used, the energy balance
    // must be extended by latent heat flow.
    //vs_SoilMoisture_const[i] = soilMoistureConst;
  }

  // Determination of the geometry parameters for soil temperature calculation
  // with Cholesky-Method
  const size_t groundLayer = _noOfTempLayers - 2;
  const size_t bottomLayer = _noOfTempLayers - 1;
  soilColumn.at(groundLayer).vs_LayerThickness = 2.0 * soilColumn.at(groundLayer - 1).vs_LayerThickness;
  soilColumn.at(bottomLayer).vs_LayerThickness = 1.0;
  _soilTemperature[groundLayer] = (_soilTemperature[groundLayer - 1] + baseTemp) * 0.5;
  _soilTemperature[bottomLayer] = baseTemp;

  _V[0] = soilColumn.at(0).vs_LayerThickness;
  _B[0] = 2.0 / soilColumn.at(0).vs_LayerThickness;
  double Ntau = _params.pt_NTau;
  for (size_t i = 1; i < _noOfTempLayers; i++) {
    const double lti_1 = soilColumn.at(i - 1).vs_LayerThickness; // [m]
    const double lti = soilColumn.at(i).vs_LayerThickness; // [m]
    _B[i] = 2.0 / (lti + lti_1); // [m]
    _V[i] = lti * Ntau; // [m3]
  }
  // End determination of the geometry parameters for soil temperature calculation

  double ts = _monica.environmentParameters().p_timeStep;

  const double dw = _params.pt_DensityWater; // [kg m-3]
  const double cw = _params.pt_SpecificHeatCapacityWater; // [J kg-1 K-1]

  const double dq = _params.pt_QuartzRawDensity;
  const double cq = _params.pt_SpecificHeatCapacityQuartz; // [J kg-1 K-1]

  const double da = _params.pt_DensityAir; // [kg m-3]
  const double ca = _params.pt_SpecificHeatCapacityAir; // [J kg-1 K-1]

  const double dh = _params.pt_DensityHumus; // [kg m-3]
  const double ch = _params.pt_SpecificHeatCapacityHumus; // [J kg-1 K-1]

  // initializing heat state variables
  // iterates only over the standard soil number of layers, the other two layers
  // will be assigned below that loop
  for (size_t i = 0; i < _noOfSoilLayers; i++) {
    ///////////////////////////////////////////////////////////////////////////////////////
    // Calculate heat conductivity following Neusypina 1979
    // Neusypina, T.A. (1979): Rascet teplovo rezima pocvi v modeli formirovanija urozaja.
    // Teoreticeskij osnovy i kolicestvennye metody programmirovanija urozaev. Leningrad,
    // 53 -62.
    // Note: in this original publication lambda is calculated in cal cm-1 s-1 K-1!
    ///////////////////////////////////////////////////////////////////////////////////////
    const double sbdi = soilColumn.at(i).vs_SoilBulkDensity();
    const double smi = soilMoistureConst; //vs_SoilMoisture_const.at(i);
    _heatConductivity[i] =
        ((3.0 * (sbdi / 1000.0) - 1.7) * 0.001)
        / (1.0 + (11.5 - 5.0 * (sbdi / 1000.0))
                 * exp((-50.0) * pow((smi / (sbdi / 1000.0)), 1.5)))
        * 86400.0
        * ts  //  gives result in [days]
        * 100.0  //  gives result in [m]
        * 4.184; // gives result in [J]
    // --> [J m-1 d-1 K-1]

    ///////////////////////////////////////////////////////////////////////////////////////
    // Calculate specific heat capacity following DAISY
    // Abrahamsen, P, and S. Hansen (2000): DAISY - An open soil-crop-atmosphere model
    // system. Environmental Modelling and Software 15, 313-330
    ///////////////////////////////////////////////////////////////////////////////////////
    const double sati = soilColumn.at(i).vs_Saturation();
    const double somi = soilColumn.at(i).vs_SoilOrganicMatter() / da * sbdi; // Converting [kg kg-1] to [m3 m-3]
    _heatCapacity[i] =
        (smi * dw * cw)
        + ((sati - smi) * da * ca)
        + (somi * dh * ch)
        + ((1.0 - sati - somi) * dq * cq);
    // --> [J m-3 K-1]
  }

  _heatCapacity[groundLayer] = _heatCapacity[groundLayer - 1];
  _heatCapacity[bottomLayer] = _heatCapacity[groundLayer];
  _heatConductivity[groundLayer] = _heatConductivity[groundLayer - 1];
  _heatConductivity[bottomLayer] = _heatConductivity[groundLayer];

  // Initialisation soil surface temperature
  _soilSurfaceTemperature = initialSurfaceTemp;

  ///////////////////////////////////////////////////////////////////////////////////////
  // Initialising Numerical Solution
  // Suckow,F. (1985): A model serving the calculation of soil
  // temperatures. Zeitschrift für Meteorologie 35 (1), 66 -70.
  ///////////////////////////////////////////////////////////////////////////////////////

  // Calculation of the mean heat conductivity per layer
  _heatConductivityMean[0] = _heatConductivity[0];

  for (size_t i = 1; i < _noOfTempLayers; i++) {
    const double lti_1 = soilColumn.at(i - 1).vs_LayerThickness;
    const double lti = soilColumn.at(i).vs_LayerThickness;
    const double hci_1 = _heatConductivity.at(i - 1);
    const double hci = _heatConductivity.at(i);

    // @todo <b>Claas: </b>Formel nochmal durchgehen
    _heatConductivityMean[i] = ((lti_1 * hci_1) + (lti * hci)) / (lti + lti_1);
  }

  // Determination of the volume matrix
  for (size_t i = 0; i < _noOfTempLayers; i++) {
    _volumeMatrix[i] = _V[i] * _heatCapacity[i]; // [J K-1]

    // If initial entry, rearrengement of volume matrix
    _volumeMatrixOld[i] = _volumeMatrix[i];

    // Determination of the matrix secondary diagonal
    _matrixSecondaryDiagonal[i] = -_B[i] * _heatConductivityMean[i]; //[J K-1]
  }

  _matrixSecondaryDiagonal[bottomLayer + 1] = 0.0;

  // Determination of the matrix primary diagonal
  for (size_t i = 0; i < _noOfTempLayers; i++) {
    _matrixPrimaryDiagonal[i] =
        _volumeMatrix[i]
        - _matrixSecondaryDiagonal[i]
        - _matrixSecondaryDiagonal[i + 1]; //[J K-1]
  }
}

SoilTemperature::SoilTemperature(MonicaModel &mm, mas::schema::model::monica::SoilTemperatureModuleState::Reader reader)
    : _soilColumn(mm.soilColumnNC())
      , _monica(mm)
      , soilColumn(_soilColumn,
                   _soilColumnGroundLayer,
                   _soilColumnBottomLayer,
                   _soilColumn.vs_NumberOfLayers())
      , _noOfTempLayers(_soilColumn.vs_NumberOfLayers() + 2)
      , _noOfSoilLayers(_soilColumn.vs_NumberOfLayers())
      , _solution(_noOfTempLayers)
      , _matrixDiagonal(_noOfTempLayers)
      , _matrixLowerTriangle(_noOfTempLayers)
      , _heatFlow(_noOfTempLayers, 0.0) {
  deserialize(reader);
}

void SoilTemperature::deserialize(mas::schema::model::monica::SoilTemperatureModuleState::Reader reader) {
  _soilSurfaceTemperature = reader.getSoilSurfaceTemperature();
  _soilColumnGroundLayer.deserialize(reader.getSoilColumnVtGroundLayer());
  _soilColumnBottomLayer.deserialize(reader.getSoilColumnVtBottomLayer());
  _params.deserialize(reader.getModuleParams());
  _noOfTempLayers = reader.getNumberOfLayers();
  _noOfSoilLayers = reader.getVsNumberOfLayers();
  //setFromCapnpList(vs_SoilMoisture_const, reader.getVsSoilMoistureConst());
  setFromCapnpList(_soilTemperature, reader.getSoilTemperature());
  setFromCapnpList(_V, reader.getV());
  setFromCapnpList(_volumeMatrix, reader.getVolumeMatrix());
  setFromCapnpList(_volumeMatrixOld, reader.getVolumeMatrixOld());
  setFromCapnpList(_B, reader.getB());
  setFromCapnpList(_matrixPrimaryDiagonal, reader.getMatrixPrimaryDiagonal());
  setFromCapnpList(_matrixSecondaryDiagonal, reader.getMatrixSecundaryDiagonal());
  //_heatFlow = reader.getHeatFlow();
  setFromCapnpList(_heatConductivity, reader.getHeatConductivity());
  setFromCapnpList(_heatConductivityMean, reader.getHeatConductivityMean());
  setFromCapnpList(_heatCapacity, reader.getHeatCapacity());
  _dampingFactor = reader.getDampingFactor();
}

void SoilTemperature::serialize(mas::schema::model::monica::SoilTemperatureModuleState::Builder builder) const {
  builder.setSoilSurfaceTemperature(_soilSurfaceTemperature);
  _soilColumnGroundLayer.serialize(builder.initSoilColumnVtGroundLayer());
  _soilColumnBottomLayer.serialize(builder.initSoilColumnVtBottomLayer());
  _params.serialize(builder.initModuleParams());
  builder.setNumberOfLayers((uint16_t) _noOfTempLayers);
  builder.setVsNumberOfLayers((uint16_t) _noOfSoilLayers);
  //setCapnpList(vs_SoilMoisture_const, builder.initVsSoilMoistureConst((capnp::uint)vs_SoilMoisture_const.size()));
  setCapnpList(_soilTemperature, builder.initSoilTemperature((capnp::uint) _soilTemperature.size()));
  setCapnpList(_V, builder.initV((capnp::uint) _V.size()));
  setCapnpList(_volumeMatrix, builder.initVolumeMatrix((capnp::uint) _volumeMatrix.size()));
  setCapnpList(_volumeMatrixOld, builder.initVolumeMatrixOld((capnp::uint) _volumeMatrixOld.size()));
  setCapnpList(_B, builder.initB((capnp::uint) _B.size()));
  setCapnpList(_matrixPrimaryDiagonal, builder.initMatrixPrimaryDiagonal((capnp::uint) _matrixPrimaryDiagonal.size()));
  setCapnpList(_matrixSecondaryDiagonal,
               builder.initMatrixSecundaryDiagonal((capnp::uint) _matrixSecondaryDiagonal.size()));
  //builder.setHeatFlow(_heatFlow);
  setCapnpList(_heatConductivity, builder.initHeatConductivity((capnp::uint) _heatConductivity.size()));
  setCapnpList(_heatConductivityMean, builder.initHeatConductivityMean((capnp::uint) _heatConductivityMean.size()));
  setCapnpList(_heatCapacity, builder.initHeatCapacity((capnp::uint) _heatCapacity.size()));
  builder.setDampingFactor(_dampingFactor);
}

//! Single calculation step
void SoilTemperature::step(double tmin, double tmax, double globrad) {
#ifdef AMEI
  _soilTempExo.settmin(tmin);
  _soilTempExo.settmax(tmax);
  _soilTempExo.setglobrad(globrad);

  _soilTempComp.Calculate_Model(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);

  for (size_t i = 0; i < _noOfSoilLayers; i++) {
    soilColumn.at(i).set_Vs_SoilTemperature(_soilTempState.getsoilTemperature().at(i));
  }
  _soilSurfaceTemperature = _soilTempState.getsoilSurfaceTemperature();
#elif
  const size_t groundLayer = _noOfTempLayers - 2;
  const size_t bottomLayer = _noOfTempLayers - 1;

  /////////////////////////////////////////////////////////////
  // Internal Subroutine Numerical Solution - Suckow,F. (1986)
  /////////////////////////////////////////////////////////////
  _soilSurfaceTemperature = calcSoilSurfaceTemperature(_soilSurfaceTemperature, tmin, tmax, globrad);
  _soilColumn.vt_SoilSurfaceTemperature = _soilSurfaceTemperature;
  _heatFlow[0] = _soilSurfaceTemperature * _B[0] * _heatConductivityMean[0]; //[J]
  //assert _heatFlow[i>0] == 0.0;

  for (size_t i = 0; i < _noOfTempLayers; i++) {
    _solution[i] =
        (_volumeMatrixOld[i]
         + (_volumeMatrix[i] - _volumeMatrixOld[i])
           / soilColumn.at(i).vs_LayerThickness)
        * _soilTemperature[i] + _heatFlow[i];
  }
  // end subroutine NumericalSolution

  /////////////////////////////////////////////////////////////
  // Internal Subroutine Cholesky Solution Method
  //
  // Solution of EX=Z with E tridiagonal and symmetric
  // according to CHOLESKY (E=LDL')
  /////////////////////////////////////////////////////////////

  // Determination of the lower matrix triangle L and the diagonal matrix D
  _matrixDiagonal[0] = _matrixPrimaryDiagonal[0];
  for (size_t i = 1; i < _noOfTempLayers; i++) {
    _matrixLowerTriangle[i] = _matrixSecondaryDiagonal[i] / _matrixDiagonal[i - 1];
    _matrixDiagonal[i] = _matrixPrimaryDiagonal[i]
                         - (_matrixLowerTriangle[i] * _matrixSecondaryDiagonal[i]);
  }

  // Solution of LY=Z
  for (size_t i = 1; i < _noOfTempLayers; i++) {
    _solution[i] = _solution[i] - (_matrixLowerTriangle[i] * _solution[i - 1]);
  }

  // Solution of L'X=D(-1)Y
  _solution[bottomLayer] = _solution[bottomLayer] / _matrixDiagonal[bottomLayer];
  for (size_t i = 0; i < bottomLayer; i++) {
    auto j = (bottomLayer - 1) - i;
    auto j_1 = j + 1;
    _solution[j] = (_solution[j] / _matrixDiagonal[j])
                   - (_matrixLowerTriangle[j_1] * _solution[j_1]);
  }
  // end subroutine CholeskyMethod

  // Internal Subroutine Rearrangement
  for (size_t i = 0; i < _noOfTempLayers; i++) {
    _soilTemperature[i] = _solution[i];
  }

  for (size_t i = 0; i < _noOfSoilLayers; i++) {
    _volumeMatrixOld[i] = _volumeMatrix[i];
    soilColumn.at(i).set_Vs_SoilTemperature(_soilTemperature[i]);
  }

  _volumeMatrixOld[groundLayer] = _volumeMatrix[groundLayer];
  _volumeMatrixOld[bottomLayer] = _volumeMatrix[bottomLayer];
#endif
}

/**
 * @brief  Soil surface temperature [B0C]
 *
 * Soil surface temperature calculation following Williams 1984
 *
 * @param prevDaySoilSurfaceTemperature the previous soil surface temperature
 * @param tmin
 * @param tmax
 * @param globrad
 */
double SoilTemperature::calcSoilSurfaceTemperature(
    double prevDaySoilSurfaceTemperature,
    double tmin, double tmax, double globrad) const {
  // corrected for very low radiation in winter
  globrad = max(8.33, globrad);

  double soilCoverage = _monica.cropGrowth() ? _monica.cropGrowth()->get_SoilCoverage() : 0.0;
  double shadingCoefficient = 0.1 + ((soilCoverage * _dampingFactor) + ((1 - soilCoverage) * (1 - _dampingFactor)));

  // Soil surface temperature caluclation following Williams 1984
  double soilSurfaceTemperature =
      (1.0 - shadingCoefficient)
      * (tmin + ((tmax - tmin) * pow((0.03 * globrad), 0.5)))
      + shadingCoefficient * prevDaySoilSurfaceTemperature;

  // damping negative temperatures due to heat loss for freezing water
  if (soilSurfaceTemperature < 0.0) soilSurfaceTemperature = soilSurfaceTemperature * 0.5;

  if (_monica.soilMoisture().get_SnowDepth() > 0.0) {
    soilSurfaceTemperature = _monica.soilMoisture().getTemperatureUnderSnow();
  }

  return soilSurfaceTemperature;
}

/**
 * @brief Returns mean soil temperature.
 * @param sumLT
 * @return Temperature
 */
/*
double SoilTemperature::getAvgTopSoilTemperature(double sumLT) const
{
  double lsum = 0;
  double tempSum = 0;
  int count = 0;

  for(size_t i = 0; i < _noOfSoilLayers; i++)
  {
    auto& layi = soilColumn.at(i);
    count++;
    tempSum += layi.get_Vs_SoilTemperature();
    lsum += layi.vs_LayerThickness;
    if(lsum >= sumLT) break;
  }

  return count < 1 ? 0 : tempSum / double(count);
}
*/
