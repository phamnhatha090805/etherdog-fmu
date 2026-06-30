#include <argparse/argparse.hpp>
#include <fmi4cpp/fmi4cpp.hpp>
#include <iostream>

#include "etherdog.hpp"
#include <kickcat/CoE/EsiParser.h>
#include <kickcat/KickCAT.h>

#include <csignal>
#include <spdlog/spdlog.h>
#include <thread>

using namespace fmi4cpp;
using namespace kickcat;

EtherDOG *GlobalEtherDOG = nullptr; // Global instance of EtherDOG to be accessed in the signal handler

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        spdlog::info("Stop signal received, stopping simulation...");
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

    std::string config_file;
    argparse::ArgumentParser program("network_simulator");

    program.add_argument("-f", "--file").help("simple configuration file").required().store_into(config_file);

    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    std::ifstream file(config_file);
    if (!file.is_open())
    {
        std::cerr << "Failed to open configuration file: " << config_file << std::endl;
        return 1;
    }

    json main_config;
    file >> main_config;

    fs::path main_config_path = fs::path(config_file);
    fs::path config_dir = main_config_path.parent_path();

    fs::path fmu_path = config_dir / main_config["fmuPath"].get<std::string>();
    EtherDOG etherdog;
    GlobalEtherDOG = &etherdog; // Set the global instance to the created EtherDOG object
    try
    {
        etherdog.StartNetworks(config_dir, main_config);
        etherdog.loadFMU(fmu_path);
        etherdog.SetupMappingFile(main_config);
        spdlog::info("Load configuration successfully. Simulation has not started yet.");
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