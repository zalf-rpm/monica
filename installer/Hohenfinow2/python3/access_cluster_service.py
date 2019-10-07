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

import monica_io3
import time
import json
import sys
import os
import capnp
capnp.add_import_hook(additional_paths=[
                      "../../../../vcpkg/packages/capnproto_x64-windows-static/include/", "../../../../capnproto_schemas/"])
import common_capnp
import climate_data_capnp
import cluster_admin_service_capnp
import model_capnp


def main():

    config = {
        "port": "6666",
        "server": "localhost",
        "sim.json": os.path.join(os.path.dirname(__file__), '../sim-min.json'),
        "crop.json": os.path.join(os.path.dirname(__file__), '../crop-min.json'),
        "site.json": os.path.join(os.path.dirname(__file__), '../site-min.json'),
        "climate.csv": os.path.join(os.path.dirname(__file__), '../climate-min.csv'),
        "shared_id": None,
        "climate_service_address": "10.10.24.186:11000",
        "admin_master_address": "10.10.24.186:8000"
    }
    # read commandline args only if script is invoked directly from commandline
    if len(sys.argv) > 1 and __name__ == "__main__":
        for arg in sys.argv[1:]:
            k, v = arg.split("=")
            if k in config:
                config[k] = v

    print("config:", config)

    with open(config["sim.json"]) as _:
        sim_json = json.load(_)

    with open(config["site.json"]) as _:
        site_json = json.load(_)

    with open(config["crop.json"]) as _:
        crop_json = json.load(_)

    env = monica_io3.create_env_json_from_json_config({
        "crop": crop_json,
        "site": site_json,
        "sim": sim_json,
        "climate": ""  # climate_csv
    })
    # env["csvViaHeaderOptions"] = sim_json["climate.csv-options"]
    env["pathToClimateCSV"] = "blbla"  # config["climate.csv"]

    with open("env.json", "r") as _:
        env = json.load(_)

    # with open("env.json", "w") as _:
    #    _.write(json.dumps(env))

    # rust_client = capnp.TwoPartyClient("localhost:4000")
    # client = capnp.TwoPartyClient("localhost:8000")

    csv_time_series = capnp.TwoPartyClient(config["climate_service_address"]).bootstrap().cast_as(
        climate_data_capnp.Climate.TimeSeries)
    # monica = capnp.TwoPartyClient("localhost:9999").bootstrap().cast_as(model_capnp.Model.EnvInstance)

    master_available = False
    while not master_available:
        try:
            admin_master = capnp.TwoPartyClient(config["admin_master_address"]).bootstrap().cast_as(
                cluster_admin_service_capnp.Cluster.AdminMaster)
            master_available = True
        except:
            # time.sleep(1)
            pass

    model_factories = admin_master.availableModels().wait().factories
    if len(model_factories) > 0:

        model_factory = model_factories[0]

        # get instance id
        print(model_factory.modelId().wait().id)

        if True:
            # get a single instance
            print("requesting single instance ...")
            cap_holder = model_factory.newInstance().instance
            # cap_holder = model_factory.restoreSturdyRef(
            #    "b37e2e43-8a72-4dc0-8cb1-2bdc416d8148").cap
            monica = cap_holder.cap().wait().cap.as_interface(model_capnp.Model.EnvInstance)
            sturdy_ref = cap_holder.save().wait().sturdyRef
            print("single instance sturdy ref:", sturdy_ref)

            proms = []
            print("running jobs on single instance ...")
            for i in range(5):
                env["customId"] = str(i)
                proms.append(monica.run({"rest": {"value": json.dumps(env), "structure": {
                             "json": None}}, "timeSeries": csv_time_series}))

            print("results from single instance ...")
            for res in capnp.join_promises(proms).wait():
                if len(res.result.value) > 0:
                    # .result["customId"])
                    print(json.loads(res.result.value)["customId"])

            # cap_holder.release().wait()

        if True:
            # get multiple instances
            print("requesting multiple instances ...")
            cap_holders = model_factory.newInstances(5).instances
            restore = True
            # cap_holders = model_factory.restoreSturdyRef(
            #    "96495c8f-e241-4289-bee4-f314fef5b16d").cap
            sturdy_ref_p = cap_holders.save()
            entries_p = cap_holders.cap()
            sturdy_ref_p, entries_p = capnp.join_promises(
                [sturdy_ref_p, entries_p]).wait()
            entries = entries_p.cap

            sturdy_ref = sturdy_ref_p.sturdyRef
            print("multi instance sturdy ref:", sturdy_ref)
            monica_proms = []
            for ent in entries:
                monica_proms.append(ent.entry.cap().then(
                    lambda res: res.cap.as_interface(model_capnp.Model.EnvInstance)))

            monicas = capnp.join_promises(monica_proms).wait()

            proms = []
            print("running jobs on multiple instances ...")
            for i in range(len(monicas)):
                env["customId"] = str(i)
                proms.append(monicas[i].run({"rest": {"value": json.dumps(
                    env), "structure": {"json": None}}, "timeSeries": csv_time_series}))

            print("results from multiple instances ...")
            for res in capnp.join_promises(proms).wait():
                if len(res.result.value) > 0:
                    # .result["customId"])
                    print(json.loads(res.result.value)["customId"])

    return

    """
    climate_service = capnp.TwoPartyClient("localhost:8000").bootstrap().cast_as(climate_data_capnp.Climate.DataService)

    sims_prom = climate_service.simulations_request().send()
    sims = sims_prom.wait()
    
    if len(sims.simulations) > 0:
        info_prom = sims.simulations[1].info()
        info = info_prom.wait()
        print("id:", info.info.id)

        sim = sims.simulations[1]
        scens_p = sim.scenarios()
        scens = scens_p.wait()

        if len(scens.scenarios) > 0:
            scen = scens.scenarios[0]
            reals_p = scen.realizations()
            reals = reals_p.wait()

            if len(reals.realizations) > 0:
                real = reals.realizations[0]
                ts_p = real.closestTimeSeriesAt({"latlon": {"lat": 52.5, "lon": 14.1}})
                ts = ts_p.wait().timeSeries

                ts.range().then(lambda r: print(r)).wait()
                ts.realizationInfo().then(lambda r: print(r.realInfo)).wait()
                ts.scenarioInfo().then(lambda r: print(r.scenInfo)).wait()
                ts.simulationInfo().then(lambda r: print(r.simInfo)).wait()
    """

    csv_time_series = capnp.TwoPartyClient("localhost:8000").bootstrap().cast_as(
        climate_data_capnp.Climate.TimeSeries)
    # header = csv_time_series.header().wait().header
    monica_instance = capnp.TwoPartyClient(
        "localhost:9999").bootstrap().cast_as(model_capnp.Model.EnvInstance)
    # monica_instance = capnp.TwoPartyClient("login01.cluster.zalf.de:9999").bootstrap().cast_as(model_capnp.Model.EnvInstance)

    proms = []
    for i in range(10):
        env["customId"] = str(i)
        proms.append(monica_instance.run({"rest": {"value": json.dumps(
            env), "structure": {"json": None}}, "timeSeries": csv_time_series}))

    for i in range(10):
        ps = proms[i * 50:i * 50 + 50]

        for res in capnp.join_promises(ps).wait():
            if len(res.result.value) > 0:
                # .result["customId"])
                print(json.loads(res.result.value)["customId"])

    return

    reslist = capnp.join_promises(proms).wait()
    # reslist.wait()

    for res in reslist:
        print(json.loads(res.result.value)["customId"])  # .result["customId"])

    #        .then(lambda res: setattr(_context.results, "result", \
    #           self.calc_yearly_tavg(res[2].startDate, res[2].endDate, res[0].header, res[1].data)))

    # result_j = monica_instance.runEnv({"jsonEnv": json.dumps(env), "timeSeries": csv_time_series}).wait().result
    # result = json.loads(result_j)

    # print("result:", result)

    """
    # req = model.run_request()
    # req.data = ts
    # result = req.send().wait().result
    tavg_ts = ts.subheader(["tavg"]).wait().timeSeries
    start_time = time.perf_counter()
    result = model.run(tavg_ts).wait().result
    end_time = time.perf_counter()
    print("rust:", result, "time:", (end_time - start_time), "s")
    """

    """
    models = climate_service.models().wait().models
    if len(models) > 0:
        model = models[0]
        # req = model.run_request()
        # req.data = ts
        # result = req.send().wait().result
        tavg_ts = ts.subheader(["tavg"]).wait().timeSeries
        start_time = time.perf_counter()
        result = model.run(tavg_ts).wait().result
        end_time = time.perf_counter()
        print("python:", result, "time:", (end_time - start_time), "s")

    # text_prom = data_services.getText_request().send()
    # text = text_prom.wait()

    # gk_coord = data_services.getCoord_request().send().wait()

    # req = data_services.getSoilDataService_request()
    # req.id = 1
    # sds_prom = req.send()
    # sds = sds_prom.wait()
    # soil_req = sds_prom.soilDataService.getSoilIdAt_request()
    # soil_req.gkCoord.meridianNo = 5
    # soil_req.gkCoord.r = 1000
    # soil_req.gkCoord.h = 2000
    # soilId_prom = soil_req.send()
    # resp = soilId_prom.wait()
    
    # print(resp.soilId)
    """


if __name__ == '__main__':
    main()
    print("bla")
