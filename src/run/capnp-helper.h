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

namespace monica {

kj::Promise<Climate::DataAccessor> dataAccessorFromTimeSeries(mas::schema::climate::TimeSeries::Client ts);

kj::Promise<Tools::J11Array> fromCapnpSoilProfile(mas::schema::soil::Profile::Client profile);

}