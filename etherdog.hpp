#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <numeric>
#include <sstream>

#include <fmi4cpp/fmi4cpp.hpp>
#include <kickcat/KickCAT.h>

#include "kickcat/CoE/EsiParser.h"
#include "kickcat/CoE/OD.h"
#include "kickcat/CoE/mailbox/response.h"
#include "kickcat/ESC/EmulatedESC.h"
#include "kickcat/Frame.h"
#include "kickcat/OS/Time.h"
#include "kickcat/PDO.h"
#include "kickcat/helpers.h"
#include "kickcat/slave/Slave.h"
#include <kickcat/AbstractSocket.h>
#include <kickcat/ESI/Device.h>
#include <kickcat/ESI/Parser.h>
#include <kickcat/ESI/SIIBuilder.h>
#include <kickcat/SIIParser.h>

using namespace fmi4cpp;
using namespace kickcat;
using namespace kickcat::slave;
using json = nlohmann::json;

namespace fs = std::filesystem;

class EtherDOG
{
  public:
    void loadFMU(const fs::path &path);
    void start();
    void run();
    void step();
    void stop();
    void requestStop();

    void FmuThread();

    int StartNetworks(const fs::path &config_dir, const nlohmann::json &main_config);
    void FrameHandler();

    void SetupMappingFile(const nlohmann::json &main_config);

    const double stopTime = 10.0;
    const double stepSize = 0.1; // this is in seconds

    enum class FmuVariableType
    {
        REAL,
        INTEGER32,
        BOOLEAN,
    };

    struct Mapping
    {
        fmi2ValueReference vr; // FMU variable ID
        size_t size;           // number of bytes
        std::string PDOname;
        size_t SlaveIndex;
        std::string FMUname;
        FmuVariableType fmuVarType;
        CoE::Entry *entry; // the CoE entry corresponding to this mapping
        bool MessagePrinted;
    };

  private:
    void ExecutePdoInputMappings();
    void ExecutePdoOutputMappings();

    double t;
    std::unique_ptr<fmi4cpp::fmi2::cs_fmu> cs_fmu;
    std::shared_ptr<const fmi4cpp::fmi2::cs_model_description> cs_md;
    std::unique_ptr<fmi4cpp::fmi2::cs_slave> fmu_slave;

    std::shared_ptr<kickcat::AbstractSocket> socket;
    std::vector<std::unique_ptr<EmulatedESC>> escs;
    std::vector<std::unique_ptr<PDO>> pdos;
    std::vector<std::unique_ptr<Slave>> slaves;
    std::vector<std::unique_ptr<mailbox::response::Mailbox>> mailboxes;
    std::vector<std::vector<uint8_t>> input_pdo;
    std::vector<std::vector<uint8_t>> output_pdo;

    std::vector<nanoseconds> stats;

    std::vector<Mapping> input_mappings;
    std::vector<Mapping> output_mappings;

    std::mutex fmu_mutex; // Mutex to protect access to FMU output variable

    std::atomic<bool> running{true}; // Atomic flag to control the running state of the simulation, can
                                     // be set to false to request stopping the simulation
};
