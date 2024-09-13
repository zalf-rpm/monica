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

#if SQ_SOIL_TEMPERATURE
#include "SQ_Soil_Temperature/SoilTemperatureComponent.h"
#endif
#include "core/monica-parameters.h"

namespace monica { class MonicaModel;}

namespace SQ_Soil_Temperature {

class MonicaInterface {
public:
  explicit MonicaInterface(monica::MonicaModel *monica);

  void init(const monica::CentralParameterProvider &cpp);

  void run();

#if SQ_SOIL_TEMPERATURE
  SoilTemperatureComponent _soilTempComp;
  SoilTemperatureState _soilTempState;
  SoilTemperatureState _soilTempState1;
  SoilTemperatureExogenous _soilTempExo;
  SoilTemperatureRate _soilTempRate;
  SoilTemperatureAuxiliary _soilTempAux;
#endif

private:
  monica::MonicaModel *_monica;
  bool _doInit{true};
};

}
