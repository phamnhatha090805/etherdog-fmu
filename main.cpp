#include <iostream>
#include <fmi4cpp/fmi4cpp.hpp>

#include <kickcat/KickCAT.h>
#include <kickcat/CoE/EsiParser.h>
#include "etherdog.hpp"

#include <thread>

using namespace fmi4cpp;
using namespace kickcat;

int main(int argc, char *argv[])
{
    EtherDOG etherdog;

    etherdog.StartNetworks(argc, argv);
    const std::string fmu_path = "/home/etherdog/fmu_test/TestEC-1.fmu";
    etherdog.loadFMU(fmu_path);
    etherdog.start();
    std::thread fmu_thread(&EtherDOG::FmuThread, &etherdog);
    etherdog.run();
    fmu_thread.join();
    etherdog.stop();
    return 0;
}