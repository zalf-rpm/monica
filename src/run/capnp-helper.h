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

#pragma once

#include "tools/date.h"
#include "climate/climate-common.h"
#include "json11/json11-helper.h"

#include "soil.capnp.h"
#include "climate.capnp.h"
#include "monica_management.capnp.h"

namespace monica {

Climate::ACD climateElementToACD(mas::schema::climate::Element e);

std::map<Climate::ACD, double> dailyClimateDataToDailyClimateMap(
  const capnp::List<mas::schema::climate::Element>::Reader& header,
  const capnp::List<double>::Reader& data);

std::map<Climate::ACD, double> dailyClimateDataToDailyClimateMap(
  const capnp::List<mas::schema::model::monica::Params::DailyWeather::KV>::Reader& dailyData);

Climate::DataAccessor fromCapnpData(
    const Tools::Date &startDate,
    const Tools::Date &endDate,
    capnp::List<mas::schema::climate::Element>::Reader header,
    capnp::List<capnp::List<float>>::Reader data);

kj::Promise<Climate::DataAccessor> dataAccessorFromTimeSeries(mas::schema::climate::TimeSeries::Client ts);

kj::Promise<Tools::J11Array> fromCapnpSoilProfile(mas::schema::soil::Profile::Client profile);

}
