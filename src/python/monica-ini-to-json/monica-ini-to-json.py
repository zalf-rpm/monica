#!/usr/bin/python
# -*- coding: UTF-8

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# Authors:
# Michael Berg-Mohnicke <michael.berg@zalf.de>
#
# Maintainers:
# Currently maintained by the authors.
#
# This file has been created at the Institute of
# Landscape Systems Analysis at the ZALF.
# Copyright (C: Leibniz Centre for Agricultural Landscape Research (ZALF)

import os
import json
import csv
from datetime import date, datetime, timedelta
from collections import defaultdict
import sys
import ConfigParser as cp

YEAR_BORDERS = [[0, 30, 2000], [31, 99, 1900]]
YEAR_SUFFIX_DIGITS = 3

def make_date(s):
  year = int(s[-2:])
  l1, u1, cent1 = YEAR_BORDERS[0]
  l2, u2, cent2 = YEAR_BORDERS[1]

  if l1 <= year and year <= u1:
    year = cent1 + year
  elif l2 <= year and year <= u2:
    year = cent2 + year

  return str(year) + "-" + s[2:4] + "-" + s[0:2]


def main():
    "main"

    config = {
        "monica.ini": "./monica.ini",
        "sim.json": "./conversion-template-sim.json",
        "out-suffix": "-from-monica-ini"
    }

    if len(sys.argv) == 2 and sys.argv[1] == "-h":
        print "usage: [python] monica-ini-to-json.py [monica.ini=path-to-monica.ini] [sim.json=path-to-template-sim.json] [out-suffix=suffix-to-append-to-sim-site-crop-climate.files]"
        print "defaults:", config
        return

    if len(sys.argv) > 1:
        for arg in sys.argv[1:]:
            k, v = arg.split("=")
            if k in config:
                config[k] = v

    dir_of_monica_ini = os.path.dirname(config["monica.ini"])
    dir_of_template_sim_json = os.path.dirname(config["sim.json"])

    ini = cp.ConfigParser()
    ini.read([config["monica.ini"]])

    with open(config["sim.json"]) as _:
        sim = json.load(_)

    def path_to_file(path_to_f, abs_prefix):
        "add abs_prefix to path of file, if path is relative"
        if os.path.isabs(path_to_f):
            return path_to_f
        else:
            return abs_prefix + "/" + path_to_f

    with open(path_to_file(sim["site.json"], dir_of_template_sim_json)) as _:
        site = json.load(_)

    with open(path_to_file(sim["crop.json"], dir_of_template_sim_json)) as _:
        crop = json.load(_)

    # reference the correct files in sim.json
    # sim.json is just for completness reasons and is unused/not needed
    sim["sim.json"] = "sim" + config["out-suffix"] + ".json"
    sim["site.json"] = "site" + config["out-suffix"] + ".json"
    sim["crop.json"] = "crop" + config["out-suffix"] + ".json"
    sim["climate.csv"] = "climate" + config["out-suffix"] + ".csv"


    # NMin method settings
    if ini.has_section("nmin_fertiliser"):
        if ini.has_option("nmin_fertiliser", "activated"): 
            sim["UseNMinMineralFertilisingMethod"] = ini.getboolean("nmin_fertiliser", "activated")

        if ini.has_option("nmin_fertiliser", "min"): 
            sim["NMinUserParams"]["min"] = ini.getfloat("nmin_fertiliser", "min")

        if ini.has_option("nmin_fertiliser", "max"): 
            sim["NMinUserParams"]["max"] = ini.getfloat("nmin_fertiliser", "max")

        if ini.has_option("nmin_fertiliser", "delay_in_days"): 
            sim["NMinUserParams"]["delayInDays"] = ini.getint("nmin_fertiliser", "delay_in_days")

        if ini.has_option("nmin_fertiliser", "mineral_fert_id"): 
            sim["NMinFertiliserPartition"][2] = ini.get("nmin_fertiliser", "mineral_fert_id")

    # automatic irrigation settings
    if ini.has_section("automatic_irrigation") and ini.has_option("automatic_irrigation", "activated") and ini.getboolean("automatic_irrigation", "activated"):
        sim["UseAutomaticIrrigation"] = True

        if ini.has_option("automatic_irrigation", "amount"):
            sim["UseAutomaticIrrigation"]["amount"] = ini.getint("automatic_irrigation", "amount")

        if ini.has_option("automatic_irrigation", "treshold"):
            sim["UseAutomaticIrrigation"]["treshold"] = ini.getfloat("automatic_irrigation", "treshold")

        if ini.has_option("automatic_irrigation", "nitrate"):
            sim["UseAutomaticIrrigation"]["AutoIrrigationParams"]["irrigationParameters"]["nitrateConcentration"][0] = ini.getfloat("automatic_irrigation", "nitrate")

        if ini.has_option("automatic_irrigation", "sulfate"):
            sim["UseAutomaticIrrigation"]["AutoIrrigationParams"]["irrigationParameters"]["sulfateConcentration"][0] = ini.getfloat("automatic_irrigation", "sulfate")

    # site settings
    if ini.has_section("site_parameters"):

        if ini.has_option("site_parameters", "latitude"):
            site["SiteParameters"]["Latitude"] = ini.getfloat("site_parameters", "latitude")

        if ini.has_option("site_parameters", "slope"):
            site["SiteParameters"]["Slope"] = ini.getfloat("site_parameters", "slope")

        if ini.has_option("site_parameters", "heightNN"):
            site["SiteParameters"]["HeightNN"] = ini.getfloat("site_parameters", "heightNN")

        if ini.has_option("site_parameters", "N_deposition"):
            site["SiteParameters"]["NDeposition"] = ini.getfloat("site_parameters", "N_deposition")

        if ini.has_option("site_parameters", "wind_speed_height"):
            site["EnvironmentParameters"]["WindSpeedHeight"] = ini.getfloat("site_parameters", "wind_speed_height")

        if ini.has_option("site_parameters", "leaching_depth"):
            site["EnvironmentParameters"]["LeachingDepth"] = ini.getfloat("site_parameters", "leaching_depth")


    #creating soil profile
    if ini.has_option("files", "soil"):
        default_soil_cn = None
        if ini.has_option("site_parameters", "soilCNRatio"):
            default_soil_cn = ini.getfloat("site_parameters", "soilCNRatio")

        with open(path_to_file(ini.get("files", "soil"), dir_of_monica_ini)) as _:
            first_id = None     

            #skip header line      
            _.next()     

            for line in _:
                es = line.split()

                #read just the first profile
                if not first_id:
                  first_id = es[0]
                if es[0] != first_id:
                  break

                l = {}
                l["Thickness"] = [int(es[3]) / 100.0, "m"]
                l["KA5TextureClass"] = es[2]
                l["SoilOrganicCarbon"] = [float(es[1]), "%"]
                l["SoilRawDensity"] = ["bulk-density-class->raw-density", int(es[4]), ["KA5-texture-class->clay", l["KA5TextureClass"]]]
                l["Sceleton"] = int(es[5]) / 100.0
                if int(es[6]) > 0:
                    l["CN"] = int(es[6])
                elif default_soil_cn:
                  l["CN"] = default_soil_cn
                if ini.has_option("site_parameters", "pH"):
                  l["pH"] = ini.getfloat("site_parameters", "pH")

                site["SiteParameters"]["SoilProfileParameters"].append(l)


    # read fertilization data
    fert_apps = defaultdict(lambda: defaultdict(list))
    if ini.has_option("files", "fertiliser"):

        with open(path_to_file(ini.get("files", "fertiliser"), dir_of_monica_ini)) as _:

            #skip header line
            _.next()

            for line in _:
                if line.strip() == "end":
                    break
                es = line.split()

                fert_apps[es[0]][datetime.strptime(make_date(es[3]), "%Y-%m-%d")].append({
                    "N": int(es[1]),
                    "type": es[2],
                    "incorporate": int(es[4]) == 1
                })

    # read irrigation data
    irrig_apps = defaultdict(list)
    if ini.has_option("files", "irrigation"):

        with open(path_to_file(ini.get("files", "irrigation"), dir_of_monica_ini)) as _:

            #skip header line
            _.next()

            for line in _:
                if line.strip() == "end":
                    break

                es = line.split()

                irrig_apps[datetime.strptime(make_date(es[3]), "%Y-%m-%d")].append({
                    "nitrate": int(es[4]),
                    "sulfate": int(es[2]),
                    "amount": int(es[1]),
                })

    # create crop rotation
    if ini.has_option("files", "croprotation"):

        with open(path_to_file(ini.get("files", "croprotation"), dir_of_monica_ini)) as _:
            first_id = None
            ferts = None
            sorted_fert_keys = None
            sorted_irrig_keys = sorted(irrig_apps)

            #skip header line
            _.next()

            def append_ferts(wss, sfk):
                data = ferts[sfk]
                for d in data:
                    if d["incorporate"]:
                        wss.append({
                            "date": sfk.strftime("%Y-%m-%d"),
                            "type": "OrganicFertilization",
                            "amount": [d["N"], "kg N"],
                            "parameters": ["ref", "fert-params", d["type"]],
                            "incorporation": d["incorporate"]
                        })
                    else:
                        wss.append({
                            "date": sfk.strftime("%Y-%m-%d"),
                            "type": "MineralFertilization",
                            "amount": [d["N"], "kg N"],
                            "partition": ["ref", "fert-params", d["type"]]
                        })

            def append_irrigs(wss, sik):
                data = irrig_apps[sik]
                for d in data:
                    wss.append({
                        "date": sik.strftime("%Y-%m-%d"),
                        "type": "Irrigation",
                        "amount": [d["amount"], "kg N"],
                        "parameters": {
                            "nitrateConcentration": [d["nitrate"], "mg dm-3"],
                            "sulfateConcentration": [d["sulfate"], "mg dm-3"]
                        }
                    })

            for line in _:
                es = line.split()

                #read just the first crop rotation
                if not first_id:
                    first_id = es[0]
                    ferts = fert_apps[first_id]
                    sorted_fert_keys = sorted(ferts)
                if es[0] != first_id:
                    break

                iso_sowing_date = make_date(es[2])
                sd = datetime.strptime(iso_sowing_date, "%Y-%m-%d")

                iso_harvest_date = make_date(es[3])
                hd = datetime.strptime(iso_harvest_date, "%Y-%m-%d")

                wss = []

                while len(sorted_fert_keys) > 0 and sorted_fert_keys[0] <= sd:
                    append_ferts(wss, sorted_fert_keys[0])
                    del sorted_fert_keys[0]

                while len(sorted_irrig_keys) > 0 and sorted_irrig_keys[0] <= sd:
                    append_irrigs(wss, sorted_irrig_keys[0])
                    del sorted_irrig_keys[0]

                wss.append({
                    "date": iso_sowing_date,
                    "type": "Sowing",
                    "crop": ["ref", "crops", es[1]]
                })

                while len(sorted_fert_keys) > 0 and sorted_fert_keys[0] <= hd:
                    append_ferts(wss, sorted_fert_keys[0])
                    del sorted_fert_keys[0]

                while len(sorted_irrig_keys) > 0 and sorted_irrig_keys[0] <= hd:
                    append_irrigs(wss, sorted_irrig_keys[0])
                    del sorted_irrig_keys[0]

                wss.append({
                    "date": iso_harvest_date,
                    "type": "Harvest"
                })

                wss.append({
                    "date": make_date(es[4]),
                    "type": "Tillage",
                    "depth": [float(es[6]) / 100.0, "m"]
                })

                crop["cropRotation"].append({"worksteps": wss})

            for sfk in sorted_fert_keys:
                append_ferts(crop["cropRotation"][-1]["worksteps"], sfk)

            for sik in sorted_irrig_keys:
                append_irrigs(crop["cropRotation"][-1]["worksteps"], sik)


    # simulation time and climate data
    if ini.has_section("simulation_time") and ini.has_option("simulation_time", "startyear") and ini.has_option("simulation_time", "endyear"):

        #set start/end year
        start_year = ini.getint("simulation_time", "startyear")
        end_year = ini.getint("simulation_time", "endyear")
        sim["climate.csv-options"]["start-date"] = str(start_year) + "-01-01"
        sim["climate.csv-options"]["end-date"] = str(end_year) + "-12-31"

        if ini.has_option("files", "climate_prefix"):

            climate_data = []
            climate_data.append([
                "iso-date",
                "tmin",
                "tavg",
                "tmax",
                "precip",
                "globrad",
                "sunhours",
                "wind",
                "relhumid"
            ])
            climate_data.append([
                "[]",
                "[°C]",
                "[°C]",
                "[°C]",
                "[mm]",
                "[MJ/m2]",
                "[h]",
                "[m/s]",
                "[%]"
            ])

            has_globrad = False
            has_sunhours = False

            for year in range(start_year, end_year+1):

                with open(path_to_file(ini.get("files", "climate_prefix") + str(year)[-YEAR_SUFFIX_DIGITS:], dir_of_monica_ini)) as _:

                    _.next()
                    _.next()
                    _.next()

                    for line in _:

                        es = line.split()

                        globrad = float(es[8])
                        if int(globrad) > 0:
                            has_globrad = True

                        sunhours = float(es[7])
                        if int(sunhours) > 0:
                            has_sunhours = True

                        climate_data.append([
                            (date(year, 1, 1) + timedelta(days=int(es[10])-1)).strftime("%Y-%m-%d"),
                            float(es[1]),
                            float(es[0]),
                            float(es[2]),
                            float(es[9]),
                            round(globrad / 100.0, 2),
                            sunhours,
                            float(es[6]),
                            float(es[11])
                        ])

            if has_globrad:
                for row in climate_data:
                    del row[6]
            elif has_sunhours:
                for row in climate_data:
                    del row[5]

            # write climate data
            with(open(dir_of_monica_ini + "/" + sim["climate.csv"], "wb")) as _:
                writer = csv.writer(_, delimiter=",")
                writer.writerows(climate_data)

    # write sim.json
    with(open(dir_of_monica_ini + "/" + sim["sim.json"], "w")) as _:
        json.dump(sim, _)

    # write site.json
    with(open(dir_of_monica_ini + "/" + sim["site.json"], "w")) as _:
        json.dump(site, _)

    # write crop.json
    with(open(dir_of_monica_ini + "/" + sim["crop.json"], "w")) as _:
        json.dump(crop, _)

main()