/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "soiltemperature.h"

#include "monica-model.h"
#include "tools/debug.h"
#include "tools/helper.h"

using namespace std;
using namespace monica;
using namespace Tools;

namespace {

SoilLayer& soilTemperatureLayerAt(SoilTemperature* st, size_t i) {
  if (i < st->noOfSoilLayers) return st->soilColumn->at(i);
  if (i < st->noOfSoilLayers + 1) return st->soilColumnGroundLayer;
  return st->soilColumnBottomLayer;
}

const SoilLayer& soilTemperatureLayerAt(const SoilTemperature* st, size_t i) {
  if (i < st->noOfSoilLayers) return st->soilColumn->at(i);
  if (i < st->noOfSoilLayers + 1) return st->soilColumnGroundLayer;
  return st->soilColumnBottomLayer;
}

} // namespace

namespace monica {
namespace soiltemperature {

kj::Own<SoilTemperature> makeSoilTemperature(MonicaModel& mm, const SoilTemperatureModuleParameters& params) {
  auto st = kj::heap<SoilTemperature>();
  st->soilColumn = mm.soilColumn.get();
  st->monica = &mm;
  st->params = params;
  st->noOfTempLayers = st->soilColumn->size() + 2;
  st->noOfSoilLayers = st->soilColumn->size();
  st->soilTemperature.resize(st->noOfTempLayers);
  st->V.resize(st->noOfTempLayers);
  st->volumeMatrix.resize(st->noOfTempLayers);
  st->volumeMatrixOld.resize(st->noOfTempLayers);
  st->B.resize(st->noOfTempLayers);
  st->matrixPrimaryDiagonal.resize(st->noOfTempLayers);
  st->matrixSecondaryDiagonal.resize(st->noOfTempLayers + 1);
  st->heatConductivity.resize(st->noOfTempLayers);
  st->heatConductivityMean.resize(st->noOfTempLayers);
  st->heatCapacity.resize(st->noOfTempLayers);
  st->solution.resize(st->noOfTempLayers);
  st->matrixDiagonal.resize(st->noOfTempLayers);
  st->matrixLowerTriangle.resize(st->noOfTempLayers);
  st->heatFlow.assign(st->noOfTempLayers, 0.0);

  debug() << "Constructor: SoilColumn" << endl;

  if (!st->soilColumn->empty()) st->soilColumnGroundLayer = st->soilColumnBottomLayer = st->soilColumn->back();

  const double soilMoistureConst = st->params.pt_SoilMoisture;

  const double baseTemp = st->params.pt_BaseTemperature;
  const double initialSurfaceTemp = st->params.pt_InitialSurfaceTemperature;
  const auto soilNols = st->noOfSoilLayers;

  for (size_t i = 0; i < soilNols; i++) {
    st->soilTemperature[i] = ((1.0 - (double(i) / soilNols)) * initialSurfaceTemp)
                             + ((double(i) / soilNols) * baseTemp);
  }

  const size_t groundLayer = st->noOfTempLayers - 2;
  const size_t bottomLayer = st->noOfTempLayers - 1;
  soilTemperatureLayerAt(st.get(), groundLayer).vs_LayerThickness =
    2.0 * soilTemperatureLayerAt(st.get(), groundLayer - 1).vs_LayerThickness;
  soilTemperatureLayerAt(st.get(), bottomLayer).vs_LayerThickness = 1.0;
  st->soilTemperature[groundLayer] = (st->soilTemperature[groundLayer - 1] + baseTemp) * 0.5;
  st->soilTemperature[bottomLayer] = baseTemp;

  st->V[0] = soilTemperatureLayerAt(st.get(), 0).vs_LayerThickness;
  st->B[0] = 2.0 / soilTemperatureLayerAt(st.get(), 0).vs_LayerThickness;
  const double Ntau = st->params.pt_NTau;
  for (size_t i = 1; i < st->noOfTempLayers; i++) {
    const double lti_1 = soilTemperatureLayerAt(st.get(), i - 1).vs_LayerThickness;
    const double lti = soilTemperatureLayerAt(st.get(), i).vs_LayerThickness;
    st->B[i] = 2.0 / (lti + lti_1);
    st->V[i] = lti * Ntau;
  }

  const double ts = st->monica->envPs.p_timeStep;
  const double dw = st->params.pt_DensityWater;
  const double cw = st->params.pt_SpecificHeatCapacityWater;
  const double dq = st->params.pt_QuartzRawDensity;
  const double cq = st->params.pt_SpecificHeatCapacityQuartz;
  const double da = st->params.pt_DensityAir;
  const double ca = st->params.pt_SpecificHeatCapacityAir;
  const double dh = st->params.pt_DensityHumus;
  const double ch = st->params.pt_SpecificHeatCapacityHumus;

  for (size_t i = 0; i < st->noOfSoilLayers; i++) {
    const double sbdi = soilTemperatureLayerAt(st.get(), i)._sps.vs_SoilBulkDensity();
    const double smi = soilMoistureConst;
    st->heatConductivity[i] =
      ((3.0 * (sbdi / 1000.0) - 1.7) * 0.001)
      / (1.0 + (11.5 - 5.0 * (sbdi / 1000.0))
               * exp((-50.0) * pow((smi / (sbdi / 1000.0)), 1.5)))
      * 86400.0
      * ts
      * 100.0
      * 4.184;

    const double sati = soilTemperatureLayerAt(st.get(), i)._sps.vs_Saturation;
    const double somi = soilTemperatureLayerAt(st.get(), i)._sps.vs_SoilOrganicMatter() / da * sbdi;
    st->heatCapacity[i] =
      (smi * dw * cw)
      + ((sati - smi) * da * ca)
      + (somi * dh * ch)
      + ((1.0 - sati - somi) * dq * cq);
  }

  st->heatCapacity[groundLayer] = st->heatCapacity[groundLayer - 1];
  st->heatCapacity[bottomLayer] = st->heatCapacity[groundLayer];
  st->heatConductivity[groundLayer] = st->heatConductivity[groundLayer - 1];
  st->heatConductivity[bottomLayer] = st->heatConductivity[groundLayer];
  st->soilSurfaceTemperature = initialSurfaceTemp;

  st->heatConductivityMean[0] = st->heatConductivity[0];
  for (size_t i = 1; i < st->noOfTempLayers; i++) {
    const double lti_1 = soilTemperatureLayerAt(st.get(), i - 1).vs_LayerThickness;
    const double lti = soilTemperatureLayerAt(st.get(), i).vs_LayerThickness;
    const double hci_1 = st->heatConductivity.at(i - 1);
    const double hci = st->heatConductivity.at(i);
    st->heatConductivityMean[i] = ((lti_1 * hci_1) + (lti * hci)) / (lti + lti_1);
  }

  for (size_t i = 0; i < st->noOfTempLayers; i++) {
    st->volumeMatrix[i] = st->V[i] * st->heatCapacity[i];
    st->volumeMatrixOld[i] = st->volumeMatrix[i];
    st->matrixSecondaryDiagonal[i] = -st->B[i] * st->heatConductivityMean[i];
  }

  st->matrixSecondaryDiagonal[bottomLayer + 1] = 0.0;

  for (size_t i = 0; i < st->noOfTempLayers; i++) {
    st->matrixPrimaryDiagonal[i] =
      st->volumeMatrix[i] - st->matrixSecondaryDiagonal[i] - st->matrixSecondaryDiagonal[i + 1];
  }

  return st;
}

kj::Own<SoilTemperature> makeSoilTemperature(
  MonicaModel& mm,
  mas::schema::model::monica::SoilTemperatureModuleState::Reader reader) {
  auto st = kj::heap<SoilTemperature>();
  st->soilColumn = mm.soilColumn.get();
  st->monica = &mm;
  st->noOfTempLayers = st->soilColumn->size() + 2;
  st->noOfSoilLayers = st->soilColumn->size();
  st->solution.resize(st->noOfTempLayers);
  st->matrixDiagonal.resize(st->noOfTempLayers);
  st->matrixLowerTriangle.resize(st->noOfTempLayers);
  st->heatFlow.assign(st->noOfTempLayers, 0.0);
  deserialize(st.get(), reader);
  return st;
}

void deserialize(SoilTemperature* st, mas::schema::model::monica::SoilTemperatureModuleState::Reader reader) {
  st->soilSurfaceTemperature = reader.getSoilSurfaceTemperature();
  soillayer::deserialize(&st->soilColumnGroundLayer, reader.getSoilColumnVtGroundLayer());
  soillayer::deserialize(&st->soilColumnBottomLayer, reader.getSoilColumnVtBottomLayer());
  st->params.deserialize(reader.getModuleParams());
  st->noOfTempLayers = reader.getNumberOfLayers();
  st->noOfSoilLayers = reader.getVsNumberOfLayers();
  setFromCapnpList(st->soilTemperature, reader.getSoilTemperature());
  setFromCapnpList(st->V, reader.getV());
  setFromCapnpList(st->volumeMatrix, reader.getVolumeMatrix());
  setFromCapnpList(st->volumeMatrixOld, reader.getVolumeMatrixOld());
  setFromCapnpList(st->B, reader.getB());
  setFromCapnpList(st->matrixPrimaryDiagonal, reader.getMatrixPrimaryDiagonal());
  setFromCapnpList(st->matrixSecondaryDiagonal, reader.getMatrixSecundaryDiagonal());
  setFromCapnpList(st->heatConductivity, reader.getHeatConductivity());
  setFromCapnpList(st->heatConductivityMean, reader.getHeatConductivityMean());
  setFromCapnpList(st->heatCapacity, reader.getHeatCapacity());
  st->dampingFactor = reader.getDampingFactor();
  st->solution.resize(st->noOfTempLayers);
  st->matrixDiagonal.resize(st->noOfTempLayers);
  st->matrixLowerTriangle.resize(st->noOfTempLayers);
  st->heatFlow.assign(st->noOfTempLayers, 0.0);
}

void serialize(const SoilTemperature* st, mas::schema::model::monica::SoilTemperatureModuleState::Builder builder) {
  builder.setSoilSurfaceTemperature(st->soilSurfaceTemperature);
  soillayer::serialize(&st->soilColumnGroundLayer, builder.initSoilColumnVtGroundLayer());
  soillayer::serialize(&st->soilColumnBottomLayer, builder.initSoilColumnVtBottomLayer());
  st->params.serialize(builder.initModuleParams());
  builder.setNumberOfLayers((uint16_t)st->noOfTempLayers);
  builder.setVsNumberOfLayers((uint16_t)st->noOfSoilLayers);
  setCapnpList(st->soilTemperature, builder.initSoilTemperature((capnp::uint)st->soilTemperature.size()));
  setCapnpList(st->V, builder.initV((capnp::uint)st->V.size()));
  setCapnpList(st->volumeMatrix, builder.initVolumeMatrix((capnp::uint)st->volumeMatrix.size()));
  setCapnpList(st->volumeMatrixOld, builder.initVolumeMatrixOld((capnp::uint)st->volumeMatrixOld.size()));
  setCapnpList(st->B, builder.initB((capnp::uint)st->B.size()));
  setCapnpList(st->matrixPrimaryDiagonal, builder.initMatrixPrimaryDiagonal((capnp::uint)st->matrixPrimaryDiagonal.size()));
  setCapnpList(st->matrixSecondaryDiagonal,
               builder.initMatrixSecundaryDiagonal((capnp::uint)st->matrixSecondaryDiagonal.size()));
  setCapnpList(st->heatConductivity, builder.initHeatConductivity((capnp::uint)st->heatConductivity.size()));
  setCapnpList(st->heatConductivityMean, builder.initHeatConductivityMean((capnp::uint)st->heatConductivityMean.size()));
  setCapnpList(st->heatCapacity, builder.initHeatCapacity((capnp::uint)st->heatCapacity.size()));
  builder.setDampingFactor(st->dampingFactor);
}

void step(SoilTemperature* st, double tmin, double tmax, double globrad) {
  const size_t groundLayer = st->noOfTempLayers - 2;
  const size_t bottomLayer = st->noOfTempLayers - 1;

  st->soilSurfaceTemperature =
    calcSoilSurfaceTemperature(st, st->soilSurfaceTemperature, tmin, tmax, globrad);
  st->soilColumn->vt_SoilSurfaceTemperature = st->soilSurfaceTemperature;
  st->heatFlow[0] = st->soilSurfaceTemperature * st->B[0] * st->heatConductivityMean[0];

  for (size_t i = 0; i < st->noOfTempLayers; i++) {
    st->solution[i] =
      (st->volumeMatrixOld[i]
       + (st->volumeMatrix[i] - st->volumeMatrixOld[i])
         / soilTemperatureLayerAt(st, i).vs_LayerThickness)
      * st->soilTemperature[i] + st->heatFlow[i];
  }

  st->matrixDiagonal[0] = st->matrixPrimaryDiagonal[0];
  for (size_t i = 1; i < st->noOfTempLayers; i++) {
    st->matrixLowerTriangle[i] = st->matrixSecondaryDiagonal[i] / st->matrixDiagonal[i - 1];
    st->matrixDiagonal[i] = st->matrixPrimaryDiagonal[i]
                            - (st->matrixLowerTriangle[i] * st->matrixSecondaryDiagonal[i]);
  }

  for (size_t i = 1; i < st->noOfTempLayers; i++) {
    st->solution[i] = st->solution[i] - (st->matrixLowerTriangle[i] * st->solution[i - 1]);
  }

  st->solution[bottomLayer] = st->solution[bottomLayer] / st->matrixDiagonal[bottomLayer];
  for (size_t i = 0; i < bottomLayer; i++) {
    const auto j = (bottomLayer - 1) - i;
    const auto j_1 = j + 1;
    st->solution[j] = (st->solution[j] / st->matrixDiagonal[j])
                      - (st->matrixLowerTriangle[j_1] * st->solution[j_1]);
  }

  for (size_t i = 0; i < st->noOfTempLayers; i++) {
    st->soilTemperature[i] = st->solution[i];
  }

  for (size_t i = 0; i < st->noOfSoilLayers; i++) {
    st->volumeMatrixOld[i] = st->volumeMatrix[i];
    soilTemperatureLayerAt(st, i).vs_SoilTemperature = st->soilTemperature[i];
  }

  st->volumeMatrixOld[groundLayer] = st->volumeMatrix[groundLayer];
  st->volumeMatrixOld[bottomLayer] = st->volumeMatrix[bottomLayer];
}

double calcSoilSurfaceTemperature(
  const SoilTemperature* st,
  double prevDaySoilSurfaceTemperature,
  double tmin,
  double tmax,
  double globrad) {
  globrad = max(8.33, globrad);

  const double soilCoverage = st->monica->currentCropModule ? st->monica->currentCropModule->vc_SoilCoverage : 0.0;
  const double shadingCoefficient =
    0.1 + ((soilCoverage * st->dampingFactor) + ((1 - soilCoverage) * (1 - st->dampingFactor)));

  double soilSurfaceTemperature =
    (1.0 - shadingCoefficient)
    * (tmin + ((tmax - tmin) * pow((0.03 * globrad), 0.5)))
    + shadingCoefficient * prevDaySoilSurfaceTemperature;

  if (soilSurfaceTemperature < 0.0) soilSurfaceTemperature = soilSurfaceTemperature * 0.5;

  if (st->monica->soilMoisture->snowComponent->vm_SnowDepth > 0.0) {
    soilSurfaceTemperature = st->monica->soilMoisture->frostComponent->vm_TemperatureUnderSnow;
  }

  return soilSurfaceTemperature;
}

} // namespace soiltemperature
} // namespace monica
