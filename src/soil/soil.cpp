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

#include "soil.h"

#include <map>
#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <mutex>

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <capnp/compat/json.h>
#include <kj/filesystem.h>
#include <kj/string.h>

#include "model/monica/soil_params.capnp.h"

#include "tools/algorithms.h"
#include "conversion.h"
#include "tools/debug.h"
#include "constants.h"

using namespace std;
using namespace Tools;
using namespace Soil;
using namespace json11;

Errors Soil::noSetPwpFcSat(SoilParameters* sp, int) {
  Errors errors;
  if (sp->vs_FieldCapacity < 0) errors.appendError("Field capacity not set!");
  if (sp->vs_Saturation < 0) errors.appendError("Saturation not set!");
  if (sp->vs_PermanentWiltingPoint < 0) errors.appendError("Permanent wilting point not set!");
  return errors;
}

SoilParameters::SoilParameters(std::function<Errors(SoilParameters*)> setPwpFcSat)
: calculateAndSetPwpFcSat(kj::mv(setPwpFcSat)) {};

void SoilParameters::serialize(mas::schema::model::monica::SoilParameters::Builder builder) const {
  builder.setSoilSandContent(vs_SoilSandContent);
  builder.setSoilClayContent(vs_SoilClayContent);
  builder.setSoilpH(vs_SoilpH);
  builder.setSoilStoneContent(vs_SoilStoneContent);
  builder.setLambda(vs_Lambda);
  builder.setFieldCapacity(vs_FieldCapacity);
  builder.setSaturation(vs_Saturation);
  builder.setPermanentWiltingPoint(vs_PermanentWiltingPoint);
  builder.setSoilTexture(vs_SoilTexture);
  builder.setSoilAmmonium(vs_SoilAmmonium);
  builder.setSoilNitrate(vs_SoilNitrate);
  builder.setSoilCNRatio(vs_Soil_CN_Ratio);
  builder.setSoilMoisturePercentFC(vs_SoilMoisturePercentFC);
  builder.setSoilRawDensity(_vs_SoilRawDensity);
  builder.setSoilBulkDensity(_vs_SoilBulkDensity);
  builder.setSoilOrganicCarbon(_vs_SoilOrganicCarbon);
  builder.setSoilOrganicMatter(_vs_SoilOrganicMatter);
}

void SoilParameters::deserialize(mas::schema::model::monica::SoilParameters::Reader reader) {
  vs_SoilSandContent = reader.getSoilSandContent();
  vs_SoilClayContent = reader.getSoilClayContent();
  vs_SoilpH = reader.getSoilpH();
  vs_SoilStoneContent = reader.getSoilStoneContent();
  vs_Lambda = reader.getLambda();
  vs_FieldCapacity = reader.getFieldCapacity();
  vs_Saturation = reader.getSaturation();
  vs_PermanentWiltingPoint = reader.getPermanentWiltingPoint();
  vs_SoilTexture = reader.getSoilTexture();
  vs_SoilAmmonium = reader.getSoilAmmonium();
  vs_SoilNitrate = reader.getSoilNitrate();
  vs_Soil_CN_Ratio = reader.getSoilCNRatio();
  vs_SoilMoisturePercentFC = reader.getSoilMoisturePercentFC();
  _vs_SoilRawDensity = reader.getSoilRawDensity();
  _vs_SoilBulkDensity = reader.getSoilBulkDensity();
  _vs_SoilOrganicCarbon = reader.getSoilOrganicCarbon();
  _vs_SoilOrganicMatter = reader.getSoilOrganicMatter();
}

//SoilParameters::SoilParameters(json11::Json j) : calculateAndSetPwpFcSat(noSetPwpFcSat){
//  merge(j);
//}

Errors SoilParameters::merge(json11::Json j) {
  Errors es;

  set_double_value(vs_SoilSandContent, j, "Sand", transformIfPercent(j, "Sand"));
  set_double_value(vs_SoilClayContent, j, "Clay", transformIfPercent(j, "Clay"));
  set_double_value(vs_SoilpH, j, "pH");
  set_double_value(vs_SoilStoneContent, j, "Sceleton", transformIfPercent(j, "Sceleton"));
  set_double_value(vs_Lambda, j, "Lambda");
  set_double_value(vs_FieldCapacity, j, "FieldCapacity", transformIfPercent(j, "FieldCapacity"));
  set_double_value(vs_Saturation, j, "PoreVolume", transformIfPercent(j, "PoreVolume"));
  set_double_value(vs_PermanentWiltingPoint, j, "PermanentWiltingPoint",
                   transformIfPercent(j, "PermanentWiltingPoint"));
  set_string_value(vs_SoilTexture, j, "KA5TextureClass");
  set_double_value(vs_SoilAmmonium, j, "SoilAmmonium");
  set_double_value(vs_SoilNitrate, j, "SoilNitrate");
  set_double_value(vs_Soil_CN_Ratio, j, "CN");
  set_double_value(vs_SoilMoisturePercentFC, j, "SoilMoisturePercentFC");
  set_double_value(_vs_SoilRawDensity, j, "SoilRawDensity");
  set_double_value(_vs_SoilBulkDensity, j, "SoilBulkDensity");
  set_double_value(_vs_SoilOrganicCarbon, j, "SoilOrganicCarbon",
                   [](double soc) { return soc / 100.0; });
  set_double_value(_vs_SoilOrganicMatter, j, "SoilOrganicMatter",
                   transformIfPercent(j, "SoilOrganicMatter"));

  auto st = vs_SoilTexture;
  // use internally just uppercase chars
  vs_SoilTexture = Tools::toUpper(vs_SoilTexture);

  if (vs_SoilSandContent < 0 && !vs_SoilTexture.empty()) {
    auto res = KA5texture2sand(vs_SoilTexture);
    if (res.success()) vs_SoilSandContent = res.result;
    else es.append(res);
  }

  if (vs_SoilClayContent < 0 && !vs_SoilTexture.empty()) {
    auto res = KA5texture2clay(vs_SoilTexture);
    if (res.success()) vs_SoilClayContent = res.result;
    else es.append(res);
  }

  if (vs_SoilClayContent >= 0 && vs_SoilSandContent >= 0 && vs_SoilTexture.empty()) {
    vs_SoilTexture = sandAndClay2KA5texture(vs_SoilSandContent, vs_SoilClayContent);
  }

  // restrict sceleton to 80%, else FC, PWP and SAT could be calculated too low, so that the water transport algorithm gets unstable
  if (vs_SoilStoneContent > 0) vs_SoilStoneContent = min(vs_SoilStoneContent, 0.8);

  if (calculateAndSetPwpFcSat) es.append(calculateAndSetPwpFcSat(this));

  // restrict FC, PWP and SAT else the water transport algorithm gets instable
  if (vs_FieldCapacity < 0.05) {
    es.appendWarning(kj::str("Field capacity is too low (", vs_FieldCapacity * 100, "%). Is being set to 5%.").cStr());
    vs_FieldCapacity = 0.05;
  }
  if (vs_PermanentWiltingPoint < 0.01) {
    es.appendWarning(kj::str("Permanent wilting point is too low (", vs_PermanentWiltingPoint * 100,
                             "%). Is being set to 1%.").cStr());
    vs_PermanentWiltingPoint = 0.01;
  }
  if (vs_Saturation < 0.1) {
    es.appendWarning(kj::str("Saturation is too low (", vs_Saturation * 100, "%). Is being set to 10%.").cStr());
    vs_Saturation = 0.1;
  }

  if (vs_Lambda < 0 && vs_SoilSandContent > 0 && vs_SoilClayContent > 0) {
    vs_Lambda = sandAndClay2lambda(vs_SoilSandContent, vs_SoilClayContent);
  }

  if (!vs_SoilTexture.empty() && KA5texture2sand(vs_SoilTexture).failure()) {
    es.appendError(kj::str("KA5TextureClass (", st, ") is unknown.").cStr());
  }
  //if (vs_SoilSandContent < 0 || vs_SoilSandContent > 1.0){
  //  es.appendError(kj::str("Sand content (", vs_SoilSandContent, ") is out of bounds [0, 1].").cStr());
  //}
  if (vs_SoilClayContent < 0 || vs_SoilClayContent > 1.0) {
    es.appendError(kj::str("Clay content (", vs_SoilClayContent, ") is out of bounds [0, 1].").cStr());
  }
  if (vs_SoilpH < 0 || vs_SoilpH > 14) {
    es.appendError(kj::str("pH value (", vs_SoilpH, ") is out of bounds [0, 14].").cStr());
  }
  if (vs_SoilStoneContent < 0 || vs_SoilStoneContent > 1.0) {
    es.appendError(kj::str("Sceleton (", vs_SoilStoneContent, ") is out of bounds [0, 1].").cStr());
  }
  if (vs_FieldCapacity < 0 || vs_FieldCapacity > 1.0) {
    es.appendError(kj::str("FieldCapacity (", vs_FieldCapacity, ") is out of bounds [0, 1].").cStr());
  }
  if (vs_Saturation < 0 || vs_Saturation > 1.0) {
    es.appendError(kj::str("PoreVolume (", vs_Saturation, ") is out of bounds [0, 1].").cStr());
  }
  if (vs_PermanentWiltingPoint < 0 || vs_PermanentWiltingPoint > 1.0) {
    es.appendError(kj::str("PermanentWiltingPoint (", vs_PermanentWiltingPoint, ") is out of bounds [0, 1].").cStr());
  }
  if (vs_SoilMoisturePercentFC < 0 || vs_SoilMoisturePercentFC > 100) {
    es.appendError(kj::str("SoilMoisturePercentFC (", vs_SoilMoisturePercentFC, ") is out of bounds [0, 100].").cStr());
  }
  if (_vs_SoilBulkDensity < 0 && (_vs_SoilRawDensity < 0 || _vs_SoilRawDensity > 2000)) {
    es.appendWarning(kj::str("SoilRawDensity (", _vs_SoilRawDensity, ") is out of bounds [0, 2000].").cStr());
  }
  if (_vs_SoilRawDensity < 0 && (_vs_SoilBulkDensity < 0 || _vs_SoilBulkDensity > 2000)) {
    es.appendWarning(kj::str("SoilBulkDensity (", _vs_SoilBulkDensity, ") is out of bounds [0, 2000].").cStr());
  }
  if (_vs_SoilOrganicMatter < 0 && (_vs_SoilOrganicCarbon < 0 || _vs_SoilOrganicCarbon > 1.0)) {
    es.appendError(kj::str("SoilOrganicCarbon content (", _vs_SoilOrganicCarbon, ") is out of bounds [0, 1].").cStr());
  }
  if (_vs_SoilOrganicCarbon < 0 && (_vs_SoilOrganicMatter < 0 || _vs_SoilOrganicMatter > 1.0)) {
    es.appendError(kj::str("SoilOrganicMatter content (", _vs_SoilOrganicMatter, ") is out of bounds [0, 1].").cStr());
  }

  return es;
}

json11::Json SoilParameters::to_json() const {
  return J11Object{
    {"type", "SoilParameters"},
    {"Sand", J11Array{vs_SoilSandContent, "% [0-1]"}},
    {"Clay", J11Array{vs_SoilClayContent, "% [0-1]"}},
    {"pH", vs_SoilpH},
    {"Sceleton", J11Array{vs_SoilStoneContent, "vol% [0-1] (m3 m-3)"}},
    {"Lambda", vs_Lambda},
    {"FieldCapacity", J11Array{vs_FieldCapacity, "vol% [0-1] (m3 m-3)"}},
    {"PoreVolume", J11Array{vs_Saturation, "vol% [0-1] (m3 m-3)"}},
    {"PermanentWiltingPoint", J11Array{vs_PermanentWiltingPoint, "vol% [0-1] (m3 m-3)"}},
    {"KA5TextureClass", vs_SoilTexture},
    {"SoilAmmonium", J11Array{vs_SoilAmmonium, "kg NH4-N m-3"}},
    {"SoilNitrate", J11Array{vs_SoilNitrate, "kg NO3-N m-3"}},
    {"CN", vs_Soil_CN_Ratio},
    {"SoilRawDensity", J11Array{_vs_SoilRawDensity, "kg m-3"}},
    {"SoilBulkDensity", J11Array{_vs_SoilBulkDensity, "kg m-3"}},
    {"SoilOrganicCarbon", J11Array{_vs_SoilOrganicCarbon * 100.0, "mass% [0-100]"}},
    {"SoilOrganicMatter", J11Array{_vs_SoilOrganicMatter, "mass% [0-1]"}},
    {"SoilMoisturePercentFC", J11Array{vs_SoilMoisturePercentFC, "% [0-100]"}}
  };
}

void CapillaryRiseRates::addRate(const std::string& soilType, size_t distance, double value) {
  //    std::cout << "Add cap rate: " << bodart.c_str() << "\tdist: " << distance << "\tvalue: " << value << std::endl;
  //cap_rates_map.insert(std::pair<std::string,std::map<int,double> >(bodart,std::pair<int,double>(distance,value)));
  capillaryRiseRates[soilType][distance] = value;
}

/**
   * Returns capillary rise rate for given soil type and distance to ground water.
   */
double CapillaryRiseRates::getRate(const std::string& soilType, size_t distance) const {
  auto it = capillaryRiseRates.find(soilType);
  if (it == capillaryRiseRates.end()) {
    it = capillaryRiseRates.find(soilType.substr(0, 3));
    if (it == capillaryRiseRates.end()) {
      it = capillaryRiseRates.find(soilType.substr(0, 2));
      if (it == capillaryRiseRates.end()) return 0.0;
    }
  }
  auto it2 = it->second.find(distance);
  if (it2 == it->second.end()) return 0.0;
  return it2->second;
}

const CapillaryRiseRates& Soil::readCapillaryRiseRates() {
  static mutex lockable;
  static bool initialized = false;
  static CapillaryRiseRates cap_rates;

  if (!initialized) {
    lock_guard<mutex> lock(lockable);

    if (!initialized) {
      auto cacheAllData = [](mas::schema::soil::CapillaryRiseRate::Reader data) {
        for (const auto& scd : data.getList()) {
          cap_rates.addRate(Tools::toUpper(scd.getSoilType()), scd.getDistance(), scd.getRate());
        }
      };

      auto fs = kj::newDiskFilesystem();
#ifdef _WIN32
      auto pathToMonicaParamsSoil = fs->getCurrentPath().evalWin32(replaceEnvVars("${MONICA_PARAMETERS}\\soil\\"));
#else
      auto pathToMonicaParamsSoil = fs->getCurrentPath().eval(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
#endif
      try {
        KJ_IF_MAYBE(file, fs->getRoot().tryOpenFile(pathToMonicaParamsSoil.append("CapillaryRiseRates.sercapnp"))) {
          auto allBytes = (*file)->readAllBytes();
          kj::ArrayInputStream aios(allBytes);
          capnp::InputStreamMessageReader message(aios);
          cacheAllData(message.getRoot<mas::schema::soil::CapillaryRiseRate>());
        } else
          KJ_IF_MAYBE(file2, fs->getRoot().tryOpenFile(pathToMonicaParamsSoil.append("CapillaryRiseRates.json"))) {
            capnp::JsonCodec json;
            capnp::MallocMessageBuilder msg;
            auto builder = msg.initRoot<mas::schema::soil::CapillaryRiseRate>();
            json.decode((*file2)->readAllBytes().asChars(), builder);
            cacheAllData(builder.asReader());
          }

        initialized = true;
      } catch (const kj::Exception& e) {
        cout << "Error: couldn't read CapillaryRiseRates.sercapnp or CapillaryRiseRates.json from folder "
          << pathToMonicaParamsSoil.toString().cStr() << " ! Exception: " << e.getDescription().cStr() << endl;
      }
      initialized = true;
    }
  }

  return cap_rates;
}

bool SoilParameters::isValid() const {
  bool is_valid = true;

  if (vs_FieldCapacity < 0) {
    debug() << "SoilParameters::Error: No field capacity defined in database for " << vs_SoilTexture
      << " , RawDensity: " << _vs_SoilRawDensity << endl;
    is_valid = false;
  }
  if (vs_Saturation < 0) {
    debug() << "SoilParameters::Error: No saturation defined in database for " << vs_SoilTexture << " , RawDensity: "
      << _vs_SoilRawDensity << endl;
    is_valid = false;
  }
  if (vs_PermanentWiltingPoint < 0) {
    debug() << "SoilParameters::Error: No saturation defined in database for " << vs_SoilTexture << " , RawDensity: "
      << _vs_SoilRawDensity << endl;
    is_valid = false;
  }

  if (vs_SoilSandContent < 0) {
    debug() << "SoilParameters::Error: Invalid soil sand content: " << vs_SoilSandContent << endl;
    is_valid = false;
  }

  if (vs_SoilClayContent < 0) {
    debug() << "SoilParameters::Error: Invalid soil clay content: " << vs_SoilClayContent << endl;
    is_valid = false;
  }

  if (vs_SoilpH < 0) {
    debug() << "SoilParameters::Error: Invalid soil ph value: " << vs_SoilpH << endl;
    is_valid = false;
  }

  if (vs_SoilStoneContent < 0) {
    debug() << "SoilParameters::Error: Invalid soil stone content: " << vs_SoilStoneContent << endl;
    is_valid = false;
  }

  if (vs_Saturation < 0) {
    debug() << "SoilParameters::Error: Invalid value for saturation: " << vs_Saturation << endl;
    is_valid = false;
  }

  if (vs_PermanentWiltingPoint < 0) {
    debug() << "SoilParameters::Error: Invalid value for permanent wilting point: " << vs_PermanentWiltingPoint << endl;
    is_valid = false;
  }
  /*
  if (_vs_SoilRawDensity<0) {
      cout << "SoilParameters::Error: Invalid soil raw density: "<< _vs_SoilRawDensity << endl;
      is_valid = false;
  }
  */
  return is_valid;
}

/**
 * @brief Returns raw density of soil
 * @return raw density of soil
 */
double SoilParameters::vs_SoilRawDensity() const {
  auto srd =
    _vs_SoilRawDensity < 0
      ? ((_vs_SoilBulkDensity / 1000.0) - (0.009 * 100.0 * vs_SoilClayContent)) * 1000.0
      : _vs_SoilRawDensity;

  return srd;
}

/**
* @brief Getter for soil bulk density.
* @return bulk density
*/
double SoilParameters::vs_SoilBulkDensity() const {
  auto sbd =
    _vs_SoilBulkDensity < 0
      ? ((_vs_SoilRawDensity / 1000.0) + (0.009 * 100.0 * vs_SoilClayContent)) * 1000.0
      : _vs_SoilBulkDensity;

  return sbd;
}

/**
 * @brief Returns soil organic carbon.
 * @return soil organic carbon
 */
double SoilParameters::vs_SoilOrganicCarbon() const {
  return _vs_SoilOrganicCarbon < 0
           ? _vs_SoilOrganicMatter * OrganicConstants::po_SOM_to_C
           : _vs_SoilOrganicCarbon;
}

/**
 * @brief Getter for soil organic matter.
 * @return Soil organic matter
 */
double SoilParameters::vs_SoilOrganicMatter() const {
  return _vs_SoilOrganicMatter < 0
           ? _vs_SoilOrganicCarbon / OrganicConstants::po_SOM_to_C
           : _vs_SoilOrganicMatter;
}

/**
 * @brief Returns lambda from soil texture
 * @param sand
 * @param clay
 * @return
 */
double SoilParameters::sandAndClay2lambda(double sand, double clay) {
  return ::sandAndClay2lambda(sand, clay);
}

EResult<SoilPMs> Soil::createEqualSizedSoilPMs(const std::function<Errors(SoilParameters*, int)>& setPwpFcSat,
                                               const J11Array& jsonSoilPMs, double layerThickness, int numberOfLayers) {
  Errors errors;

  SoilPMs soilPMs;
  int layerCount = 0;
  for (size_t spi = 0, spsCount = jsonSoilPMs.size(); spi < spsCount; spi++) {
    const Json& sp = jsonSoilPMs.at(spi);

    //repeat layers if there is an associated Thickness parameter
    int repeatLayer = 1;
    if (!sp["Thickness"].is_null()) {
      auto transf = transformIfNotMeters(sp, "Thickness");
      const auto lt = transf(double_valueD(sp, "Thickness", layerThickness));
      auto noOfMonicaLayers = static_cast<int>(Tools::round(lt / layerThickness));
      repeatLayer = min(max(1, noOfMonicaLayers), numberOfLayers - layerCount);
    }

    //simply repeat the last layer as often as necessary to fill the 20 layers
    if (spi + 1 == spsCount) repeatLayer = numberOfLayers - layerCount;

    for (int i = 1; i <= repeatLayer; i++) {
      SoilParameters sps([=](SoilParameters* sp) { return setPwpFcSat(sp, i); });
      auto es = sps.merge(sp);
      soilPMs.push_back(sps);
      if (es.failure()) {
        errors.appendError(kj::str("Config-layer:", spi + 1, "Monica-layer:", layerCount + i, ":").cStr());
        errors.append(es);
      }
    }

    layerCount += repeatLayer;
  }

  return {soilPMs, errors};
}

EResult<SoilPMs> Soil::createSoilPMs(const std::function<Errors(SoilParameters*)>& setPwpFcSat,
                                     const J11Array& jsonSoilPMs) {
  Errors errors;
  SoilPMs soilPMs;
  int layerCount = 0;
  for (const auto& sp : jsonSoilPMs) {
    SoilParameters sps(setPwpFcSat);;
    auto es = sps.merge(sp);
    auto transf = transformIfNotMeters(sp, "Thickness");
    const auto lt = transf(double_valueD(sp, "Thickness", 0.1));
    sps.thickness = lt;
    soilPMs.push_back(sps);
    if (es.failure()) errors.append(es);
  }

  return {soilPMs, errors};
}


namespace {
struct RPSCDRes {
  double sat{0.0}; // [m3 m-3]
  double fc{0.0}; // [m3 m-3]
  double pwp{0.0}; // [m3 m-3]
  bool unset{true};
};

EResult<RPSCDRes> readPrincipalSoilCharacteristicData(const std::string& pathToSoilDir, const string& soilType,
                                                      double rawDensity) {
  static mutex lockable;
  typedef map<int, RPSCDRes> M1;
  typedef map<string, M1> M2;
  static M2 m;
  static bool initialized = false;
  static Errors errors;
  if (!initialized) {
    lock_guard<mutex> lock(lockable);

    if (!initialized) {
      auto cacheAllData = [](mas::schema::soil::SoilCharacteristicData::Reader data) {
        for (const auto& scd : data.getList()) {
          const double ac = scd.getAirCapacity();
          const double fc = scd.getFieldCapacity();
          const double nfc = scd.getNFieldCapacity();

          RPSCDRes r;
          r.sat = ac + fc;
          r.fc = fc;
          r.pwp = fc - nfc;
          r.unset = false;

          m[Tools::toUpper(scd.getSoilType())][int(scd.getSoilRawDensity() / 100.0)] = r;
        }
      };

      auto fs = kj::newDiskFilesystem();
#ifdef _WIN32
      auto pathToMonicaParamsSoil = fs->getCurrentPath().evalWin32(pathToSoilDir);
#else
      auto pathToMonicaParamsSoil = fs->getCurrentPath().eval(pathToSoilDir);
#endif
      try {
        KJ_IF_MAYBE(file, fs->getRoot().tryOpenFile(pathToMonicaParamsSoil.append("SoilCharacteristicData.sercapnp"))) {
          auto allBytes = (*file)->readAllBytes();
          kj::ArrayInputStream aios(allBytes);
          capnp::InputStreamMessageReader message(aios);
          cacheAllData(message.getRoot<mas::schema::soil::SoilCharacteristicData>());
        } else
          KJ_IF_MAYBE(file2,
                    fs->getRoot().tryOpenFile(pathToMonicaParamsSoil.append("SoilCharacteristicData.json"))) {
            capnp::JsonCodec json;
            capnp::MallocMessageBuilder msg;
            auto builder = msg.initRoot<mas::schema::soil::SoilCharacteristicData>();
            json.decode((*file2)->readAllBytes().asChars(), builder);
            cacheAllData(builder.asReader());
          } else {
            errors.
              appendError(kj::str("Wessolek2009: Could neither load SoilCharacteristicData.sercapnp nor SoilCharacteristicData.json from folder ",
                                  pathToMonicaParamsSoil.toString(), ". No PWP, FC, SAT calculation possible!").cStr());
          }

        initialized = true;
      } catch (const kj::Exception& e) {
        errors.
          appendError(kj::str("Wessolek2009: Couldn't read SoilCharacteristicData.sercapnp nor SoilCharacteristicData.json from folder ",
                              pathToMonicaParamsSoil.toString(), " ! Exception: ", e.getDescription()).cStr());
      }
    }
  }

  auto ci = m.find(soilType);
  if (ci != m.end()) {
    int rd10 = int(rawDensity * 10);
    int delta = rd10 < 15 ? 2 : -2;

    M1::const_iterator ci2;
    //if we didn't find values for a given raw density, e.g. 1.1 (= 11)
    //we try to find the closest next one (up (1.1) or down (1.9))
    while ((ci2 = ci->second.find(rd10)) == ci->second.end() && (11 <= rd10 && rd10 <= 19)) {
      rd10 += delta;
    }

    return ci2 != ci->second.end()
             ? ci2->second
             : EResult<RPSCDRes>(RPSCDRes(), kj::str("Couldn't find soil characteristic data for soil type ", soilType,
                                                     " and raw density ", rawDensity).cStr());
  }

  return {{}, errors};
}

EResult<RPSCDRes> readSoilCharacteristicModifier(const std::string& pathToSoilDir, const string& soilType,
                                                 double organicMatter) {
  static mutex lockable;
  typedef map<int, RPSCDRes> M1;
  typedef map<string, M1> M2;
  static M2 m;
  static bool initialized = false;
  static Errors errors;
  if (!initialized) {
    lock_guard<mutex> lock(lockable);

    if (!initialized) {
      auto cacheAllData = [](mas::schema::soil::SoilCharacteristicModifier::Reader data) {
        for (const auto& scd : data.getList()) {
          const double ac = scd.getAirCapacity();
          const double fc = scd.getFieldCapacity();
          const double nfc = scd.getNFieldCapacity();

          RPSCDRes r;
          r.sat = ac + fc;
          r.fc = fc;
          r.pwp = fc - nfc;
          r.unset = false;

          m[Tools::toUpper(scd.getSoilType())][int(scd.getOrganicMatter() * 10)] = r;
        }
      };

      auto fs = kj::newDiskFilesystem();
#ifdef _WIN32
      auto pathToMonicaParamsSoil = fs->getCurrentPath().evalWin32(pathToSoilDir);
#else
      auto pathToMonicaParamsSoil = fs->getCurrentPath().eval(pathToSoilDir);
#endif
      try {
        KJ_IF_MAYBE(file,
                    fs->getRoot().tryOpenFile(pathToMonicaParamsSoil.append("SoilCharacteristicModifier.sercapnp"))) {
          auto allBytes = (*file)->readAllBytes();
          kj::ArrayInputStream aios(allBytes);
          capnp::InputStreamMessageReader message(aios);
          cacheAllData(message.getRoot<mas::schema::soil::SoilCharacteristicModifier>());
        } else
          KJ_IF_MAYBE(file2, fs->getRoot().tryOpenFile(
                      pathToMonicaParamsSoil.append("SoilCharacteristicModifier.json"))) {
            capnp::JsonCodec json;
            capnp::MallocMessageBuilder msg;
            auto builder = msg.initRoot<mas::schema::soil::SoilCharacteristicModifier>();
            json.decode((*file2)->readAllBytes().asChars(), builder);
            cacheAllData(builder.asReader());
          } else {
            errors.
              appendError(kj::str("Wessolek2009: Could neither load SoilCharacteristicModifier.sercapnp nor SoilCharacteristicModifier.json from folder ",
                                  pathToMonicaParamsSoil.toString(), ". No PWP, FC, SAT calculation possible!").cStr());
          }

        initialized = true;
      } catch (const kj::Exception& e) {
        errors.
          appendError(kj::str("Couldn't read SoilCharacteristicModifier.sercapnp nor SoilCharacteristicModifier.json from folder ",
                              pathToMonicaParamsSoil.toString(), " ! Exception: ", e.getDescription()).cStr());
      }
    }
  }

  auto ci = m.find(Tools::toUpper(soilType));
  if (ci != m.end()) {
    auto ci2 = ci->second.find(int(organicMatter * 10));
    return ci2 != ci->second.end()
             ? ci2->second
             : EResult<RPSCDRes>(RPSCDRes(), kj::str("Couldn't find soil characteristic data for soil type ", soilType,
                                                     " and organic matter ", organicMatter).cStr());
  }

  return {{}, errors};
}

struct FcSatPwp {
  double fc{0.0};
  double sat{0.0};
  double pwp{0.0};
};

EResult<FcSatPwp> fcSatPwpFromKA5textureClass(const std::string& pathToSoilDir,
                                              std::string texture,
                                              double stoneContent,
                                              double soilRawDensity,
                                              double soilOrganicMatter) {
  debug() << "soilCharacteristicsKA5" << endl;
  texture = Tools::toUpper(texture);

  if (texture.empty()) return EResult<FcSatPwp>({}, "No soil texture given.");

  FcSatPwp res;
  double srd = soilRawDensity / 1000.0; // [kg m-3] -> [g cm-3]
  double som = soilOrganicMatter * 100.0; // [kg kg-1] -> [%]

  // ***************************************************************************
  // *** The following boundaries are extracted from:            ***
  // *** Wessolek, G., M. Kaupenjohann, M. Renger (2009) Bodenphysikalische  ***
  // *** Kennwerte und Berechnungsverfahren für die Praxis. Bodenökologie  ***
  // *** und Bodengenese 40, Selbstverlag Technische Universität Berlin    ***
  // *** (Tab. 4).                               ***
  // ***************************************************************************

  double srd_lowerBound = 0.0;
  double srd_upperBound = 0.0;
  if (srd < 1.1) {
    srd_lowerBound = 1.1;
    srd_upperBound = 1.1;
  } else if ((srd >= 1.1) && (srd < 1.3)) {
    srd_lowerBound = 1.1;
    srd_upperBound = 1.3;
  } else if ((srd >= 1.3) && (srd < 1.5)) {
    srd_lowerBound = 1.3;
    srd_upperBound = 1.5;
  } else if ((srd >= 1.5) && (srd < 1.7)) {
    srd_lowerBound = 1.5;
    srd_upperBound = 1.7;
  } else if ((srd >= 1.7) && (srd < 1.9)) {
    srd_lowerBound = 1.7;
    srd_upperBound = 1.9;
  } else if (srd >= 1.9) {
    srd_lowerBound = 1.9;
    srd_upperBound = 1.9;
  }

  // special treatment for "torf" soils
  if (texture == "HH" || texture == "HN") {
    srd_lowerBound = -1;
    srd_upperBound = -1;
  }

  // Boundaries for linear interpolation
  auto lbRes = readPrincipalSoilCharacteristicData(pathToSoilDir, texture, srd_lowerBound);
  if (lbRes.failure()) return EResult<FcSatPwp>({}, lbRes.errors);
  double sat_lowerBound = lbRes.result.sat;
  double fc_lowerBound = lbRes.result.fc;
  double pwp_lowerBound = lbRes.result.pwp;

  auto ubRes = readPrincipalSoilCharacteristicData(pathToSoilDir, texture, srd_upperBound);
  if (ubRes.failure()) return EResult<FcSatPwp>({}, ubRes.errors);
  double sat_upperBound = ubRes.result.sat;
  double fc_upperBound = ubRes.result.fc;
  double pwp_upperBound = ubRes.result.pwp;

  //  cout << "Soil Raw Density:\t" << vs_SoilRawDensity << endl;
  //  cout << "Saturation:\t\t" << vs_SaturationLowerBoundary << "\t" << vs_SaturationUpperBoundary << endl;
  //  cout << "Field Capacity:\t" << vs_FieldCapacityLowerBoundary << "\t" << vs_FieldCapacityUpperBoundary << endl;
  //  cout << "PermanentWP:\t" << vs_PermanentWiltingPointLowerBoundary << "\t" << vs_PermanentWiltingPointUpperBoundary << endl;
  //  cout << "Soil Organic Matter:\t" << vs_SoilOrganicMatter << endl;

  // ***************************************************************************
  // *** The following boundaries are extracted from:            ***
  // *** Wessolek, G., M. Kaupenjohann, M. Renger (2009) Bodenphysikalische  ***
  // *** Kennwerte und Berechnungsverfahren für die Praxis. Bodenökologie  ***
  // *** und Bodengenese 40, Selbstverlag Technische Universität Berlin    ***
  // *** (Tab. 5).                               ***
  // ***************************************************************************

  double som_lowerBound = 0.0;
  double som_upperBound = 0.0;

  if (som >= 0.0 && som < 1.0) {
    som_lowerBound = 0.0;
    som_upperBound = 0.0;
  } else if (som >= 1.0 && som < 1.5) {
    som_lowerBound = 0.0;
    som_upperBound = 1.5;
  } else if (som >= 1.5 && som < 3.0) {
    som_lowerBound = 1.5;
    som_upperBound = 3.0;
  } else if (som >= 3.0 && som < 6.0) {
    som_lowerBound = 3.0;
    som_upperBound = 6.0;
  } else if (som >= 6.0 && som < 11.5) {
    som_lowerBound = 6.0;
    som_upperBound = 11.5;
  } else if (som >= 11.5) {
    som_lowerBound = 11.5;
    som_upperBound = 11.5;
  }

  // special treatment for "torf" soils
  if (texture == "HH" || texture == "HN") {
    som_lowerBound = 0.0;
    som_upperBound = 0.0;
  }

  // Boundaries for linear interpolation
  double fc_mod_lowerBound = 0.0;
  double sat_mod_lowerBound = 0.0;
  double pwp_mod_lowerBound = 0.0;
  // modifier values are given only for organic matter > 1.0% (class h2)
  if (som_lowerBound != 0.0) {
    auto lbRes2 = readSoilCharacteristicModifier(pathToSoilDir, texture, som_lowerBound);
    if (lbRes2.failure()) return EResult<FcSatPwp>({}, lbRes2.errors);
    sat_mod_lowerBound = lbRes2.result.sat;
    fc_mod_lowerBound = lbRes2.result.fc;
    pwp_mod_lowerBound = lbRes2.result.pwp;
  }

  double fc_mod_upperBound = 0.0;
  double sat_mod_upperBound = 0.0;
  double pwp_mod_upperBound = 0.0;
  if (som_upperBound != 0.0) {
    auto ubRes2 = readSoilCharacteristicModifier(pathToSoilDir, texture, som_upperBound);
    if (ubRes2.failure()) return EResult<FcSatPwp>({}, ubRes2.errors);
    sat_mod_upperBound = ubRes2.result.sat;
    fc_mod_upperBound = ubRes2.result.fc;
    pwp_mod_upperBound = ubRes2.result.pwp;
  }

  //   cout << "Saturation-Modifier:\t" << sat_mod_lowerBound << "\t" << sat_mod_upperBound << endl;
  //   cout << "Field capacity-Modifier:\t" << fc_mod_lowerBound << "\t" << fc_mod_upperBound << endl;
  //   cout << "PWP-Modifier:\t" << pwp_mod_lowerBound << "\t" << pwp_mod_upperBound << endl;

  // Linear interpolation
  double fcUnmod = fc_lowerBound;
  if (fc_upperBound < 0.5 && fc_lowerBound >= 1.0) fcUnmod = fc_lowerBound;
  else if (fc_lowerBound < 0.5 && fc_upperBound >= 1.0) fcUnmod = fc_upperBound;
  else if (srd_upperBound != srd_lowerBound) {
    fcUnmod = (srd - srd_lowerBound) /
              (srd_upperBound - srd_lowerBound) *
              (fc_upperBound - fc_lowerBound) + fc_lowerBound;
  }

  double satUnmod = sat_lowerBound;
  if (sat_upperBound < 0.5 && sat_lowerBound >= 1.0) satUnmod = sat_lowerBound;
  else if (sat_lowerBound < 0.5 && sat_upperBound >= 1.0) satUnmod = sat_upperBound;
  else if (srd_upperBound != srd_lowerBound) {
    satUnmod = (srd - srd_lowerBound) /
               (srd_upperBound - srd_lowerBound) *
               (sat_upperBound - sat_lowerBound) + sat_lowerBound;
  }

  double pwpUnmod = pwp_lowerBound;
  if (pwp_upperBound < 0.5 && pwp_lowerBound >= 1.0) pwpUnmod = pwp_lowerBound;
  else if (pwp_lowerBound < 0.5 && pwp_upperBound >= 1.0) pwpUnmod = pwp_upperBound;
  else if (srd_upperBound != srd_lowerBound) {
    pwpUnmod = (srd - srd_lowerBound) /
               (srd_upperBound - srd_lowerBound) *
               (pwp_upperBound - pwp_lowerBound) + pwp_lowerBound;
  }

  //in this case upper and lower boundary are equal, so doesn't matter.
  double fcMod = fc_mod_lowerBound;
  double satMod = sat_mod_lowerBound;
  double pwpMod = pwp_mod_lowerBound;
  if (som_upperBound != som_lowerBound) {
    fcMod = (som - som_lowerBound) /
            (som_upperBound - som_lowerBound) *
            (fc_mod_upperBound - fc_mod_lowerBound) + fc_mod_lowerBound;

    satMod = (som - som_lowerBound) /
             (som_upperBound - som_lowerBound) *
             (sat_mod_upperBound - sat_mod_lowerBound) + sat_mod_lowerBound;

    pwpMod = (som - som_lowerBound) /
             (som_upperBound - som_lowerBound) *
             (pwp_mod_upperBound - pwp_mod_lowerBound) + pwp_mod_lowerBound;
  }

  // Modifying the principal values by organic matter
  res.fc = (fcUnmod + fcMod) / 100.0; // [m3 m-3]
  res.sat = (satUnmod + satMod) / 100.0; // [m3 m-3]
  res.pwp = (pwpUnmod + pwpMod) / 100.0; // [m3 m-3]

  // Modifying the principal values by stone content
  res.fc *= (1.0 - stoneContent);
  res.sat *= (1.0 - stoneContent);
  res.pwp *= (1.0 - stoneContent);

  debug() << "SoilTexture:\t\t\t" << texture << endl;
  debug() << "Saturation:\t\t\t" << res.sat << endl;
  debug() << "FieldCapacity:\t\t" << res.fc << endl;
  debug() << "PermanentWiltingPoint:\t" << res.pwp << endl << endl;

  return res;
}

FcSatPwp fcSatPwpFromVanGenuchtenVereecken(double sandFrac,
                                           double clayFrac,
                                           double stoneFrac,
                                           double bulkDensityKgPerM3,
                                           double organicCarbonFrac) {
  FcSatPwp res;
  res.pwp = (0.015 + 0.5 * clayFrac + 1.4 * organicCarbonFrac) * (1.0 - stoneFrac);
  res.sat = (0.81 - 0.283 * (bulkDensityKgPerM3 / 1000.0) + 0.1 * clayFrac) * (1.0 - stoneFrac);

  //***** Van Genuchten retention curve to calculate volumetric water content at
  //***** moisture equivalent (Field capacity definition KA5)
  double fc_pF = 2.1;
  if (sandFrac > 0.48 && sandFrac <= 0.9 && clayFrac <=
      0.12)
    fc_pF = 2.1 - (0.476 * (sandFrac - 0.48));
  else if (sandFrac > 0.9 && clayFrac <= 0.05) fc_pF = 1.9;
  else if (clayFrac > 0.45) fc_pF = 2.5;
  else if (clayFrac > 0.30 && sandFrac < 0.2) fc_pF = 2.4;
  else if (clayFrac > 0.35) fc_pF = 2.3;
  else if (clayFrac > 0.25 && sandFrac < 0.1) fc_pF = 2.3;
  else if (clayFrac > 0.17 && sandFrac > 0.68) fc_pF = 2.2;
  else if (clayFrac > 0.17 && sandFrac < 0.33) fc_pF = 2.2;
  else if (clayFrac > 0.08 && sandFrac < 0.27) fc_pF = 2.2;
  else if (clayFrac > 0.25 && sandFrac < 0.25) fc_pF = 2.2;

  double matricHead = pow(10, fc_pF);

  auto vgps = calcVanGenuchtenVereeckenParams(res.pwp, res.sat, sandFrac, clayFrac,
                                              bulkDensityKgPerM3, organicCarbonFrac, stoneFrac, matricHead);
  res.fc = vgps.volumetricWaterContentAtMatricHead;
  return res;
}


FcSatPwp fcSatPwpFromVanGenuchtenToth(bool isTopSoil,
                                      double sandFrac,
                                      double clayFrac,
                                      double stoneFrac,
                                      double bulkDensityKgPerM3,
                                      double organicCarbonFrac) {
  FcSatPwp res;

  //***** Van Genuchten Toth retention curve to calculate volumetric water content at
  //***** moisture equivalent, Field capacity and wilting point
  // hFC = -100 for coarse soils
  // hFC = -330 for medium/fine soils
  double fcMatricHead = sandFrac > 0.60 ? -100 : -330;
  auto fcVgps = calcVanGenuchtenTothParams(isTopSoil, sandFrac, clayFrac, bulkDensityKgPerM3, organicCarbonFrac,
                                           stoneFrac, fcMatricHead);
  res.sat = fcVgps.thetaS;
  res.fc = fcVgps.volumetricWaterContentAtMatricHead;

  double pwpMatricHead = -15000;
  auto pwpVgps = calcVanGenuchtenTothParams(isTopSoil, sandFrac, clayFrac, bulkDensityKgPerM3, organicCarbonFrac,
                                            stoneFrac, pwpMatricHead);
  res.pwp = pwpVgps.volumetricWaterContentAtMatricHead;

  // I need to set a safegard
  if (res.pwp <= pwpVgps.thetaR) res.pwp = fcVgps.thetaR + 0.01;

  return res;
}


FcSatPwp fcSatPwpFromToth(double sandContent,
                          double clayContent,
                          double stoneContent,
                          double soilBulkDensity,
                          double soilOrganicCarbon) {
  FcSatPwp res;
  res.sat = (0.81 - 0.283 * (soilBulkDensity / 1000.0) + 0.1 * clayContent) * (1.0 - stoneContent);
  // sat function from MONICA, maybe not necessary

  double sluf = 100.0 - clayContent * 100.0 - sandContent * 100.0;
  // transform from [0 to 1] to [0 to 100] , in the future, I will change and put the conversions inside the functions
  double ton = clayContent * 100.0;
  double oc = soilOrganicCarbon * 100.0; // The SOC was 0.001 from the input, that’s why I added this line

  res.fc = 0.24490 - 0.1887 * (1 / (oc + 1)) + 0.0045270 * ton + 0.001535 * sluf +
           0.001442 * sluf * (1 / (oc + 1)) - 0.0000511 * sluf * ton +
           0.0008676 * ton * (1 / (oc + 1));

  res.pwp = 0.09878 + 0.002127 * ton - 0.0008366 * sluf - 0.0767 * (1 / (oc + 1)) +
            0.00003853 * sluf * ton + 0.00233 * ton * (1 / (oc + 1)) +
            0.0009498 * sluf * (1 / (oc + 1));

  return res;
}

Errors updateUnsetPwpFcSatFromKA5textureClass(const std::string& pathToSoilDir, SoilParameters* sp) {
  if (sp->vs_SoilTexture.empty()) return {"No soil texture defined!"};

  // we only need to update something, if any of the values is not already set
  if (sp->vs_FieldCapacity < 0 || sp->vs_Saturation < 0 || sp->vs_PermanentWiltingPoint < 0) {
    auto res = fcSatPwpFromKA5textureClass(pathToSoilDir,
                                           sp->vs_SoilTexture,
                                           sp->vs_SoilStoneContent,
                                           sp->vs_SoilRawDensity(),
                                           sp->vs_SoilOrganicMatter());
    if (res.failure()) return res.errors;
    if (sp->vs_FieldCapacity < 0) sp->vs_FieldCapacity = res.result.fc;
    if (sp->vs_Saturation < 0) sp->vs_Saturation = res.result.sat;
    if (sp->vs_PermanentWiltingPoint < 0) sp->vs_PermanentWiltingPoint = res.result.pwp;
  }
  return {};
}
}

VanGenuchtenParams Soil::calcVanGenuchtenVereeckenParams(double pwp, double sat,
                                                         double sandFrac, double clayFrac,
                                                         double bulkDensityKgPerM3,
                                                         double organicCarbonFrac,
                                                         double stoneFrac, double matricHead) {
  VanGenuchtenParams res;
  res.thetaR = pwp;
  res.thetaS = sat;
  res.alpha = exp(-2.486
                  + 2.5 * sandFrac
                  - 35.1 * organicCarbonFrac
                  - 2.617 * (bulkDensityKgPerM3 / 1000.0)
                  - 2.3 * clayFrac);

  res.m = 1.0;
  res.n = exp(0.053
              - 0.9 * sandFrac
              - 1.3 * clayFrac
              + 1.5 * (pow(sandFrac, 2.0)));

  if (stoneFrac >= 0)
    res.volumetricWaterContentAtMatricHead = (res.thetaR + ((res.thetaS - res.thetaR) /
                                                            (pow(1.0 + pow(res.alpha * matricHead, res.n), res.m))))
                                             * (1.0 - stoneFrac);
  return res;
}

VanGenuchtenParams Soil::calcVanGenuchtenTothParams(bool isTopSoil,
                                                    double sandFrac,
                                                    double clayFrac,
                                                    double bulkDensityKgPerM3,
                                                    double organicCarbonFrac,
                                                    double stoneFrac,
                                                    double matricHead) {
  VanGenuchtenParams res;
  double sandContentPerc = sandFrac * 100;
  double clayContentPerc = clayFrac * 100;
  double siltContentPerc = 100 - sandContentPerc - clayContentPerc;
  double soilOrganicCarbonPerc = organicCarbonFrac * 100;
  double soilBulkDensityGPerCm3 = bulkDensityKgPerM3 / 1000;

  //cout << "PWP and FC are calculated with Toth-estimated van Genuchten parameters" << endl;
  if (sandContentPerc >= 20) res.thetaR = 0.041;
  else if (sandContentPerc < 20) res.thetaR = 0.179;
  res.thetaS = 0.8308 - 0.28217 * soilBulkDensityGPerCm3 + 0.0002728 * clayContentPerc + 0.000187 * siltContentPerc;

  res.alpha = pow(10, -0.43348
                      - 0.41729 * soilBulkDensityGPerCm3
                      - 0.04762 * soilOrganicCarbonPerc
                      - 0.2181 * (isTopSoil ? 1 : 0)
                      - 0.01581 * clayContentPerc
                      - 0.01207 * siltContentPerc);
  res.n = pow(10, 0.22236
                  - 0.30189 * soilBulkDensityGPerCm3
                  - 0.05558 * (isTopSoil ? 1 : 0)
                  - 0.005306 * clayContentPerc
                  - 0.003084 * siltContentPerc
                  - 0.01072 * soilOrganicCarbonPerc) + 1;
  res.m = 1 - 1 / res.n;

  //***** Van Genuchten Toth retention curve to calculate volumetric water content at
  //***** moisture equivalent, Field capacity and wilting point
  res.volumetricWaterContentAtMatricHead = (res.thetaR + ((res.thetaS - res.thetaR) /
                                                          (pow(1.0 + pow(res.alpha * abs(matricHead), res.n), res.m))))
                                           * (1.0 - stoneFrac);
  return res;
}

std::function<Errors(SoilParameters*, int)> Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(
  const std::string& pathToSoilDir) {
  return [pathToSoilDir](SoilParameters* sp, int) {
    return updateUnsetPwpFcSatFromKA5textureClass(pathToSoilDir, sp);
  };
}

Errors Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken(SoilParameters* sp, int) {
  if (sp->vs_FieldCapacity < 0 || sp->vs_Saturation < 0 || sp->vs_PermanentWiltingPoint < 0) {
    auto res = fcSatPwpFromVanGenuchtenVereecken(sp->vs_SoilSandContent,
                                                 sp->vs_SoilClayContent,
                                                 sp->vs_SoilStoneContent,
                                                 sp->vs_SoilBulkDensity(),
                                                 sp->vs_SoilOrganicCarbon());
    if (sp->vs_FieldCapacity < 0) sp->vs_FieldCapacity = res.fc;
    if (sp->vs_Saturation < 0) sp->vs_Saturation = res.sat;
    if (sp->vs_PermanentWiltingPoint < 0) sp->vs_PermanentWiltingPoint = res.pwp;
  }
  return {};
}

Errors Soil::updateUnsetPwpFcSatFromVanGenuchtenToth(SoilParameters* sp, int layerNo) {
  if (sp->vs_FieldCapacity < 0 || sp->vs_Saturation < 0 || sp->vs_PermanentWiltingPoint < 0) {
    auto res = fcSatPwpFromVanGenuchtenToth(layerNo <= 3,
                                            sp->vs_SoilSandContent,
                                            sp->vs_SoilClayContent,
                                            sp->vs_SoilStoneContent,
                                            sp->vs_SoilBulkDensity(),
                                            sp->vs_SoilOrganicCarbon());
    if (sp->vs_FieldCapacity < 0) sp->vs_FieldCapacity = res.fc;
    if (sp->vs_Saturation < 0) sp->vs_Saturation = res.sat;
    if (sp->vs_PermanentWiltingPoint < 0) sp->vs_PermanentWiltingPoint = res.pwp;
  }
  return {};
}

Errors Soil::updateUnsetPwpFcSatFromToth(SoilParameters* sp, int) {
  if (sp->vs_FieldCapacity < 0 || sp->vs_Saturation < 0 || sp->vs_PermanentWiltingPoint < 0) {
    auto res = fcSatPwpFromToth(sp->vs_SoilSandContent,
                                sp->vs_SoilClayContent,
                                sp->vs_SoilStoneContent,
                                sp->vs_SoilBulkDensity(),
                                sp->vs_SoilOrganicCarbon());
    if (sp->vs_FieldCapacity < 0) sp->vs_FieldCapacity = res.fc;
    if (sp->vs_Saturation < 0) sp->vs_Saturation = res.sat;
    if (sp->vs_PermanentWiltingPoint < 0) sp->vs_PermanentWiltingPoint = res.pwp;
  }
  return {};
}
