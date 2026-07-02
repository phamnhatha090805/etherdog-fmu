
#include "etherdog.hpp"
#include "PdoFmuConverter.hpp"
#include <spdlog/spdlog.h>
#include <thread>

enum class PdoDirection
{
    Input,  // TxPDO: slave -> master, assigned PDOs 0x1A00-0x1BFF, lives in the input PI
    Output, // RxPDO: master -> slave, assigned PDOs 0x1600-0x17FF, lives in the output PI
};

// One resolved process-data field. data_entry is the object the slave actually maps
// (its is_mapped flips true at SAFE_OP); pi_bit_offset/bit_len locate the field in
// the SM image (the buffer passed to PDO::setInput / PDO::setOutput).
struct PdoBinding
{
    kickcat::CoE::Entry *data_entry = nullptr;
    std::string channel_name; // mapping object name, e.g. "Channel 1"
    std::string entry_name;   // mapping entry description, e.g. "Input"
    uint16_t data_index = 0;  // e.g. 0x3101
    uint8_t data_sub = 0;     // e.g. 1
    uint32_t pi_bit_offset = 0;
    uint16_t bit_len = 0;
    PdoDirection direction = PdoDirection::Input;
};

bool isTxPdoIndex(uint16_t pdo_index)
{
    return (pdo_index >= 0x1A00 and pdo_index <= 0x1BFF);
}

// Replays PDO::configureMapping / parsePdoMap over the dictionary: walk every SM
// assignment object (0x1C10 + SM index), then each assigned PDO map, accumulating
// the bit offset exactly like the slave does (it restarts at 0 per direction). For
// every mapped field, follow the mapping word to its data object and record the
// binding. Call AFTER the dictionary is attached to the slave; the data_entry
// pointers stay valid as long as the dictionary is not reallocated.
std::vector<PdoBinding> enumeratePdoBindings(kickcat::CoE::Dictionary &dict)
{
    using namespace kickcat;
    std::vector<PdoBinding> bindings;

    uint32_t input_bit_offset = 0;
    uint32_t output_bit_offset = 0;

    for (uint16_t assign = 0x1C10; assign <= 0x1C1F; ++assign)
    {
        auto [assign_obj, assign_count] = CoE::findObject(dict, assign, 0);
        if (assign_count == nullptr or assign_count->data == nullptr)
        {
            continue;
        }
        uint8_t pdo_count = *static_cast<uint8_t *>(assign_count->data);

        for (uint8_t i = 1; i <= pdo_count; ++i)
        {
            auto [assign_entry_obj, assign_entry] = CoE::findObject(dict, assign, i);
            if (assign_entry == nullptr or assign_entry->data == nullptr)
            {
                continue;
            }
            uint16_t pdo_index = *static_cast<uint16_t *>(assign_entry->data);

            PdoDirection dir = PdoDirection::Output;
            if (isTxPdoIndex(pdo_index))
            {
                dir = PdoDirection::Input;
            }

            uint32_t *bit_offset = &output_bit_offset;
            if (dir == PdoDirection::Input)
            {
                bit_offset = &input_bit_offset;
            }

            auto [pdo_obj, pdo_count_entry] = CoE::findObject(dict, pdo_index, 0);
            if (pdo_count_entry == nullptr or pdo_count_entry->data == nullptr)
            {
                continue;
            }
            uint8_t entry_count = *static_cast<uint8_t *>(pdo_count_entry->data);

            for (uint8_t s = 1; s <= entry_count; ++s)
            {
                auto [map_obj, map_entry] = CoE::findObject(dict, pdo_index, s);
                if (map_entry == nullptr or map_entry->data == nullptr)
                {
                    continue;
                }
                uint32_t word = *static_cast<uint32_t *>(map_entry->data);
                uint16_t tgt_index = static_cast<uint16_t>((word & CoE::PDO::MAPPING_INDEX_MASK) >> CoE::PDO::MAPPING_INDEX_SHIFT);
                uint8_t tgt_sub = static_cast<uint8_t>((word & CoE::PDO::MAPPING_SUB_MASK) >> CoE::PDO::MAPPING_SUB_SHIFT);
                uint16_t bits = static_cast<uint16_t>(word & CoE::PDO::MAPPING_LENGTH_MASK);

                // ETG.1000.6 Tables 74/75: index 0 is a padding gap, no data object.
                if (tgt_index == 0)
                {
                    *bit_offset += bits;
                    continue;
                }

                auto [tgt_obj, tgt_entry] = CoE::findObject(dict, tgt_index, tgt_sub);

                PdoBinding b;
                b.data_entry = tgt_entry;
                b.data_index = tgt_index;
                b.data_sub = tgt_sub;
                b.pi_bit_offset = *bit_offset;
                b.bit_len = bits;
                b.direction = dir;
                if (pdo_obj != nullptr)
                {
                    b.channel_name = pdo_obj->name;
                }
                b.entry_name = tgt_entry->description;
                bindings.push_back(std::move(b));

                *bit_offset += bits;
            }
        }
    }

    return bindings;
}

// Resolve a JSON PDO name ("Channel 1/Input" or just "Input") against the bindings.
// Matching uses the human-readable mapping-object metadata, but the returned binding
// points at the DATA object -- this is the is_mapped fix.
PdoBinding const &resolvePdoBinding(std::vector<PdoBinding> const &bindings, std::string const &pdo_name)
{
    std::string channel;
    std::string entry = pdo_name;
    auto slash = pdo_name.find('/');
    if (slash != std::string::npos)
    {
        channel = pdo_name.substr(0, slash);
        entry = pdo_name.substr(slash + 1);
    }

    for (auto const &b : bindings)
    {
        if (not channel.empty() and b.channel_name != channel)
        {
            continue;
        }
        if (b.entry_name == entry)
        {
            return b;
        }
    }

    throw std::runtime_error("PDO entry not found: " + pdo_name);
}

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

int EtherDOG::StartNetworks(const fs::path &config_dir, const nlohmann::json &main_config)
{
    // This function can be used to set up the EtherCAT network simulation, including parsing command-line arguments, initializing the slaves and PDOs, and starting the network communication.

    std::string interface = main_config["interface"];

    auto &slaves_config = main_config["slaves"];
    size_t slave_count = slaves_config.size();

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

            bool esi_coe_advertised = false; // true => device declares a CoE mailbox (SDO on the wire)
            if (not device.dictionary.empty())
            {
                esi_coe_advertised = (device.mailbox and device.mailbox->coe);
            }

            slave->setDictionary(dictionary);

            if (esi_coe_advertised)
            {
                auto mbx = std::make_unique<mailbox::response::Mailbox>(esc.get(), 1024);
                mbx->enableCoE(*dictionary);
                slave->setMailbox(mbx.get());
                mailboxes.push_back(std::move(mbx));
            }
            slave_dictionaries.push_back(dictionary);
        }
        else
        {
            throw std::runtime_error("Slave configuration does not contain 'coe_xml' field");
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
        // spdlog::warn("No data received or error occurred while reading from socket. Received: {}", received);
        return;
    }

    if (!network->route(frame, false))
    {
        return;
    }

    fmu_mutex.lock(); // Lock MUTEX here if needed to safely read fmu_output while it's being updated by FmuThread
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
}

void LoadMapping(std::shared_ptr<const fmi4cpp::fmi2::cs_model_description> cs_md, size_t slave_index, kickcat::CoE::Dictionary &dict, json &out_map, std::vector<EtherDOG::Mapping> &mappings,
                 const bool verbose)
{
    // This function loads the mapping between FMU variables and EtherCAT PDOs from the provided JSON configuration and the CoE dictionary, and stores it in the provided mappings vector.

    // is_mapped fix: resolve every PDO name to the DATA object (whose is_mapped flips
    // true at SAFE_OP), not the PDO mapping object. One walk replays the slave-side
    // mapping so we also learn each field's absolute bit offset in the SM image.
    auto bindings = enumeratePdoBindings(dict);

    if (verbose)
    {
        std::cerr << "\n===== DICTIONARY DUMP slave " << slave_index << " =====\n";
        std::cerr << "dict size = " << dict.size() << "\n";
        for (auto &object : dict)
        {
            std::cout << "Object 0x" << std::hex << object.index << std::dec << " name='" << object.name << "'\n";

            for (auto &entry : object.entries)
            {
                std::cout << "  sub=" << (int)entry.subindex << " desc='" << entry.description << "'"
                          << " bitlen=" << entry.bitlen << " bitoff=" << entry.bitoff << "\n";
            }
        }

        // Print bindings for debugging
        std::cerr << "\n===== BINDINGS DUMP slave " << slave_index << " =====\n";
        for (const auto &b : bindings)
        {
            std::cout << "Binding: channel='" << b.channel_name << "' entry='" << b.entry_name << "' index=0x" << std::hex << b.data_index << std::dec << " sub=" << (int)b.data_sub
                      << " pi_bit_offset=" << b.pi_bit_offset << " bit_len=" << b.bit_len << " is_mapped=" << (b.data_entry ? b.data_entry->is_mapped : false) << "\n";
        }
    }

    for (auto i = out_map.begin(); i != out_map.end(); ++i)
    {
        std::string fmu_var = i.key();
        std::string pdo_name = i.value().get<std::string>();
        spdlog::info("Key: {}, Value: {}", fmu_var, pdo_name);

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

        // Bind to the DATA object (is_mapped lives here) with the real PI bit offset.
        // resolvePdoBinding throws if the name is unknown, matching the old behaviour.
        PdoBinding const &b = resolvePdoBinding(bindings, pdo_name);

        std::cout << "[DEBUG] JSON PDO name '" << pdo_name << "' resolved to DATA object 0x" << std::hex << b.data_index << ":" << std::dec << int(b.data_sub) << " channel='" << b.channel_name << "'"
                  << " entry='" << b.entry_name << "'"
                  << " pi_bit_offset=" << b.pi_bit_offset << " bitlen=" << b.bit_len << " is_mapped=" << (b.data_entry ? b.data_entry->is_mapped : false) << std::endl;

        if (b.bit_len % 8 != 0)
        {
            throw std::runtime_error("PDO entry bit length is not a multiple of 8 for entry '" + b.entry_name + "'.");
        }

        EtherDOG::Mapping m{vr, (size_t)((b.bit_len + 7) / 8), pdo_name, slave_index, fmu_var, fmuVarType, b.data_entry, false};

        m.PiBitOffset = b.pi_bit_offset;
        m.BitLen = b.bit_len;
        mappings.push_back(m);
    }
}

void EtherDOG::SetupMappingFile(const nlohmann::json &main_config, const bool verbose)
{
    auto &slaves_config = main_config["slaves"];

    for (size_t i = 0; i < slaves_config.size(); ++i)
    {
        const auto &slave_config = slaves_config[i];

        if (i >= slave_dictionaries.size())
        {
            std::cerr << "No dictionary found for slave " << i << std::endl;
            continue;
        }

        auto &dict = *slave_dictionaries[i];

        if (slave_config.contains("input-mappings"))
        {
            auto input_map = slave_config["input-mappings"];
            LoadMapping(cs_md, i, dict, input_map, input_mappings, verbose);
        }

        if (slave_config.contains("output-mappings"))
        {
            auto output_map = slave_config["output-mappings"];
            LoadMapping(cs_md, i, dict, output_map, output_mappings, verbose);
        }
    }

    // set process image for each mapping:
    for (auto &m : input_mappings)
    {
        m.input_process_image = input_pdo[m.SlaveIndex].data();
    }

    // set process image for each mapping:
    for (auto &m : output_mappings)
    {
        m.output_process_image = output_pdo[m.SlaveIndex].data();
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
    // This function can be called in Step() before stepping the FMU to read the latest values from the PDO memory based on the mappings set up in SetupMapping and write them to the corresponding FMU
    // variables.

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

void EtherDOG::loadFMU(const fs::path &path)
{
    // This function loads the FMU from the specified path and initializes the cs_fmu, cs_md, and fmu_slave members.

    spdlog::info("Loading FMU from path: {}", path.string());
    fmi2::fmu fmu(path);
    cs_fmu = fmu.as_cs_fmu();
    cs_md = cs_fmu->get_model_description(); // smart pointer to a cs_model_description instance
    spdlog::info("model_identifier={}", cs_md->model_identifier);
    fmu_slave = cs_fmu->new_instance();
}

void EtherDOG::start()
{
    // This function can be used to perform any necessary initialization before starting the simulation, such as setting up the FMU experiment and entering initialization mode.

    spdlog::info("Starting simulation...");
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

    ExecutePdoOutputMappings(); // Call ExecutePdoOutputMappings to read the latest values from the PDO memory based on the mappings set up in SetupMapping and write them to the corresponding FMU
                                // variables.

    if (!fmu_slave->step(stepSize))
    {
        spdlog::error("Error! step() returned with status: {}", to_string(fmu_slave->last_status()));
        return;
    }
    t = fmu_slave->get_simulation_time();
    spdlog::info("t = {} FMU simulation is running", t);

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