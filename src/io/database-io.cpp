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
#include <iostream>
#include <mutex>
#include <memory>

#include "tools/debug.h"
#include "db/abstract-db-connections.h"
#include "soil/soil.h"
#include "tools/algorithms.h"
#include "../run/run-monica.h"
#include "../core/monica-typedefs.h"

#include "database-io.h"

using namespace Monica;
using namespace Tools;
using namespace Db;
using namespace std;


/**
* @brief Returns data structure for crop parameters with values from DB
*
* A datastructure for crop parameters is created, initialized with
* database values and returned. This structure will be initialized only
* once. Similar to a singleton pattern.
*
* @param cropId
*
* @return Reference to crop parameters
*/
const CropParameters* Monica::getCropParametersFromMonicaDB(int cropId)
{
	static mutex lockable;

	static bool initialized = false;
	typedef std::shared_ptr<CropParameters> CPPtr;
	typedef map<int, CPPtr> CPS;

	static CPS cpss;

	// only initialize once
	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		//test if after waiting for the lock the other thread
		//already initialized the whole thing
		if (!initialized)
		{

			DB *con = newConnection("monica");
			DBRow row;
			std::string text_request = "select id, name, perennial, max_assimilation_rate, "
				"carboxylation_pathway, minimum_temperature_for_assimilation, "
				"crop_specific_max_rooting_depth, min_n_content, "
				"n_content_pn, n_content_b0, "
				"n_content_above_ground_biomass, n_content_root, initial_kc_factor, "
				"development_acceleration_by_nitrogen_stress, fixing_n, "
				"luxury_n_coeff, max_crop_height, residue_n_ratio, "
				"sampling_depth, target_n_sampling_depth, target_n30, "
				"default_radiation_use_efficiency, crop_height_P1, crop_height_P2, "
				"stage_at_max_height, max_stem_diameter, stage_at_max_diameter, "
				"heat_sum_irrigation_start, heat_sum_irrigation_end, "
				"max_N_uptake_p, root_distribution_p, plant_density, "
				"root_growth_lag, min_temperature_root_growth, initial_rooting_depth, "
				"root_penetration_rate, root_form_factor, specific_root_length, "
				"stage_after_cut, crit_temperature_heat_stress, "
				"lim_temperature_heat_stress, begin_sensitive_phase_heat_stress, "
				"end_sensitive_phase_heat_stress, drought_impact_on_fertility_factor, "
				"cutting_delay_days, field_condition_modifier, assimilate_reallocation, "
				"LT50cultivar, frost_hardening, frost_dehardening, "
				"low_temperature_exposure, respiratory_stress, latest_harvest_doy from crop";
			con->select(text_request.c_str());

			debug() << text_request.c_str() << endl;
			while (!(row = con->getRow()).empty())
			{

				int i = 0;
				int id = satoi(row[i++]);

				debug() << "Reading in crop Parameters for: " << id << endl;
				CPS::iterator cpsi = cpss.find(id);
				CPPtr cps;

				if (cpsi == cpss.end())
					cpss.insert(make_pair(id, cps = std::shared_ptr<CropParameters>(new CropParameters)));
				else
					cps = cpsi->second;

				cps->pc_CropName = row[i++].c_str();
        cps->pc_Perennial = satob(row[i++]);
				cps->pc_MaxAssimilationRate = satof(row[i++]);
				cps->pc_CarboxylationPathway = satoi(row[i++]);
				cps->pc_MinimumTemperatureForAssimilation = satof(row[i++]);
				cps->pc_CropSpecificMaxRootingDepth = satof(row[i++]);
				cps->pc_MinimumNConcentration = satof(row[i++]);
				cps->pc_NConcentrationPN = satof(row[i++]);
				cps->pc_NConcentrationB0 = satof(row[i++]);
				cps->pc_NConcentrationAbovegroundBiomass = satof(row[i++]);
				cps->pc_NConcentrationRoot = satof(row[i++]);
				cps->pc_InitialKcFactor = satof(row[i++]);
				cps->pc_DevelopmentAccelerationByNitrogenStress = satoi(row[i++]);
				cps->pc_PartBiologicalNFixation = satof(row[i++]);
				cps->pc_LuxuryNCoeff = satof(row[i++]);
				cps->pc_MaxCropHeight = satof(row[i++]);
				cps->pc_ResidueNRatio = satof(row[i++]);
				cps->pc_SamplingDepth = satof(row[i++]);
				cps->pc_TargetNSamplingDepth = satof(row[i++]);
				cps->pc_TargetN30 = satof(row[i++]);
				cps->pc_DefaultRadiationUseEfficiency = satof(row[i++]);
				cps->pc_CropHeightP1 = satof(row[i++]);
				cps->pc_CropHeightP2 = satof(row[i++]);
				cps->pc_StageAtMaxHeight = satof(row[i++]);
				cps->pc_MaxCropDiameter = satof(row[i++]);
				cps->pc_StageAtMaxDiameter = satof(row[i++]);
				cps->pc_HeatSumIrrigationStart = satof(row[i++]);
				cps->pc_HeatSumIrrigationEnd = satof(row[i++]);
				cps->pc_MaxNUptakeParam = satof(row[i++]);
				cps->pc_RootDistributionParam = satof(row[i++]);
				cps->pc_PlantDensity = satof(row[i++]);
				cps->pc_RootGrowthLag = satof(row[i++]);
				cps->pc_MinimumTemperatureRootGrowth = satof(row[i++]);
				cps->pc_InitialRootingDepth = satof(row[i++]);
				cps->pc_RootPenetrationRate = satof(row[i++]);
				cps->pc_RootFormFactor = satof(row[i++]);
				cps->pc_SpecificRootLength = satof(row[i++]);
				cps->pc_StageAfterCut = satoi(row[i++]);
				cps->pc_CriticalTemperatureHeatStress = satof(row[i++]);
				cps->pc_LimitingTemperatureHeatStress = satof(row[i++]);
				cps->pc_BeginSensitivePhaseHeatStress = satof(row[i++]);
				cps->pc_EndSensitivePhaseHeatStress = satof(row[i++]);
				cps->pc_DroughtImpactOnFertilityFactor = satof(row[i++]);
				cps->pc_CuttingDelayDays = satoi(row[i++]);
				cps->pc_FieldConditionModifier = satof(row[i++]);
				cps->pc_AssimilateReallocation = satof(row[i++]);
				cps->pc_LT50cultivar = satof(row[i++]);
				cps->pc_FrostHardening = satof(row[i++]);
				cps->pc_FrostDehardening = satof(row[i++]);
				cps->pc_LowTemperatureExposure = satof(row[i++]);
				cps->pc_RespiratoryStress = satof(row[i++]);
				cps->pc_LatestHarvestDoy = satoi(row[i++]);

			}
			std::string req2 = "select o.crop_id, o.id, o.initial_organ_biomass, "
				"o.organ_maintainance_respiration, o.is_above_ground, "
				"o.organ_growth_respiration, o.is_storage_organ "
				"from organ as o inner join crop as c on c.id = o.crop_id "
				"order by o.crop_id, c.id";
			con->select(req2.c_str());

			debug() << req2.c_str() << endl;

			while (!(row = con->getRow()).empty())
			{

				int cropId = satoi(row[0]);
				//        debug() << "Organ for crop: " << cropId << endl;
				auto cps = cpss[cropId];

				cps->pc_NumberOfOrgans++;
				cps->pc_InitialOrganBiomass.push_back(satof(row[2]));
				cps->pc_OrganMaintenanceRespiration.push_back(satof(row[3]));
				cps->pc_AbovegroundOrgan.push_back(satoi(row[4]) == 1);
				cps->pc_OrganGrowthRespiration.push_back(satof(row[5]));
				cps->pc_StorageOrgan.push_back(satoi(row[6]));
			}

			std::string req4 = "select crop_id, id, stage_temperature_sum, "
				"base_temperature, opt_temperature, vernalisation_requirement, "
				"day_length_requirement, base_day_length, "
				"drought_stress_threshold, critical_oxygen_content, "
				"specific_leaf_area, stage_max_root_n_content, "
				"stage_kc_factor "
				"from dev_stage "
				"order by crop_id, id";

			con->select(req4.c_str());
			debug() << req4.c_str() << endl;
			while (!(row = con->getRow()).empty())
			{
				int cropId = satoi(row[0]);
				auto cps = cpss[cropId];
				cps->pc_NumberOfDevelopmentalStages++;
				cps->pc_StageTemperatureSum.push_back(satof(row[2]));
				cps->pc_BaseTemperature.push_back(satof(row[3]));
				cps->pc_OptimumTemperature.push_back(satof(row[4]));
				cps->pc_VernalisationRequirement.push_back(satof(row[5]));
				cps->pc_DaylengthRequirement.push_back(satof(row[6]));
				cps->pc_BaseDaylength.push_back(satof(row[7]));
				cps->pc_DroughtStressThreshold.push_back(satof(row[8]));
				cps->pc_CriticalOxygenContent.push_back(satof(row[9]));
				cps->pc_SpecificLeafArea.push_back(satof(row[10]));
				cps->pc_StageMaxRootNConcentration.push_back(satof(row[11]));
				cps->pc_StageKcFactor.push_back(satof(row[12]));
			}

			for (CPS::value_type vt : cpss)
			{
				vt.second->resizeStageOrganVectors();
			}
			//for (CPS::iterator it = cpss.begin(); it != cpss.end(); it++)
			//  it->second->resizeStageOrganVectors();

			std::string req3 = "select crop_id, organ_id, dev_stage_id, "
				"ods_dependent_param_id, value "
				"from crop2ods_dependent_param "
				"order by crop_id, ods_dependent_param_id, dev_stage_id, organ_id";
			con->select(req3.c_str());
			debug() << req3.c_str() << endl;

			while (!(row = con->getRow()).empty())
			{

				int cropId = satoi(row[0]);
				//        debug() << "ods_dependent_param " << cropId << "\t" << satoi(row[3]) <<"\t" << satoi(row[2]) <<"\t" << satoi(row[1])<< endl;
				auto cps = cpss[cropId];
				vector<vector<double> >& sov = satoi(row[3]) == 1
					? cps->pc_AssimilatePartitioningCoeff
					: cps->pc_OrganSenescenceRate;
				sov[satoi(row[2]) - 1][satoi(row[1]) - 1] = satof(row[4]);
			}

			con->select("SELECT crop_id, organ_id, is_primary, percentage, dry_matter FROM yield_parts");
			debug() << "SELECT crop_id, organ_id, is_primary, percentage, dry_matter FROM yield_parts" << endl;
			while (!(row = con->getRow()).empty())
			{
				int cropId = satoi(row[0]);
				int organId = satoi(row[1]);
				bool isPrimary = satoi(row[2]) == 1;
				double percentage = satof(row[3]) / 100.0;
				double yieldDryMatter = satof(row[4]);

				auto cps = cpss[cropId];

				// normal case, uses yield partitioning from crop database
				if (isPrimary) {
					//            cout << cropId<< " Add primary organ: " << organId << endl;
					cps->pc_OrganIdsForPrimaryYield.push_back(Monica::YieldComponent(organId, percentage, yieldDryMatter));

				}
				else {
					//            cout << cropId << " Add secondary organ: " << organId << endl;
					cps->pc_OrganIdsForSecondaryYield.push_back(Monica::YieldComponent(organId, percentage, yieldDryMatter));
				}

			}

			// get cutting parts if there are some data available
			con->select("SELECT crop_id, organ_id, is_primary, percentage, dry_matter FROM cutting_parts");
			while (!(row = con->getRow()).empty())
			{
				int cropId = satoi(row[0]);
				int organId = satoi(row[1]);
				//bool isPrimary = satoi(row[2]) == 1;
				double percentage = satof(row[3]) / 100.0;
				double yieldDryMatter = satof(row[4]);

				auto cps = cpss[cropId];
				cps->pc_OrganIdsForCutting.push_back(Monica::YieldComponent(organId, percentage, yieldDryMatter));
				//if (cropId!=18) {
				//    // do not add cutting part organ id for sudan gras because they are already added
				//    cps->pc_OrganIdsForPrimaryYield.push_back(Monica::YieldComponent(organId, percentage, yieldDryMatter));
				//}
			}


			delete con;
			initialized = true;

			/*
			for(CPS::const_iterator it = cpss.begin(); it != cpss.end(); it++)
			cout << it->second->toString();
			//*/
		}
	}

	static CropParameters nothing;

	CPS::const_iterator ci = cpss.find(cropId);

	debug() << "Find crop parameter: " << cropId << endl;
	return ci != cpss.end() ? ci->second.get() : &nothing;
}

void Monica::writeCropParameters(string path)
{
	for (auto p : availableMonicaCrops())
	{
		ofstream ofs;
		ofs.open(path + "/" + p.second + ".json");

		if (ofs.good())
		{
			auto cp = getCropParametersFromMonicaDB(p.first);
			auto s = cp->to_json().dump();
			//cout << "id: " << p.first << " name: " << p.second << " ----> " << endl << s << endl;
			ofs << s;
			ofs.close();
		}
	}
}

//------------------------------------------------------------------------------

const map<int, MineralFertiliserParameters>& Monica::getAllMineralFertiliserParametersFromMonicaDB()
{
	static mutex lockable;
	static bool initialized = false;
	static map<int, MineralFertiliserParameters> m;

	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
      DB *con = newConnection("monica");
			DBRow row;
      con->select("select id, name, no3, nh4, carbamid, type from mineral_fertilisers");
			while (!(row = con->getRow()).empty())
			{
				int id = satoi(row[0]);
        string type = row[5];
				string name = row[1];
				double no3 = satof(row[2]);
				double nh4 = satof(row[3]);
				double carbamid = satof(row[4]);

        m.insert(make_pair(id, MineralFertiliserParameters(type, name, carbamid, no3, nh4)));
			}
			delete con;

			initialized = true;
		}
	}

	return m;
}

/**
* @brief Reads mineral fertiliser parameters from monica DB
* @param id of the fertiliser
* @return mineral fertiliser parameters value object with values from database
*/
MineralFertiliserParameters Monica::getMineralFertiliserParametersFromMonicaDB(int id)
{
	auto m = getAllMineralFertiliserParametersFromMonicaDB();
	auto ci = m.find(id);
	return ci != m.end() ? ci->second : MineralFertiliserParameters();
}

void Monica::writeMineralFertilisers(string path)
{
	for (auto p : getAllMineralFertiliserParametersFromMonicaDB())
	{
		auto mf = p.second;

		ofstream ofs;
		ofs.open(path + "/" + mf.getId() + ".json");

		if (ofs.good())
		{
			auto s = mf.to_json().dump();
			//cout << "id: " << mf.getId() << " name: " << mf.getName() << " ----> " << endl << s << endl;
			ofs << s;
			ofs.close();
		}
	}
}

//--------------------------------------------------------------------------------------

const map<int, OMPPtr>& Monica::getAllOrganicFertiliserParametersFromMonicaDB()
{
	static mutex lockable;
	static bool initialized = false;
	typedef map<int, OMPPtr> Map;
	static Map m;

	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
			DB *con = newConnection("monica");
			DBRow row;
			con->select("select om_Type, dm, nh4_n, no3_n, nh2_n, k_slow, k_fast, part_s, "
				"part_f, cn_s, cn_f, smb_s, smb_f, id, type "
				"from organic_fertiliser");
			while (!(row = con->getRow()).empty())
			{
				auto omp = OMPPtr(new OMP);
				omp->id = row[14];
				omp->name = row[0];
				omp->vo_AOM_DryMatterContent = satof(row[1]);
				omp->vo_AOM_NH4Content = satof(row[2]);
				omp->vo_AOM_NO3Content = satof(row[3]);
				omp->vo_AOM_CarbamidContent = satof(row[4]);
				omp->vo_AOM_SlowDecCoeffStandard = satof(row[5]);
				omp->vo_AOM_FastDecCoeffStandard = satof(row[6]);
				omp->vo_PartAOM_to_AOM_Slow = satof(row[7]);
				omp->vo_PartAOM_to_AOM_Fast = satof(row[8]);
				omp->vo_CN_Ratio_AOM_Slow = satof(row[9]);
				omp->vo_CN_Ratio_AOM_Fast = satof(row[10]);
				omp->vo_PartAOM_Slow_to_SMB_Slow = satof(row[11]);
				omp->vo_PartAOM_Slow_to_SMB_Fast = satof(row[12]);
				int id = satoi(row[13]);

				m.insert(make_pair(id, omp));
			}
			delete con;

			initialized = true;
		}
	}

	return m;
}

/**
* @brief Reads organic fertiliser parameters from monica DB
* @param organ_fert_id ID of fertiliser
* @return organic fertiliser parameters with values from database
*/
OrganicMatterParameters* Monica::getOrganicFertiliserParametersFromMonicaDB(int id)
{
	static OrganicMatterParameters nothing;

	auto m = getAllOrganicFertiliserParametersFromMonicaDB();
	auto ci = m.find(id);
	return ci != m.end() ? ci->second.get() : &nothing;
}

void Monica::writeOrganicFertilisers(string path)
{
	for (auto p : getAllOrganicFertiliserParametersFromMonicaDB())
	{
		auto of = p.second;

		ofstream ofs;
		ofs.open(path + "/" + of->id + ".json");

		if (ofs.good())
		{
			auto s = of->to_json().dump();
			//cout << "id: " << of->id << " name: " << of->name << " ----> " << endl << s << endl;
			ofs << s;
			ofs.close();
		}
	}
}


//--------------------------------------------------------------------------------------

const map<int, OMPPtr>& Monica::getAllResidueParametersFromMonicaDB()
{
	static mutex lockable;
	static bool initialized = false;
	typedef map<int, OMPPtr> Map;
	static Map m;

	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
			DB *con = newConnection("monica");
			DBRow row;
			con->select("select residue_type, dm, nh4, no3, nh2, k_slow, k_fast, part_s, "
				"part_f, cn_s, cn_f, smb_s, smb_f, crop_id "
				"from residue_table");
			while (!(row = con->getRow()).empty())
			{
				auto omp = OMPPtr(new OMP);
				omp->id = row[13];
				omp->name = row[0];
				omp->vo_AOM_DryMatterContent = satoi(row[1]);
				omp->vo_AOM_NH4Content = satof(row[2]);
				omp->vo_AOM_NO3Content = satof(row[3]);
				omp->vo_AOM_CarbamidContent = satof(row[4]);
				omp->vo_AOM_SlowDecCoeffStandard = satof(row[5]);
				omp->vo_AOM_FastDecCoeffStandard = satof(row[6]);
				omp->vo_PartAOM_to_AOM_Slow = satof(row[7]);
				omp->vo_PartAOM_to_AOM_Fast = satof(row[8]);
				omp->vo_CN_Ratio_AOM_Slow = satof(row[9]);
				omp->vo_CN_Ratio_AOM_Fast = satof(row[10]);
				omp->vo_PartAOM_Slow_to_SMB_Slow = satof(row[11]);
				omp->vo_PartAOM_Slow_to_SMB_Fast = satof(row[12]);
				int id = satoi(row[13]);

				m.insert(make_pair(id, omp));
			}
			delete con;

			initialized = true;
		}
	}

	return m;
}

/**
* @brief Reads residue parameters from monica DB
* @param crop_id ID of crop
* @return Residues parameters with values from database
*/
const OrganicMatterParameters* Monica::getResidueParametersFromMonicaDB(int cropId)
{
	static OrganicMatterParameters nothing;

	auto m = getAllResidueParametersFromMonicaDB();
	auto ci = m.find(cropId);
	return ci != m.end() ? ci->second.get() : &nothing;
}

void Monica::writeResidues(string path)
{
	for (auto p : getAllResidueParametersFromMonicaDB())
	{
		auto r = p.second;

		ofstream ofs;
		ofs.open(path + "/" + r->name + ".json");

		if (ofs.good())
		{
			auto s = r->to_json().dump();
			//cout << "id: " << of->id << " name: " << of->name << " ----> " << endl << s << endl;
			ofs << s;
			ofs.close();
		}
	}
}

//------------------------------------------------------------------------------

CentralParameterProvider Monica::readUserParameterFromDatabase(int type)
{
	static mutex lockable;

	static bool initialized = false;

	static CentralParameterProvider centralParameterProvider;

	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
			debug() << "DB Conncection user parameters" << endl;
			DB *con = newConnection("monica");
			DBRow row;
			switch (type)
			{
			case MODE_HERMES:
				con->select("select name, value_hermes from user_parameter");
				break;
			case MODE_EVA2:
				con->select("select name, value_eva2 from user_parameter");
				break;
			case MODE_MACSUR_SCALING:
				con->select("select name, value_macsur_scaling from user_parameter");
				break;
			default:
				con->select("select name, value_hermes from user_parameter");
				break;
			}

			UserCropParameters& user_crops =
				centralParameterProvider.userCropParameters;
			UserEnvironmentParameters& user_env =
				centralParameterProvider.userEnvironmentParameters;
			UserSoilMoistureParameters& user_soil_moisture =
				centralParameterProvider.userSoilMoistureParameters;

      user_soil_moisture.getCapillaryRiseRate = [](string soilTexture, int distance)
      {
        return Soil::readCapillaryRiseRates().getRate(soilTexture, distance);
      };

			UserSoilTemperatureParameters& user_soil_temperature =
				centralParameterProvider.userSoilTemperatureParameters;
			UserSoilTransportParameters& user_soil_transport =
				centralParameterProvider.userSoilTransportParameters;
			UserSoilOrganicParameters& user_soil_organic =
				centralParameterProvider.userSoilOrganicParameters;
			UserInitialValues& user_init_values =
				centralParameterProvider.userInitValues;

			while (!(row = con->getRow()).empty())
			{
				std::string name = row[0];
				if (name == "tortuosity")
					user_crops.pc_Tortuosity = stof(row[1]);
				else if (name == "canopy_reflection_coefficient")
					user_crops.pc_CanopyReflectionCoefficient = stof(row[1]);
				else if (name == "reference_max_assimilation_rate")
					user_crops.pc_ReferenceMaxAssimilationRate = stof(row[1]);
				else if (name == "reference_leaf_area_index")
					user_crops.pc_ReferenceLeafAreaIndex = stof(row[1]);
				else if (name == "maintenance_respiration_parameter_2")
					user_crops.pc_MaintenanceRespirationParameter2 = stof(row[1]);
				else if (name == "maintenance_respiration_parameter_1")
					user_crops.pc_MaintenanceRespirationParameter1 = stof(row[1]);
				else if (name == "minimum_n_concentration_root")
					user_crops.pc_MinimumNConcentrationRoot = stof(row[1]);
				else if (name == "minimum_available_n")
					user_crops.pc_MinimumAvailableN = stof(row[1]);
				else if (name == "reference_albedo")
					user_crops.pc_ReferenceAlbedo = stof(row[1]);
				else if (name == "stomata_conductance_alpha")
					user_crops.pc_StomataConductanceAlpha = stof(row[1]);
				else if (name == "saturation_beta")
					user_crops.pc_SaturationBeta = stof(row[1]);
				else if (name == "growth_respiration_redux")
					user_crops.pc_GrowthRespirationRedux = stof(row[1]);
				else if (name == "max_crop_n_demand")
					user_crops.pc_MaxCropNDemand = stof(row[1]);
				else if (name == "growth_respiration_parameter_2")
					user_crops.pc_GrowthRespirationParameter2 = stof(row[1]);
				else if (name == "growth_respiration_parameter_1")
					user_crops.pc_GrowthRespirationParameter1 = stof(row[1]);
				else if (name == "use_automatic_irrigation")
					user_env.p_UseAutomaticIrrigation = stoi(row[1]) == 1;
				else if (name == "use_nmin_mineral_fertilising_method")
					user_env.p_UseNMinMineralFertilisingMethod = stoi(row[1]) == 1;
				else if (name == "layer_thickness")
					user_env.p_LayerThickness = stof(row[1]);
				else if (name == "number_of_layers")
					user_env.p_NumberOfLayers = stoi(row[1]);
				else if (name == "start_pv_index")
					user_env.p_StartPVIndex = stoi(row[1]);
				else if (name == "albedo")
					user_env.p_Albedo = stof(row[1]);
				else if (name == "athmospheric_co2")
					user_env.p_AtmosphericCO2 = stof(row[1]);
				else if (name == "wind_speed_height")
					user_env.p_WindSpeedHeight = stof(row[1]);
				else if (name == "use_secondary_yields")
					user_env.p_UseSecondaryYields = stoi(row[1]) == 1;
				else if (name == "julian_day_automatic_fertilising")
					user_env.p_JulianDayAutomaticFertilising = stoi(row[1]);
				else if (name == "critical_moisture_depth")
					user_soil_moisture.pm_CriticalMoistureDepth = stof(row[1]);
				else if (name == "saturated_hydraulic_conductivity")
					user_soil_moisture.pm_SaturatedHydraulicConductivity = stof(row[1]);
				else if (name == "surface_roughness")
					user_soil_moisture.pm_SurfaceRoughness = stof(row[1]);
				else if (name == "hydraulic_conductivity_redux")
					user_soil_moisture.pm_HydraulicConductivityRedux = stof(row[1]);
				else if (name == "snow_accumulation_treshold_temperature")
					user_soil_moisture.pm_SnowAccumulationTresholdTemperature = stof(row[1]);
				else if (name == "kc_factor")
					user_soil_moisture.pm_KcFactor = stof(row[1]);
				else if (name == "time_step")
					user_env.p_timeStep = stof(row[1]);
				else if (name == "temperature_limit_for_liquid_water")
					user_soil_moisture.pm_TemperatureLimitForLiquidWater = stof(row[1]);
				else if (name == "correction_snow")
					user_soil_moisture.pm_CorrectionSnow = stof(row[1]);
				else if (name == "correction_rain")
					user_soil_moisture.pm_CorrectionRain = stof(row[1]);
				else if (name == "snow_max_additional_density")
					user_soil_moisture.pm_SnowMaxAdditionalDensity = stof(row[1]);
				else if (name == "new_snow_density_min")
					user_soil_moisture.pm_NewSnowDensityMin = stof(row[1]);
				else if (name == "snow_retention_capacity_min")
					user_soil_moisture.pm_SnowRetentionCapacityMin = stof(row[1]);
				else if (name == "refreeze_parameter_2")
					user_soil_moisture.pm_RefreezeParameter2 = stof(row[1]);
				else if (name == "refreeze_parameter_1")
					user_soil_moisture.pm_RefreezeParameter1 = stof(row[1]);
				else if (name == "refreeze_temperature")
					user_soil_moisture.pm_RefreezeTemperature = stof(row[1]);
				else if (name == "snowmelt_temperature")
					user_soil_moisture.pm_SnowMeltTemperature = stof(row[1]);
				else if (name == "snow_packing")
					user_soil_moisture.pm_SnowPacking = stof(row[1]);
				else if (name == "snow_retention_capacity_max")
					user_soil_moisture.pm_SnowRetentionCapacityMax = stof(row[1]);
				else if (name == "evaporation_zeta")
					user_soil_moisture.pm_EvaporationZeta = stof(row[1]);
				else if (name == "xsa_critical_soil_moisture")
					user_soil_moisture.pm_XSACriticalSoilMoisture = stof(row[1]);
				else if (name == "maximum_evaporation_impact_depth")
					user_soil_moisture.pm_MaximumEvaporationImpactDepth = stof(row[1]);
				else if (name == "ntau")
					user_soil_temperature.pt_NTau = stof(row[1]);
				else if (name == "initial_surface_temperature")
					user_soil_temperature.pt_InitialSurfaceTemperature = stof(row[1]);
				else if (name == "base_temperature")
					user_soil_temperature.pt_BaseTemperature = stof(row[1]);
				else if (name == "quartz_raw_density")
					user_soil_temperature.pt_QuartzRawDensity = stof(row[1]);
				else if (name == "density_air")
					user_soil_temperature.pt_DensityAir = stof(row[1]);
				else if (name == "density_water")
					user_soil_temperature.pt_DensityWater = stof(row[1]);
				else if (name == "specific_heat_capacity_air")
					user_soil_temperature.pt_SpecificHeatCapacityAir = stof(row[1]);
				else if (name == "specific_heat_capacity_quartz")
					user_soil_temperature.pt_SpecificHeatCapacityQuartz = stof(row[1]);
				else if (name == "specific_heat_capacity_water")
					user_soil_temperature.pt_SpecificHeatCapacityWater = stof(row[1]);
				else if (name == "soil_albedo")
					user_soil_temperature.pt_SoilAlbedo = stof(row[1]);
				else if (name == "dispersion_length")
					user_soil_transport.pq_DispersionLength = stof(row[1]);
				else if (name == "AD")
					user_soil_transport.pq_AD = stof(row[1]);
				else if (name == "diffusion_coefficient_standard")
					user_soil_transport.pq_DiffusionCoefficientStandard = stof(row[1]);
				else if (name == "leaching_depth")
					user_env.p_LeachingDepth = stof(row[1]);
				else if (name == "groundwater_discharge")
					user_soil_moisture.pm_GroundwaterDischarge = stof(row[1]);
				else if (name == "density_humus")
					user_soil_temperature.pt_DensityHumus = stof(row[1]);
				else if (name == "specific_heat_capacity_humus")
					user_soil_temperature.pt_SpecificHeatCapacityHumus = stof(row[1]);
				else if (name == "max_percolation_rate")
					user_soil_moisture.pm_MaxPercolationRate = stof(row[1]);
				else if (name == "max_groundwater_depth")
					user_env.p_MaxGroundwaterDepth = stof(row[1]);
				else if (name == "min_groundwater_depth")
					user_env.p_MinGroundwaterDepth = stof(row[1]);
				else if (name == "min_groundwater_depth_month")
					user_env.p_MinGroundwaterDepthMonth = stoi(row[1]);
				else if (name == "SOM_SlowDecCoeffStandard")
					user_soil_organic.po_SOM_SlowDecCoeffStandard = stof(row[1]);
				else if (name == "SOM_FastDecCoeffStandard")
					user_soil_organic.po_SOM_FastDecCoeffStandard = stof(row[1]);
				else if (name == "SMB_SlowMaintRateStandard")
					user_soil_organic.po_SMB_SlowMaintRateStandard = stof(row[1]);
				else if (name == "SMB_FastMaintRateStandard")
					user_soil_organic.po_SMB_FastMaintRateStandard = stof(row[1]);
				else if (name == "SMB_SlowDeathRateStandard")
					user_soil_organic.po_SMB_SlowDeathRateStandard = stof(row[1]);
				else if (name == "SMB_FastDeathRateStandard")
					user_soil_organic.po_SMB_FastDeathRateStandard = stof(row[1]);
				else if (name == "SMB_UtilizationEfficiency")
					user_soil_organic.po_SMB_UtilizationEfficiency = stof(row[1]);
				else if (name == "SOM_SlowUtilizationEfficiency")
					user_soil_organic.po_SOM_SlowUtilizationEfficiency = stof(row[1]);
				else if (name == "SOM_FastUtilizationEfficiency")
					user_soil_organic.po_SOM_FastUtilizationEfficiency = stof(row[1]);
				else if (name == "AOM_SlowUtilizationEfficiency")
					user_soil_organic.po_AOM_SlowUtilizationEfficiency = stof(row[1]);
				else if (name == "AOM_FastUtilizationEfficiency")
					user_soil_organic.po_AOM_FastUtilizationEfficiency = stof(row[1]);
				else if (name == "AOM_FastMaxC_to_N")
					user_soil_organic.po_AOM_FastMaxC_to_N = stof(row[1]);
				else if (name == "PartSOM_Fast_to_SOM_Slow")
					user_soil_organic.po_PartSOM_Fast_to_SOM_Slow = stof(row[1]);
				else if (name == "PartSMB_Slow_to_SOM_Fast")
					user_soil_organic.po_PartSMB_Slow_to_SOM_Fast = stof(row[1]);
				else if (name == "PartSMB_Fast_to_SOM_Fast")
					user_soil_organic.po_PartSMB_Fast_to_SOM_Fast = stof(row[1]);
				else if (name == "PartSOM_to_SMB_Slow")
					user_soil_organic.po_PartSOM_to_SMB_Slow = stof(row[1]);
				else if (name == "PartSOM_to_SMB_Fast")
					user_soil_organic.po_PartSOM_to_SMB_Fast = stof(row[1]);
				else if (name == "CN_Ratio_SMB")
					user_soil_organic.po_CN_Ratio_SMB = stof(row[1]);
				else if (name == "LimitClayEffect")
					user_soil_organic.po_LimitClayEffect = stof(row[1]);
				else if (name == "AmmoniaOxidationRateCoeffStandard")
					user_soil_organic.po_AmmoniaOxidationRateCoeffStandard = stof(row[1]);
				else if (name == "NitriteOxidationRateCoeffStandard")
					user_soil_organic.po_NitriteOxidationRateCoeffStandard = stof(row[1]);
				else if (name == "TransportRateCoeff")
					user_soil_organic.po_TransportRateCoeff = stof(row[1]);
				else if (name == "SpecAnaerobDenitrification")
					user_soil_organic.po_SpecAnaerobDenitrification = stof(row[1]);
				else if (name == "ImmobilisationRateCoeffNO3")
					user_soil_organic.po_ImmobilisationRateCoeffNO3 = stof(row[1]);
				else if (name == "ImmobilisationRateCoeffNH4")
					user_soil_organic.po_ImmobilisationRateCoeffNH4 = stof(row[1]);
				else if (name == "Denit1")
					user_soil_organic.po_Denit1 = stof(row[1]);
				else if (name == "Denit2")
					user_soil_organic.po_Denit2 = stof(row[1]);
				else if (name == "Denit3")
					user_soil_organic.po_Denit3 = stof(row[1]);
				else if (name == "HydrolysisKM")
					user_soil_organic.po_HydrolysisKM = stof(row[1]);
				else if (name == "ActivationEnergy")
					user_soil_organic.po_ActivationEnergy = stof(row[1]);
				else if (name == "HydrolysisP1")
					user_soil_organic.po_HydrolysisP1 = stof(row[1]);
				else if (name == "HydrolysisP2")
					user_soil_organic.po_HydrolysisP2 = stof(row[1]);
				else if (name == "AtmosphericResistance")
					user_soil_organic.po_AtmosphericResistance = stof(row[1]);
				else if (name == "N2OProductionRate")
					user_soil_organic.po_N2OProductionRate = stof(row[1]);
				else if (name == "Inhibitor_NH3")
					user_soil_organic.po_Inhibitor_NH3 = stof(row[1]);
			}
			delete con;
			initialized = true;
#ifdef RUN_MACSUR_SCALING
			initialized = false;
#endif

//			centralParameterProvider.capillaryRiseRates = Soil::readCapillaryRiseRates();
		}
	}

	return centralParameterProvider;
}

void Monica::writeUserParameters(int type, string path)
{
	string typeName = "hermes";
	switch (type)
	{
	case MODE_EVA2: typeName = "eva2"; break;
	case MODE_MACSUR_SCALING: typeName = "macsur"; break;
	default:;
	}
	
	auto ups = readUserParameterFromDatabase(type);

	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-crop.json");
		if (ofs.good())
		{
			ofs << ups.userCropParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-environment.json");
		if (ofs.good())
		{
			ofs << ups.userEnvironmentParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-soil-moisture.json");
		if (ofs.good())
		{
			ofs << ups.userSoilMoistureParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-soil-temperature.json");
		if (ofs.good())
		{
			ofs << ups.userSoilTemperatureParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-soil-transport.json");
		if (ofs.good())
		{
			ofs << ups.userSoilTransportParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-soil-organic.json");
		if (ofs.good())
		{
			ofs << ups.userSoilOrganicParameters.to_json().dump();
			ofs.close();
		}
	}
//	{
//		ofstream ofs;
//		ofs.open(path + "/" + typeName + "-sensitivity-analysis.json");
//		if (ofs.good())
//		{
//			ofs << ups.sensitivityAnalysisParameters.to_json().dump();
//			ofs.close();
//		}
//	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-init.json");
		if (ofs.good())
		{
			ofs << ups.userInitValues.to_json().dump();
			ofs.close();
		}
	}
}

//----------------------------------------------------------------------------------

const map<int, string>& Monica::availableMonicaCrops()
{
	static mutex lockable;
	static map<int, string> m;
	static bool initialized = false;
	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
			DBPtr con(newConnection("monica"));

			string query("select id, name "
				"from crop "
				"order by id");
			con->select(query.c_str());

			DBRow row;
			while (!(row = con->getRow()).empty())
				m[satoi(row[0])] = capitalize(row[1]);

			initialized = true;
		}
	}

	return m;
}
