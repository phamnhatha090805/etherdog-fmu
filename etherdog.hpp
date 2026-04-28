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

private:
    double t;
    // fmi2::fmu fmu;
    std::unique_ptr<fmi4cpp::fmi2::cs_fmu> cs_fmu;
    std::shared_ptr<const fmi4cpp::fmi2::cs_model_description> cs_md;
    std::unique_ptr<fmi4cpp::fmi2::cs_slave> fmu_slave;
    // fmi4cpp::fmi2::real_variable var;
    // fmi2ValueReference vr;
};
