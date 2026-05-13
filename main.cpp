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
    try
    {
        etherdog.StartNetworks(argc, argv);
        etherdog.loadFMU(etherdog.fmu_path);
        etherdog.SetupMappingFile();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during initialization: " << e.what() << std::endl;
        return 1;
    }

    etherdog.start();
    std::thread fmu_thread(&EtherDOG::FmuThread, &etherdog);
    etherdog.run();
    fmu_thread.join();
    etherdog.stop();
    return 0;
}