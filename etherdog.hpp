#pragma once

#include <iostream>
#include <fmi4cpp/fmi4cpp.hpp>

#include <kickcat/KickCAT.h>
#include <algorithm>
#include <argparse/argparse.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <numeric>

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
    void loadFMU(const std::string &fmu_path);
    void start();
    void run();
    void step();
    void stop();

    int StartNetworks(int argc, char *argv[]);
    void FrameHandler();

    const double stopTime = 100.0;
    const double stepSize = 0.01;

    double fmu_output;

private:
    double t;
    // fmi2::fmu fmu;
    std::unique_ptr<fmi4cpp::fmi2::cs_fmu> cs_fmu;
    std::shared_ptr<const fmi4cpp::fmi2::cs_model_description> cs_md;
    std::unique_ptr<fmi4cpp::fmi2::cs_slave> fmu_slave;
    // fmi4cpp::fmi2::real_variable var;
    // fmi2ValueReference vr;

    std::shared_ptr<kickcat::AbstractSocket> socket;
    std::vector<std::unique_ptr<EmulatedESC>> escs;
    std::vector<std::unique_ptr<PDO>> pdos;
    std::vector<std::unique_ptr<Slave>> slaves;
    std::vector<std::unique_ptr<mailbox::response::Mailbox>> mailboxes;
    std::vector<std::vector<uint8_t>> input_pdo;
    std::vector<std::vector<uint8_t>> output_pdo;

    std::string interface;
    int slave_number = 0;
    std::vector<std::string> slave_configs;
    std::vector<nanoseconds> stats;
};
