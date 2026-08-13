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

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "model/monica/monica_params.capnp.h"

#include "json11/json11-helper.h"
#include "json11/json11.hpp"

namespace Soil {
struct SoilParameters;
Tools::Errors noSetPwpFcSat(SoilParameters *sp, int = -1);

//! @author Claas Nendel, Michael Berg
struct SoilParameters {
  std::function<Tools::Errors(SoilParameters *)> calculateAndSetPwpFcSat;

  // members
  double vs_SoilSandContent{
      -1.0}; //!< Soil layer's sand content [kg kg-1] //{0.4}
  double vs_SoilClayContent{
      -1.0};             //!< Soil layer's clay content [kg kg-1] (Ton) //{0.05}
  double vs_SoilpH{6.9}; //!< Soil pH value [] //{7.0}
  double vs_SoilStoneContent{
      0.0};               //!< Soil layer's stone content in soil [m3 m-3]
  double vs_Lambda{-1.0}; //!< Soil water conductivity coefficient [] //{0.5}
  double vs_FieldCapacity{-1.0};         //{0.21} //!< [m3 m-3]
  double vs_Saturation{-1.0};            //{0.43} //!< [m3 m-3]
  double vs_PermanentWiltingPoint{-1.0}; //{0.08} //!< [m3 m-3]
  std::string vs_SoilTexture;
  double vs_SoilAmmonium{0.0005}; //!< soil ammonium content [kg NH4-N m-3]
  double vs_SoilNitrate{0.005};   //!< soil nitrate content [kg NO3-N m-3]
  double vs_Soil_CN_Ratio{10.0};
  double vs_SoilMoisturePercentFC{100.0};

  double thickness{0}; // layer thickness in m

  // Raw/override values; -1 means "unset" and the resolved value has to be
  // computed via the corresponding soilparameters::soilXyz() free function
  // (e.g. from the other value + clay content, or from organic carbon<->matter
  // conversion). Kept as directly-named fields (not wrapped in accessors) -
  // read/write the override directly if that's really what's needed, otherwise
  // use the resolved soilparameters::... getter.
  double _vs_SoilRawDensity{-1.0};    //!< [kg m-3]
  double _vs_SoilBulkDensity{-1.0};   //!< [kg m-3]
  double _vs_SoilOrganicCarbon{-1.0}; //!< [kg kg-1]
  double _vs_SoilOrganicMatter{-1.0}; //!< [kg kg-1]
};

SoilParameters makeSoilParameters(
    std::function<Tools::Errors(SoilParameters *)> setPwpFcSat =
        [](SoilParameters *sp) { return noSetPwpFcSat(sp); });

namespace soilparameters {

void serialize(const SoilParameters *sp,
               mas::schema::model::monica::SoilParameters::Builder builder);
void deserialize(SoilParameters *sp,
                 mas::schema::model::monica::SoilParameters::Reader reader);

Tools::Errors merge(SoilParameters *sp, json11::Json j);

json11::Json to_json(const SoilParameters *sp);

//! Soil layer's silt content [kg kg-1] (Schluff)
double soilSiltContent(const SoilParameters *sp);

//! Resolved soil raw density (falls back to bulk density + clay content if unset)
double soilRawDensity(const SoilParameters *sp);

//! Resolved soil bulk density (falls back to raw density + clay content if unset)
double soilBulkDensity(const SoilParameters *sp);

//! Resolved soil organic carbon [kg C kg-1] (falls back to organic matter if unset)
double soilOrganicCarbon(const SoilParameters *sp);

//! Resolved soil organic matter [kg OM kg-1] (falls back to organic carbon if unset)
double soilOrganicMatter(const SoilParameters *sp);

bool isValid(const SoilParameters *sp);

} // namespace soilparameters

// Data structure that holds information about capillary rise rates.
class CapillaryRiseRates {
public:
  // Adds a capillary rise rate to data structure.
  void addRate(const std::string &soilType, size_t distance, double value);

  // Returns capillary rise rate for given soil type and distance to ground
  // water.
  double getRate(const std::string &soilType, size_t distance) const;

  // Returns number of elements of internal map data structure.
  size_t size() const { return capillaryRiseRates.size(); }

private:
  std::map<std::string, std::map<size_t, double>> capillaryRiseRates;
};

const CapillaryRiseRates &readCapillaryRiseRates();

typedef std::vector<SoilParameters> SoilPMs;
typedef std::shared_ptr<SoilPMs> SoilPMsPtr;

Tools::EResult<SoilPMs> createEqualSizedSoilPMs(
    const std::function<Tools::Errors(SoilParameters *, int)> &setPwpFcSat,
    const Tools::J11Array &jsonSoilPMs, double layerThickness = 0.1,
    int numberOfLayers = 20);

Tools::EResult<SoilPMs>
createSoilPMs(const std::function<Tools::Errors(SoilParameters *)> &setPwpFcSat,
              const Tools::J11Array &jsonSoilPMs);

std::function<Tools::Errors(SoilParameters *, int)>
getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(
    const std::string &pathToSoilDir);

struct VanGenuchtenParams {
  double thetaR{0};
  double thetaS{0};
  double alpha{0};
  double m{0};
  double n{0};
  double volumetricWaterContentAtMatricHead{-1};
};

// calc volumetricWaterContentAtMatricHead if stone fraction and matric head are
// provided
VanGenuchtenParams
calcVanGenuchtenVereeckenParams(double pwp, double sat, double sandFrac,
                                double clayFrac, double bulkDensityKgPerM3,
                                double organicCarbonFrac, double stoneFrac = -1,
                                double matricHead = -1);

VanGenuchtenParams
calcVanGenuchtenTothParams(bool isTopSoil, double sandFrac, double clayFrac,
                           double bulkDensityKgPerM3, double organicCarbonFrac,
                           double stoneFrac = -1, double matricHead = -1);

Tools::Errors updateUnsetPwpFcSatFromVanGenuchtenVereecken(SoilParameters *sp,
                                                           int layerNo = -1);

Tools::Errors updateUnsetPwpFcSatFromVanGenuchtenToth(SoilParameters *sp,
                                                      int layerNo = 0);

Tools::Errors updateUnsetPwpFcSatFromToth(SoilParameters *sp, int layerNo = -1);
} // namespace Soil
