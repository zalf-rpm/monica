/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include <fstream>

#include "tools/debug.h"

#include "file-io.h"

using namespace Monica;
using namespace Tools;
using namespace std;

void Monica::writeCropParameters(std::string path, const Crop& crop)
{
  ofstream parameterOutputFile;
  parameterOutputFile.open(path + "/crop_parameters-" + crop.speciesName() + ".txt");
  if(parameterOutputFile.fail())
  {
    debug() << "Could not write file\"" << path << "crop_parameters-" << crop.speciesName() << ".txt" << "\"" << endl;
    return;
  }

  parameterOutputFile << crop.cropParameters()->toString();
  parameterOutputFile.close();
}

//------------------------------------------------------------------------------
