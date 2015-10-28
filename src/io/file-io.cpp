/**
Authors: 
Jan Vaillant <jan.vaillant@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) 2007-2013, Leibniz Centre for Agricultural Landscape Research (ZALF)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
