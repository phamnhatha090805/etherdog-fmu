#pragma once

#include <iostream>
#include <fmi4cpp/fmi4cpp.hpp>

#include <kickcat/KickCAT.h>
#include <kickcat/CoE/EsiParser.h>

using namespace fmi4cpp;
using namespace kickcat;

class EtherDOG
{
public:
    EtherDOG()
    {
    }
    void loadFMU(const std::string &fmu_path);
    void start();
    void run();
    void step();
    void stop();
    int StartNetworks(int argc, char *argv[]);

private:
    double t;
    // fmi2::fmu fmu;
    std::unique_ptr<fmi4cpp::fmi2::cs_fmu> cs_fmu;
    std::shared_ptr<const fmi4cpp::fmi2::cs_model_description> cs_md;
    std::unique_ptr<fmi4cpp::fmi2::cs_slave> fmu_slave;
    // fmi4cpp::fmi2::real_variable var;
    // fmi2ValueReference vr;

    /*std::vector<std::unique_ptr<EmulatedESC>> escs;
    std::vector<std::unique_ptr<PDO>> pdos;
    std::vector<std::unique_ptr<Slave>> slaves;
    std::vector<std::unique_ptr<mailbox::response::Mailbox>> mailboxes;
    std::vector<std::vector<uint8_t>> input_pdo;
    std::vector<std::vector<uint8_t>> output_pdo;*/
};
