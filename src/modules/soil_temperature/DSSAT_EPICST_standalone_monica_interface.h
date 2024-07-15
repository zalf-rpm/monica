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

#if DSSAT_EPICST_STANDALONE
#include "DSSAT_EPICST_standalone/STEMP_EPIC_Component.h"
#endif
#include "core/monica-parameters.h"

namespace monica { class MonicaModel;}

namespace DSSAT_EPICST_standalone {

struct MonicaInterface {
public:
  explicit MonicaInterface(monica::MonicaModel *monica);

  void init(const monica::CentralParameterProvider &cpp);

  void run();

#if DSSAT_EPICST_STANDALONE
  STEMP_EPIC_Component soilTempComp;
  STEMP_EPIC_State soilTempState;
  STEMP_EPIC_State soilTempState1;
  STEMP_EPIC_Exogenous soilTempExo;
  STEMP_EPIC_Rate soilTempRate;
  STEMP_EPIC_Auxiliary soilTempAux;
#endif

private:
  monica::MonicaModel *_monica;
  bool _doInit{true};
};

}
