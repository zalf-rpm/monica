/**
Authors: 
Michael Berg <michael.berg@zalf.de>
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the util library used by models created at the Institute of 
Landscape Systems Analysis at the ZALF.
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

#ifndef CONVERSION_H_
#define CONVERSION_H_

#include <string>

namespace Tools
{
	double humus_st2corg(int humus_st);

	double ld_eff2trd(int ldEff, double clay);

	double texture2lambda(double sand, double clay);

	//! sand and clay [0 - 1]
	std::string texture2KA5(double sand, double clay);

	double KA52sand(std::string soilType);

	double KA52clay(std::string soilType);
}


#endif //CONVERSION_H_
