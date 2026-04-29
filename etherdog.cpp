
#include "etherdog.hpp"

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

using namespace kickcat;
using namespace kickcat::slave;
using json = nlohmann::json;
namespace fs = std::filesystem;

const double stopTime = 1;
const double stepSize = 1;

CoE::Device findDeviceByVendorAndProduct(std::vector<CoE::Device> &&devices, uint32_t vendor_id, uint32_t product_code, uint32_t revision_number)
{
    for (CoE::Device &device : devices)
    {
        if (device.vendor_id == vendor_id && device.product_code == product_code && device.revision_number == revision_number)
        {
            printf("Found matching device in ESI file for vendor_id 0x%08x, product_code 0x%08x, revision_number 0x%08x\n", vendor_id, product_code, revision_number);
            return std::move(device);
        }
    }
    std::stringstream ss;
    ss << "No matching device found for vendor_id 0x" << std::hex << vendor_id << " and product_code 0x" << product_code << " and revision_number 0x" << revision_number;
    throw std::runtime_error(ss.str());
}

int EtherDOG::StartNetworks(int argc, char *argv[])
{
    argparse::ArgumentParser program("network_simulator");

    std::string interface;
    program.add_argument("-i", "--interface")
        .help("network interface name")
        .required()
        .store_into(interface);

    int slave_number = 0;
    program.add_argument("-n", "--count")
        .help("Number of slaves to simulate")
        .default_value(0)
        .scan<'i', int>()
        .store_into(slave_number);

    std::vector<std::string> slave_configs;
    program.add_argument("-s", "--slaves")
        .help("JSON configuration files for slaves")
        .remaining()
        .store_into(slave_configs);

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

    if (slave_configs.empty())
    {
        std::cerr << "No slave configuration files provided" << std::endl;
        std::cerr << program;
        return 1;
    }

    std::vector<std::string> expanded_slave_configs;

    if (slave_number > 0)
    {
        if (slave_configs.size() != 1)
        {
            std::cerr << "When using --count/-n, you must provide exactly one JSON config file with --slaves/-s" << std::endl;
            return 1;
        }

        expanded_slave_configs.reserve(slave_number);
        for (int i = 0; i < slave_number; ++i)
        {
            expanded_slave_configs.push_back(slave_configs[0]);
        }
    }
    else
    {
        expanded_slave_configs = slave_configs;
    }

    size_t slave_count = expanded_slave_configs.size();
    return 0;
}

void EtherDOG::loadFMU(const std::string &fmu_path)
{
    std::cout << "Loading FMU from path: " << fmu_path << std::endl;
    fmi2::fmu fmu(fmu_path);
    cs_fmu = fmu.as_cs_fmu();
    cs_md = cs_fmu->get_model_description(); // smart pointer to a cs_model_description instance
    std::cout << "model_identifier=" << cs_md->model_identifier << std::endl;
    fmu_slave = cs_fmu->new_instance();

    auto var = cs_md->get_variable_by_name("Out1").as_real();
    // auto var2 = cs_md->get_variable_by_name("Out2").as_real();
    std::cout << "Name=" << var.name() << ", start=" << var.start().value_or(0) << std::endl;
    // std::cout << "Name=" << var2.name() << ", start=" << var2.start().value_or(0) << std::endl;

    auto vr = var.valueReference();
    // auto vr2 = var2.valueReference();
}

void EtherDOG::start()
{
    std::cout << "Starting simulation..." << std::endl;
    fmu_slave->setup_experiment();
    fmu_slave->enter_initialization_mode();
    fmu_slave->exit_initialization_mode();
}

void EtherDOG::run()
{
    while ((t = fmu_slave->get_simulation_time()) <= stopTime)
    {
        step();
    }
}

void EtherDOG::step()
{
    auto var = cs_md->get_variable_by_name("Out1").as_real();
    auto vr = var.valueReference();
    double value;
    // double value2;

    if (!fmu_slave->step(stepSize))
    {
        std::cerr << "Error! step() returned with status: " << to_string(fmu_slave->last_status()) << std::endl;
        return;
    }

    if (!fmu_slave->read_real(vr, value))
    {
        std::cerr << "Error! step() returned with status: " << to_string(fmu_slave->last_status()) << std::endl;
        return;
    }
    std::cout << "t=" << t << ", " << var.name() << "=" << value << std::endl;
    // std::cout << "t=" << t << ", " << var.name() << "=" << value << ", " << var2.name() << "=" << value2 << std::endl;
}

void EtherDOG::stop()
{
    fmu_slave->terminate();
}
