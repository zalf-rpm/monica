/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Michael Berg <michael.berg@zalf.de>
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#ifndef MONICA_TYPEDEFS_H
#define MONICA_TYPEDEFS_H

#include <string>

namespace Monica
{
  typedef int CropId;

	enum
	{
		MODE_LC_DSS = 0,
		MODE_ACTIVATE_OUTPUT_FILES,
		MODE_HERMES,
		MODE_EVA2,
		MODE_SENSITIVITY_ANALYSIS,
		MODE_CC_GERMANY,
		MODE_MACSUR_SCALING,
		MODE_MACSUR_SCALING_CALIBRATION,
		MODE_CARBIOCIAL_CLUSTER
	};

}

#endif // MONICA_TYPEDEFS_H
