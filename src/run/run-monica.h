/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

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

#ifndef RUN_MONICA_H_
#define RUN_MONICA_H_

#include "../core/monica.h"

namespace Monica
{
  /*!
   * main function for running monica under a given Env(ironment)
   * @param env the environment completely defining what the model needs and gets
   * @return a structure with all the Monica results
   */
#ifndef	MONICA_GUI
  Result runMonica(Env env);
#else
  Result runMonica(Env env, Configuration* cfg = NULL);
#endif
}

#endif
