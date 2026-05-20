#pragma once

#include <algorithm>
#include <argparse/argparse.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <numeric>
#include <mutex>
#include <atomic>

#include <fmi4cpp/fmi4cpp.hpp>
#include <kickcat/KickCAT.h>

#include "kickcat/CoE/EsiParser.h"
#include "kickcat/CoE/mailbox/response.h"
#include "kickcat/ESC/EmulatedESC.h"
#include "kickcat/Frame.h"
#include "kickcat/OS/Time.h"
#include "kickcat/PDO.h"
#include "kickcat/helpers.h"
#include "kickcat/slave/Slave.h"
#include <kickcat/AbstractSocket.h>

using namespace fmi4cpp;
using namespace kickcat;
using namespace kickcat::slave;
using json = nlohmann::json;

namespace fs = std::filesystem;

class EtherDOG
{
public:
    std::string fmu_path;
    void loadFMU(const std::string &path);
    void start();
    void run();
    void step();
    void stop();
    void requestStop();

    void FmuThread();

    int StartNetworks(int argc, char *argv[]);
    void FrameHandler();

    void SetupMappingFile();

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
        CoE::Entry &entry; // the CoE entry corresponding to this mapping, for debug/info purposes
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

    std::string config_file;
    std::string interface;
    json main_config;
    std::vector<nanoseconds> stats;

    std::vector<Mapping> input_mappings;
    std::vector<Mapping> output_mappings;

    std::mutex fmu_mutex; // Mutex to protect access to FMU output variable

    std::atomic<bool> running{true}; // Atomic flag to control the running state of the simulation, can be set to false to request stopping the simulation
};
