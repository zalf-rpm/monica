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

#include "ApsimCampbell/SoiltempComponent.h"
#include "core/monica-parameters.h"

namespace monica { class MonicaModel;}

namespace ApsimCampbell {

class MonicaInterface : public monica::Run {
public:
  explicit MonicaInterface(monica::MonicaModel *monica);

  void init(const monica::CentralParameterProvider &cpp);

  void run() override;

  SoiltempComponent _soilTempComp;
  SoiltempState _soilTempState;
  SoiltempState _soilTempState1;
  SoiltempExogenous _soilTempExo;
  SoiltempRate _soilTempRate;
  SoiltempAuxiliary _soilTempAux;

private:
  monica::MonicaModel *_monica;
  bool _doInit{true};
};

}
