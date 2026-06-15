
#include "etherdog.hpp"
#include "PdoFmuConverter.hpp"
#include <thread>
#include <spdlog/spdlog.h>

static kickcat::eeprom::SII ReadEepromSII(std::filesystem::path const &eeprom_path)
{
    std::ifstream eeprom_file(eeprom_path, std::ios::binary | std::ios::ate);
    if (!eeprom_file.is_open())
    {
        throw std::runtime_error("Failed to open EEPROM file: " + eeprom_path.string());
    }

    auto size = eeprom_file.tellg();
    eeprom_file.seekg(0, std::ios::beg);

    std::vector<uint8_t> eeprom_data(static_cast<size_t>(size));
    eeprom_file.read(reinterpret_cast<char *>(eeprom_data.data()), size);

    kickcat::eeprom::SII sii;
    sii.parse(eeprom_data.data(), eeprom_data.size());

    return sii;
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

    fs::path main_config_path = fs::path(config_file);
    fs::path config_dir = main_config_path.parent_path();

    escs.reserve(slave_count);
    pdos.reserve(slave_count);
    slaves.reserve(slave_count);
    mailboxes.reserve(slave_count);
    dictionaries.reserve(slave_count);
    slave_dictionaries.reserve(slave_count);
    input_pdo.reserve(slave_count);
    output_pdo.reserve(slave_count);

    constexpr uint32_t PDO_MAX_SIZE = 4096;
    kickcat::ESI::Parser parser;

    for (const auto &config : slaves_config)
    {

        fs::path eeprom_full_path = config_dir / config["eeprom"].get<std::string>();

        auto esc = std::make_unique<EmulatedESC>(eeprom_full_path.string().c_str());
        auto pdo = std::make_unique<PDO>(esc.get());
        auto slave = std::make_unique<Slave>(esc.get(), pdo.get());

        if (config.contains("coe_xml"))
        {
            fs::path coe_xml_full_path = config_dir / config["coe_xml"].get<std::string>();

            auto mbx = std::make_unique<mailbox::response::Mailbox>(esc.get(), 1024);

            auto sii = ReadEepromSII(eeprom_full_path);

            uint32_t vendor_id = sii.info.vendor_id;
            uint32_t product_code = sii.info.product_code;
            uint32_t revision_number = sii.info.revision_number;

            spdlog::info("EEPROM info for slave {}): vendor_id=0x{:X}, product_code=0x{:X}, revision_number=0x{:X}", slaves.size(), vendor_id, product_code, revision_number);

            ESI::DeviceFilter filter;
            filter.product_code = product_code;
            filter.revision_no = revision_number;

            ESI::Device device = parser.loadDevice(coe_xml_full_path.string(), filter);

            spdlog::info("Found matching device in ESI XML");
            spdlog::info(": vendor_id=0x{:X}, product_code=0x{:X}, revision_no=0x{:X}", device.vendor_id, device.product_code, device.revision_no);

            if (device.vendor_id != vendor_id)
            {
                throw std::runtime_error("ESI vendor ID does not match EEPROM vendor ID");
            }

            if (device.product_code != product_code)
            {
                throw std::runtime_error("ESI product code does not match EEPROM product code");
            }

            if (device.revision_no != revision_number)
            {
                throw std::runtime_error("ESI revision number does not match EEPROM revision number");
            }

            CoE::materializeStorage(device.dictionary);

            dictionaries.push_back(std::make_unique<CoE::Dictionary>(std::move(device.dictionary)));

            CoE::Dictionary *dictionary = dictionaries.back().get();

            slave->setDictionary(dictionary);

            mbx->enableCoE(*dictionary);
            slave->setMailbox(mbx.get());

            mailboxes.push_back(std::move(mbx));
            slave_dictionaries.push_back(dictionary);
        }
        else
        {
            slave_dictionaries.push_back(nullptr);
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

    std::vector<EmulatedESC *> esc_ptrs;
    esc_ptrs.reserve(escs.size());

    for (auto &esc : escs)
    {
        esc_ptrs.push_back(esc.get());
    }

    network = std::make_unique<EmulatedNetwork>(std::move(esc_ptrs));

    auto [socket2, _] = createSockets(interface, "");
    socket = std::move(socket2);
    socket->setTimeout(1ns);

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

    if (received <= 0)
    {
        return;
    }

    if (!network->route(frame, false))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(fmu_mutex);

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
    }

    socket->write(frame.data(), received);
}

void LoadMapping(std::shared_ptr<const fmi4cpp::fmi2::cs_model_description> cs_md, size_t slave_index, kickcat::CoE::Dictionary &dict, json &out_map, std::vector<EtherDOG::Mapping> &mappings)
{
    // This function loads the mapping between FMU variables and EtherCAT PDOs from the provided JSON configuration and the CoE dictionary, and stores it in the provided mappings vector.

    for (auto &object : dict)
    {
        std::cerr << "\n===== DICTIONARY DUMP slave " << slave_index << " =====\n";
        std::cerr << "dict size = " << dict.size() << "\n";
        std::cout << "Object 0x" << std::hex << object.index
                  << std::dec << " name='" << object.name << "'\n";

        for (auto &entry : object.entries)
        {
            std::cout << "  sub=" << (int)entry.subindex
                      << " desc='" << entry.description << "'"
                      << " bitlen=" << entry.bitlen
                      << " bitoff=" << entry.bitoff
                      << "\n";
        }
    }

    for (auto i = out_map.begin(); i != out_map.end(); ++i)
    {
        std::cout << "Key: " << i.key() << ", Value: " << i.value() << std::endl;

        std::string fmu_var = i.key();
        std::string pdo_name = i.value().get<std::string>();

        auto variable = cs_md->get_variable_by_name(fmu_var);
        fmi2ValueReference vr;
        EtherDOG::FmuVariableType fmuVarType;

        if (variable.is_real())
        {
            auto var = variable.as_real();
            vr = variable.value_reference;
            fmuVarType = EtherDOG::FmuVariableType::REAL;
        }
        else if (variable.is_integer())
        {
            auto var = variable.as_integer();
            vr = variable.value_reference;
            fmuVarType = EtherDOG::FmuVariableType::INTEGER32;
        }
        else if (variable.is_boolean())
        {
            auto var = variable.as_boolean();
            vr = variable.value_reference;
            fmuVarType = EtherDOG::FmuVariableType::BOOLEAN;
        }
        else
        {
            std::cerr << "Unsupported FMU variable type for variable '" << fmu_var << "'. Skipping mapping for this variable." << std::endl;
            continue; // Skip unsupported variable types
        }

        bool mapping_found = false;

        std::string ChannelName;
        std::string EntryName = pdo_name;

        auto slash = pdo_name.find('/');
        if (slash != std::string::npos)
        {
            ChannelName = pdo_name.substr(0, slash);
            EntryName = pdo_name.substr(slash + 1);
        }

        for (auto &object : dict)
        {
            if (!ChannelName.empty() && object.name != ChannelName)
            {
                continue;
            }

            for (auto &entry : object.entries)
            {
                if (entry.description == EntryName)
                {
                    std::cout
                        << "[DEBUG] JSON PDO name '" << pdo_name
                        << "' resolved to object 0x"
                        << std::hex << object.index
                        << ":" << std::dec << int(entry.subindex)
                        << " object_name='" << object.name << "'"
                        << " entry_desc='" << entry.description << "'"
                        << " bitlen=" << entry.bitlen
                        << " is_mapped=" << entry.is_mapped
                        << std::endl;

                    EtherDOG::Mapping m{
                        vr,
                        (size_t)((entry.bitlen + 7) / 8),
                        pdo_name,
                        slave_index,
                        fmu_var,
                        fmuVarType,
                        &entry,
                        false};

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

        if (i >= slave_dictionaries.size() || slave_dictionaries[i] == nullptr)
        {
            std::cerr << "No dictionary found for slave " << i << std::endl;
            continue;
        }

        auto &dict = *slave_dictionaries[i];
        printf("EtherDOG dict address = %p\n", (void *)&dict);

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
        if (!m.entry->is_mapped)
        {
            if (!m.MessagePrinted) // Print the warning message only once for each unmapped entry to avoid spamming the console
            {
                spdlog::warn("[Slave index {}] Warning: CoE entry for PDO '{}' is not mapped. Skipping mapping for this entry.", m.SlaveIndex, m.PDOname);
                m.MessagePrinted = true; // Set the flag to true after printing the warning message
            }
        }
        else
        {
            switch (m.fmuVarType)
            {
            case FmuVariableType::REAL:
            {
                double fmu_output;
                if (!fmu_slave->read_real(m.vr, fmu_output))
                {
                    spdlog::error("[Slave index {}] Error reading FMU variable with VR {}: {}", m.SlaveIndex, m.vr, to_string(fmu_slave->last_status()));
                    continue;
                }
                WriteFmuDoubleToPdo(m, fmu_output);
                spdlog::info("[Slave index {}] Mapping FMU variable '{}' to PDO variable '{}' value= {}", m.SlaveIndex, m.FMUname, m.PDOname, fmu_output);
                break;
            }

            case FmuVariableType::INTEGER32:
            {
                int32_t fmu_output;
                if (!fmu_slave->read_integer(m.vr, fmu_output))
                {
                    spdlog::error("[Slave index {}] Error reading FMU variable with VR {}: {}", m.SlaveIndex, m.vr, to_string(fmu_slave->last_status()));
                    continue;
                }
                WriteFmuIntToPdo(m, fmu_output);
                spdlog::info("[Slave index {}] Mapping FMU variable '{}' to PDO variable '{}' value= {}", m.SlaveIndex, m.FMUname, m.PDOname, fmu_output);
                break;
            }

            case FmuVariableType::BOOLEAN:
            {
                fmi2Boolean fmu_output;
                if (!fmu_slave->read_boolean(m.vr, fmu_output))
                {
                    spdlog::error("[Slave index {}] Error reading FMU variable with VR {}: {}", m.SlaveIndex, m.vr, to_string(fmu_slave->last_status()));
                    continue;
                }
                WriteFmuBoolToPdo(m, fmu_output);
                spdlog::info("[Slave index {}] Mapping FMU variable '{}' to PDO variable '{}' value= {}", m.SlaveIndex, m.FMUname, m.PDOname, fmu_output);
                break;
            }

            default:
            {
                spdlog::warn("[Slave index {}] Unsupported FMU variable type for variable '{}'. Skipping writing to PDO for this variable.", m.SlaveIndex, m.FMUname);
                continue; // Skip unsupported variable types
            }
            }
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
        if (!m.entry->is_mapped)
        {
            if (!m.MessagePrinted)
            {
                spdlog::warn("[Slave index {}] Warning: CoE entry for PDO '{}' is not mapped. Skipping mapping for this entry.", m.SlaveIndex, m.PDOname);
                m.MessagePrinted = true; // Set the flag to true after printing the warning message
            }
        }
        else
        {
            switch (m.fmuVarType)
            {
            case FmuVariableType::REAL:
            {
                double fmu_input = ReadPdoToFmuDouble(m);
                if (!fmu_slave->write_real(m.vr, fmu_input))
                {
                    spdlog::error("[Slave index {}] Error writing FMU variable with VR {}: {}", m.SlaveIndex, m.vr, to_string(fmu_slave->last_status()));
                    continue;
                }
                spdlog::info("[Slave index {}] Mapping PDO variable '{}' to FMU variable '{}' value= {}", m.SlaveIndex, m.PDOname, m.FMUname, fmu_input);
                break;
            }

            case FmuVariableType::INTEGER32:
            {
                int32_t fmu_input = ReadPdoToFmuInt(m);
                if (!fmu_slave->write_integer(m.vr, fmu_input))
                {
                    spdlog::error("[Slave index {}] Error writing FMU variable with VR {}: {}", m.SlaveIndex, m.vr, to_string(fmu_slave->last_status()));
                    continue;
                }
                spdlog::info("[Slave index {}] Mapping PDO variable '{}' to FMU variable '{}' value= {}", m.SlaveIndex, m.PDOname, m.FMUname, fmu_input);
                break;
            }

            case FmuVariableType::BOOLEAN:
            {
                fmi2Boolean fmu_input = ReadPdoToFmuBool(m);
                if (!fmu_slave->write_boolean(m.vr, fmu_input))
                {
                    spdlog::error("[Slave index {}] Error writing FMU variable with VR {}: {}", m.SlaveIndex, m.vr, to_string(fmu_slave->last_status()));
                    continue;
                }
                spdlog::info("[Slave index {}] Mapping PDO variable '{}' to FMU variable '{}' value= {}", m.SlaveIndex, m.PDOname, m.FMUname, fmu_input);
                break;
            }
            default:
            {
                spdlog::warn("[Slave index {}] Unsupported FMU variable type for variable '{}'. Skipping reading from PDO for this variable.", m.SlaveIndex, m.FMUname);
                continue; // Skip unsupported variable types
            }
            }
        }
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
    while (running)
    {
        FrameHandler();
    }
}

void EtherDOG::FmuThread()
{
    while (running)
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
    spdlog::info("t={} FMU Simulation is running", t);

    ExecutePdoInputMappings(); // Call ExecutePdoInputMappings to update the PDO memory with the latest values from the FMU variables based on the mappings set up in SetupMapping.
}

void EtherDOG::requestStop()
{
    // This function can be called to request stopping the simulation by setting the running flag to false, which will cause the main loop in run() and FmuThread() to exit gracefully.

    running = false;
}

void EtherDOG::stop()
{
    fmu_slave->terminate();
}