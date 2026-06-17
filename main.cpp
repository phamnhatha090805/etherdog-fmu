#include <iostream>
#include <fmi4cpp/fmi4cpp.hpp>

#include <kickcat/KickCAT.h>
#include <kickcat/CoE/EsiParser.h>
#include "etherdog.hpp"

#include <thread>
#include <csignal>
#include <spdlog/spdlog.h>

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
    spdlog::set_level(spdlog::level::debug);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    spdlog::info("EtherDOG FMU simulation starting...");
    EtherDOG etherdog;
    GlobalEtherDOG = &etherdog; // Set the global instance to the created EtherDOG object
    try
    {
        etherdog.StartNetworks(argc, argv);
        etherdog.loadFMU(etherdog.fmu_path);
        etherdog.SetupMappingFile();
        spdlog::info("Load configuration successfully. Simulation has not started yet.");

        if (etherdog.load_config_only)
        {
            spdlog::info("Load configuration only mode selected. Exiting before simulation start.");
            return 0;
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("Error during initialization: {}", e.what());
        return 1;
    }

    etherdog.start();
    std::thread fmu_thread(&EtherDOG::FmuThread, &etherdog);
    etherdog.run();
    if (fmu_thread.joinable())
    {
        fmu_thread.join();
        spdlog::info("FMU thread finished.");
    }
    spdlog::info("Simulation stopped gracefully.");
    etherdog.stop();
    spdlog::info("EtherDOG FMU simulation terminated successfully.");
    return 0;
}