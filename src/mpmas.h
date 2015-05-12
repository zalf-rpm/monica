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

#ifndef _MPMAS_H_
#define _MPMAS_H_

#include <vector>
#include <map>
#include <set>

#include "monica-parameters.h"
#include "tools/date.h"
#include "db/orm--.h"
#ifndef NO_GRIDS
#include "grid/grid+.h"
#endif

#ifndef NO_MPMAS
#include "MpmasMaster.h"
#endif

namespace mpmasScope
{	// just for now, so that this simple exit function can still be used
	//mpmas* mpmasPointer;
	void exitfun();
}

namespace Mpmas
{
	typedef int SoilClassId;
	typedef int ProfileId;
	typedef int Year;

	struct IdPlusCode : public Db::Identifiable
	{
		//! default constructor
		IdPlusCode(int id = -1, const std::string& name = std::string(),
								const std::string& code = std::string())
			: Identifiable(id, name), code(code) {}

		std::string code;

		//! output will include the description
		virtual std::string toString(const std::string& indent = std::string(),
																 bool detailed = false) const;
	};

	template<class T>
	struct SetCodeFields : public Db::SetFields<T>
	{
		//! 3 columns
		enum { noOfCols = 3 };
		//! call operator
		void operator()(int col, T* t, const std::string& value)
		{
			switch(col)
			{
			case 0:
			case 1: Db::SetNameFields<T>()(col, t, value); break;
			case 2: t->code = value; break;
			}
		}
	};

	//----------------------------------------------------------------------------
	
	struct SoilClass : public IdPlusCode
	{
	public: //static
		typedef std::vector<SoilClass*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetCodeFields<SoilClass>>("carbiocial")
					("SELECT soil_type_id, soil_type_txt, soil_type_code "
					 "FROM tbl_soil_types "
					 "ORDER BY soil_type_id");
		}

		static SoilClass* sc4id(int id){ return Db::t4id<SoilClass>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All soil classes: ");
		}
	};

	//----------------------------------------------------------------------------

	struct Municipality;

	typedef int SectorId;
	struct Sector : public Db::Identifiable
	{
		Municipality* municipality;
		std::map<SoilClassId, int> soilClassId2percentage;
	};

	//----------------------------------------------------------------------------

	typedef int MunicipalityId;
	struct Municipality : public IdPlusCode
	{
		std::map<SectorId, Sector*> sectorId2sector;

	public: //static
		struct PostCode_SetSectors
		{
			template<class PS>
			void operator()(PS& ms)
			{
				std::map<MunicipalityId, Municipality*> id2m;
        for(Municipality* m : ms)
				{
					id2m[m->id] = m;
				}

				Db::DBPtr con(Db::newConnection("carbiocial"));
				Db::DBRow row;

				con->select("SELECT municip_id, sector_id, soil_type_id, percentage "
										"FROM tbl_unique_sectors_in_municipality "
										"ORDER BY municip_id, sector_id, soil_type_id");
				while(!(row=con->getRow()).empty())
				{
					int municipId = Tools::satoi(row[0]);
					Municipality* m = id2m[municipId];

					int sectorId = Tools::satoi(row[1]);
					int soilClassId = Tools::satoi(row[2]);
					int percentage = Tools::satoi(row[3]);
					Sector* s = NULL;
					std::map<int, Sector*>::iterator it = m->sectorId2sector.find(sectorId);
					if(it != m->sectorId2sector.end())
						s = it->second;
					else
					{
						s = new Sector;
						s->id = sectorId;
						s->municipality = m;
						s->name = row[1];
						m->sectorId2sector[sectorId] = s;
					}

					s->soilClassId2percentage[soilClassId] = percentage;
				}
			}
		};

		typedef std::vector<Municipality*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetCodeFields<Municipality>, PostCode_SetSectors>("carbiocial")
					("SELECT municip_id, municip_name, municip_code "
					 "FROM tbl_municipalities "
					 "ORDER BY municip_id");
		}

		static Municipality* m4id(int id){ return Db::t4id<Municipality>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All municipalities: ");
		}
	};

	//----------------------------------------------------------------------------

	typedef int FertilizerId;
	struct Fertilizer : public IdPlusCode
	{
		Fertilizer() : n(0.0), p(0.0), k(0.0), price(0.0) {}

		double n; //percent
		double p; //percent
		double k; //percent
		double price; //per ton

	public: //static
		struct SetFertilizerFields : public Db::SetFields<Fertilizer>
		{
			enum { noOfCols = 7 };
			void operator()(int col, Fertilizer* s, const std::string& value)
			{
				switch(col)
				{
				case 0:
				case 1:
				case 2:
					SetCodeFields<Fertilizer>()(col, s, value);
					break;
				case 3:
					s->n = Tools::satof(value, 0.0);
					break;
				case 4:
					s->p = Tools::satof(value, 0.0);
					break;
				case 5:
					s->k = Tools::satof(value, 0.0);
					break;
				case 6:
					s->price = Tools::satof(value, 0.0);
					break;
				}
			}
		};

		typedef std::vector<Fertilizer*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetFertilizerFields>("carbiocial")(
						"SELECT fertilizer_id, fertilizer_txt, fertilizer_code, "
						"n_content_pct, p_content_pct, k_content_pct, price_ton "
						"FROM tbl_fertilizer "
						"ORDER BY fertilizer_id");
		}

		static Fertilizer* f4id(int id){ return Db::t4id<Fertilizer>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All fertilizers: ");
		}

	};

	//----------------------------------------------------------------------------

	typedef int FarmId;
	struct Farm : public Db::Identifiable
	{
		std::string agriculturalType;

	public: //static
		struct SetFarmFields : public Db::SetFields<Farm>
		{
			enum { noOfCols = 3 };
			void operator()(int col, Farm* s, const std::string& value)
			{
				switch(col)
				{
				case 0:
				case 1:
					Db::SetNameFields<Farm>()(col, s, value);
					break;
				case 2:
					s->agriculturalType = value;
					break;
				}
			}
		};

		typedef std::vector<Farm*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetFarmFields>("carbiocial")
					("SELECT farm_type_id, farm_size_txt, agriculture_type_txt "
					 "FROM tbl_farm_types "
					 "ORDER BY farm_type_id");
		}

		static Farm* f4id(int id){ return Db::t4id<Farm>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All farms: ");
		}
	};

	//----------------------------------------------------------------------------

	typedef int SeasonId;
	struct Season : public IdPlusCode
	{
		Season() : startMonth(0), endMonth(0) {}

		int startMonth;
		int endMonth;

	public: //static
		struct SetSeasonFields : public Db::SetFields<Season>
		{
			enum { noOfCols = 5 };
			void operator()(int col, Season* s, const std::string& value)
			{
				switch(col)
				{
				case 0:
				case 1:
				case 2:
					SetCodeFields<Season>()(col, s, value);
					break;
				case 3:
					s->startMonth = Tools::satoi(value, 0);
					break;
				case 4:
					s->endMonth = Tools::satoi(value, 0);
						break;
				}
			}
		};

		typedef std::vector<Season*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetSeasonFields>("carbiocial")(
						"SELECT season_id, season_txt, season_code, start_month, end_month "
						"FROM tbl_seasons "
						"ORDER BY season_id");
		}

		static Season* s4id(int id){ return Db::t4id<Season>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All seasons: ");
		}

	};

	//----------------------------------------------------------------------------

	typedef int OperationId;
	struct Operation : public IdPlusCode
	{
	public: //static
		typedef std::vector<Operation*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetCodeFields<Operation>>("carbiocial")
					("SELECT operation_id, operation_txt, operation_code "
					 "FROM tbl_operations "
					 "ORDER BY operation_id");
		}

		static const Operation* o4id(int id){ return Db::t4id<Operation>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All operations: ");
		}
	};

	//----------------------------------------------------------------------------

	typedef int ProductId;
	struct Product : public IdPlusCode
	{
		Product() : season(NULL) {}

		std::string unit;
		Season* season;
		std::multimap<OperationId, int> operationId2month;

	public: //static
		struct SetProductFields : public Db::SetFields<Product>
		{
			enum { noOfCols = 5 };
			void operator()(int col, Product* p, const std::string& value)
			{
				switch(col)
				{
				case 0:
				case 1:
				case 2:
					SetCodeFields<Product>()(col, p, value);
					break;
				case 3:
					p->unit = value;
					break;
				case 4:
					p->season = Season::s4id(Tools::satoi(value, 0));
					break;
				}
			}
		};

		struct PostCode_SetOperationsOnProduct
		{
			template<class PS>
			void operator()(PS& ps)
			{
				std::map<ProductId, Product*> id2p;
        for(Product* p : ps)
				{
					id2p[p->id] = p;
				}

				Db::DBPtr con(Db::newConnection("carbiocial"));
				Db::DBRow row;

				con->select("SELECT product_id, operation_id, month "
										"FROM tbl_crop_calendar "
										"ORDER BY product_id, operation_id");
				while(!(row=con->getRow()).empty())
				{
					Product* p = id2p[Tools::satoi(row[0])];
					int operationId = Tools::satoi(row[1]);
					int month = Tools::satoi(row[2]);
					p->operationId2month.insert(std::make_pair(operationId, month));
				}
			}
		};

		typedef std::vector<Product*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetProductFields, PostCode_SetOperationsOnProduct>("carbiocial")(
						"SELECT product_id, product_txt, product_code, unit, season_id "
						"FROM tbl_products "
						"ORDER BY product_id");
		}

		static Product* p4id(int id){ return Db::t4id<Product>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All products: ");
		}

	};

	//---------------------------------------------------------------------------

	typedef int ProductionPracticeId;
	struct ProductionPractice : public IdPlusCode
	{
		ProductionPractice() : product(NULL) {}

		std::string source;
		Product* product;
		std::map<FertilizerId, std::pair<Year, int>> fertilizerId2appYearAndAmountKg;

	public: //static
		struct SetProductionPracticeFields : public Db::SetFields<ProductionPractice>
		{
			enum { noOfCols = 5 };
			void operator()(int col, ProductionPractice* p, const std::string& value)
			{
				switch(col)
				{
				case 0:
				case 1:
				case 2:
					SetCodeFields<ProductionPractice>()(col, p, value);
					break;
				case 3:
					p->source = value;
					break;
				case 4:
					p->product = Product::p4id(Tools::satoi(value, 0));
					break;
				}
			}
		};

		struct PostCode_SetFerilizersOnProductionPractice
		{
			template<class PS>
			void operator()(PS& pps)
			{
				std::map<ProductionPracticeId, ProductionPractice*> id2pp;
        for(ProductionPractice* pp : pps)
				{
					id2pp[pp->id] = pp;
				}

				Db::DBPtr con(Db::newConnection("carbiocial"));
				Db::DBRow row;

				con->select("SELECT pr_practice_id, fertilizer_id, unit, 0, "
										"fertilizer_quantity "
										"FROM tbl_fertilizer_use "
										"union "
										"SELECT pr_practice_id, fertilizer_id, unit, year, "
										"fertilizer_quantity "
										"FROM tbl_fertilizer_use_perennials "
										"ORDER BY pr_practice_id");
				while(!(row=con->getRow()).empty())
				{
					ProductionPractice* p = id2pp[Tools::satoi(row[0])];
					int fertilizerId = Tools::satoi(row[1]);
					int year = Tools::satoi(row[3]);
					int amountKg = row[2] == "ton" ? Tools::satoi(row[4])*1000 : 0;
					p->fertilizerId2appYearAndAmountKg[fertilizerId] = std::make_pair(year, amountKg);
				}
			}
		};

		typedef std::vector<ProductionPractice*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetProductionPracticeFields, PostCode_SetFerilizersOnProductionPractice>("carbiocial")(
						"SELECT pr_practice_id, practice_txt, practice_code, source, product_id "
						"FROM tbl_production_practices "
						"ORDER BY pr_practice_id");
		}

		static ProductionPractice* pp4id(int id){ return Db::t4id<ProductionPractice>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All production practices: ");
		}

	};

	//----------------------------------------------------------------------------

	Monica::ProductionProcess productionProcessFrom(ProductionPractice* prp);

	//----------------------------------------------------------------------------

	typedef int CropActivityId;
	struct CropActivity : public Db::Identifiable
	{
		CropActivity() : productionPractice(NULL), soilClass(NULL) {}

		ProductionPractice* productionPractice;
		SoilClass* soilClass;

	public: //static
		struct SetCropActivityFields : public Db::SetFields<CropActivity>
		{
			enum { noOfCols = 4 };
			void operator()(int col, CropActivity* ca, const std::string& value)
			{
				switch(col)
				{
				case 0:
				case 1:
					Db::SetNameFields<CropActivity>()(col, ca, value);
					break;
				case 2:
					ca->productionPractice = ProductionPractice::pp4id(Tools::satoi(value));
					break;
				case 3:
					ca->soilClass = SoilClass::sc4id(Tools::satoi(value));
					break;
				}
			}
		};

		typedef std::vector<CropActivity*> Collection;
		static Collection& all()
		{
			return Db::LoadAllOfT<SetCropActivityFields>("carbiocial")(
						"SELECT lp_column, lp_column, pr_practice_id, soil_type_id "
						"FROM mpmas_crop_activity_ids "
						"ORDER BY lp_column");
		}

		static CropActivity* p4id(int id){ return Db::t4id<CropActivity>(id); }

		static std::string collectionToString()
		{
			return Db::ptrCollectionToString(all(), "All crop activities: ");
		}

	};

	//----------------------------------------------------------------------------

	inline int makeAgentId(Municipality* m, int sectorId, Farm* f)
	{ 
		return m->id*1000000 + sectorId*100 + f->id;
	}

	inline int makeSectorId(MunicipalityId mid, int sid){ return int(mid)*10000 + sid; }
	
	void writeInactiveSectorsFile(int activeSectorId, const std::string& pathToFile);

	//----------------------------------------------------------------------------


	// run mode
	enum RunMode
	{
		dynamicMonicaStaticMPMAS = 0,
		staticMonicaDynamicMPMAS
	};

	typedef int CropActivityId;

	//CropActivity aufschlüsseln
	/*
	struct CropActivity
	{
		CropActivity()
			: cropActivityId(0),
				cropId(fallow),
				period(offseason),
				soil(Arenosol),
				intensity(low) {}

		CropActivity(CropActivityId caid, CropId cId, Period p, SoilClassId s, Intensity i)
			: cropActivityId(caid),
				cropId(cId),
				period(p),
				soil(s),
				intensity(i) {}

		CropActivityId cropActivityId; //the actual activity id, can be divided into

		CropId cropId; //the actual crop to use -> so MONICA knows which crop to put into the sub-yearly crop rotation
		Period period; //the actual period the crop is used within the year -> so MONICA knows where to put the crop in the rotation, 1st, 2nd, 3rd place
		SoilClassId soil; //the soil grown at -> so MONICA knows what soil profile(s) to use
		Intensity intensity; //the intensity -> so MONICA knows how much fertilizer to use
	};
	*/

	struct MonicaYield
	{
		MonicaYield(double py, double sy) : primaryYield(py), stoverYield(sy) {}
		double primaryYield;
		double stoverYield;
	};

	struct MpmasResult
	{
		MpmasResult() : farmProfit(0) {}

		std::map<CropActivityId, double> cropActivityId2grossMargin;
		double farmProfit;
	};

	struct RunMpmas 
	{
		RunMpmas(const std::string& pathToMpmas);

		~RunMpmas();

		//this method returns the landuse for the given input parameters
		//the only thing that seams to be really needed for MONICA is a plain list of CropActivity, from which MONICA has to determine
		//which soils are involved CropActivity.soil, at at all distinct soils which crop rotations to put there (determined by crop and period)
		//and what other parameters to apply to each crop (nitrogen use etc.)
		//!!!! calling this method multiple times will mean that MPMAS in is dynamic use and we want to observe the changes of landuse over the years
		//-> is option 4 static MONICA and dynamic MPMAS
		std::vector<CropActivity*> landuse(int year,
																			 const std::map<int, int>& soilClassId2areaPercent,
																			 std::vector<ProductionPractice*> pps);

		//call this method after landuse() in order to run MPMAS once to calculate for the given yields and year the economic indicators, 
		//return for every CropActivity the gross-margin and for the whole farm the farm-profit
		//!!!! this method can be called multiple times with different yields for the option 2 when MPMAS is used statically
		//-> the landuse won't change, and initially the year is not used, as product prices and the like won't change either
		MpmasResult calculateFarmEconomy(int year, Municipality* municipality,
																		 int sectorId, Farm* farm,
																		 std::map<CropActivityId, double> caId2monicaYields);

#ifndef NO_MPMAS
		RunMpmas* _mpmas;
#else
		void* _mpmas;
#endif
		int _noOfYears;
		int _noOfSpinUpYears;
		int _noOfCropActivities;
		int* _cropActivityIds;
		double* _cropAreas;
		double* _monicaYields;
		double* _monicaStoverYields;
		double* _grossMargins;

		bool _cropActivitiesDisabled;

		char* _inputPathToMpmas;
		char* _outputPathToMpmas;
		char* _pathToInactiveSectorsFile;
	};

  typedef std::shared_ptr<RunMpmas> RunMpmasPtr;

	std::map<SoilClassId, std::vector<std::vector<Monica::ProductionProcess> > >
	cropRotationsFromUsedCropActivities(std::vector<CropActivity*> cas);

//	std::map<SoilClassId, std::vector<std::vector<Monica::ProductionProcess> > >
//	cropRotationsFromUsedCropActivities2(vector<CropActivity*> cas);

	//just temporary define to not have to include all the grids stuff if I don't need the rounded soil frequency
	//stupid thing is actually that it's hard to not use a grid as parameter, because either I have to move
	//the frequency code out, which includes the rounding which is special to the frequency stuff
	//or I have to make the soil grid a linear vector = a simple common datastructure, but then
	//I have to do this also for the grid->frequency method and also it's getting probably prohibitively 
	//expenive for large grids, because the vector can get very large
	//so only way to circumvent this is to add an iterator to GridP which is difficult, so I won't do it .... :-(
#ifndef NO_GRIDS
	std::map<SoilClassId, int> roundedSoilFrequency(const Grids::GridP* soilGrid, int roundToDigits);
#endif	

	void runMonicaCarbiocial();
}

#endif //_CARBIOCIAL_H_
