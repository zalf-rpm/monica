#!/usr/bin/python
# -*- coding: ISO-8859-15-*-

import sys

sys.path.append('../monica-src')   # path to monica.py

import mpi_helper
import monica
import os
import datetime
import numpy
import analyse_monica_outputs
import shutil
import time

import csv
from mpi4py import MPI

# MPI related initialisations
comm = MPI.COMM_WORLD
rank = comm.Get_rank()          # number of the processor
size = comm.Get_size()          # number of all participating processes
name = MPI.Get_processor_name() 


sep = ","
remove_monica_files_after_simulation = True  # True

######################################################
# Configuration
######################################################

start_year=1982
end_year=2012

simulation_types = ["potential", "water_limited", "nutrient_water_limited"]

crops =["winter_wheat", "maize"]  # "maize", "winter_wheat"

soil_files_directory = "../input_data/phase_2/step2_3/soil_files/"

lookup_table_file = "../input_data/phase_2/step2_3/MACSUR_WP3_cell_lookup.csv"

# -------------------------------
# if rerun_list is not empty, only provided project_ids will be run
rerun_list=[]
#rerun_list = ["Res1_C183:R285","Res1_C222:R286","Res1_C223:R285","Res1_C3:R439"]
# -------------------------------

step = 2    # or 3

resolutions = [ "100",
                "50",
                "25",
                "10",
                "1"
              ]

soil_files = {}
soil_files["1"]   = "MACSUR_WP3_soil_r1.csv"
soil_files["10"]  = "MACSUR_WP3_soil_r10.csv"
soil_files["25"]  = "MACSUR_WP3_soil_r25.csv"
soil_files["50"]  = "MACSUR_WP3_soil_r50.csv"
soil_files["100"] = "MACSUR_WP3_soil_r100.csv"



######################################################
# End of configuration
######################################################

"""
main routine of the macsur scaling simulation
"""
def main():
    
    ts = time.time()
    output_path = "../runs/phase_2/step" + str(step) + "/" + datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d_%H-%M')

    lookup_tables = read_lookup_table(lookup_table_file)

    s_res = ""
    c_res = ""

    for crop in crops:

        crop_id = ""
        if (crop == "maize"):
            crop_id = "SM"
        elif (crop == "winter_wheat"):
            crop_id = "WW"


        for res in resolutions:

            lookup_table = lookup_tables[res]
            #for k,v in lookup_table.iteritems():
            #    print k, v

            soil_file = ""
            if (step == 2) :

                s_res = "s" + str(res)
                c_res = "c1"

                soil_file = soil_files_directory + soil_files[res]
            elif (step == 3):

                s_res = "s1"
                c_res = "c" + str(res)

                soil_file = soil_files_directory + soil_files["1"]


            for simulation_type in simulation_types:
      
              print rank, "Simulation type: ", simulation_type, "\tResolution: ", res
              output_prefix, input_path, ini_file = getSimulationConfig(simulation_type, crop)
          
              splitted_meta_info_list = None
              
               
              if (rank == 0):
                  # only one processor reads in the meta information
                  meta_info_list = readMetaInfo(res, step, lookup_table)
                      
                  #for meta in meta_info_list:
                  #    print meta.start_date, meta.end_date, meta.project_id, meta.climate_file, meta.row_id, meta.col_id
                      
                  # split the meta info list into "number of processors" sublists 
                  splitted_meta_info_list = mpi_helper.splitListForNodes(meta_info_list, size)

              ###################################################
              # parallel part
              ##################################################
              
              # send each sublist of the splitted list to on processor
              node_specific_meta_info_list = comm.scatter(splitted_meta_info_list, root=0)      
          
              # each processor received a specific number of meta_info_objects
              # that he has to process
              print rank, "Received meta info list with", len(node_specific_meta_info_list), "elements" 

              start_date = str(start_year) + "-01-01"
              end_date = str(end_year) + "-01-31"

              monica_simulation_config = monica.MacsurScalingConfiguration()
              monica_simulation_config.setInputPath(input_path)
              monica_simulation_config.setIniFile(ini_file)
              monica_simulation_config.setCropName(crop)
              
              node_simulation_results = []
              
              for index, meta in enumerate(node_specific_meta_info_list):

                  #if (index >= 5):
                  #    continue

                  col_row_list = lookup_table[str([str(meta.col_id),str(meta.row_id)])]
                  lookup_col = int(col_row_list[0])
                  lookup_row = int(col_row_list[1])

                  lookup_project_id = "Res" + str(res) + "_C" + str(lookup_col) + ":R" + str(lookup_row)



                  print rank, "###################################"
                  print rank, str(index+1) +  "/" + str(len(node_specific_meta_info_list)) 
                  print rank, "Calculating", meta.project_id, "\tLookup_project_id:", lookup_project_id
                  print rank, meta.start_date, meta.end_date
                  print rank, meta.climate_file
                  print rank, soil_file

                  monica_simulation_config.setStartDate(meta.start_date)
                  monica_simulation_config.setEndDate(meta.end_date)
                  monica_simulation_config.setClimateFile(meta.climate_file)
                  monica_simulation_config.setProjectId(meta.project_id)
                  monica_simulation_config.setLookupProjectId(lookup_project_id)
                  monica_simulation_config.setRowId(meta.row_id)
                  monica_simulation_config.setColId(meta.col_id)
                  monica_simulation_config.setPhase(2)
                  monica_simulation_config.setStep(step)
                  monica_simulation_config.setSoilFile(soil_file)
                  
                  path = path = output_path + "/" + crop + "_" + simulation_type + "/" + meta.project_id
                  monica_simulation_config.setOutputPath(path) 
                  
                  if not (os.path.exists(path)):
                      print rank, "create_directory: ", path
                      os.makedirs(path)
                  
                  monica_simulation_config.setLatitude(meta.latitude)
                  monica_simulation_config.setElevation(meta.elevation)
                          
                  monica.activateDebugOutput(False);
                  monica.runMacsurScalingSimulation(monica_simulation_config)
                  sim_result = analyseSimulationOutput(path, meta.row_id, meta.col_id, crop)
                  node_simulation_results.extend(sim_result)
                  
                  # remove simulation result dir
                  if (remove_monica_files_after_simulation):
                      shutil.rmtree(path, True)

                  print rank, "###################################"
                  
                  #if (index == 1):
                  #    break
                  
          
              ###################################################
              # end of parallel part
              ##################################################              


                
              result_list = comm.gather(node_simulation_results, root=0)
            
              if (rank == 0):
                
                
                  output_filename = output_prefix + "_" + s_res + "x" + c_res + ".csv"       
                  output_filehandle = open(output_path + "/" + output_filename, "wb")
                  output_csv = csv.writer(output_filehandle, delimiter=sep)
                
                  header = ["gridcell", "year", "Yield (t DM/ha)", "Total above ground biomass (t DM/ha)",
                        "Total ET over the growing season (mm/growing season)", 
                        "Total intercepted PAR over the growing season (MJ/ha/growing season)",
                        "Maximum LAI during the growing season (m2/m2)", 
                        "Anthesis date (DOY)", "Maturity date (DOY)", 
                        "SOC at sowing date at 30 cm (gC/m2)", 
                        "SOC at sowing date at 2.0 m (gC/m2)", 
                        "Total net ecosystem exchange over the growing season (gC/m2/growing season)", 
                        "Total net primary productivity over the growing season (gC/m2/growing season)",
                        "Total N20 over the growing season (kg N/ha/growing season)",
                        "Total annual N20 (kg N/ha/year)",
                        "Total annual N leaching over 1.5 m (kg N/ha/year)", 
                        "Total N leaching over the growing season over 1.5 m (kg N/ha/growing season)",
                        "Total annual water loss below 1.5 m (mm/ha/year)", 
                        "Total water loss below 1.5 m over the growing season (mm/ha/growing season)", 
                        "Maximum rooted soil depth (m)",
                        "Total annual irrigated water amount (mm)"]
                  output_csv.writerow(header)
              
  
               
                  for node_list in result_list:   
                      for result_row in node_list:
                          output_csv.writerow(result_row)
                        
                  output_filehandle.close()
        
#############################################################
#############################################################
#############################################################   
   
def analyseSimulationOutput(output_path, row_id, col_id, crop):
    
    rmout_file = output_path + "/rmout.dat"
    smout_file = output_path + "/smout.dat"

    rmout = None
    smout = None
        
    if (os.path.exists(rmout_file) and os.path.exists(smout_file)):
        rmout = read_monica_file(rmout_file)
        smout = read_monica_file(smout_file)  
        
    growth_seasons = find_growing_seasons(smout)
    
    simulation_results = []
    
    for season in growth_seasons:
        
        gridcell = "C" + str(col_id) + ":R" + str(row_id)
        
        sowing_index = season[0]
        harvest_index = season[1]
        
        
         
        d = smout[harvest_index]["Datum"].split("/")
        harvest_date = datetime.date(int(d[2]), int(d[1]), int(d[0]))

        year = harvest_date.year
        
        first_day_of_year = analyse_monica_outputs.get_index_of_date(datetime.date(year, 1, 1), rmout)
        last_day_of_year = analyse_monica_outputs.get_index_of_date(datetime.date(year, 12, 31), rmout)
        
        # biomass outputs
        yield_biomass = float(smout[harvest_index]["Yield"]) / 1000.0 # t DM / ha
        yield_biomass = round(yield_biomass, 2)
        
        above_ground_biomass = float(smout[harvest_index]["AbBiom"]) / 1000.0 # t DM / ha
        above_ground_biomass = round(above_ground_biomass, 2)
        
        # total ET over growing season
        total_et = analyse_monica_outputs.get_cumm_ET(sowing_index, harvest_index, rmout)
        
        total_par = "na"
        
        max_lai = analyse_monica_outputs.get_max_LAI(sowing_index, harvest_index, smout)
        
        # anthesis
        anthesis_date, anthesis_index = analyse_monica_outputs.get_anthesis_date(smout, sowing_index, harvest_index)        
        anthesis_doy = "na"
        if (anthesis_date != None):
            anthesis_doy = anthesis_date.timetuple().tm_yday
            
        # maturity
        maturity_date, maturity_index = analyse_monica_outputs.get_maturity_date(smout, sowing_index, harvest_index, crop)        
        maturity_doy = "na"
        if (maturity_date != None):
            maturity_doy = maturity_date.timetuple().tm_yday  
#        else:
#            maturity_doy = harvest_date.timetuple().tm_yday
        
        
        soc_30 = soc_200 = analyse_monica_outputs.get_soc30(sowing_index, rmout) # g C / m2
        soc_200 = analyse_monica_outputs.get_soc200(sowing_index, rmout)# g C / m2


        total_NEE = analyse_monica_outputs.get_cumm_NEE(sowing_index, harvest_index, rmout) # kg C / ha
        total_NEE = (total_NEE * 1000.0) / (100*100)     # g C / m2
                        
        total_NPP = analyse_monica_outputs.get_cumm_NPP(sowing_index, harvest_index, rmout) # kg C / ha
        total_NPP = (total_NPP * 1000.0) / (100*100)       # kg C / ha --> g C / m2
        
        total_n2o = analyse_monica_outputs.get_cumm_n2o(first_day_of_year, last_day_of_year, rmout) # kg N / ha       
        growing_season_n2o = analyse_monica_outputs.get_cumm_n2o(sowing_index, harvest_index, rmout) # kg N / ha        
        
        total_n_leaching = analyse_monica_outputs.get_cumm_n_leached(first_day_of_year, last_day_of_year, rmout)
        n_leaching_growing_season = analyse_monica_outputs.get_cumm_n_leached(sowing_index, harvest_index, rmout)
        
        total_waterloss = analyse_monica_outputs.get_cumm_waterloss(first_day_of_year, last_day_of_year, rmout)
        growing_season_waterloss = analyse_monica_outputs.get_cumm_waterloss(sowing_index, harvest_index, rmout)
        
        maximum_rooted_soil_depth = analyse_monica_outputs.get_max_rooting_depth(sowing_index, harvest_index, rmout)
        
        irrigation_amount = analyse_monica_outputs.get_cumm_irrigation_amount(first_day_of_year, last_day_of_year, smout)
        
        row = [gridcell, year, yield_biomass, above_ground_biomass, total_et, total_par, max_lai, anthesis_doy,
               maturity_doy, soc_30, soc_200, total_NEE, total_NPP, growing_season_n2o, total_n2o, total_n_leaching, 
               n_leaching_growing_season, total_waterloss, growing_season_waterloss, maximum_rooted_soil_depth,
               irrigation_amount]
        
        simulation_results.append(row)
        
        #print gridcell,"\t", year,"\t", yield_biomass,"\t", above_ground_biomass, "\t",total_et, "\t", total_par,
        #print "\t", max_lai, "\t", anthesis_doy, "\t", maturity_doy, "\t", soc_30, "\t", soc_200, "\t", total_NEE, "\t",
        #print total_NPP, "\t", total_n2o, "\t", total_n_leaching, "\t", n_leaching_growing_season, "\t", total_waterloss, 
        #print "\t", growing_season_waterloss, "\t", maximum_rooted_soil_depth
        
    return simulation_results
   
#############################################################
#############################################################
#############################################################

"""
Reads in MONICA files and saves the results into a list of maps
"""
def read_monica_file(file):
         
    data = csv.reader(open(file), delimiter="\t")  

    # Read the column names from the first line of the file  
    fields = data.next()
    
    # skip the line with units
    data.next()
    
    map_list = []  
    for row in data:  
        # Zip together the field names and values  
        items = zip(fields, row)  
        item = {}  
        # Add the value to our dictionary  
        
        for (name, value) in items:
            #print name, value  
            item[name.strip()] = value.strip()
        map_list.append(item)
    
    return map_list

#############################################################
#############################################################
#############################################################

def find_growing_seasons(smout):
    
    seasons = []
    
    old_name = ""
    sowing_index = 0
    harvest_index = 0
    for index, row in enumerate(smout):
        
        name = row["Crop"]
        
                
        if (name != "" and old_name == ""):
            sowing_index = index
            
        if (name == "" and old_name != ""):
            harvest_index = index-1
            seasons.append([sowing_index, harvest_index])
        
        old_name = name
            
    #for s in seasons:
        #print s
            
    return seasons   
 
 


#############################################################
#############################################################
#############################################################

"""
Reads all relevant information from the provided 
meta information file
""" 
def readMetaInfo(resolution, step, lookup_table):
    #print "readMetaInfo"    

    # open meta info file
    csv_file = open("../input_data/phase_2/step2_3/NRW_Climate_Metadata.csv", "rb")
    
    latitude_elevation_map = get_latitude_and_evelation_map("../input_data/NRW_Climate_Metadata.csv")

    # use csv.reader to read the csv file
    meta_csv = csv.reader(csv_file, delimiter=";")
    
    # skip header of csv file
    meta_csv.next()
   
    # list that will store all meta info objects 
    meta_info_list = []
    
    # go through each row of the csv file
    for row in meta_csv:
        if (len(row)<=1):
            print "Ignore empty row in meta file ..."
            continue
        
        res = int(row[7])

        if (res == 1):

            # parse columns of the csv row
            start_date = row[3]
            end_date = row[4]
            project_id = row[1]
            climate_file = None 
            row_id = int(row[9])
            col_id = int(row[8])

            lat_ele_list = latitude_elevation_map[project_id]
            lat = lat_ele_list[0]
            ele = lat_ele_list[1]

           
            if (step == 2):
                climate_file = "../input_data/phase_2/step2_3/climate_files/MACSUR_WP3_NRW_1x1/"  + row[5]
            elif (step == 3):
                col_row_list = lookup_table[str([str(col_id),str(row_id)])]
                #print "col_row_list", str([str(col_id),str(row_id)]), "\t", col_row_list

                if (col_row_list[0] == "NaN"):
                    continue

                lookup_col = int(col_row_list[0])
                lookup_row = int(col_row_list[1])
                climate_file = "../input_data/phase_2/step2_3/climate_files/MACSUR_WP3_NRW_10_100/daily_mean_RES" + str(resolution) + "_CROPLAND0_C" + str(lookup_col) + "R" + str(lookup_row) + ".csv"
                #print climate_file

            # create a meta config objects that stores all relevant information
            meta_config = macsur_meta_config(start_date, end_date, project_id, climate_file, row_id, col_id, lat, ele, res)
            
            # test only specific point
            #if (row_id == 7 and col_id == 3):
                #meta_info_list.append(meta_config)
                
            # add object to the meta info list
            meta_info_list.append(meta_config)
            
    csv_file.close()
    
    return meta_info_list

#############################################################
#############################################################
#############################################################

def get_latitude_and_evelation_map(filename):

    latitude_elevation_map = {}
    
    # open meta info file
    csv_file = open(filename, "rb")
    
    # use csv.reader to read the csv file
    meta_csv = csv.reader(csv_file, delimiter="\t")
    
    # skip header of csv file
    meta_csv.next()
   
    # list that will store all meta info objects 
    meta_info_list = []
    
    # go through each row of the csv file
    for row in meta_csv:
        project_id = row[5]
        lat = float(row[13])
        ele = float(row[14])
           
        latitude_elevation_map[project_id] =  [lat, ele]       
            
    csv_file.close()
    
    return latitude_elevation_map

#############################################################
#############################################################
#############################################################

""" 
Reads data from csv file for the cell lookup table
"""
def read_lookup_table(lookup_table_file):

    # open meta info file
    csv_file = open(lookup_table_file, "rb")
    
    # use csv.reader to read the csv file
    lookup_csv = csv.reader(csv_file, delimiter=",")

    # skip header rows of csv file
    lookup_csv.next()
    lookup_csv.next()
    lookup_csv.next()

    lookup1 = {}
    lookup10 = {}
    lookup25 = {}
    lookup50 = {}
    lookup100 = {}

    # go through each row of the csv file
    for row in lookup_csv:

        if (len(row)<=1):
            print "Ignoring empty row in lookup table ..."
            continue

        lookup1[str([row[0],row[1]])] = [row[0],row[1]]
        lookup10[str([row[0],row[1]])] = [row[2],row[3]]
        lookup25[str([row[0],row[1]])] = [row[4],row[5]]
        lookup50[str([row[0],row[1]])] = [row[6],row[7]]
        lookup100[str([row[0],row[1]])] = [row[8],row[9]]

    lookup_table = {}
    lookup_table["1"] = lookup1
    lookup_table["10"] = lookup10
    lookup_table["25"] = lookup25
    lookup_table["50"] = lookup50
    lookup_table["100"] = lookup100

    return lookup_table

            
#############################################################
#############################################################
#############################################################

def getSimulationConfig(simulation_type, crop):
    
    ini_file = ""
    input_path = ""
    output_prefix = ""

    # potential
    if (simulation_type == "potential"):
        
        
        
        if (crop == "maize"):
            output_prefix = "M_PLP"
            input_path = "../input_data/phase_2/step2_3/crop_management/maize/maize_PLP/"
            ini_file = "maize_PLP.ini"        
            
        elif (crop == "winter_wheat"):
            output_prefix = "W_PLP"
            input_path = "../input_data/phase_2/step2_3/crop_management/winter_wheat/winter_wheat_PLP/"
            ini_file = "winter_wheat_PLP.ini"
            
    
    # water limited        
    elif (simulation_type == "water_limited"):
        
        output_prefix = "PLW"
        
        if (crop == "maize"):

            output_prefix = "M_PLW"
            input_path = "../input_data/phase_2/step2_3/crop_management/maize/maize_PLW/"
            ini_file = "maize_PLW.ini"
        
        elif (crop == "winter_wheat"):
            
            output_prefix = "W_PLW"
            input_path = "../input_data/phase_2/step2_3/crop_management/winter_wheat/winter_wheat_PLW/" 
            ini_file = "winter_wheat_PLW.ini"
        
    # nutrient water limited    
    elif (simulation_type == "nutrient_water_limited"):
        
        output_prefix = "PLN"
        
        if (crop == "maize"):
            output_prefix = "M_PLN"
            input_path = "../input_data/phase_2/step2_3/crop_management/maize/maize_PLN/"
            ini_file = "maize_PLN.ini"
            
        elif (crop == "winter_wheat"):

            output_prefix = "W_PLN"
            input_path = "../input_data/phase_2/step2_3/crop_management/winter_wheat/winter_wheat_PLN/"
            ini_file = "winter_wheat_PLN.ini"

    return output_prefix, input_path, ini_file
    
"""
Datastructure for the relevant meta information
"""
class macsur_meta_config():
    def __init__(self, start_date, end_date, project_id, climate_file, row_id, col_id, lat, elevation, resolution):
        
        self.start_date = start_date
        self.end_date = end_date
        self.project_id = project_id        
        self.climate_file = climate_file
        self.row_id = row_id
        self.col_id=col_id
        self.latitude = lat
        self.elevation = elevation
        self.resolution = resolution
        
        
    
    
main()
