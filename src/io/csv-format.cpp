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

#include <string>

#include "tools/debug.h"

#include "csv-format.h"

using namespace monica;
using namespace Tools;
using namespace std;
using namespace json11;

//copied from MSVC string.h, because gcc 4.7.2 seams not to know ""s
inline string operator "" _s(const char *_Str, size_t _Len)
{	// construct literal from [_Str, _Str + _Len)
	return (string(_Str, _Len));
}

void monica::writeOutputHeaderRows(ostream& out,
																	 const vector<OId>& outputIds,
																	 string csvSep,
																	 bool includeHeaderRow,
																	 bool includeUnitsRow,
																	 bool includeTimeAgg)
{
	ostringstream oss1, oss2, oss3, oss4;

	//using namespace std::string_literals;
	string escapeTokens = "\n\""_s + csvSep;

	size_t j = 0;
	auto oidsSize = outputIds.size();
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
			ostringstream oss11;
			if(isOrgan)
				oss11 << (oid.displayName.empty() ? oid.name + "/" + oid.toString(oid.organ) : oid.displayName);
			else if(isRange)
				oss11 << (oid.displayName.empty() ? oid.name + "_" + to_string(i) : oid.displayName);
			else
				oss11 << (oid.displayName.empty() ? oid.name : oid.displayName);
			auto csvSep_ = j + 1 == oidsSize && i == toLayer ? "" : csvSep;
			oss1 << (oss11.str().find_first_of(escapeTokens) == string::npos ? oss11.str() : "\""_s + oss11.str() + "\""_s) << csvSep_;
			auto os2 = "["_s + oid.unit + "]"_s;
			oss2 << (os2.find_first_of(escapeTokens) == string::npos ? os2 : "\""_s + os2 + "\""_s) << csvSep_;
			auto os3 = "m:"_s + oid.toString(includeTimeAgg);
			oss3 << (os3.find_first_of(escapeTokens) == string::npos ? os3 : "\""_s + os3 + "\""_s) << csvSep_;
			auto os4 = "j:"_s + replace(oid.jsonInput, "\"", "");
			oss4 << (os4.find_first_of(escapeTokens) == string::npos ? os4 : "\""_s + os4 + "\""_s) << csvSep_;
		}
		++j;
	}
	
	if(includeHeaderRow)
		out << oss1.str() << endl;
	if(includeUnitsRow)
		out << oss2.str() << endl;
	if(includeTimeAgg)
		out 
		<< oss3.str() << endl
		<< oss4.str() << endl;
}

void monica::writeOutput(ostream& out,
												 const vector<OId>& outputIds,
												 const vector<J11Array>& values,
												 string csvSep)
{
	//using namespace std::string_literals;
	string escapeTokens = "\n\""_s + csvSep;

	if(!values.empty())
	{
		for(size_t k = 0, size = values.begin()->size(); k < size; k++)
		{
			size_t i = 0;
			auto oidsSize = outputIds.size();
			for(auto oid : outputIds)
			{
				auto csvSep_ = i + 1 ==  oidsSize ? "" : csvSep;
				Json j = values.at(i).at(k);
				switch(j.type())
				{
				case Json::NUMBER: out << j.number_value() << csvSep_; break;
				case Json::STRING: 
					out 
					<< (j.string_value().find_first_of(escapeTokens) == string::npos 
																	 ? j.string_value() 
																	 : "\""_s + j.string_value() + "\""_s) 
					<< csvSep_; break;
				case Json::BOOL: out << j.bool_value() << csvSep_; break;
				case Json::ARRAY:
				{
					size_t jvi = 0;
					auto jSize = j.array_items().size();
					for(Json jv : j.array_items())
					{
						auto csvSep__ = jvi + 1 == jSize ? "" : csvSep;
						switch(jv.type())
						{
						case Json::NUMBER: out << jv.number_value() << csvSep__; break;
						case Json::STRING: 
							out 
								<< (jv.string_value().find_first_of(escapeTokens) == string::npos
										? jv.string_value()
										: "\""_s + jv.string_value() + "\""_s)
								<< csvSep__; break;
						case Json::BOOL: out << jv.bool_value() << csvSep__; break;
						default: out << "UNKNOWN" << csvSep__;
						}
						++jvi;
					}
					out << csvSep_; 
					break;
				}
				default: out << "UNKNOWN" << csvSep_;
				}

				++i;
			}
			out << endl;
		}
	}
	out.flush();
}

void monica::writeOutputObj(ostream& out,
														const vector<OId>& outputIds,
														const vector<J11Object>& values,
														string csvSep)
{
	//using namespace std::string_literals;
	string escapeTokens = "\n\""_s + csvSep;

	if(!values.empty())
	{
		for(const auto& o : values)
		{
			size_t i = 0;
			auto oidsSize = outputIds.size();
			for(auto oid : outputIds)
			{
				auto csvSep_ = i + 1 == oidsSize ? "" : csvSep;
				auto oi = o.find(oid.outputName());
				if(oi != o.end())
				{
					Json j = oi->second;
					switch(j.type())
					{
					case Json::NUMBER: out << j.number_value() << csvSep_; break;
					case Json::STRING:
						out
							<< (j.string_value().find_first_of(escapeTokens) == string::npos
									? j.string_value()
									: "\""_s + j.string_value() + "\""_s)
							<< csvSep_; break;
					case Json::BOOL: out << j.bool_value() << csvSep_; break;
					case Json::ARRAY:
					{
						size_t jvi = 0;
						auto jSize = j.array_items().size();
						for(Json jv : j.array_items())
						{
							auto csvSep__ = jvi + 1 == jSize ? "" : csvSep;
							switch(jv.type())
							{
							case Json::NUMBER: out << jv.number_value() << csvSep__; break;
							case Json::STRING:
								out
									<< (jv.string_value().find_first_of(escapeTokens) == string::npos
											? jv.string_value()
											: "\""_s + jv.string_value() + "\""_s)
									<< csvSep__; break;
							case Json::BOOL: out << jv.bool_value() << csvSep__; break;
							default: out << "UNKNOWN" << csvSep__;
							}
							++jvi;
						}
						out << csvSep_;
						break;
					}
					default: out << "UNKNOWN" << csvSep_;
					}
				}
				++i;
			}
			out << endl;


		}
	}
	out.flush();
}


