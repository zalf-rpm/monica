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

#if MONICA_SOILTEMP
#include "Monica_SoilTemp/SoilTemperatureCompComponent.h"
#endif
#include "core/monica-parameters.h"

namespace monica { class MonicaModel;}

namespace Monica_SoilTemp {

class MonicaInterface {
public:
  explicit MonicaInterface(monica::MonicaModel *monica);

  void init(const monica::CentralParameterProvider &cpp);

  void run();

#if MONICA_SOILTEMP
  SoilTemperatureCompComponent soilTempComp;
  SoilTemperatureCompState soilTempState;
  SoilTemperatureCompState soilTempState1;
  SoilTemperatureCompExogenous soilTempExo;
  SoilTemperatureCompRate soilTempRate;
  SoilTemperatureCompAuxiliary soilTempAux;
#endif

private:
  monica::MonicaModel *_monica;
  bool _doInit{true};
};

}