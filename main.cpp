#include <iostream>
#include <fmi4cpp/fmi4cpp.hpp>

#include <kickcat/KickCAT.h>
#include <kickcat/CoE/EsiParser.h>
#include "etherdog.hpp"

using namespace fmi4cpp;
using namespace kickcat;

int main(int argc, char *argv[])
{
    EtherDOG etherdog;

    etherdog.StartNetworks(argc, argv);
    const std::string fmu_path = "/home/etherdog/fmu_test/Test2-1.fmu";
    etherdog.loadFMU(fmu_path);
    etherdog.start();
    etherdog.run();
    etherdog.stop();
    return 0;
}