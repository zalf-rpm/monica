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

#include "Stics_soil_temperature/soil_tempComponent.h"
#include "core/monica-parameters.h"

namespace monica { class MonicaModel;}

namespace Stics_soil_temperature {

class MonicaInterface : public monica::Run {
public:
  explicit MonicaInterface(monica::MonicaModel *monica);

  void init(const monica::CentralParameterProvider &cpp);

  void run() override;

  soil_tempComponent soilTempComp;
  soil_tempState soilTempState;
  soil_tempState soilTempState1;
  soil_tempExogenous soilTempExo;
  soil_tempRate soilTempRate;
  soil_tempAuxiliary soilTempAux;

private:
  monica::MonicaModel *_monica;
  bool _doInit{true};
};

}
