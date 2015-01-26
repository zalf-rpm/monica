/**
Authors: 
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

#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <cassert>

#include "boost/foreach.hpp"

#include "db/abstract-db-connections.h"
#include "climate/climate-common.h"
#include "tools/helper.h"
#include "tools/algorithms.h"

#include "carbiocial.h"
#include "debug.h"
#include "monica-parameters.h"
#include "conversion.h"
#include "simulation.h"

#define LOKI_OBJECT_LEVEL_THREADING
#include "loki/Threads.h"

using namespace Db;
using namespace std;
using namespace Carbiocial;
using namespace Monica;
using namespace Tools;

namespace
{
	struct L: public Loki::ObjectLevelLockable<L> {};
}

void mpmasScope::exitfun()
{
#ifndef NO_MPMAS
	cout << "EXITING ..." << endl;
	
	//deallocateAll();
	//delete mpmasPointer;

	cout << "-------------------------------------------------------------------------" << endl << "SimName:  "
			<< SIMNAME().c_str() << endl;
	cout << "INDIR():  " << INDIR().c_str() << " true: " << strcmp(INDIR().c_str(), ".") << endl;
	cout << "OUTDIR(): " << OUTDIR().c_str() << " true: " << strcmp(OUTDIR().c_str(), ".") << endl;
	cout << "XMLDIR(): " << XMLDIR().c_str() << endl;
	cout << "KEYDIR(): " << KEYDIR().c_str() << endl
			<< "-------------------------------------------------------------------------" << endl;
	//exit(1);
#endif
}

#ifndef NO_MPMAS

string IdPlusCode::toString(const string& indent, bool /*detailed*/) const
{
	ostringstream s;
	s << Identifiable::toString(indent) << ", " << code;
	return s.str();
}

/*
CropData Carbiocial::cropDataFor(CropId cropId)
{
	CropData cd;
	switch(cropId)
	{
	case maize:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case sugarcane:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case soy:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case cotton:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case millet:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	case fallow:
		cd.sowing = Date::relativeDate(1,1);
		cd.harvesting = Date::relativeDate(8,8);
		break;
	default: ;
	}
	return cd;
}
*/

Monica::ProductionProcess Carbiocial::productionProcessFrom(ProductionPractice* prp)
{
	Monica::CropPtr cp;
	//CropData cd = cropDataFor(pp);

	Product* p = prp->product;

	int pid = prp->product->id;
	if(pid == 1 && pid == 25) //cassava
	{
		cp = Monica::CropPtr(new Monica::Crop(8, p->name));
	}
	else if(2 <= pid && pid <= 7) //cotton
	{
		cp = Monica::CropPtr(new Monica::Crop(43, p->name));
	}
	else if(11 <= pid && pid <= 14) //maize
	{
		cp = Monica::CropPtr(new Monica::Crop(6, p->name));
	}
	else if(pid == 15) //millet
	{
		cp = Monica::CropPtr(new Monica::Crop(21, p->name));
	}
	else if(18 <= pid && pid <= 19) //rice
	{
		cp = Monica::CropPtr(new Monica::Crop("fallow"));
	}
	else if(pid == 20) //sorghum
	{
		cp = Monica::CropPtr(new Monica::Crop(21, p->name));
	}
	else if(21 <= pid && pid <= 22) //soy
	{
		cp = Monica::CropPtr(new Monica::Crop(37, p->name));
	}
	else if(pid == 23) //sugar cane
	{
		cp = Monica::CropPtr(new Monica::Crop("fallow"));
	}
	else if(pid == 24) //sun flower
	{
		cp = Monica::CropPtr(new Monica::Crop("fallow"));
	}
	else if(pid == 26) //brachiaria
	{
		cp = Monica::CropPtr(new Monica::Crop("fallow"));
	}
	else //fallow
	{
		cp = Monica::CropPtr(new Monica::Crop("fallow"));
	}

	if(cp->isValid())
	{
		auto smonths = p->operationId2month.equal_range(2);
		auto hmonths = p->operationId2month.equal_range(3);
		//there should be just one seeding and havesting month
		int smonth = smonths.first->second;
		int hmonth = hmonths.first->second;
		Date seeding = Date::relativeDate(1, smonth, hmonth < smonth ? -1 : 0);
		Date harvesting = Date::relativeDate(Date::daysInMonth(Date::notALeapYear(), hmonth), hmonth);

		cp->setSeedAndHarvestDate(seeding, harvesting);
		cp->setCropParameters(getCropParametersFromMonicaDB(cp->id()));
		cp->setResidueParameters(getResidueParametersFromMonicaDB(cp->id()));
	}

	ProductionProcess pp(cp->name(), cp);
	pp.setCustomId(prp->id);

	/*
	BOOST_FOREACH(Date tillage, cd.tillages)
	{
		pp.addApplication(TillageApplication(tillage, 0.3));
	}
	*/

	return pp;
}

void Carbiocial::writeInactiveSectorsFile(int activeSectorId, const std::string& pathToFile)
{
	ostringstream oss;

	vector<Municipality*> ms = Municipality::all();
	int count = 0;
	BOOST_FOREACH(Municipality* m, ms)
	{
		const map<SectorId, Sector*>& sid2s = m->sectorId2sector;
		for_each(sid2s.begin(), sid2s.end(), [&](pair<SectorId, Sector*> p)
		{
			int inactiveSectorId = makeSectorId(m->id, p.first);
			if(inactiveSectorId != activeSectorId)
			{
				oss << inactiveSectorId << endl;
				count++;
			}
		});
	}

	ofstream ofs(pathToFile);
	ofs << count << endl
			<< oss.str() << endl;
}

//-------------------------------------------------------------------------------------------------------

Mpmas::Mpmas(const string& pathToMpmas)
	: _mpmas(NULL),
		_noOfYears(0),
		_noOfSpinUpYears(0),
		_noOfCropActivities(0),
		_cropAreas(NULL),
		_monicaYields(NULL),
		_monicaStoverYields(NULL),
		_cropActivitiesDisabled(false),
		_inputPathToMpmas(NULL),
		_outputPathToMpmas(NULL),
		_pathToInactiveSectorsFile(NULL)
{
	ostringstream oss;
	oss << "-B" << pathToMpmas << "/input/dat/tf__InactiveSectors0.dat";
	_pathToInactiveSectorsFile = new char[oss.str().size()];
	strcpy(_pathToInactiveSectorsFile, oss.str().c_str());

	ostringstream oss2;
	oss2 <<  "-I" << pathToMpmas;
	_inputPathToMpmas = new char[oss2.str().size()];
	strcpy(_inputPathToMpmas, oss2.str().c_str());

	ostringstream oss3;
	oss3 <<  "-O" << pathToMpmas;
	_outputPathToMpmas = new char[oss3.str().size()];
	strcpy(_outputPathToMpmas, oss3.str().c_str());

	char* argv[] =
	{
		"mpmas-lib",
		_inputPathToMpmas,
		_outputPathToMpmas,
		"-Ntf__",
		"-T82",
		"-Pgis/carbiocial_typical_farms/",
		_pathToInactiveSectorsFile,
//		"-T93",
//		"-T94",
//		"-T1",
//		"-T34",
		"-T19"
	};

//#ifndef NO_MPMAS
	_mpmas = new mpmas(8, argv);
	//mpmasScope::mpmasPointer = _mpmas;

	//allocate memory
	_mpmas->allocateMemoryForMonica(CropActivity::all().size());

			//get length of simulation horizon and number of spin-up rounds
	_noOfYears = _mpmas->getNumberOfYearsToSimulate();
	_noOfSpinUpYears = _mpmas->getNumberOfSpinUpRounds();
//#endif

	_noOfCropActivities = CropActivity::all().size();

	_cropActivityIds = new int[_noOfCropActivities];
	_cropAreas = new double[_noOfCropActivities];
	_monicaYields = new double[_noOfCropActivities];
	_monicaStoverYields = new double[_noOfCropActivities];
	_grossMargins = new double[_noOfCropActivities];
}

Mpmas::~Mpmas()
{
	delete[] _cropActivityIds;
	delete[] _cropAreas;
	delete[] _monicaYields;
	delete[] _monicaStoverYields;
	delete[] _grossMargins;
//	delete _inputPathToMpmas;
//	delete _outputPathToMpmas;
//	delete _pathToInactiveSectorsFile;
}

vector<CropActivity*> Mpmas::landuse(int year,
																		 const map<int, int>& /*soilClassId2areaPercent*/,
																		 vector<ProductionPractice*> pps)
{
//	cout << "entering Mpmas::landuse" << endl;

	map<int, ProductionPractice*> id2pp;
	for_each(pps.begin(), pps.end(), [&](ProductionPractice* pp)
	{
		id2pp[pp->id] = pp;
	});

//	vector<CropActivityId> disCAs;
//	BOOST_FOREACH(CropActivity* ca, CropActivity::all())
//	{
//		if(soilClassId2areaPercent.find(ca->soilClass->id) == soilClassId2areaPercent.end() ||
//			 id2pp.find(ca->productionPractice->id) == id2pp.end())
//			disCAs.push_back(ca->id);
//	}

//	if(!_cropActivitiesDisabled)
//	{
//		int* disabledCropActivities = new int[disCAs.size()];
//		for(int i = 0, size = disCAs.size(); i < size; i++)
//			disabledCropActivities[i] = disCAs.at(i);

//		//if needed, exclude crop activities from being grown
//		_mpmas->disableCropActivities(disCAs.size(), disabledCropActivities);

//		_cropActivitiesDisabled = true;
//	}

	//get landuse from MPMAS
	int rtCode = -1;
//#ifndef NO_MPMAS
	rtCode = _mpmas->simulateOnePeriodExportingLandUse(year, _noOfCropActivities, _cropActivityIds, _cropAreas);
//#endif

	vector<CropActivity*> res;
	for(int i = 0; rtCode >= 0 && i < _noOfCropActivities; i++)
	{
//		cout << "caId: " << _cropActivityIds[i];
		if(_cropAreas[i] > 0)
		{
//			cout << " area: " << _cropAreas[i] << " !!!!!!!" << endl;
			res.push_back(CropActivity::p4id(_cropActivityIds[i]));
		}
//		else
//		{
//			cout << " area: " << _cropAreas[i] << endl;
//		}
	}

//	cout << "leaving Mpmas::landuse" << endl;
//	cout.flush();

	return res;
}

MpmasResult Mpmas::calculateFarmEconomy(int year, Municipality* municipality, int sectorId,
																				Farm* farm, std::map<CropActivityId, double> caId2monicaYields)
{
//	cout << "entering Mpmas::calculateFarmEconomy" << endl;

//	cout << "storing monica yields in mpmas structure: " << endl;
	for(int i = 0; i < _noOfCropActivities; i++)
	{
		int caId = _cropActivityIds[i];
		auto cit = caId2monicaYields.find(caId);
		_monicaYields[i] = cit == caId2monicaYields.end() ? 0.0 : cit->second;

//		cout << "caId: " << caId << " -> " << _monicaYields[i] << " dt/ha" << endl;

		_monicaStoverYields[i] = 0.0;
	}

	//import yields maps
	int rtCode = -1;
//#ifndef NO_MPMAS
	rtCode = _mpmas->simulateOnePeriodImportingYields(year, _noOfCropActivities,
																												_cropActivityIds, _monicaYields,
																												_monicaStoverYields);
//#endif

	int noOfAgents = 1;
	int agentIds[1];
	agentIds[0] = makeAgentId(municipality, sectorId, farm);
	double farmIncome[1];
	double* individualGrossMargins[1];
	individualGrossMargins[0] = _grossMargins;

	//get also economic indicators of selected agents
#ifndef NO_MPMAS
	_mpmas->getPerformanceDataForSelectedAgents(noOfAgents, agentIds, farmIncome,
																							_noOfCropActivities, _cropActivityIds,
																							individualGrossMargins);
#endif

	MpmasResult r;
	r.farmProfit = farmIncome[0];

	for(int i = 0; i < _noOfCropActivities; i++)
	{
		CropActivityId caId = _cropActivityIds[i];
		if(caId2monicaYields.find(caId) != caId2monicaYields.end())
		{
//			cout << "caId: " << _cropActivityIds[i] << " -> gm: " << _grossMargins[i] << endl;

			if(_grossMargins[i] > 0)
			{
				r.cropActivityId2grossMargin[caId] = _grossMargins[i];
			}
			else
			{
				r.cropActivityId2grossMargin[caId] = 0.0;
			}
		}
	}

	//assert(r.cropActivityId2grossMargin.size() == caId2monicaYields.size());

//	cout << "leaving Mpmas::calculateFarmEconomy" << endl;
//	cout.flush();

	return r;
}

//-------------------------------------------------------------------------------------------------------


//map<SoilClassId, vector<vector<ProductionProcess> > >
//Carbiocial::cropRotationsFromUsedCropActivities2(vector<CropActivity*> cas)
//{
//	map<SoilClassId, vector<vector<ProductionProcess>>> scId2crs;

//	map<SoilClassId, map<SeasonId, vector<CropActivity*>>> scId2season2cas;
//	BOOST_FOREACH(CropActivity* ca, cas)
//	{
//		scId2season2cas[ca->soilClass->id][ca->productionPractice->product->season->id].push_back(ca);
//	}

//	//iterate over all soil classes
//	for(auto cit = scId2season2cas.begin(); cit != scId2season2cas.end(); cit++)
//	{
//		SoilClassId scId = cit->first;

//		//do we have safra crops?
//		if(cit->second.find(1) != cit->second.end())
//		{
//			BOOST_FOREACH(CropActivity* safraCA, (cit->second)[1]) //safra
//			{
//				ProductionProcess safraPP = productionProcessFrom(safraCA->productionPractice);
//				safraPP.setCustomId(safraCA->id);

//				//do we have safrinha crops?
//				if(cit->second.find(2) != cit->second.end())
//				{
//					BOOST_FOREACH(CropActivity* safrinhaCA, (cit->second)[2]) //safrinha
//					{
//						ProductionProcess safrinhaPP = productionProcessFrom(safrinhaCA->productionPractice);
//						safrinhaPP.setCustomId(safrinhaCA->id);

//						//do we have offseason crops?
//						if(cit->second.find(3) != cit->second.end())
//						{
//							BOOST_FOREACH(CropActivity* offseasonCA, (cit->second)[3]) //offseason
//							{
//								ProductionProcess offseasonPP = productionProcessFrom(offseasonCA->productionPractice);
//								offseasonPP.setCustomId(offseasonCA->id);

//								vector<ProductionProcess> cr;
//								cr.push_back(safraPP);
//								cr.push_back(safrinhaPP);
//								cr.push_back(offseasonPP);

//								scId2crs[scId].push_back(cr);
//							}
//						}
//						else //no offseason, but store safra and safrinha
//						{
//							vector<ProductionProcess> cr;
//							cr.push_back(safraPP);
//							cr.push_back(safrinhaPP);

//							scId2crs[scId].push_back(cr);
//						}
//					}
//				}
//				else //no safrinha, but maybe offseason
//				{
//					//do we have offseason crops?
//					if(cit->second.find(3) != cit->second.end())
//					{
//						BOOST_FOREACH(CropActivity* offseasonCA, (cit->second)[3]) //offseason
//						{
//							ProductionProcess offseasonPP = productionProcessFrom(offseasonCA->productionPractice);
//							offseasonPP.setCustomId(offseasonCA->id);

//							vector<ProductionProcess> cr;
//							cr.push_back(safraPP);
//							cr.push_back(offseasonPP);

//							scId2crs[scId].push_back(cr);
//						}
//					}
//					else //no offseason, but at least we got safra left
//					{
//						vector<ProductionProcess> cr;
//						cr.push_back(safraPP);

//						scId2crs[scId].push_back(cr);
//					}
//				}
//			}
//		}
//		else //no safra crops, just possibly safrinha and offseason
//		{
//			//but do we have safrinha crops?
//			if(cit->second.find(2) != cit->second.end())
//			{
//				BOOST_FOREACH(CropActivity* safrinhaCA, (cit->second)[2]) //safrinha
//				{
//					ProductionProcess safrinhaPP = productionProcessFrom(safrinhaCA->productionPractice);
//					safrinhaPP.setCustomId(safrinhaCA->id);

//					//do we have offseason crops?
//					if(cit->second.find(3) != cit->second.end())
//					{
//						BOOST_FOREACH(CropActivity* offseasonCA, (cit->second)[3]) //offseason
//						{
//							ProductionProcess offseasonPP = productionProcessFrom(offseasonCA->productionPractice);
//							offseasonPP.setCustomId(offseasonCA->id);

//							vector<ProductionProcess> cr;
//							cr.push_back(safrinhaPP);
//							cr.push_back(offseasonPP);

//							scId2crs[scId].push_back(cr);
//						}
//					}
//					else //no safra, no offseason, store safrinha crop
//					{
//						vector<ProductionProcess> cr;
//						cr.push_back(safrinhaPP);

//						scId2crs[scId].push_back(cr);
//					}
//				}
//			}
//			else //no safra, no safrinha, but maybe offseason
//			{
//				//finally check for offseason
//				if(cit->second.find(3) != cit->second.end())
//				{
//					BOOST_FOREACH(CropActivity* offseasonCA, (cit->second)[3]) //offseason
//					{
//						ProductionProcess offseasonPP = productionProcessFrom(offseasonCA->productionPractice);
//						offseasonPP.setCustomId(offseasonCA->id);

//						vector<ProductionProcess> cr;
//						cr.push_back(offseasonPP);

//						scId2crs[scId].push_back(cr);
//					}
//				}
//			}
//		}
//	}

//	return scId2crs;
//}

map<SoilClassId, vector<vector<ProductionProcess> > >
Carbiocial::cropRotationsFromUsedCropActivities(vector<CropActivity*> cas)
{
//	cout << "entering Carbiocial::cropRotationsFromUsedCropActivities" << endl;

	map<SoilClassId, vector<vector<ProductionProcess>>> scId2crs;

	map<SoilClassId, vector<CropActivity*>> scId2cas;
	BOOST_FOREACH(CropActivity* ca, cas)
	{
		scId2cas[ca->soilClass->id].push_back(ca);
	}

	vector<ProductionProcess> unsupportedPPs;

	//all crop activities for a soil class are treated independently
	for(auto cit = scId2cas.begin(); cit != scId2cas.end(); cit++)
	{
		SoilClassId scId = cit->first;

		//the temporary list crop rotations being creatable
		//from the CropActivities for soilclass with id scId
		vector<vector<ProductionProcess>> crs(1);

		map<int, ProductionProcess> customId2pp;

		//try to fit every CropActivity into one or more crop rotations
		BOOST_FOREACH(CropActivity* ca, cit->second)
		{
			ProductionProcess newPP = productionProcessFrom(ca->productionPractice);
			if(newPP.isFallow())
			{
				unsupportedPPs.push_back(newPP);
				continue;
			}

			customId2pp[newPP.customId()] = newPP;

			//vector<vector<ProductionProcess>> newCrs;
			map<int, map<int, map<int, int>>> newCrs;

			BOOST_FOREACH(vector<ProductionProcess>& cr, crs)
			{
				//if cr is empty just insert the current pp
				if(cr.empty())
				{
					cr.push_back(newPP);
				}
				//normal case, try to put new pp somewhere
				else
				{
					vector<ProductionProcess> newCr;

					auto prevCri = cr.begin();
					for(auto cri = cr.begin(); cri != cr.end(); cri++)
					{
						ProductionProcess currentPP = *cri;
//						customId2pp[currentPP.customId()] = currentPP;
						ProductionProcess prevPP = *prevCri;

						//new pp lies after previous pp and before current pp
						if((prevPP.end() < newPP.start() && newPP.end() < currentPP.start()) ||
							 (cri == prevCri && newPP.end() < currentPP.start()))
						{
							cr.insert(cr.begin(), newPP);

							//delete eventually build up newCr, because newPP has gone into old crop rotation
							newCr.clear();

							break;
						}
						//new pp starts after current pp, so copy current pp to new cr
						else if(currentPP.end() < newPP.start())
						{
							newCr.push_back(currentPP);
						}
						//new pp starts somewhere before, so copy cr and replace current pp
						else
						{
							newCr.push_back(newPP);
						}

						prevCri = cri;
					}

					//if we reached the end of the current crop rotation and
					//haven't put the newPP somewhere, append it to the end of newCr
					//(basically just happends when the currentPP gets copied to newCr in
					//the else if part
					if(!newCr.empty() && newCr.back().customId() != newPP.customId())
						newCr.push_back(newPP);

					if(!newCr.empty())
					{
						if(newCr.size() == 1)
							newCrs[newCr.at(0).customId()];
						else if(newCr.size() == 2)
							newCrs[newCr.at(0).customId()][newCr.at(1).customId()];
						else if(newCr.size() == 3)
							newCrs[newCr.at(0).customId()][newCr.at(1).customId()][newCr.at(2).customId()];
						//newCrs.push_back(newCr);
					}
				}
			}

//			//add copies of crop rotations with replaced elements to total list of crop rotations
//			BOOST_FOREACH(vector<ProductionProcess>& newCr, newCrs)
//			{
//				crs.push_back(newCr);
//			}
			for(auto ci = newCrs.begin(); ci != newCrs.end(); ci++)
			{
				ProductionProcess fstPP = customId2pp[ci->first];
				vector<ProductionProcess> newCr;
				newCr.push_back(fstPP);

				if(ci->second.empty())
					crs.push_back(newCr);
				else
				{
					for(auto ci2 = ci->second.begin(); ci2 != ci->second.end(); ci2++)
					{
						ProductionProcess sndPP = customId2pp[ci2->first];
						vector<ProductionProcess> newCr2 = newCr;
						newCr2.push_back(sndPP);

						if(ci2->second.empty())
							crs.push_back(newCr2);
						else
						{
							for(auto ci3 = ci2->second.begin(); ci3 != ci2->second.end(); ci3++)
							{
								ProductionProcess thrdPP = customId2pp[ci3->second];
								vector<ProductionProcess> newCr3 = newCr2;
								newCr3.push_back(thrdPP);

								crs.push_back(newCr3);
							}
						}
					}
				}
			}
		}

		scId2crs[scId] = crs;
	}

	scId2crs[-1].push_back(unsupportedPPs);

//	cout << "leaving Carbiocial::cropRotationsFromUsedCropActivities" << endl;
//	cout.flush();

	return scId2crs;
}


//map<SoilClassId, vector<vector<ProductionProcess> > >
//cropRotationsFromUsedCropActivities3(vector<CropActivity*> cas)
//{
//	map<SoilClassId, vector<vector<ProductionProcess>>> scId2crs;

//	map<SoilClassId, vector<CropActivity*>> scId2cas;
//	BOOST_FOREACH(CropActivity* ca, cas)
//	{
//		scId2cas[ca->soilClass->id].push_back(ca);
//	}

//	//all crop activities for a soil class are treated independently
//	for(auto cit = scId2cas.begin(); cit != scId2cas.end(); cit++)
//	{
//		SoilClassId scId = cit->first;

//		//the temporary list crop rotations being creatable
//		//from the CropActivities for soilclass with id scId
//		vector<list<ProductionProcess>> crs(1);

//		//try to fit every CropActivity into one or more crop rotations
//		BOOST_FOREACH(CropActivity* ca, cit->second)
//		{
//			ProductionProcess newPP = productionProcessFrom(ca->productionPractice);
//			bool putIntoNewCropRotation = true;
//			vector<list<ProductionProcess>> newCrs;

//			BOOST_FOREACH(list<ProductionProcess>& cr, crs)
//			{
//				//if cr is empty just insert the current pp
//				if(cr.empty())
//				{
//					cr.push_back(newPP);
//					putIntoNewCropRotation = false;
//				}
//				//normal case, try to put new pp somewhere
//				else
//				{
//					//try to figure out where in the current crop rotation put the new pp
//					for(auto cri = cr.begin(); cri != cr.end(); cri++)
//					{
//						ProductionProcess currentPP = *cri;

//						//new pp lies completely before current pp
//						if(newPP.end() < currentPP.start())
//						{
//							cr.insert(cr.begin(), newPP);
//							putIntoNewCropRotation = false;
//							break; //go to next crop rotation
//						}
//						//new pp lies completely behind current pp
//						else if(currentPP.end() < newPP.start())
//						{
//							auto cri1 = cri;
//							cri1++;
//							//has the crop rotation one more element?
//							if(cri1 != cr.end())
//							{
//								ProductionProcess nextPP = *cri1;
//								//new pp lies completely before next pp
//								if(newPP.end() < nextPP.start())
//								{
//									cr.insert(cri1, newPP);
//									putIntoNewCropRotation = false;
//									break; //go to next crop rotation
//								}
//								//new pp lies after current pp but ends before or exactly with the next pp,
//								//thus we could replace next pp by new pp, means we copy the whole
//								//pp and replace next pp by new pp
//								else if(newPP.end() <= nextPP.end())
//								{
//									//new crop rotation copy
//									list<ProductionProcess> newCr;
//									//copy until/including current element
//									copy(cr.begin(), cri1, back_inserter(newCr));
//									//add new pp instead of next pp
//									newCr.push_back(newPP);
//									//get interator to element after next pp
//									auto cri2 = cri1;
//									cri2++;
//									//copy everything after next pp to new crop rotation
//									copy(cri2, cr.end(), back_inserter(newCr));
//									//store new crop rotation to be append to crop rotation list after loop
//									newCrs.push_back(newCr);
//								}
//								//new pp lies after current pp but ends also after next pp,
//								//but maybe still before the next next pp starts
//								else
//								{
//									auto cri2 = cri1;
//									cri2++;

//									//there is a next next pp
//									if(cri2 != cr.end())
//									{
//										ProductionProcess nextNextPP = *cri2;

//										//new pp ends before next next pp starts
//										if(newPP.end() < nextNextPP.start())
//										{
//											//new crop rotation copy
//											list<ProductionProcess> newCr;
//											//copy until/including current element
//											copy(cr.begin(), cri1, back_inserter(newCr));
//											//add new pp instead of next pp
//											newCr.push_back(newPP);
//											//copy everything starting from next next pp to new crop rotation
//											copy(cri2, cr.end(), back_inserter(newCr));
//											//store new crop rotation to be append to crop rotation list after loop
//											newCrs.push_back(newCr);
//										}
//										//new pp ends somewhere in between next next pp, thus
//										//we've got to create a new crop rotation for the new pp
//										else
//										{
//											list<ProductionProcess> newCr;
//											newCr.push_back(newPP);
//											newCrs.push_back(newCr);
//											break;
//										}
//									}

//									else
//									{

//									}

//									continue; //try next pair of elements in crop rotation
//								}
//							}
//							//no more element, just insert the current pp at the end
//							else
//							{
//								cr.insert(cr.end(), newPP);
//								putIntoNewCropRotation = false;
//								break; //go to next crop rotation
//							}
//						}
//						//new pp lies somewhere inbetween, thus create stop traversing this
//						//crop rotation
//						else
//							break;

//					}
//				}
//			}

//			//add a left over element into its own crop rotation
//			if(putIntoNewCropRotation)
//			{
//				list<ProductionProcess> cr;
//				cr.push_back(newPP);
//				crs.push_back(cr);
//			}

//			//add copies of crop rotations with replaced elements to total list of crop rotations
//			BOOST_FOREACH(list<ProductionProcess>& newCr, newCrs)
//			{
//				crs.push_back(newCr);
//			}
//		}

//		//transform list of pps to vector of pps and store for current soilclass id
//		vector<vector<ProductionProcess>> crs2;
//		BOOST_FOREACH(list<ProductionProcess>& cr, crs)
//		{
//			vector<ProductionProcess> cr2;
//			BOOST_FOREACH(ProductionProcess& pp, cr)
//			{
//				cr2.push_back(pp);
//			}
//			crs2.push_back(cr2);
//		}

//		scId2crs[scId] = crs2;
//	}

//	return scId2crs;
//}


//-------------------------------------------------------------------------------------------------------



pair<int, int> roundTo5(int value)
{
	auto v = div(value, 10);
	switch(v.rem)
	{
	case 0: case 1: case 2: return make_pair(v.quot*10, v.rem);
	case 3: case 4: case 5: case 6: case 7: return make_pair(v.quot*10 + 5, 5 - v.rem);
	case 8: case 9: return make_pair(v.quot*10 + 10, 10 - v.rem);
	default: ;
	}
}

#ifndef NO_GRIDS
map<SoilClassId, int> Carbiocial::roundedSoilFrequency(const Grids::GridP* soilGrid, int roundToDigits)
{
	auto roundValue = [=](double v){ return Tools::roundRT<int>(v, 0); };
	auto roundPercentage = [=](double v){ return Tools::roundRT<int>(v, roundToDigits); };
	auto rsf = soilGrid->frequency<int, int>(false, roundValue, roundPercentage);
	int sumRoundedPercentage =
			accumulate(rsf.begin(), rsf.end(), 0,
								 [](int acc, pair<SoilClassId, int> p){ return acc + p.second; });
				
	int roundError = sumRoundedPercentage - 100;
	if(roundError != 0)
	{
		int delta = roundError > 0
								? -1 //we got too much percent = over 100%
								: 1; //we got too few percent = below 100%

		map<SoilClassId, int>::iterator i = rsf.begin();
		while(roundError != 0)
		{
			if(i == rsf.end())
				i = rsf.begin();

			i->second += delta;
			roundError += delta;

			i++;
		}

		map<SoilClassId, int> rsf2;
		for_each(rsf.begin(), rsf.end(), [&](pair<SoilClassId, int> p)
		{
			if(p.second > 0)
				rsf2.insert(p);
		});

		return rsf2;
	}

	return rsf;
}
#endif


void Carbiocial::runMonicaCarbiocial()
{
	/*
	Monica::activateDebug = true;

  debug() << "Running hermes with configuration information from \"" << output_path.c_str() << "\"" << endl;

  HermesSimulationConfiguration *hermes_config = new HermesSimulationConfiguration();
  hermes_config->setOutputPath(output_path);
  string ini_path;
  ini_path.append(output_path);
  ini_path.append("/monica.ini");

  IniParameterMap ipm(ini_path);

  // files input
  hermes_config->setSoilParametersFile(ipm.value("files", "soil"));
  hermes_config->setWeatherFile(ipm.value("files", "climate_prefix"));
  hermes_config->setRotationFile(ipm.value("files", "croprotation"));
  hermes_config->setFertiliserFile(ipm.value("files", "fertiliser"));
  hermes_config->setIrrigationFile(ipm.value("files", "irrigation"));

  // simulation time
  hermes_config->setStartYear(ipm.valueAsInt("simulation_time", "startyear"));
  hermes_config->setEndYear(ipm.valueAsInt("simulation_time", "endyear"));

  bool use_nmin_fertiliser = ipm.valueAsInt("nmin_fertiliser", "activated") == 1;
  if (use_nmin_fertiliser) {
      hermes_config->setOrganicFertiliserID(ipm.valueAsInt("nmin_fertiliser", "organic_fert_id"));
      hermes_config->setMineralFertiliserID(ipm.valueAsInt("nmin_fertiliser", "mineral_fert_id"));
      double min = ipm.valueAsDouble("nmin_fertiliser", "min", 10.0);
      double max = ipm.valueAsDouble("nmin_fertiliser", "max",100.0);
      int delay = ipm.valueAsInt("nmin_fertiliser", "delay_in_days",30);
      hermes_config->setNMinFertiliser(true);
      hermes_config->setNMinUserParameters(min, max, delay);
  }

  bool use_automatic_irrigation = ipm.valueAsInt("automatic_irrigation", "activated") == 1;
  if (use_automatic_irrigation) {
      double amount = ipm.valueAsDouble("automatic_irrigation", "amount", 0.0);
      double treshold = ipm.valueAsDouble("automatic_irrigation", "treshold", 0.15);
      double nitrate = ipm.valueAsDouble("automatic_irrigation", "nitrate", 0.0);
      double sulfate = ipm.valueAsDouble("automatic_irrigation", "sulfate", 0.0);
      hermes_config->setAutomaticIrrigation(true);
      hermes_config->setAutomaticIrrigationParameters(amount, treshold, nitrate, sulfate);
  }

  //site configuration
  hermes_config->setLattitude(ipm.valueAsDouble("site_parameters", "latitude", -1.0));
  hermes_config->setSlope(ipm.valueAsDouble("site_parameters", "slope", -1.0));
  hermes_config->setHeightNN(ipm.valueAsDouble("site_parameters", "heightNN", -1.0));
  hermes_config->setSoilCNRatio(ipm.valueAsDouble("site_parameters", "soilCNRatio", -1.0));
  hermes_config->setAtmosphericCO2(ipm.valueAsDouble("site_parameters", "atmospheric_CO2", -1.0));
  hermes_config->setWindSpeedHeight(ipm.valueAsDouble("site_parameters", "wind_speed_height", -1.0));
  hermes_config->setLeachingDepth(ipm.valueAsDouble("site_parameters", "leaching_depth", -1.0));
  hermes_config->setMinGWDepth(ipm.valueAsDouble("site_parameters", "groundwater_depth_min", -1.0));
  hermes_config->setMaxGWDepth(ipm.valueAsDouble("site_parameters", "groundwater_depth_max", -1.0));
  hermes_config->setMinGWDepthMonth(ipm.valueAsInt("site_parameters", "groundwater_depth_min_month", -1));

  hermes_config->setGroundwaterDischarge(ipm.valueAsDouble("site_parameters", "groundwater_discharge", -1.0));
  hermes_config->setLayerThickness(ipm.valueAsDouble("site_parameters", "layer_thickness", -1.0));
  hermes_config->setNumberOfLayers(ipm.valueAsDouble("site_parameters", "number_of_layers", -1.0));
  hermes_config->setCriticalMoistureDepth(ipm.valueAsDouble("site_parameters", "critical_moisture_depth", -1.0));
  hermes_config->setSurfaceRoughness(ipm.valueAsDouble("site_parameters", "surface_roughness", -1.0));
  hermes_config->setDispersionLength(ipm.valueAsDouble("site_parameters", "dispersion_length", -1.0));
  hermes_config->setMaxPercolationRate(ipm.valueAsDouble("site_parameters", "max_percolation_rate", -1.0));
  hermes_config->setPH(ipm.valueAsDouble("site_parameters", "pH", -1.0));
  hermes_config->setNDeposition(ipm.valueAsDouble("site_parameters", "N_deposition", -1.0));

  // general parameters
  hermes_config->setSecondaryYields(ipm.valueAsBool("general_parameters", "use_secondary_yields", true));

  // initial values
  hermes_config->setInitPercentageFC(ipm.valueAsDouble("init_values", "init_percentage_FC", -1.0));
  hermes_config->setInitSoilNitrate(ipm.valueAsDouble("init_values", "init_soil_nitrate", -1.0));
  hermes_config->setInitSoilAmmonium(ipm.valueAsDouble("init_values", "init_soil_ammonium", -1.0));


  const Monica::Result result = runWithHermesData(hermes_config);

  delete hermes_config;
  return result;
	*/


	/*
	PVPsList PVPflanze::disallowedFor(Betrieb* b)
	{
		static Private::L lockable;
		static bool initialized = false;
		typedef map<int, PVPsList> Int2List;
		static Int2List mfId2dpvs; //Marktfruchtbau
		static Int2List ffbId2dpvs; //Feldfutterbau
		static Int2List bewId2dpvs; //Bewirtschaftungsweise
		static Int2List zfId2dpvs; //Zwischenfrucht

		if(!initialized)
		{
			Private::L::Lock lock(lockable);

			if(!initialized)
			{
				const Collection& allPvs = PVPflanze::all();
				//make pvs fast accessible via their id
				map<int, PVPflanze*> id2pv;
				for(Collection::const_iterator i = allPvs.begin(); i != allPvs.end(); i++)
					id2pv.insert(make_pair((*i)->id, *i));

				DBPtr con(newConnection("eom"));
				DBRow row;

				con->select("select MFVarNr, PVPNr FROM AWPVPVarMF");
				while(!(row = con->getRow()).empty())
				{
					PVPsList& dpvs = mfId2dpvs[satoi(row[0])];
					dpvs.push_back(id2pv[satoi(row[1])]);
				}

				con->select("select FFBVarNr, PVPNr FROM AWPVPVarFFB");
				while(!(row = con->getRow()).empty())
				{
					PVPsList& dpvs = ffbId2dpvs[satoi(row[0])];
					dpvs.push_back(id2pv[satoi(row[1])]);
				}

				con->select("select BewNr, PVPNr FROM AWPVPBew");
				while(!(row = con->getRow()).empty())
				{
					PVPsList& dpvs = bewId2dpvs[satoi(row[0])];
					dpvs.push_back(id2pv[satoi(row[1])]);
				}

				con->select("select ZFVarNr, PVPNr FROM AWPVPZF");
				while(!(row = con->getRow()).empty())
				{
					PVPsList& dpvs = zfId2dpvs[satoi(row[0])];
					dpvs.push_back(id2pv[satoi(row[1])]);
				}

				initialized = true;
			}
		}

		PVPsSet result;
		Int2List::const_iterator it;

		if(b->marktfruchtbau &&
			 (it = mfId2dpvs.find(b->marktfruchtbau->id)) != mfId2dpvs.end())
		{
			result.insert(it->second.begin(), it->second.end());
		}

		if(b->feldfutterbau &&
			 (it = ffbId2dpvs.find(b->feldfutterbau->id)) != ffbId2dpvs.end())
		{
			result.insert(it->second.begin(), it->second.end());
		}

		if(b->bewirtschaftungsweise &&
			 (it = bewId2dpvs.find(b->bewirtschaftungsweise->id)) != bewId2dpvs.end())
		{
			result.insert(it->second.begin(), it->second.end());
		}

		if(b->zwischenfruchtanbau &&
			 (it = zfId2dpvs.find(b->zwischenfruchtanbau->id)) != zfId2dpvs.end())
		{
			result.insert(it->second.begin(), it->second.end());
		}

		return PVPsList(result.begin(), result.end());
	}
	*/
}

#endif

std::pair<const SoilPMs *, SoilClassId>
Carbiocial::carbiocialSoilParameters(int profileId, int layerThicknessCm,
int maxDepthCm)
{
	//cout << "getting soilparameters for STR: " << str << endl;
	int maxNoOfLayers = int(double(maxDepthCm) / double(layerThicknessCm));

	static L lockable;

	typedef map<Carbiocial::ProfileId, SoilPMsPtr> Map;
	typedef map<Carbiocial::ProfileId, Carbiocial::SoilClassId> PId2SCId;
	static bool initialized = false;
	static Map spss;
	static PId2SCId profileId2soilClassId;

	if (!initialized)
	{
		L::Lock lock(lockable);

		if (!initialized)
		{
			DBPtr con(newConnection("carbiocial"));
			DBRow row;

			ostringstream s;
			s << "select id, count(horizon_id) "
				"from soil_profile_data "
				"where id not null "
				"group by id";
			con->select(s.str().c_str());

			map<int, int> id2layerCount;
			while (!(row = con->getRow()).empty())
				id2layerCount[satoi(row[0])] = satoi(row[1]);
			con->freeResultSet();

			ostringstream s2;
			s2 << "select id, horizon_id, soil_class_id, "
				"upper_horizon_cm, lower_horizon_cm, "
				"silt_percent, clay_percent, sand_percent, "
				"ph_kcl, c_org_percent, c_n, bulk_density_t_per_m3 "
				"from soil_profile_data "
				"where id not null "
				"order by id, horizon_id";
			con->select(s2.str().c_str());

			while (!(row = con->getRow()).empty())
			{
				Carbiocial::ProfileId id = Carbiocial::ProfileId(satoi(row[0]));

				Carbiocial::SoilClassId soilClassId = Carbiocial::SoilClassId(satoi(row[2]));
				if (profileId2soilClassId.find(id) == profileId2soilClassId.end())
					profileId2soilClassId[id] = soilClassId;

				Map::iterator spsi = spss.find(id);
				SoilPMsPtr sps;

				if (spsi == spss.end())
					spss.insert(make_pair(id, sps = SoilPMsPtr(new SoilPMs)));
				else
					sps = spsi->second;

				int hcount = id2layerCount[int(id)];
				int currenth = satoi(row[1]);

				int ho = sps->size()*layerThicknessCm;
				int hu = satoi(row[4]) ? satoi(row[4]) : maxDepthCm;
				int hsize = max(0, hu - ho);
				int subhcount = Tools::roundRT<int>(double(hsize) / double(layerThicknessCm), 0);
				if (currenth == hcount && (int(sps->size()) + subhcount) < maxNoOfLayers)
					subhcount += maxNoOfLayers - sps->size() - subhcount;

				SoilParameters p;
				if (!row[8].empty())
					p.vs_SoilpH = satof(row[8]);
				p.set_vs_SoilOrganicCarbon(row[9].empty() ? 0 : satof(row[9]) / 100.0);
				p.set_vs_SoilRawDensity(satof(row[11]));
				p.vs_SoilSandContent = satof(row[7]) / 100.0;
				p.vs_SoilClayContent = satof(row[6]) / 100.0;
				p.vs_SoilTexture = texture2KA5(p.vs_SoilSandContent, p.vs_SoilClayContent);
				p.vs_SoilStoneContent = 0.0;
				p.vs_Lambda = texture2lambda(p.vs_SoilSandContent, p.vs_SoilClayContent);

				// initialization of saturation, field capacity and perm. wilting point
				soilCharacteristicsKA5(p);

				bool valid_soil_params = p.isValid();
				if (!valid_soil_params)
				{
					cout << "Error in soil parameters. Aborting now simulation";
					exit(-1);
				}

				for (int i = 0; i < subhcount; i++)
					sps->push_back(p);
			}

			initialized = true;
		}
	}

	static SoilPMs nothing;
	Map::const_iterator ci = spss.find(profileId);
	return ci != spss.end() ? make_pair(ci->second.get(), profileId2soilClassId[profileId])
		: make_pair(&nothing, -1);
}


/**
* Method for starting a simulation with coordinates from ascii grid.
* A configuration object is passed that stores all relevant
* information e.g. location, output path etc.
*
* @param simulation_config Configuration object that stores all simulation information
*/
std::map<int, double> Carbiocial::runCarbiocialSimulation(const CarbiocialConfiguration* simulation_config)
{
	//std::cout << "Start Carbiocial cluster Simulation" << std::endl;

	//  int phase = simulation_config->getPhase();
	//  int step = simulation_config->getStep();

	std::string input_path = simulation_config->getInputPath();
	std::string output_path = simulation_config->getOutputPath();

	// read in ini - file ------------------------------------------
	IniParameterMap ipm(input_path + simulation_config->getIniFile());

	std::string crop_rotation_file = ipm.value("files", "croprotation");
	std::string fertilisation_file = ipm.value("files", "fertiliser");

	//std::cout << "soil_file: " << soil_file.c_str() << "\t" << ipm.value("files", "soil").c_str() << endl;
	//std::cout << "crop_rotation_file: " << crop_rotation_file.c_str() << endl;
	//std::cout << "fertilisation_file: " << fertilisation_file.c_str() << endl << endl;

	bool use_automatic_irrigation = ipm.valueAsInt("automatic_irrigation", "activated") == 1;
	double amount, treshold, nitrate, sulfate;
	if (use_automatic_irrigation) {
		amount = ipm.valueAsDouble("automatic_irrigation", "amount", 0.0);
		treshold = ipm.valueAsDouble("automatic_irrigation", "treshold", 0.15);
		nitrate = ipm.valueAsDouble("automatic_irrigation", "nitrate", 0.0);
		sulfate = ipm.valueAsDouble("automatic_irrigation", "sulfate", 0.0);
	}
	//  std::cout << "use_automatic_irrigation: " << use_automatic_irrigation << endl;
	//  std::cout << "amount: " << amount << endl;
	//  std::cout << "treshold: " << treshold << endl;
	//  std::cout << "nitrate: " << nitrate << endl;
	//  std::cout << "sulfate: " << sulfate << endl << endl;


	//site configuration
	double latitude = ipm.valueAsDouble("site_parameters", "latitude", -1.0);
	//  double slope = ipm.valueAsDouble("site_parameters", "slope", -1.0);
	//  double heightNN = ipm.valueAsDouble("site_parameters", "heightNN", -1.0);
	double CN_ratio = ipm.valueAsDouble("site_parameters", "soilCNRatio", -1.0);
	double atmospheric_CO2 = ipm.valueAsDouble("site_parameters", "atmospheric_CO2", -1.0);
	double wind_speed_height = ipm.valueAsDouble("site_parameters", "wind_speed_height", -1.0);
	double leaching_depth = ipm.valueAsDouble("site_parameters", "leaching_depth", -1.0);
	//  double critical_moisture_depth = ipm.valueAsDouble("site_parameters", "critical_moisture_depth", -1.0);
	//double ph = ipm.valueAsDouble("site_parameters", "pH", -1.0);

	//  std::cout << "latitude: " << latitude << endl;
	//  std::cout << "slope: " << slope << endl;
	//  std::cout << "heightNN: " << heightNN << endl;
	//  std::cout << "CN_ratio: " << CN_ratio << endl;
	//  std::cout << "atmospheric_CO2: " << atmospheric_CO2 << endl;
	//  std::cout << "wind_speed_height: " << wind_speed_height << endl;
	//  std::cout << "leaching_depth: " << leaching_depth << endl;
	//  std::cout << "critical_moisture_depth: " << critical_moisture_depth << endl;
	//  std::cout << "ph: " << ph << endl << endl;

	// general parameters
	bool n_response = ipm.valueAsInt("general_parameters", "nitrogen_response_on", 1) == 1;
	bool water_deficit_response = ipm.valueAsInt("general_parameters", "water_deficit_response_on", 1) == 1;
	bool emergence_flooding_control = ipm.valueAsInt("general_parameters", "emergence_flooding_control_on", 1) == 1;
	bool emergence_moisture_control = ipm.valueAsInt("general_parameters", "emergence_moisture_control_on", 1) == 1;
	//  std::cout << "n_response: " << n_response << endl;
	//  std::cout << "water_deficit_response: " << water_deficit_response << endl << endl;

	// initial values
	double init_FC = ipm.valueAsDouble("init_values", "init_percentage_FC", -1.0);


	//  std::cout << "init_FC: " << init_FC << endl;

	// ---------------------------------------------------------------------

	CentralParameterProvider centralParameterProvider = readUserParameterFromDatabase();
	centralParameterProvider.userEnvironmentParameters.p_AthmosphericCO2 = atmospheric_CO2;
	centralParameterProvider.userEnvironmentParameters.p_WindSpeedHeight = wind_speed_height;
	centralParameterProvider.userEnvironmentParameters.p_LeachingDepth = leaching_depth;
	centralParameterProvider.userInitValues.p_initPercentageFC = init_FC;

	SiteParameters siteParams;
	siteParams.vs_Latitude = simulation_config->getLatitude();
	siteParams.vs_Slope = 0.01;
	siteParams.vs_HeightNN = simulation_config->getElevation();
	siteParams.vs_Soil_CN_Ratio = CN_ratio; // TODO: xeh CN_Ratio aus Bodendatei auslesen

	//cout << "Site parameters " << endl;
	//cout << siteParams.toString().c_str() << endl;

	double layer_thickness = centralParameterProvider.userEnvironmentParameters.p_LayerThickness;
	double profile_depth = layer_thickness * double(centralParameterProvider.userEnvironmentParameters.p_NumberOfLayers);
	double max_mineralisation_depth = 0.4;

	GeneralParameters gps = GeneralParameters(layer_thickness, profile_depth, max_mineralisation_depth, n_response, water_deficit_response, emergence_flooding_control, emergence_moisture_control);

	//soil data
	const SoilPMs* sps;

	//  std::string project_id = simulation_config->getProjectId();
	//  std::string lookup_project_id = simulation_config->getLookupProjectId();

	pair<const SoilPMs*, Carbiocial::SoilClassId> p =
		Carbiocial::carbiocialSoilParameters(simulation_config->getProfileId(),
		int(layer_thickness * 100),
		int(profile_depth * 100));
	sps = p.first;

	//no soil available, return no yields
	if (sps->empty())
		return std::map<int, double>();

	//	sps = soilParametersFromCarbiocialFile(soil_file, gps, ph);

	//cout << "Groundwater min:\t" << centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepth << endl;
	//cout << "Groundwater max:\t" << centralParameterProvider.userEnvironmentParameters.p_MaxGroundwaterDepth << endl;
	//cout << "Groundwater month:\t" << centralParameterProvider.userEnvironmentParameters.p_MinGroundwaterDepthMonth << endl;

	// climate data
	Climate::DataAccessor climateData = 
		climateDataFromCarbiocialFiles(simulation_config->getClimateFile(), centralParameterProvider, latitude, simulation_config);
	//cout << "Return from climateDataFromMacsurFiles" << endl;

	// fruchtfolge
	std::string file = input_path + crop_rotation_file;
	vector<ProductionProcess> ff = cropRotationFromHermesFile(file);
	//cout << "Return from cropRotationFromHermesFile" << endl;
	// fertilisation
	file = input_path + fertilisation_file;
	attachFertiliserApplicationsToCropRotation(ff, file);
	//cout << "Return from attachFertiliserApplicationsToCropRotation" << endl;
	//  BOOST_FOREACH(const ProductionProcess& pv, ff){
	//    cout << "pv: " << pv.toString() << endl;
	//  }


	//build up the monica environment
	Env env(sps, centralParameterProvider);
	env.general = gps;
	env.pathToOutputDir = output_path;
	env.setMode(Env::MODE_CARBIOCIAL_CLUSTER);
	env.site = siteParams;

	env.da = climateData;
	env.cropRotation = ff;

	if (use_automatic_irrigation) 
	{
		env.useAutomaticIrrigation = true;
		env.autoIrrigationParams = AutomaticIrrigationParameters(amount, treshold, nitrate, sulfate);
	}
	//cout << env.toString() << endl;
	//cout << "Before runMonica" << endl;
	Monica::Result res = runMonica(env);
	//cout << "After runMonica" << endl;

	std::map<int, double> year2yield;
	int ey = env.da.endDate().year();
	for (int i = 0, size = res.pvrs.size(); i < size; i++)
	{
		int year = ey - size + 1 + i;
		double yield = res.pvrs[i].pvResults[primaryYieldTM] / 10.0;
		debug() << "year: " << year << " yield: " << yield << " tTM" << endl;
		year2yield[year] = Tools::round(yield, 3);
	}

	return year2yield;
}

/**
* Read climate information from macsur file
*/
Climate::DataAccessor Carbiocial::climateDataFromCarbiocialFiles(const std::string& pathToFile,
	const CentralParameterProvider& cpp, double latitude, const CarbiocialConfiguration* simulationConfig)
{
	bool reorderData = simulationConfig->create2013To2040ClimateData;
	map<int, map<int, map<int, vector<int>>>> year2month2day2newDMY;
	if (reorderData)
	{
		string pathToReorderingFile = simulationConfig->pathToClimateDataReorderingFile;
		ifstream ifs(pathToReorderingFile.c_str());
		if (!ifs.good()) {
			cerr << "Could not open file " << pathToReorderingFile << " . Aborting now!" << endl;
			exit(1);
		}

		string s;
		while (getline(ifs, s)) 
		{
			istringstream iss(s);
			string arrow;
			int fy, fm, fd;
			vector<int> to(3);

			iss >> to[0] >> to[1] >> to[2] >> arrow >> fd >> fm >> fy;

			year2month2day2newDMY[fy][fm][fd] = to;
		}
	}


	//cout << "climateDataFromMacsurFiles: " << pathToFile << endl;
	Climate::DataAccessor da(simulationConfig->getStartDate(), simulationConfig->getEndDate());

	Date startDate = simulationConfig->getStartDate();
	Date endDate = simulationConfig->getEndDate();
	int dayCount = endDate - startDate + 1;
	//  cout << "startDate: " << startDate.toString() << endl;
	//  cout << "endDate: " << endDate.toString() << endl;

	ifstream ifs(pathToFile.c_str(), ios::binary);
	if (!ifs.good()) {
		cerr << "Could not open file " << pathToFile.c_str() << " . Aborting now!" << endl;
		exit(1);
	}

	//if have to store all data before adding them to the DataAccessor
	//because the climate files may contain duplicate entries and might
	//not be fully ordered due to problems during processing
	typedef vector<double> Values;
	typedef map<int, Values> DailyValues;
	typedef map<int, DailyValues> MonthlyValues;
	typedef map<int, MonthlyValues> YearlyValues;
	YearlyValues data;
	
	string s;
	while (getline(ifs, s)) 
	{
		//skip (repeated) headers
		if (s.substr(0, 3) == "day")
			continue;

		vector<string> r = splitString(s, ",");

		int day = satoi(r.at(0));
		int month = satoi(r.at(1));
		int year = satoi(r.at(2));

		if (reorderData)
		{
			auto to = year2month2day2newDMY[year][month][day];
			day = to[0];
			month = to[1];
			year = to[2];
		}
		
		if (year < startDate.year())
			continue;
		else if (month < startDate.month())
			continue;
		else if (day < startDate.day())
			continue;

		if (year > endDate.year())
			continue;
		else if (month > endDate.month())
			continue;
		else if (day > endDate.day())
			continue;

		//double tmin, tavg, tmax ,precip, globrad, relhumid, windspeed;
		vector<double> d;
		d.push_back(satof(r.at(4)));
		d.push_back(satof(r.at(5)));
		d.push_back(satof(r.at(6)));
		d.push_back(satof(r.at(7)));
		d.push_back(satof(r.at(8)));
		d.push_back(satof(r.at(9)));
		d.push_back(satof(r.at(10)));

		//    cout << "[" << day << "." << month << "." << year << "] ";

		data[year][month][day] = d;
	}

	//  cout << endl;

	vector<double> tavgs, tmins, tmaxs, precips, globrads, relhumids, winds;

	int daysAdded = 0;
	for (auto yci = data.begin(); yci != data.end(); yci++)
	{
		auto md = yci->second;
		for (auto mci = md.begin(); mci != md.end(); mci++)
		{
			auto dd = mci->second;
			for (auto dci = dd.begin(); dci != dd.end(); dci++)
			{
				auto d = dci->second;
				tavgs.push_back(d.at(0));
				tmins.push_back(d.at(1));
				tmaxs.push_back(d.at(2));
				precips.push_back(d.at(3));
				globrads.push_back(d.at(4));
				relhumids.push_back(d.at(5));
				winds.push_back(d.at(6));

				daysAdded++;
			}
		}
	}

	//  cout << "daysAdded: " << daysAdded << endl;
	if (daysAdded != dayCount) {
		cout << "Wrong number of days in " << pathToFile.c_str() << " ." << " Found " << daysAdded << " days but should have been "
			<< dayCount << " days. Aborting." << endl;
		exit(1);
	}

	da.addClimateData(Climate::tmin, tmins);
	da.addClimateData(Climate::tmax, tmaxs);
	da.addClimateData(Climate::tavg, tavgs);
	da.addClimateData(Climate::precip, precips);
	da.addClimateData(Climate::globrad, globrads);
	da.addClimateData(Climate::relhumid, relhumids);
	da.addClimateData(Climate::wind, winds);

	return da;
}


