
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
#include <kj/filesystem.h>
#include <kj/timer.h>

#include <capnp/any.h>
#include <capnp/compat/json.h>

#include "tools/debug.h"
#include "common/rpc-connection-manager.h"
#include "common/common.h"
#include "resource/version.h"
#include "toml/toml.hpp"

#include "run-monica-capnp.h"
#include "run-monica.h"
#include "capnp-helper.h"
#include "channel.h"
#include "PortConnector.h"
#include "climate-file-io.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "fbp.capnp.h"

//#define KJ_MVCAP(var) var = kj::mv(var)

using namespace std;
using namespace monica;
using namespace Tools;

class FBPMain {
public:
  enum PORTS { STATE_IN, ENV, EVENTS, STATE_OUT, RESULT };
  std::map<int, kj::StringPtr> inPortNames = {
    {STATE_IN, "serialized_state"},
    {ENV, "env"},
    {EVENTS, "events"},
  };
  std::map<int, kj::StringPtr> outPortNames = {
    {STATE_OUT, "serialized_state"},
    {RESULT, "result"},
  };

  explicit FBPMain(kj::ProcessContext &context)
  : ioContext(kj::setupAsyncIo())
  , conMan(ioContext)
  , ports(conMan, inPortNames, outPortNames)
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

  kj::MainBuilder::Validity setGenerateTOMLConfig() {
    doGenerateTOMLConfig = true;
    return true;
  }

  kj::MainBuilder::Validity setPortInfosReaderSr(kj::StringPtr name) {
    portInfosReaderSr = kj::str(name);
    return true;
  }

  // generate a TOML configuration file
  void generateTOMLConfig() {
    auto toml = toml::table{
      {"id", "e.g UUID4 3cd47d38-eec7-4df5-a52b-8ca11d41a9a4"},
      {"component_id", "de.zalf.cdp.mas.fbp.monica.daily"},
      {"name", "Daily MONICA FBP component"},
      {"params", toml::table{}},
      {"ports", toml::table{
        {"in", toml::table{
          {inPortNames[ENV], toml::table{
            {"sr", ""},
            {"type", "common.capnp::StructuredText::json"},
            {"description", "json data representing the monica env"}
          }},
          {inPortNames[STATE_IN], toml::table{
            {"sr", ""},
            {"type", "model/monica/monica_state.capnp::RuntimeState"},
            {"description", "serialized MONICA state"}
          }},
          {inPortNames[EVENTS], toml::table{
            {"sr", ""},
            {"type", "model/monica/monica_management.capnp::Event"},
            {"description", "MONICA events"}
          }}
        }},
        {"out", toml::table{
          {outPortNames[RESULT], toml::table{
            {"sr", ""},
            {"type", "common.capnp::StructuredText::json"},
            {"description", "results of a MONICA simulation"}
          }},
          {outPortNames[STATE_OUT], toml::table{
            {"sr", ""},
            {"type", "model/monica/monica_state.capnp::RuntimeState"},
            {"description", "serialized MONICA state after current day"}
          }}
        }}
      }}
    };

    if (portInfosReaderSr.size() > 0) {
      // write a file and treat the reader sr as a file name
      auto fs = kj::newDiskFilesystem();
      auto file = isAbsolutePath(portInfosReaderSr.cStr())
                  ? fs->getRoot().openFile(fs->getCurrentPath().eval(portInfosReaderSr),
                                           kj::WriteMode::CREATE | kj::WriteMode::MODIFY)
                  : fs->getCurrent().openFile(kj::Path::parse(portInfosReaderSr),
                                              kj::WriteMode::CREATE | kj::WriteMode::MODIFY);
      ostringstream ss;
      ss << toml << endl;
      file->writeAll(ss.str());
    } else { // output to std::out
      std::cout << toml << endl;
    }
  }

  toml::table parseTomlConfig(string_view toml) {
    try {
        return toml::parse(toml);
    }
    catch (const toml::parse_error& err) {
      KJ_LOG(INFO, "Parsing TOML configuration failed. Error:\n", err.what(), "\nTOML:\n", string(toml));
    }
    return toml::table();
  }

  void initMonica() {
    returnObjOutputs = env.returnObjOutputs();
    out.customId = env.customId;
    dailyOut.customId = env.customId;

    activateDebug = env.debugMode;

    KJ_LOG(INFO, "starting Monica");

    monica->simulationParametersNC().startDate = env.climateData.startDate();
    monica->simulationParametersNC().endDate = env.climateData.endDate();
    monica->simulationParametersNC().noOfPreviousDaysSerializedClimateData = env.params.simulationParameters.
      noOfPreviousDaysSerializedClimateData;

    store = setupStorage(env.events, env.climateData.startDate(), env.climateData.endDate());
    monica->addEvent("run-started");
  }


  void runMonica(){//vector<Workstep> dailyWorksteps) {
      debug() << "currentDate: " << monica->currentStepDate().toString() << endl;

      monica->dailyReset();

      // test if monica's crop has been dying in previous step
      // if yes, it will be incorporated into soil
      if (monica->cropGrowth() && monica->cropGrowth()->isDying()) monica->incorporateCurrentCrop();

      //monica main stepping method
      monica->step();

      //store results
      for (auto& s : store) s.storeResultsIfSpecApplies(*monica, returnObjOutputs);
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

  void finalizeDaily() {
    for (auto& sd : store) {
      Output::Data d;
      d.origSpec = sd.spec.origSpec.dump();
      d.outputIds = sd.outputIds;
      if (returnObjOutputs) {
        sd.aggregateResultsObj();
        d.resultsObj.push_back(sd.resultsObj.back());
      } else {
        sd.aggregateResults();
        d.results.push_back(sd.results.back());
      }
      dailyOut.data.emplace_back(d);;
    }
  }

  kj::MainBuilder::Validity startComponent() {
    KJ_LOG(INFO, "MONICA: starting daily MONICA Cap'n Proto FBP component");
    typedef mas::schema::fbp::IP IP;

    if (doGenerateTOMLConfig) {
      generateTOMLConfig();
      return true;
    }

    ports.connectFromPortInfos(portInfosReaderSr);
    auto &timer = ioContext.provider->getTimer();

    while ((ports.isInConnected(STATE_IN) || ports.isInConnected(ENV))
      && (ports.isOutConnected(RESULT) || ports.isOutConnected(STATE_OUT))) {

      bool envOrStateReceived = false;
      // read serialized state and create a monica instance with that state
      if (ports.isInConnected(ENV)) {
        KJ_LOG(INFO, "trying to read from env IN port");
        auto msg = ports.in(ENV).readIfMsgRequest().send().wait(ioContext.waitScope);
        switch (msg.which()) {
        case mas::schema::fbp::Channel<IP>::Msg::NO_MSG: break;
        case mas::schema::fbp::Channel<IP>::Msg::DONE:
          KJ_LOG(INFO, "received done on env port");
          ports.setInDisconnected(ENV);
          continue;
        case mas::schema::fbp::Channel<IP>::Msg::VALUE:
          try {
            auto ip = msg.getValue();
            auto stEnv = ip.getContent().getAs<mas::schema::common::StructuredText>();
            std::string err;
            const json11::Json& envJson = json11::Json::parse(stEnv.getValue().cStr(), err);
            auto envJsonStr = envJson.dump();
            //cout << "runMonica: " << envJson["customId"].dump() << endl;
            auto pathToSoilDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
            env.params.siteParameters.calculateAndSetPwpFcSatFunctions["Wessolek2009"] =
              Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir);
            env.params.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchten"] =
              Soil::updateUnsetPwpFcSatFromVanGenuchten;
            env.params.siteParameters.calculateAndSetPwpFcSatFunctions["Toth"] =
              Soil::updateUnsetPwpFcSatFromToth;
            auto errors = env.merge(envJson);
            monica = nullptr;
            monica = kj::heap<MonicaModel>(env.params);
            //monica->initComponents(env.params);
            initMonica();
            envOrStateReceived = true;
            break;
          } catch (kj::Exception& e) {
            KJ_LOG(INFO, "Exception reading env: ", e.getDescription());
            // treat env channel as disconnected and possibly leaf outer loop
            ports.setInDisconnected(ENV);
            continue;
          }
        }
      }
      // no env could be read
      if (!envOrStateReceived && ports.isInConnected(STATE_IN)) {
        KJ_LOG(INFO, "trying to read from serialized_state IN port");
        auto msg = ports.in(STATE_IN).readIfMsgRequest().send().wait(ioContext.waitScope);
        switch (msg.which()) {
        case mas::schema::fbp::Channel<IP>::Msg::NO_MSG: break;
        case mas::schema::fbp::Channel<IP>::Msg::DONE:
          KJ_LOG(INFO, "received done on serialized_state port");
          ports.setInDisconnected(STATE_IN);
          continue;
        case mas::schema::fbp::Channel<IP>::Msg::VALUE:
          try {
            auto ip = msg.getValue();
            try {
              auto runtimeState = ip.getContent().getAs<mas::schema::model::monica::RuntimeState>();
              if (monica.get() == nullptr) monica = kj::heap<MonicaModel>(runtimeState.getModelState());
              else monica->deserialize(runtimeState.getModelState());
              envOrStateReceived = true;
            } catch (kj::Exception& e) {
              auto jsonState = ip.getContent().getAs<capnp::Text>();
              const capnp::JsonCodec json;
              capnp::MallocMessageBuilder mmb;
              auto runtimeStateBuilder = mmb.initRoot<mas::schema::model::monica::RuntimeState>();
              json.decode(jsonState.asBytes().asChars(), runtimeStateBuilder);
              auto runtimeState = runtimeStateBuilder.asReader();
              if (monica.get() == nullptr) monica = kj::heap<MonicaModel>(runtimeState.getModelState());
              else monica->deserialize(runtimeState.getModelState());
              envOrStateReceived = true;
            }
            break;
          } catch (kj::Exception& e) {
            KJ_LOG(INFO, "Exception reading serialized state:", e.getDescription());
            // treat state channel as disconnected and possibly leaf outer loop
            ports.setInDisconnected(STATE_IN);
            continue;
          }
        }
      }
      if (!envOrStateReceived) {
        // wait for a second before trying again to read an env or state, thus create a monica instance
        timer.afterDelay(1*kj::SECONDS).wait(ioContext.waitScope);
        continue;
      }

      try {
        auto waitForMoreEvents = true;
        auto bracketOpened = false;
        while (waitForMoreEvents) {
          // now wait for events
          KJ_LOG(INFO, "trying to read from events IN port");
          auto msg = ports.in(EVENTS).readRequest().send().wait(ioContext.waitScope);
          KJ_LOG(INFO, "received msg from events IN port");
          // check for end of data from in port
          if (msg.isDone() || msg.getValue().getType() == IP::Type::CLOSE_BRACKET) {
            KJ_LOG(INFO, "received done -> finalizing monica run");
            //finalizeMonica(monica->currentStepDate());
            // send results to out port
            if (false && ports.isOutConnected(RESULT)) {
              auto wrq = ports.out(RESULT).writeRequest();
              auto st = wrq.initValue().initContent().initAs<mas::schema::common::StructuredText>();
              st.getStructure().setJson();
              st.setValue(out.to_json().dump());
              wrq.send().wait(ioContext.waitScope);
              KJ_LOG(INFO, "sent MONICA result on output channel");
              out.data.clear();
              out.warnings.clear();
              out.errors.clear();
            }
            waitForMoreEvents = false;
          } else {
            auto ip = msg.getValue();
            if (ip.getType() == IP::Type::OPEN_BRACKET) {
              KJ_LOG(INFO, "received open bracket IP");
              bracketOpened = true;
            } else { // IP::Type::STANDARD
              KJ_LOG(INFO, "received standard event IP");
              typedef mas::schema::model::monica::Event Event;
              auto event = ip.getContent().getAs<Event>();
              auto d = event.getAt().getDate();
              auto eventDate = Date(d.getDay(), d.getMonth(), d.getYear());
              switch (event.getType()) {
              case Event::ExternalType::WEATHER: {
                if (event.getParams().isNull() || !event.isAt()) continue;
                KJ_LOG(INFO, "received weather data at", eventDate.toIsoDateString());
                auto dw = event.getParams().getAs<mas::schema::model::monica::Params::DailyWeather>();
                auto climateData = dailyClimateDataToDailyClimateMap(dw.getData());
                monica->setCurrentStepDate(eventDate);
                monica->setCurrentStepClimateData(climateData);
                runMonica();
                //create daily output
                finalizeDaily();
                // send results to out port
                if (ports.isOutConnected(RESULT)) {
                  auto wrq = ports.out(RESULT).writeRequest();
                  auto st = wrq.initValue().initContent().initAs<mas::schema::common::StructuredText>();
                  st.getStructure().setJson();
                  st.setValue(dailyOut.to_json().dump());
                  wrq.send().wait(ioContext.waitScope);
                  KJ_LOG(INFO, "sent MONICA daily result on output channel");
                  dailyOut.data.clear();
                  dailyOut.warnings.clear();
                  dailyOut.errors.clear();
                }
                break;
              }
              case Event::ExternalType::SOWING: {
                auto sp = event.getParams().getAs<mas::schema::model::monica::Params::Sowing>();
                if (sp.hasCrop()) {
                  auto snRes = sp.getCrop().speciesRequest().send().wait(ioContext.waitScope);
                  auto speciesName = snRes.getInfo().getName();
                  auto cnRes = sp.getCrop().cultivarRequest().send().wait(ioContext.waitScope);
                  auto cultivarName = cnRes.getInfo().getName();
                  auto res = sp.getCrop().parametersRequest().send().wait(ioContext.waitScope);
                  auto cropParams = res.getParams().getAs<mas::schema::model::monica::CropSpec>();
                  KJ_LOG(INFO, "received sowing event for crop", speciesName, "/", cultivarName, " at",
                         eventDate.toIsoDateString());
                  monica->seedCrop(cropParams);
                  monica->addEvent("Sowing");
                }
                break;
              }
              case Event::ExternalType::HARVEST: {
                auto hp = event.getParams().getAs<mas::schema::model::monica::Params::Harvest>();
                if (monica->isCropPlanted()) {
                  KJ_LOG(INFO, "received harvest event at", eventDate.toIsoDateString());
                  Harvest::Spec spec;
                  monica->harvestCurrentCrop(hp.getExported(), spec);
                  monica->addEvent("Harvest");
                }
                break;
              }
              case Event::ExternalType::AUTOMATIC_SOWING: break;
              case Event::ExternalType::AUTOMATIC_HARVEST: break;
              case Event::ExternalType::IRRIGATION: {
                KJ_LOG(INFO, "received irrigation event at", eventDate.toIsoDateString());
                auto irr = event.getParams().getAs<mas::schema::model::monica::Params::Irrigation>();
                monica->applyIrrigation(irr.getAmount(), irr.hasParams()
                  ? irr.getParams().getNitrateConcentration() : 0.0);
                monica->addEvent("Irrigation");
                break;
              }
              case Event::ExternalType::TILLAGE: {
                KJ_LOG(INFO, "received tillage event at", eventDate.toIsoDateString());
                auto till = event.getParams().getAs<mas::schema::model::monica::Params::Tillage>();
                monica->applyTillage(till.getDepth());
                monica->addEvent("Tillage");
                break;
              }
              case Event::ExternalType::ORGANIC_FERTILIZATION: {
                auto of = event.getParams().getAs<mas::schema::model::monica::Params::OrganicFertilization>();
                if (of.hasParams() && of.getParams().hasParams()) {
                  KJ_LOG(INFO, "received organic fertilization event at", eventDate.toIsoDateString());
                  monica->applyOrganicFertiliser(OrganicMatterParameters(of.getParams().getParams()),
                    of.getAmount(), of.getIncorporation());
                  monica->addEvent("OrganicFertilization");
                }
                break;
              }
              case Event::ExternalType::MINERAL_FERTILIZATION: {
                auto mf = event.getParams().getAs<mas::schema::model::monica::Params::MineralFertilization>();
                if (mf.hasPartition()) {
                  KJ_LOG(INFO, "received mineral fertilization event at", eventDate.toIsoDateString());
                  monica->applyMineralFertiliser(mf.getPartition(), mf.getAmount());
                  monica->addEvent("MineralFertilization");
                }
                break;
              }
              case Event::ExternalType::N_DEMAND_FERTILIZATION: break;
              case Event::ExternalType::CUTTING: {
                auto c = event.getParams().getAs<mas::schema::model::monica::Params::Cutting>();
                if (c.hasCuttingSpec() && c.getCuttingSpec().size() > 0) {
                  KJ_LOG(INFO, "received cutting event at", eventDate.toIsoDateString());
                  std::map<int, Cutting::Value> organId2cuttingSpec;
                  std::map<int, double> organId2exportFraction;
                  for (auto cs : c.getCuttingSpec()) {
                    int organId = -1;
                    typedef mas::schema::model::monica::PlantOrgan PA;
                    switch (cs.getOrgan()) {
                    case PA::ROOT: organId = static_cast<int>(OId::ROOT); break;
                    case PA::LEAF: static_cast<int>(OId::LEAF); break;
                    case PA::SHOOT: static_cast<int>(OId::SHOOT); break;
                    case PA::FRUIT: static_cast<int>(OId::FRUIT); break;
                    case PA::STRUKT: static_cast<int>(OId::STRUCT); break;
                    case PA::SUGAR: static_cast<int>(OId::SUGAR); break;
                    }
                    typedef mas::schema::model::monica::Params::Cutting C;
                    Cutting::CL cl = Cutting::none;
                    switch (cs.getCutOrLeft()) {
                    case C::CL::CUT: cl = Cutting::cut; break;
                    case C::CL::LEFT: cl = Cutting::left; break;
                    }
                    Cutting::Unit unit = Cutting::percentage;
                    switch (cs.getUnit()) {
                    case C::Unit::PERCENTAGE: unit = Cutting::percentage; break;
                    case C::Unit::BIOMASS: unit = Cutting::biomass; break;
                    case C::Unit::LAI: unit = Cutting::LAI; break;
                    }
                    if (organId >= 0) {
                      organId2cuttingSpec[organId] = {cs.getValue(), unit, cl};
                      organId2exportFraction[organId] = cs.getExportPercentage() / 100.0;
                    }
                  }
                  monica->cropGrowth()->applyCutting(organId2cuttingSpec, organId2exportFraction,
                    c.getCutMaxAssimilationRatePercentage() / 100.0);
                  monica->addEvent("Cutting");
                }
                break;
              }
              case Event::ExternalType::SET_VALUE: break;
              case Event::ExternalType::SAVE_STATE: {
                if (ports.isOutConnected(STATE_OUT)) {
                  try {
                    auto ss = event.getParams().getAs<mas::schema::model::monica::Params::SaveState>();
                    KJ_LOG(INFO, "received save state event at", eventDate.toIsoDateString());

                    monica->simulationParametersNC().noOfPreviousDaysSerializedClimateData = ss.getNoOfPreviousDaysSerializedClimateData();

                    capnp::MallocMessageBuilder message;
                    auto runtimeState = message.initRoot<mas::schema::model::monica::RuntimeState>();
                    const auto modelState = runtimeState.initModelState();
                    monica->serialize(modelState);

                    auto wrq = ports.out(STATE_OUT).writeRequest();
                    if (ss.getAsJson()) {
                      const capnp::JsonCodec json;
                      const auto jStr = json.encode(runtimeState);
                      wrq.initValue().initContent().setAs<capnp::Text>(jStr);
                    } else {
                      wrq.initValue().initContent().setAs<mas::schema::model::monica::RuntimeState>(runtimeState);
                    }

                    wrq.send().wait(ioContext.waitScope);
                    auto asWhat = ss.getAsJson() ? "as JSON" : "as capnp binary";
                    KJ_LOG(INFO, "sent serialized MONICA state on output channel", asWhat);
                  } catch (kj::Exception &e) {
                    KJ_LOG(INFO, "Exception on attempt to serialize MONICA state:", e.getDescription());
                  }
                }
                break;
              }
              }
            }
          }
        }
      } catch (const kj::Exception& e) {
        KJ_LOG(INFO, "Exception:", e.getDescription());
      }

      // free monica instance
      monica = nullptr;
    }

    ports.closeOutPorts();
    return true;
  }

  kj::MainFunc getMain() {
    return kj::MainBuilder(context, kj::str("MONICA FBP Component v", VER_FILE_VERSION_STR),
                           "Offers a MONICA service.")
          .addOption({'g', "generate_toml_config"}, KJ_BIND_METHOD(*this, setGenerateTOMLConfig)
            , "Give this component a name.")
          .expectOptionalArg("port_infos_reader_SR", KJ_BIND_METHOD(*this, setPortInfosReaderSr))
           // .addOptionWithArg({'f', "from_attr"}, KJ_BIND_METHOD(*this, setFromAttr),
           //                   "<attr>", "Which attribute to read the MONICA env from.")
           // .addOptionWithArg({'t', "to_attr"}, KJ_BIND_METHOD(*this, setToAttr),
           //                   "<attr>", "Which attribute to write the MONICA result to.")
    .callAfterParsing(KJ_BIND_METHOD(*this, startComponent))
           .build();
  }

private:
  kj::AsyncIoContext ioContext;
  mas::infrastructure::common::ConnectionManager conMan;
  mas::infrastructure::common::PortConnector ports;
  kj::String name;
  kj::ProcessContext& context;
  kj::String fromAttr;
  kj::String toAttr;
  bool doGenerateTOMLConfig{false};
  kj::String portInfosReaderSr;
  Env env;
  bool returnObjOutputs{false};
  kj::Own<MonicaModel> monica;
  std::vector<StoreData> store;
  Output out;
  Output dailyOut;
  //std::list<Workstep> dynamicWorksteps;
  std::map<int, std::vector<double>> dailyValues;
  std::vector<std::function<void()>> applyDailyFuncs;
};

KJ_MAIN(FBPMain)
