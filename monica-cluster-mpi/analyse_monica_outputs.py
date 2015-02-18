#!/usr/bin/python
# -*- coding: UTF-8

import numpy
import datetime

#############################################################
#############################################################
#############################################################

def get_cumm_transp(sowing_day_index, harvest_day_index, rmout):
    list = rmout[sowing_day_index:harvest_day_index+1]
    transp = 0
    for item in list:
        transp += float(item["act_ET"])-float(item["act_Ev"])
    #print "get_cumm_transp:\t\t", transp
    return round(transp,2)

#############################################################
#############################################################
#############################################################

def get_cumm_ET(sowing_day_index, harvest_day_index, rmout):
    list = rmout[sowing_day_index:harvest_day_index+1]
    ET = 0
    for item in list:
        ET += float(item["act_ET"])
    #print "get_cumm_transp:\t\t", transp
    return round(ET,2)

#############################################################
#############################################################
#############################################################

def get_cumm_NEE(sowing_day_index, harvest_day_index, rmout):
    list = rmout[sowing_day_index:harvest_day_index+1]
    nee = 0.0
    for item in list:
        nee += float(item["NEE"])
    #print "get_cumm_transp:\t\t", transp
    return round(nee,2)

#############################################################
#############################################################
#############################################################

def get_cumm_NPP(sowing_day_index, harvest_day_index, rmout):
    list = rmout[sowing_day_index:harvest_day_index+1]
    npp = 0.0
    for item in list:
        npp += float(item["NPP"])
    #print "get_cumm_transp:\t\t", transp
    return round(npp,2)

#############################################################
#############################################################
#############################################################

def get_cumm_n_leached(sowing_day_index, harvest_day_index, rmout):
    list = rmout[sowing_day_index:harvest_day_index+1]
    nleach = 0
    for item in list:
        nleach += float(item["NLeach"])
    #print "get_cumm_n_leached:\t", nleach
    return round(nleach, 2)

#############################################################
#############################################################
#############################################################

def get_cumm_waterloss(sowing_day_index, harvest_day_index, rmout):
    list = rmout[sowing_day_index:harvest_day_index+1]
    waterloss = 0
    for item in list:
        waterloss += float(item["Recharge"])
    #print "get_cumm_waterloss:\t", waterloss
    return round(waterloss,2)


#############################################################
#############################################################
#############################################################

def get_cumm_irrigation_amount(start_index, end_index, smout):
    list = smout[start_index:end_index+1]
    irrigation_amount = 0
    for item in list:
        irrigation_amount += float(item["Irrig"])
    #print "get_cumm_waterloss:\t", waterloss
    return round(irrigation_amount,2)

#############################################################
#############################################################
#############################################################

def get_max_LAI(sowing_day_index, harvest_day_index, smout):
    if (harvest_day_index == None or sowing_day_index == None):
        return 0.0
    list = smout[sowing_day_index:harvest_day_index+1]
    lai_list = []
    for item in list:
        lai_list.append(float(item["LAI"]))
    #print "get_max_LAI:\t\t", numpy.max(lai_list)
    return numpy.max(lai_list)

#############################################################
#############################################################
#############################################################

def get_max_rooting_depth(sowing_day_index, harvest_day_index, rmout):
    if (harvest_day_index == None or sowing_day_index == None):
        return 0.0
    list = rmout[sowing_day_index:harvest_day_index+1]
    root_depth_list = []
    for item in list:
        root_depth_list.append(float(item["RootDep"]) / 10.0)    # layer --> m
    #print "get_max_LAI:\t\t", numpy.max(lai_list)
    return numpy.max(root_depth_list)

#############################################################
#############################################################
#############################################################

def get_cumm_n2o(sowing_day_index, harvest_day_index, rmout):
    list = rmout[sowing_day_index:harvest_day_index+1]
    n2o = 0
    for item in list:
        n2o += float(item["N2O"]) / 10.0 # kgN/m2 --> gN/ha
   
    return round(n2o)

#############################################################
#############################################################
#############################################################
def get_soc200(date_index, rmout):
    soc200 = float(rmout[date_index]["SOC-0-200"])
    return round(soc200)

#############################################################
#############################################################
#############################################################
def get_soc30(date_index, rmout):
    soc30 = float(rmout[date_index]["SOC-0-30"])
    return round(soc30)

#############################################################
#############################################################
#############################################################
"""
Identifying of anthesis date based on change of dev_stage values.
If current dev_stage is 5 and dev_stage of yesterday was 4
than today must be the anthesis day
@param data Rmout data of a single year
@return (date, julian_day)
"""
def get_anthesis_date(smout, sowing_index, harvest_index):
    
    old_dev_stage = None
    
    for index, map in enumerate(smout):
        
        if (index>=sowing_index and index <= harvest_index):
            
            d = map["Datum"].split("/")
            date = datetime.date(int(d[2]), int(d[1]), int(d[0]))        
            dev_stage = int(map["Stage"])
            
            if ( (dev_stage == 5) and (old_dev_stage == 4) ):
                #print "Anthesis: \t\t", date, index  
                return (date, index)
            
            old_dev_stage = dev_stage
        
    return (None, None)

#############################################################
#############################################################
#############################################################


"""
"""
def get_maturity_date(data, sowing_index, harvest_index, crop_name):
    old_dev_stage = None
    old_date = None
    stage5_counter = 0
    
    maturity_stage = 6
    if (crop_name == "maize"):
        maturity_stage = 7
        
    for index, map in enumerate(data):
        if (index>=sowing_index and index <= harvest_index):
            d = map["Datum"].split("/")
            date = datetime.date(int(d[2]), int(d[1]), int(d[0])) 
                   
            dev_stage = int(map["Stage"])
            if (dev_stage == maturity_stage):
                stage5_counter += 1
            
            if ( stage5_counter >=3):
                #print "Harvest1:\t\t", date, index
                return (date, index)
    
            if ((dev_stage == 0) and (old_dev_stage > 0) ):
                #print "Harvest2:\t\t", old_date, index-1
                return (old_date, index-1)
            
            old_dev_stage = dev_stage
            old_date = date
        
    return (None, None)

#############################################################
#############################################################
#############################################################

def get_index_of_date(date, rmout):
    
    for index, row in enumerate(rmout):
        d = row["Datum"].split("/")
        current_date = datetime.date(int(d[2]), int(d[1]), int(d[0]))
        
        if (current_date == date):
            return index
        
    return None 
