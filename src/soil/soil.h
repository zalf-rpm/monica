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

This file is part of the util library used by models created at the Institute of
Landscape Systems Analysis at the ZALF.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <iostream>

#include "kj/function.h"

#include "model/monica/monica_params.capnp.h"

#include "json11/json11.hpp"
#include "json11/json11-helper.h"

namespace Soil {
class SoilParameters;
Tools::Errors noSetPwpFcSat(SoilParameters* sp, int = -1);

//! @author Claas Nendel, Michael Berg 
struct SoilParameters : public Tools::Json11Serializable {
  explicit SoilParameters(std::function<Tools::Errors(SoilParameters*)> setPwpFcSat = [](SoilParameters* sp) {
    return noSetPwpFcSat(sp);
  });

  //explicit SoilParameters(json11::Json object);

  void serialize(mas::schema::model::monica::SoilParameters::Builder builder) const;
  void deserialize(mas::schema::model::monica::SoilParameters::Reader reader);

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  //! Soil layer's silt content [kg kg-1] (Schluff)
  double vs_SoilSiltContent() const { return 1.0 - vs_SoilSandContent - vs_SoilClayContent; }

  double vs_SoilRawDensity() const;
  void set_vs_SoilRawDensity(double srd) { _vs_SoilRawDensity = srd; }

  double vs_SoilBulkDensity() const;
  void set_vs_SoilBulkDensity(double sbd) { _vs_SoilBulkDensity = sbd; }

  //! returns soc [% [0-1]]
  double vs_SoilOrganicCarbon() const; //!< Soil layer's organic carbon content [kg C kg-1]
  //! soc [% [0-1]]
  void set_vs_SoilOrganicCarbon(double soc) { _vs_SoilOrganicCarbon = soc; }

  //!< Soil layer's organic matter content [kg OM kg-1]
  double vs_SoilOrganicMatter() const;
  void set_vs_SoilOrganicMatter(double som) { _vs_SoilOrganicMatter = som; }

  static double sandAndClay2lambda(double sand, double clay);

  bool isValid() const;

  std::function<Tools::Errors(SoilParameters*)> calculateAndSetPwpFcSat;

  // members
  double vs_SoilSandContent{-1.0}; //!< Soil layer's sand content [kg kg-1] //{0.4}
  double vs_SoilClayContent{-1.0}; //!< Soil layer's clay content [kg kg-1] (Ton) //{0.05}
  double vs_SoilpH{6.9}; //!< Soil pH value [] //{7.0}
  double vs_SoilStoneContent{0.0}; //!< Soil layer's stone content in soil [m3 m-3]
  double vs_Lambda{-1.0}; //!< Soil water conductivity coefficient [] //{0.5}
  double vs_FieldCapacity{-1.0}; //{0.21} //!< [m3 m-3]
  double vs_Saturation{-1.0}; //{0.43} //!< [m3 m-3]
  double vs_PermanentWiltingPoint{-1.0}; //{0.08} //!< [m3 m-3]
  std::string vs_SoilTexture;
  double vs_SoilAmmonium{0.0005}; //!< soil ammonium content [kg NH4-N m-3]
  double vs_SoilNitrate{0.005}; //!< soil nitrate content [kg NO3-N m-3]
  double vs_Soil_CN_Ratio{10.0};
  double vs_SoilMoisturePercentFC{100.0};

  double thickness{0}; // layer thickness in m
private:
  double _vs_SoilRawDensity{-1.0}; //!< [kg m-3]
  double _vs_SoilBulkDensity{-1.0}; //!< [kg m-3]
  double _vs_SoilOrganicCarbon{-1.0}; //!< [kg kg-1]
  double _vs_SoilOrganicMatter{-1.0}; //!< [kg kg-1]
};

// Data structure that holds information about capillary rise rates.
class CapillaryRiseRates {
public:
  //Adds a capillary rise rate to data structure.
  void addRate(const std::string& soilType, size_t distance, double value);

  //Returns capillary rise rate for given soil type and distance to ground water.
  double getRate(const std::string& soilType, size_t distance) const;

  //Returns number of elements of internal map data structure.
  size_t size() const { return capillaryRiseRates.size(); }

private:
  std::map<std::string, std::map<size_t, double>> capillaryRiseRates;
};

const CapillaryRiseRates& readCapillaryRiseRates();

typedef std::vector<SoilParameters> SoilPMs;
typedef std::shared_ptr<SoilPMs> SoilPMsPtr;

Tools::EResult<SoilPMs> createEqualSizedSoilPMs(const std::function<Tools::Errors(SoilParameters*, int)>& setPwpFcSat,
                                                const Tools::J11Array& jsonSoilPMs, double layerThickness = 0.1,
                                                int numberOfLayers = 20);

Tools::EResult<SoilPMs> createSoilPMs(const std::function<Tools::Errors(SoilParameters*)>& setPwpFcSat,
                                      const Tools::J11Array& jsonSoilPMs);

std::function<Tools::Errors(SoilParameters*, int)> getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(
  const std::string& pathToSoilDir);

struct VanGenuchtenParams {
  double thetaR{0};
  double thetaS{0};
  double alpha{0};
  double m{0};
  double n{0};
  double volumetricWaterContentAtMatricHead{-1};
};

// calc volumetricWaterContentAtMatricHead if stone fraction and matric head are provided
VanGenuchtenParams calcVanGenuchtenVereeckenParams(double pwp, double sat,
                                                   double sandFrac, double clayFrac,
                                                   double bulkDensityKgPerM3,
                                                   double organicCarbonFrac,
                                                   double stoneFrac = -1, double matricHead = -1);

VanGenuchtenParams calcVanGenuchtenTothParams(bool isTopSoil,
                                              double sandFrac,
                                              double clayFrac,
                                              double bulkDensityKgPerM3,
                                              double organicCarbonFrac,
                                              double stoneFrac = -1,
                                              double matricHead = -1);

Tools::Errors updateUnsetPwpFcSatFromVanGenuchtenVereecken(SoilParameters* sp, int layerNo = -1);

Tools::Errors updateUnsetPwpFcSatFromVanGenuchtenToth(SoilParameters* sp, int layerNo = 0);

Tools::Errors updateUnsetPwpFcSatFromToth(SoilParameters* sp, int layerNo = -1);
} // namespace Soil
