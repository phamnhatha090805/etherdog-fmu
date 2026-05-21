#include <iostream>
#include <fmi4cpp/fmi4cpp.hpp>

#include <kickcat/KickCAT.h>
#include <kickcat/CoE/EsiParser.h>
#include "etherdog.hpp"

#include <thread>
#include <csignal>

using namespace fmi4cpp;
using namespace kickcat;

EtherDOG *GlobalEtherDOG = nullptr; // Global instance of EtherDOG to be accessed in the signal handler

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        std::cout << "\nStop signal received, stopping simulation..." << std::endl;
        if (GlobalEtherDOG != nullptr)
        {
            GlobalEtherDOG->requestStop();
        }
    }
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    EtherDOG etherdog;
    GlobalEtherDOG = &etherdog; // Set the global instance to the created EtherDOG object
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
    if (fmu_thread.joinable())
    {
        fmu_thread.join();
        std::cout << "FMU thread finished." << std::endl;
    }
    std::cout << "Simulation stopped gracefully." << std::endl;
    etherdog.stop();
    std::cout << "Simulation terminated." << std::endl;
    return 0;
}