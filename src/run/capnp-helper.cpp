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

using namespace monica;
using namespace Tools;
using namespace json11;
using namespace Climate;

namespace {

DataAccessor fromCapnpData(
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
    for (capnp::uint k = 0; k < vs.size(); k++)
      d[k] = vs[k];
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
            l["Sand"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::CLAY:
            l["Clay"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::SILT:
            l["Silt"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::ORGANIC_CARBON:
            l["SoilOrganicCarbon"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::ORGANIC_MATTER:
            l["SoilOrganicMatter"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::BULK_DENSITY:
            l["SoilBulkDensity"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::RAW_DENSITY:
            l["SoilRawDensity"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::P_H:
            l["pH"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::SOIL_TYPE:
            l["KA5TextureClass"] = prop.getType().cStr();
            break;
          case mas::schema::soil::PropertyName::PERMANENT_WILTING_POINT:
            l["PermanentWiltingPoint"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::FIELD_CAPACITY:
            l["FieldCapacity"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::SATURATION:
            l["PoreVolume"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::SOIL_WATER_CONDUCTIVITY_COEFFICIENT:
            l["Lambda"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::SCELETON:
            l["Sceleton"] = prop.getF32Value() / 100.0;
            break;
          case mas::schema::soil::PropertyName::AMMONIUM:
            l["SoilAmmonium"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::NITRATE:
            l["SoilNitrate"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::CN_RATIO:
            l["CN"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::SOIL_MOISTURE:
            l["SoilMoisturePercentFC"] = prop.getF32Value();
            break;
          case mas::schema::soil::PropertyName::IN_GROUNDWATER:
            l["is_in_groundwater"] = prop.getBValue();
            break;
          case mas::schema::soil::PropertyName::IMPENETRABLE:
            l["is_impenetrable"] = prop.getBValue();
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
