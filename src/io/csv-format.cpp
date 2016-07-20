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

#include "tools/debug.h"

#include "csv-format.h"

using namespace Monica;
using namespace Tools;
using namespace std;
using namespace json11;

void Monica::writeOutputHeaderRows(ostream& out,
																	 const vector<OId>& outputIds,
																	 string csvSep,
																	 bool includeHeaderRow,
																	 bool includeUnitsRow,
																	 bool includeTimeAgg)
{
	ostringstream oss1, oss2, oss3, oss4;
	for(auto oid : outputIds)
	{
		int fromLayer = oid.fromLayer, toLayer = oid.toLayer;
		bool isOrgan = oid.isOrgan();
		bool isRange = oid.isRange() && oid.layerAggOp == OId::NONE;
		if(isOrgan)
			toLayer = fromLayer = int(oid.organ); // organ is being represented just by the value of fromLayer currently
		else if(isRange)
			fromLayer++, toLayer++; // display 1-indexed layer numbers to users
		else
			toLayer = fromLayer; // for aggregated ranges, which aren't being displayed as range

		for(int i = fromLayer; i <= toLayer; i++)
		{
			oss1 << "\"";
			if(isOrgan)
				oss1 << oid.name << "/" << oid.toString(oid.organ);
			else if(isRange)
				oss1 << oid.name << "_" << to_string(i);
			else
				oss1 << oid.name;
			oss1 << "\"" << csvSep;
			oss4 << "\"j:" << replace(oid.jsonInput, "\"", "") << "\"" << csvSep;
			oss3 << "\"m:" << oid.toString(includeTimeAgg) << "\"" << csvSep;
			oss2 << "\"[" << oid.unit << "]\"" << csvSep;
		}
	}

	if(includeHeaderRow)
		out
		<< oss1.str() << endl
		<< oss4.str() << endl
		<< oss3.str() << endl
		<< oss2.str() << endl;
}

void Monica::writeOutput(ostream& out,
												 const vector<OId>& outputIds,
												 const vector<J11Array>& values,
												 string csvSep,
												 bool includeHeaderRow,
												 bool includeUnitsRow)
{
	if(!values.empty())
	{
		for(size_t k = 0, size = values.begin()->size(); k < size; k++)
		{
			size_t i = 0;
			for(auto oid : outputIds)
			{
				Json j = values.at(i).at(k);
				switch(j.type())
				{
				case Json::NUMBER: out << j.number_value() << csvSep; break;
				case Json::STRING: out << j.string_value() << csvSep; break;
				case Json::BOOL: out << j.bool_value() << csvSep; break;
				case Json::ARRAY:
				{
					for(Json jv : j.array_items())
					{
						switch(jv.type())
						{
						case Json::NUMBER: out << jv.number_value() << csvSep; break;
						case Json::STRING: out << jv.string_value() << csvSep; break;
						case Json::BOOL: out << jv.bool_value() << csvSep; break;
						default: out << "UNKNOWN" << csvSep;
						}
					}
					break;
				default: out << "UNKNOWN" << csvSep;
				}
				}

				++i;
			}
			out << endl;
		}
	}
	out.flush();
}


