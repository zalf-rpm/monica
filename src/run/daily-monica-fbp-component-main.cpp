/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include <iostream>

#include <kj/debug.h>
#include <kj/common.h>
#include <kj/main.h>

#include <capnp/any.h>

#include "tools/debug.h"
#include "common/rpc-connection-manager.h"
#include "common/common.h"
#include "resource/version.h"

#include "run-monica-capnp.h"
#include "run-monica.h"
#include "capnp-helper.h"
#include "climate-file-io.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "fbp.capnp.h"

#define KJ_MVCAP(var) var = kj::mv(var)

using namespace std;
using namespace monica;
using namespace Tools;

class FBPMain {
public:
  explicit FBPMain(kj::ProcessContext& context)
  : ioContext(kj::setupAsyncIo())
  , conMan(ioContext)
  , context(context) {}

  kj::MainBuilder::Validity setName(kj::StringPtr n) {
    name = str(n);
    return true;
  }

  kj::MainBuilder::Validity setFromAttr(kj::StringPtr name) {
    fromAttr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setToAttr(kj::StringPtr name) {
    toAttr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setEventsInSr(kj::StringPtr name) {
    eventsInSr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setOutSr(kj::StringPtr name) {
    outSr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setSerializedStateInSr(kj::StringPtr name) {
    serializedStateInSr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setEnvInSr(kj::StringPtr name) {
    envInSr = kj::str(name);
    return true;
  }

  void initMonica() {
    returnObjOutputs = env.returnObjOutputs();
    out.customId = env.customId;

    activateDebug = env.debugMode;

    //prefer multiple crop rotations, but use a single rotation if there
    if (env.cropRotations.empty() && !env.cropRotation.empty()) {
      env.cropRotations.emplace_back(env.climateData.startDate(), env.climateData.endDate(), env.cropRotation);
    }

    debug() << "starting Monica" << endl;
    debug() << "-----" << endl;

    monica->simulationParametersNC().startDate = env.climateData.startDate();
    monica->simulationParametersNC().endDate = env.climateData.endDate();
    monica->simulationParametersNC().noOfPreviousDaysSerializedClimateData = env.params.simulationParameters.
      noOfPreviousDaysSerializedClimateData;

    //debug() << "currentDate" << endl;
    //Date currentDate = env.climateData.startDate();

    // create a way for worksteps to let the runtime calculate at a daily basis things a workstep needs when being executed
    // e.g. to actually accumulate values from days before the workstep (for calculating a moving window of past values)
    int dailyFuncId = 0;


    //iterate through all the worksteps in the croprotation(s) and check for functions which have to run daily
    for (auto& cr : env.cropRotations) {
      for (auto& cm : cr.cropRotation) {
        for (const auto& wsptr : cm.getWorksteps()) {
          auto df = wsptr->registerDailyFunction(
                                                 [this, dailyFuncId]() -> vector<double>& {
                                                   return dailyValues[dailyFuncId];
                                                 });
          if (df) {
            applyDailyFuncs.emplace_back([this, df, dailyFuncId] {
              dailyValues[dailyFuncId].push_back(df(monica.get()));
            });
          }
          dailyFuncId++;
        }
      }
    }

    auto crit = env.cropRotations.begin();

    //cropRotation is a shadow of the env.cropRotation, which will hold pointers to CMs in env.cropRotation, but might shrink
    //if pure absolute CMs are finished
    vector<CultivationMethod*> cropRotation;

    auto checkAndInitShadowOfNextCropRotation_ = [](auto& envCropRotations, auto& crit, auto& cropRotation,
                                                    Date currentDate) {
      if (crit != envCropRotations.end()) {
        //if current cropRotation is finished, try to move to next
        if (crit->end.isValid() && currentDate == crit->end + 1) {
          crit++;
          cropRotation.clear();
        }

        //check again, because we might have moved to next cropRotation
        if (crit != envCropRotations.end()) {
          //if a new cropRotation starts, copy the pointers to the CMs to the shadow CR
          if (crit->start.isValid() && currentDate == crit->start) {
            for (auto& cm : crit->cropRotation) cropRotation.push_back(&cm);
            return true;
          }
        }
      }
      return false;
    };

    auto checkAndInitShadowOfNextCropRotation = [&](Date currentDate) {
      return checkAndInitShadowOfNextCropRotation_(env.cropRotations, crit, cropRotation, currentDate);
    };

    //iterator through the crop rotation
    auto cmit = cropRotation.begin();

    auto findNextCultivationMethod_ = [&](Date currentDate,
                                          auto& cropRotation,
                                          auto& cmit,
                                          bool advanceToNextCM = true) {
      CultivationMethod* currentCM = nullptr;
      Date nextAbsoluteCMApplicationDate;

      //it might be possible that the next cultivation method has to be skipped (if cover/catch crop)
      bool notFoundNextCM = true;
      while (notFoundNextCM) {
        if (advanceToNextCM) {
          //delete fully cultivation methods with only absolute worksteps,
          //because they won't participate in a new run when wrapping the crop rotation
          if ((*cmit)->areOnlyAbsoluteWorksteps() || !(*cmit)->repeat()) cmit = cropRotation.erase(cmit);
          else cmit++;

          //start anew if we reached the end of the crop rotation
          if (cmit == cropRotation.end()) cmit = cropRotation.begin();
        }

        //check if there's at least a cultivation method left in cropRotation
        if (cmit != cropRotation.end()) {
          advanceToNextCM = true;
          currentCM = *cmit;

          //addedYear tells that the start of the cultivation method was before currentDate and thus the whole
          //CM had to be moved into the next year
          //is possible for relative dates
          bool addedYear = currentCM->reinit(currentDate);
          if (addedYear) {
            //current CM is a cover crop, check if the latest sowing date would have been before current date,
            //if so, skip current CM
            if (currentCM->isCoverCrop()) {
              //if current CM's latest sowing date is actually after current date, we have to
              //reinit current CM again, but this time prevent shifting it to the next year
              if (!(notFoundNextCM =
                    currentCM->absLatestSowingDate().withYear(currentDate.year()) < currentDate)) {
                currentCM->reinit(currentDate, true);
              }
            } else notFoundNextCM = currentCM->canBeSkipped(); //if current CM was marked skipable, skip it
          } else { //not added year or CM was had also absolute dates
            if (currentCM->isCoverCrop()) notFoundNextCM = currentCM->absLatestSowingDate() < currentDate;
            else if (currentCM->canBeSkipped()) notFoundNextCM = currentCM->absStartDate() < currentDate;
            else notFoundNextCM = false;
          }

          if (notFoundNextCM) nextAbsoluteCMApplicationDate = Date();
          else {
            nextAbsoluteCMApplicationDate = currentCM->staticWorksteps().empty()
                                              ? Date()
                                              : currentCM->absStartDate(
                                                                        false);
            debug() << "new valid next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
          }
        } else {
          currentCM = nullptr;
          nextAbsoluteCMApplicationDate = Date();
          notFoundNextCM = false;
        }
      }

      return make_pair(currentCM, nextAbsoluteCMApplicationDate);
    };

    auto findNextCultivationMethod = [&](Date currentDate, bool advanceToNextCM = true) {
      return findNextCultivationMethod_(currentDate, cropRotation, cmit, advanceToNextCM);
    };

    //direct handle to current cultivation method
    //CultivationMethod* currentCM{nullptr};
    //Date nextAbsoluteCMApplicationDate;
    //tie(currentCM, nextAbsoluteCMApplicationDate) = findNextCultivationMethod(currentDate, false);

    store = setupStorage(env.events, env.climateData.startDate(), env.climateData.endDate());

    monica->addEvent("run-started");

  }


  void runMonica(){//vector<Workstep> dailyWorksteps) {
      debug() << "currentDate: " << monica->currentStepDate().toString() << endl;

      monica->dailyReset();

      // test if monica's crop has been dying in previous step
      // if yes, it will be incorporated into soil
      if (monica->cropGrowth() && monica->cropGrowth()->isDying()) {
        monica->incorporateCurrentCrop();
      }

      //try to apply dynamic worksteps marked to run before everything else that day
      // for (auto& dws : list(dynamicWorksteps)) {
      //   if (dws.runAtStartOfDay() && dws.applyWithPossibleCondition(monica.get())) dynamicWorksteps.remove(dws);
      // }

      //for (auto& ws : dailyWorksteps) ws.apply(monica.get());

      //monica main stepping method
      monica->step();
      debug() << std::endl;

      // call all daily functions, assuming it's better to do this after the steps, than before
      // so the daily monica calculations will be taken into account
      // but means also that a workstep which gets executed before the steps, can't take the
      // values into account by applying a daily function
      for (auto& f : applyDailyFuncs) f();

      //try to apply dynamic worksteps marked to run AFTER everything else that day
      // for (auto& dws : list(dynamicWorksteps)) {
      //   if (!dws.runAtStartOfDay() && dws.applyWithPossibleCondition(monica.get())) dynamicWorksteps.remove(dws);
      // }

      //store results
      for (auto& s : store) s.storeResultsIfSpecApplies(*monica, returnObjOutputs);

      //if the next application date is not valid, we're at the end
      //of the application list of this cultivation method
      //and go to the next one in the crop rotation
      //if (currentCM && currentCM->allDynamicWorkstepsFinished() && !nextAbsoluteCMApplicationDate.isValid()) {
        // to count the applied fertiliser for the next production process
//!!!!!!!        monica->resetFertiliserCounter();
        //tie(currentCM, nextAbsoluteCMApplicationDate) = findNextCultivationMethod(currentDate + 1);
      //}
  }

  void finalizeMonica(Date currentDate) {
    if (env.params.simulationParameters.serializeMonicaStateAtEnd) {
      SaveMonicaState sms(currentDate, env.params.simulationParameters.pathToSerializationAtEndFile,
        env.params.simulationParameters.serializeMonicaStateAtEndToJson,
        env.params.simulationParameters.noOfPreviousDaysSerializedClimateData);
      sms.apply(monica.get());
    }

    for (auto& sd : store) {
      //aggregate results of while events or unfinished other from/to ranges (where to event didn't happen yet)
      if (returnObjOutputs) sd.aggregateResultsObj();
      else sd.aggregateResults();
      out.data.push_back({sd.spec.origSpec.dump(), sd.outputIds, sd.results, sd.resultsObj});
    }
  }


  kj::MainBuilder::Validity startComponent() {
    bool startedServerInDebugMode = false;

    debug() << "MONICA: starting MONICA Cap'n Proto FBP component" << endl;
    typedef mas::schema::fbp::IP IP;
    typedef mas::schema::fbp::Channel<IP> Channel;
    typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText,
                                            mas::schema::common::StructuredText> MonicaEnvInstance;

    //std::cout << "inSr: " << inSr.cStr() << std::endl;
    //std::cout << "outSr: " << outSr.cStr() << std::endl;
    bool serializedStateChannelConnected = serializedStateInSr.size() > 0;
    auto ssp = serializedStateChannelConnected
                                ? conMan.tryConnectB(serializedStateInSr.cStr()).castAs<Channel::ChanReader>()
                                : nullptr;
    auto envp = conMan.tryConnectB(envInSr.cStr()).castAs<Channel::ChanReader>();
    auto eventsp = conMan.tryConnectB(eventsInSr.cStr()).castAs<Channel::ChanReader>();
    auto outp = conMan.tryConnectB(outSr.cStr()).castAs<Channel::ChanWriter>();

    // read serialized state and create a monica instance with that state
    if (!monica && serializedStateChannelConnected) {
      if (auto msg = ssp.readRequest().send().wait(ioContext.waitScope); !msg.isDone()) {
        auto ip = msg.getValue();
        auto runtimeState = ip.getContent().getAs<mas::schema::model::monica::RuntimeState>();
        monica = kj::heap<MonicaModel>(runtimeState.getModelState());
      }
    }
    // if we didn't get a monica from the serialized state, we have to create a new one from the supplied env
    if (!monica) {
      auto msg = envp.readRequest().send().wait(ioContext.waitScope);
      if (msg.isDone()) {
        KJ_LOG(INFO, "received done on env port -> exiting main loop");
        return true;
      }
      auto ip = msg.getValue();
      auto stEnv = ip.getContent().getAs<mas::schema::common::StructuredText>();
      std::string err;
      const json11::Json &envJson = json11::Json::parse(stEnv.getValue().cStr(), err);
      auto envJsonStr = envJson.dump();
      //cout << "runMonica: " << envJson["customId"].dump() << endl;
      Env env;
      auto pathToSoilDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
      env.params.siteParameters.calculateAndSetPwpFcSatFunctions["Wessolek2009"] = Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir);
      env.params.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchten"] = Soil::updateUnsetPwpFcSatFromVanGenuchten;
      env.params.siteParameters.calculateAndSetPwpFcSatFunctions["Toth"] = Soil::updateUnsetPwpFcSatFromToth;
      auto errors = env.merge(envJson);
      monica = kj::heap<MonicaModel>(env.params);
      initMonica();
    }

    try {
      while (true) {
        // now wait for events
        KJ_LOG(INFO, "trying to read from events IN port");
        auto msg = eventsp.readRequest().send().wait(ioContext.waitScope);
        KJ_LOG(INFO, "received msg from events IN port");
        // check for end of data from in port
        if (msg.isDone()) {
          KJ_LOG(INFO, "received done -> exiting main loop");
          break;
        } else {
          KJ_LOG(INFO, "received env -> running MONICA");
          auto ip = msg.getValue();
          typedef capnp::List<mas::schema::climate::Element> ListOfElements;
          auto events = ip.getContent().getAs<capnp::List<mas::schema::model::monica::Event>>();
          //vector<Workstep> dailyWorksteps;
          for (const auto& event : events) {
            typedef mas::schema::model::monica::Event Event;
            switch (event.getType()) {
            case Event::ExternalType::WEATHER: {
              KJ_LOG(INFO, "received weather data at: ", monica->currentStepDate().toString());
              if (event.getParams().isNull() || !event.isAt()) continue;
              auto dw = event.getParams().getAs<mas::schema::model::monica::Params::DailyWeather>();
              auto climateData = dailyClimateDataToDailyClimateMap(dw.getData());
              auto d = event.getAt().getDate();
              monica->setCurrentStepDate(Tools::Date(d.getDay(), d.getMonth(), d.getYear()));
              monica->setCurrentStepClimateData(climateData);
              break;
            }
            case Event::ExternalType::SOWING: {
              auto sp = event.getParams().getAs<mas::schema::model::monica::Params::Sowing>();
              //auto speciesName = sp.getCrop().speciesRequest().send().then([](auto &&res){ return res.getInfo().getName(); });
              //auto cultivarName = sp.getCrop().cultivarRequest().send().then([](auto &&res){ return res.getInfo().getName(); });
              auto snRes = sp.getCrop().speciesRequest().send().wait(ioContext.waitScope);
              auto speciesName = snRes.getInfo().getName();
              auto cnRes = sp.getCrop().cultivarRequest().send().wait(ioContext.waitScope);
              auto cultivarName = cnRes.getInfo().getName();
              auto res = sp.getCrop().parametersRequest().send().wait(ioContext.waitScope);
              auto cropParams = res.getParams().getAs<mas::schema::model::monica::CropSpec>();
              KJ_LOG(INFO, "received sowing event for crop: ", speciesName, "/", cultivarName, " at: ", monica->currentStepDate().toString());
              monica->seedCrop(cropParams);
              monica->addEvent("Sowing");
            }
            case Event::ExternalType::HARVEST: {
              auto hp = event.getParams().getAs<mas::schema::model::monica::Params::Harvest>();
              if (monica->isCropPlanted()){
                Harvest::Spec spec;
                monica->harvestCurrentCrop(hp.getExported(), spec);
                KJ_LOG(INFO, "received harvest event at: ", monica->currentStepDate().toString());
                monica->addEvent("Harvest");
              }
            }
            }
          }
          runMonica();//dailyWorksteps);
        }
      }

      KJ_LOG(INFO, "closing OUT port");
      outp.closeRequest().send().wait(ioContext.waitScope);
    } catch (const kj::Exception& e) {
      KJ_LOG(INFO, "Exception: ", e.getDescription());
      std::cerr << "Exception: " << e.getDescription().cStr() << endl;
    }

    return true;
  }

  kj::MainFunc getMain() {
    return kj::MainBuilder(context, kj::str("MONICA FBP Component v", VER_FILE_VERSION_STR),
                           "Offers a MONICA service.")
           .addOptionWithArg({'n', "name"}, KJ_BIND_METHOD(*this, setName),
                             "<component-name>", "Give this component a name.")
           .addOptionWithArg({'f', "from_attr"}, KJ_BIND_METHOD(*this, setFromAttr),
                             "<attr>", "Which attribute to read the MONICA env from.")
           .addOptionWithArg({'t', "to_attr"}, KJ_BIND_METHOD(*this, setToAttr),
                             "<attr>", "Which attribute to write the MONICA result to.")
           .addOptionWithArg({'i', "events_in_sr"}, KJ_BIND_METHOD(*this, setEventsInSr),
                             "<sturdy_ref>", "Sturdy ref to input channel.")
           .addOptionWithArg({'o', "result_out_sr"}, KJ_BIND_METHOD(*this, setOutSr),
                             "<sturdy_ref>", "Sturdy ref to output channel.")
           .addOptionWithArg({'s', "serialized_state_in_sr"}, KJ_BIND_METHOD(*this, setSerializedStateInSr),
                             "<sturdy_ref>", "Sturdy ref to serialized state channel.")
           .addOptionWithArg({'e', "env_in_sr"}, KJ_BIND_METHOD(*this, setEnvInSr),
                             "<sturdy_ref>", "Sturdy ref to env channel (IIP).")
           .callAfterParsing(KJ_BIND_METHOD(*this, startComponent))
           .build();
  }

private:
  kj::AsyncIoContext ioContext;
  mas::infrastructure::common::ConnectionManager conMan;
  kj::String name;
  kj::ProcessContext& context;
  kj::String eventsInSr;
  kj::String outSr;
  kj::String fromAttr;
  kj::String toAttr;
  kj::String serializedStateInSr;
  kj::String envInSr;
  Env env;
  bool returnObjOutputs{false};
  kj::Own<MonicaModel> monica;
  std::vector<StoreData> store;
  Output out;
  //std::list<Workstep> dynamicWorksteps;
  std::map<int, std::vector<double>> dailyValues;
  std::vector<std::function<void()>> applyDailyFuncs;
};

KJ_MAIN(FBPMain)
