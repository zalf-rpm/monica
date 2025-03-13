/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "capnp-helper.h"

#include <vector>

#include <kj/common.h>

#define KJ_MVCAP(var) var = kj::mv(var)

#include <capnp/message.h>

#include "tools/algorithms.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "monica_management.capnp.h"

using namespace monica;
using namespace Tools;
using namespace json11;
using namespace Climate;

ACD monica::climateElementToACD(mas::schema::climate::Element e) {
  switch (e) {
    case mas::schema::climate::Element::TMIN:
      return ACD::tmin;
    case mas::schema::climate::Element::TAVG:
      return ACD::tavg;
    case mas::schema::climate::Element::TMAX:
      return ACD::tmax;
    case mas::schema::climate::Element::PRECIP:
      return ACD::precip;
    case mas::schema::climate::Element::RELHUMID:
      return ACD::relhumid;
    case mas::schema::climate::Element::WIND:
      return ACD::wind;
    case mas::schema::climate::Element::GLOBRAD:
      return ACD::globrad;
    default:
      return ACD::skip;
  }
}

std::map<ACD, double> monica::dailyClimateDataToDailyClimateMap(const capnp::List<mas::schema::climate::Element>::Reader& header,
  const capnp::List<double>::Reader& data) {
  std::map<ACD, double> res;
  KJ_ASSERT(header.size() == data.size())
  for (const auto i : kj::indices(header)){
     res[climateElementToACD(header[i])] = data[i];
   }
   return res;
 }

std::map<ACD, double> monica::dailyClimateDataToDailyClimateMap(
  const capnp::List<mas::schema::model::monica::Params::DailyWeather::KV>::Reader& dailyData) {
  std::map<ACD, double> res;
  for (const auto& kv : dailyData){
    res[climateElementToACD(kv.getKey())] = kv.getValue();
  }
  return res;
}


DataAccessor monica::fromCapnpData(
    const Tools::Date &startDate,
    const Tools::Date &endDate,
    capnp::List<mas::schema::climate::Element>::Reader header,
    capnp::List<capnp::List<float>>::Reader data) {
  typedef mas::schema::climate::Element E;

  if (data.size() == 0) return {};

  DataAccessor da(startDate, endDate);
  for (capnp::uint i = 0; i < header.size(); i++) {
    auto vs = data[i];
    std::vector<double> d(data[0].size());
    for (capnp::uint k = 0; k < vs.size(); k++) d[k] = vs[k];
    switch (header[i]) {
      case E::TMIN:
        da.addClimateData(ACD::tmin, kj::mv(d));
        break;
      case E::TAVG:
        da.addClimateData(ACD::tavg, kj::mv(d));
        break;
      case E::TMAX:
        da.addClimateData(ACD::tmax, kj::mv(d));
        break;
      case E::PRECIP:
        da.addClimateData(ACD::precip, kj::mv(d));
        break;
      case E::RELHUMID:
        da.addClimateData(ACD::relhumid, kj::mv(d));
        break;
      case E::WIND:
        da.addClimateData(ACD::wind, kj::mv(d));
        break;
      case E::GLOBRAD:
        da.addClimateData(ACD::globrad, kj::mv(d));
        break;
      default:;
    }
  }
  return da;
}

kj::Promise<DataAccessor> monica::dataAccessorFromTimeSeries(mas::schema::climate::TimeSeries::Client ts) {
  auto rangeProm = ts.rangeRequest().send();
  auto headerProm = ts.headerRequest().send();
  auto dataTProm = ts.dataTRequest().send();

  return rangeProm
      .then([KJ_MVCAP(headerProm), KJ_MVCAP(dataTProm)](auto &&rangeResponse) mutable {
        return headerProm
            .then([KJ_MVCAP(rangeResponse), KJ_MVCAP(dataTProm)](auto &&headerResponse) mutable {
              return dataTProm
                  .then([KJ_MVCAP(rangeResponse), KJ_MVCAP(headerResponse)](auto &&dataTResponse) mutable {
                    auto sd = rangeResponse.getStartDate();
                    auto ed = rangeResponse.getEndDate();
                    return ::fromCapnpData(
                        Tools::Date(sd.getDay(), sd.getMonth(), sd.getYear()),
                        Tools::Date(ed.getDay(), ed.getMonth(), ed.getYear()),
                        headerResponse.getHeader(), dataTResponse.getData());
                  }, [](auto &&e) {
                    KJ_LOG(ERROR, "Error while trying to get transposed time series data.", e);
                    return DataAccessor();
                  });
            }, [](auto &&e) {
              KJ_LOG(ERROR, "Error while trying to get header data.", e);
              return DataAccessor();
            });
      }, [](auto &&e) {
        KJ_LOG(ERROR, "Error while trying to get range data.", e);
        return DataAccessor();
      });
}


kj::Promise<J11Array> monica::fromCapnpSoilProfile(mas::schema::soil::Profile::Client profile) {
  return profile.dataRequest().send().then([](auto &&data) {
    J11Array ls;
    for (const auto &layer: data.getLayers()) {
      J11Object l;
      l["Thickness"] = layer.getSize();
      for (const auto &prop: layer.getProperties()) {
        switch (prop.getName()) {
          case mas::schema::soil::PropertyName::SAND:
            if (prop.isF32Value()) l["Sand"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::CLAY:
            if (prop.isF32Value()) l["Clay"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::SILT:
            if (prop.isF32Value()) l["Silt"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::ORGANIC_CARBON:
            if (prop.isF32Value()) l["SoilOrganicCarbon"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::ORGANIC_MATTER:
            if (prop.isF32Value()) l["SoilOrganicMatter"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::BULK_DENSITY:
            if (prop.isF32Value()) l["SoilBulkDensity"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::RAW_DENSITY:
            if (prop.isF32Value()) l["SoilRawDensity"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::P_H:
            if (prop.isF32Value()) l["pH"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::SOIL_TYPE:
            if (prop.isType()) l["KA5TextureClass"] = prop.getType().cStr();
            break;
          case mas::schema::soil::PropertyName::PERMANENT_WILTING_POINT:
            if (prop.isF32Value()) l["PermanentWiltingPoint"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::FIELD_CAPACITY:
            if (prop.isF32Value()) l["FieldCapacity"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::SATURATION:
            if (prop.isF32Value()) l["PoreVolume"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::SOIL_WATER_CONDUCTIVITY_COEFFICIENT:
            if (prop.isF32Value()) l["Lambda"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::SCELETON:
            if (prop.isF32Value()) l["Sceleton"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::AMMONIUM:
            l["SoilAmmonium"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::NITRATE:
            if (prop.isF32Value()) l["SoilNitrate"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::CN_RATIO:
            if (prop.isF32Value()) l["CN"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::SOIL_MOISTURE:
            l["SoilMoisturePercentFC"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::IN_GROUNDWATER:
            if (prop.isBValue()) l["is_in_groundwater"] = prop.getBValue();
            break;
          case mas::schema::soil::PropertyName::IMPENETRABLE:
            if (prop.isBValue()) l["is_impenetrable"] = prop.getBValue();
            break;
        }
      }
      ls.emplace_back(l);
    }
    return kj::mv(ls);
  }, [](auto &&e) {
    KJ_LOG(ERROR, "Error while trying to get soil profile data.", e);
    return J11Array();
  });
}
