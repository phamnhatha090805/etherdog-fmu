
#include "etherdog.hpp"
#include <thread>

CoE::Device findDeviceByVendorAndProduct(std::vector<CoE::Device> &&devices, uint32_t vendor_id, uint32_t product_code, uint32_t revision_number)
{
    // This function searches through the provided list of CoE::Device objects to find one that matches the given vendor_id, product_code, and revision_number.

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
    // This function can be used to set up the EtherCAT network simulation, including parsing command-line arguments, initializing the slaves and PDOs, and starting the network communication.

    argparse::ArgumentParser program("network_simulator");

    program.add_argument("-f", "--file")
        .help("simple configuration file")
        .required()
        .store_into(config_file);

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

    file >> main_config;

    interface = main_config["interface"];
    fmu_path = main_config["fmuPath"];

    auto &slaves_config = main_config["slaves"];
    size_t slave_count = slaves_config.size();

    escs.reserve(slave_count);
    pdos.reserve(slave_count);
    slaves.reserve(slave_count);
    mailboxes.reserve(slave_count);
    input_pdo.reserve(slave_count);
    output_pdo.reserve(slave_count);

    constexpr uint32_t PDO_MAX_SIZE = 32;
    CoE::EsiParser parser;

    for (const auto &config : slaves_config)
    {

        std::string eeprom_path = config["eeprom"];
        fs::path eeprom_full_path = eeprom_path;
        auto esc = std::make_unique<EmulatedESC>(eeprom_full_path.string().c_str());
        auto pdo = std::make_unique<PDO>(esc.get());
        auto slave = std::make_unique<Slave>(esc.get(), pdo.get());

        if (config.contains("coe_xml"))
        {
            std::string coe_xml_path = config["coe_xml"];
            fs::path coe_xml_full_path = coe_xml_path;
            auto mbx = std::make_unique<mailbox::response::Mailbox>(esc.get(), 1024);
            auto devices = parser.loadDevicesFromFile(coe_xml_full_path.string());

            // search for productcode / vendor id:
            uint32_t vendor_id = esc->getVendorId();             // from EEPROM
            uint32_t product_code = esc->getProductCode();       // from EEPROM
            uint32_t revision_number = esc->getRevisionNumber(); // from EEPROM
            CoE::Device device = findDeviceByVendorAndProduct(std::move(devices), vendor_id, product_code, revision_number);
            mbx->enableCoE(std::move(device.dictionary));
            slave->setMailbox(mbx.get());
            mailboxes.push_back(std::move(mbx));
        }

        input_pdo.emplace_back(PDO_MAX_SIZE);
        std::iota(input_pdo.back().begin(), input_pdo.back().end(), 0);
        output_pdo.emplace_back(PDO_MAX_SIZE, 0xFF);

        pdo->setInput(input_pdo.back().data(), PDO_MAX_SIZE);
        pdo->setOutput(output_pdo.back().data(), PDO_MAX_SIZE);

        escs.push_back(std::move(esc));
        pdos.push_back(std::move(pdo));
        slaves.push_back(std::move(slave));
    }

    // Configure DL status for each ESC based on its position in the chain.
    // Port 0 is always connected (upstream to master or previous slave).
    // Port 1 is connected if there is a downstream slave.
    for (size_t i = 0; i < escs.size(); ++i)
    {
        uint16_t dl_status = 0;
        dl_status |= (1 << 4); // PL_port0
        dl_status |= (1 << 9); // COM_port0

        if (i + 1 < escs.size())
        {
            dl_status |= (1 << 5);  // PL_port1
            dl_status |= (1 << 11); // COM_port1
        }

        escs[i]->write(reg::ESC_DL_STATUS, &dl_status, sizeof(dl_status));
    }

    auto [socket2, _] = createSockets(interface, "");
    socket = std::move(socket2);
    socket->setTimeout(-1ns);

    stats.reserve(1000);

    for (auto &slave : slaves)
    {
        slave->start();
    }
    return 0;
}

void EtherDOG::FrameHandler()
{
    // This function is called in a loop to handle incoming EtherCAT frames, process them with the slaves and ESCs, and send responses back.
    // It also measures the processing time for performance statistics.

    Frame frame;
    int32_t received = socket->read(frame.data(), ETH_MAX_SIZE);

    auto t1 = since_epoch();

    fmu_mutex.lock(); // Lock MUTEX here if needed to safely read fmu_output while it's being updated by FmuThread
    while (true)
    {
        auto [header, data, wkc] = frame.peekDatagram();
        if (header == nullptr)
        {
            break;
        }

        for (auto &esc : escs)
        {
            esc->processDatagram(header, data, wkc);
        }
    }

    for (size_t i = 0; i < slaves.size(); ++i)
    {
        slaves[i]->routine();
        if (slaves[i]->state() == State::SAFE_OP)
        {
            if (output_pdo[i][1] != 0xFF)
            {
                slaves[i]->validateOutputData();
            }
        }
    }

    fmu_mutex.unlock(); // Unlock MUTEX here if it was locked before to allow FmuThread to update fmu_output again
    int32_t written = socket->write(frame.data(), received);

    auto t2 = since_epoch();

    stats.push_back(t2 - t1);
    if (stats.size() >= 1000)
    {
        std::sort(stats.begin(), stats.end());

        printf("[%f] frame processing time: \n\t min: %f\n\t max: %f\n\t avg: %f\n", seconds_f(since_start()).count(),
               stats.front().count() / 1000.0,
               stats.back().count() / 1000.0,
               (std::reduce(stats.begin(), stats.end()) / stats.size()).count() / 1000.0);
        stats.clear();
    }
}

void LoadMapping(std::shared_ptr<const fmi4cpp::fmi2::cs_model_description> cs_md, size_t slave_index, kickcat::CoE::Dictionary &dict, json &out_map, std::vector<EtherDOG::Mapping> &mappings)
{
    // This function loads the mapping between FMU variables and EtherCAT PDOs from the provided JSON configuration and the CoE dictionary, and stores it in the provided mappings vector.

    for (auto i = out_map.begin(); i != out_map.end(); ++i)
    {
        std::cout << "Key: " << i.key() << ", Value: " << i.value() << std::endl;

        std::string fmu_var = i.key();
        std::string pdo_name = i.value().get<std::string>();

        auto var = cs_md->get_variable_by_name(fmu_var).as_real();
        auto vr = var.valueReference();

        bool mapping_found = false;

        for (auto &object : dict)
        {
            for (auto &entry : object.entries)
            {
                if (entry.description == pdo_name)
                {
                    EtherDOG::Mapping m{
                        vr,
                        (size_t)(entry.bitlen / 8),
                        pdo_name,
                        slave_index,
                        fmu_var,
                        entry,
                    };

                    mappings.push_back(m);
                    mapping_found = true;
                    break;
                }
            }

            if (mapping_found)
            {
                break;
            }
        }

        if (!mapping_found)
        {
            std::cerr << "PDO entry not found: " << pdo_name << std::endl;
            throw std::runtime_error("PDO entry not found: " + pdo_name);
        }
    }
}

void EtherDOG::SetupMappingFile()
{
    auto &slaves_config = main_config["slaves"];

    for (size_t i = 0; i < slaves_config.size(); ++i)
    {
        const auto &slave_config = slaves_config[i];

        if (i >= mailboxes.size())
        {
            std::cerr << "No mailbox found for slave " << i << std::endl;
            continue;
        }

        auto &dict = mailboxes[i]->getDictionary();

        if (slave_config.contains("input-mappings"))
        {
            auto input_map = slave_config["input-mappings"];
            LoadMapping(cs_md, i, dict, input_map, input_mappings);
        }

        if (slave_config.contains("output-mappings"))
        {
            auto output_map = slave_config["output-mappings"];
            LoadMapping(cs_md, i, dict, output_map, output_mappings);
        }
    }
}

void EtherDOG::ExecutePdoInputMappings()
{
    //  This function can be called in Step() after stepping the FMU to update the PDO memory with the latest values from the FMU variables based on the mappings set up in SetupMapping.

    fmu_mutex.lock(); // Lock MUTEX here if needed to safely read FMU variable while it's being updated by FmuThread
    for (auto &m : input_mappings)
    {
        double fmu_output;

        if (!fmu_slave->read_real(m.vr, fmu_output))
        {
            std::cerr << "Error reading FMU variable with VR " << m.vr << ": " << to_string(fmu_slave->last_status()) << std::endl;
            continue;
        }

        if (m.entry.is_mapped)
        {
            std::memcpy(m.entry.data, &fmu_output, m.size); // Copy bytes from FMU variable into PDO memory
            std::cout << "[Slave index " << m.SlaveIndex << "] " << "Mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "' value= " << fmu_output << std::endl;
        }
        else
        {
            std::cerr << "[Slave index " << m.SlaveIndex << "] " << "Warning: CoE entry for PDO '" << m.PDOname << "' is not mapped. Skipping mapping for this entry." << std::endl;
        }
    }
    fmu_mutex.unlock(); // Unlock MUTEX here if it was locked before to allow FmuThread to update FMU variable again
}

void EtherDOG::ExecutePdoOutputMappings()
{
    // This function can be called in Step() before stepping the FMU to read the latest values from the PDO memory based on the mappings set up in SetupMapping and write them to the corresponding FMU variables.

    fmu_mutex.lock(); // Lock MUTEX here if needed to safely read FMU variable while it's being updated by FmuThread
    for (auto &m : output_mappings)
    {
        double fmu_input;

        if (m.entry.is_mapped)
        {
            std::memcpy(&fmu_input, m.entry.data, m.size); // Copy PDO bytes into local variable
        }
        else
        {
            std::cerr << "[Slave index " << m.SlaveIndex << "] " << "Warning: CoE entry for PDO '" << m.PDOname << "' is not mapped. Skipping mapping for this entry." << std::endl;
        }

        if (!fmu_slave->write_real(m.vr, fmu_input))
        {
            std::cerr << "Error writing FMU variable with VR " << m.vr << ": " << to_string(fmu_slave->last_status()) << std::endl;
            continue;
        }

        std::cout << "[Slave index " << m.SlaveIndex << "] " << "Mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "' value= " << fmu_input << std::endl;
    }
    fmu_mutex.unlock(); // Unlock MUTEX here if it was locked before to allow FmuThread to update FMU variable again
}

void EtherDOG::loadFMU(const std::string &path)
{
    // This function loads the FMU from the specified path and initializes the cs_fmu, cs_md, and fmu_slave members.

    std::cout << "Loading FMU from path: " << path << std::endl;
    fmi2::fmu fmu(path);
    cs_fmu = fmu.as_cs_fmu();
    cs_md = cs_fmu->get_model_description(); // smart pointer to a cs_model_description instance
    std::cout << "model_identifier=" << cs_md->model_identifier << std::endl;
    fmu_slave = cs_fmu->new_instance();
}

void EtherDOG::start()
{
    // This function can be used to perform any necessary initialization before starting the simulation, such as setting up the FMU experiment and entering initialization mode.

    std::cout << "Starting simulation..." << std::endl;
    fmu_slave->setup_experiment();
    fmu_slave->enter_initialization_mode();
    fmu_slave->exit_initialization_mode();
}

void EtherDOG::run()
{
    while (true)
    {
        FrameHandler();
    }
}

void EtherDOG::FmuThread()
{
    while (true)
    {
        step();
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(stepSize * 1000)));
    }
}

void EtherDOG::step()
{
    // This function performs a single simulation step, including executing the PDO output mappings to update the FMU variables with the latest values from the PDO memory,
    // stepping the FMU, and then executing the PDO input mappings to update the PDO memory with the latest values from the FMU variables.

    ExecutePdoOutputMappings(); // Call ExecutePdoOutputMappings to read the latest values from the PDO memory based on the mappings set up in SetupMapping and write them to the corresponding FMU variables.

    if (!fmu_slave->step(stepSize))
    {
        std::cerr << "Error! step() returned with status: " << to_string(fmu_slave->last_status()) << std::endl;
        return;
    }
    t = fmu_slave->get_simulation_time();
    std::cout << "t=" << t << std::endl;

    ExecutePdoInputMappings(); // Call ExecutePdoInputMappings to update the PDO memory with the latest values from the FMU variables based on the mappings set up in SetupMapping.
}

void EtherDOG::stop()
{
    fmu_slave->terminate();
}
