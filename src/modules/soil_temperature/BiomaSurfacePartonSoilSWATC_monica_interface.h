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

#if BIOMASURFACEPARTONSOILSWATC
#include "BiomaSurfacePartonSoilSWATC/SurfacePartonSoilSWATCComponent.h"
#endif
#include "core/monica-parameters.h"

namespace monica { class MonicaModel;}

namespace BiomaSurfacePartonSoilSWATC {

class MonicaInterface {
public:
  explicit MonicaInterface(monica::MonicaModel *monica);

  void init(const monica::CentralParameterProvider &cpp);

  void run();

#if BIOMASURFACEPARTONSOILSWATC
  SurfacePartonSoilSWATCComponent soilTempComp;
  SurfacePartonSoilSWATCState soilTempState;
  SurfacePartonSoilSWATCState soilTempState1;
  SurfacePartonSoilSWATCExogenous soilTempExo;
  SurfacePartonSoilSWATCRate soilTempRate;
  SurfacePartonSoilSWATCAuxiliary soilTempAux;
#endif

private:
  monica::MonicaModel *_monica;
  bool _doInit{true};
};

}