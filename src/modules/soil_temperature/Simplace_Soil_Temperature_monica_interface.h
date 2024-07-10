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

#if SIMPLACE_SOIL_TEMPERATURE
#include "Simplace_Soil_Temperature/SoilTemperatureComponent.h"
#endif
#include "core/monica-parameters.h"

namespace monica { class MonicaModel;}

namespace Simplace_Soil_Temperature {

class MonicaInterface {
public:
  explicit MonicaInterface(monica::MonicaModel *monica);

  void init(const monica::CentralParameterProvider &cpp);

  void run();

#if SIMPLACE_SOIL_TEMPERATURE
  SoilTemperatureComponent soilTempComp;
  SoilTemperatureState soilTempState;
  SoilTemperatureState soilTempState1;
  SoilTemperatureExogenous soilTempExo;
  SoilTemperatureRate soilTempRate;
  SoilTemperatureAuxiliary soilTempAux;
#endif

private:
  monica::MonicaModel *_monica;
  bool _doInit{true};
};

}
