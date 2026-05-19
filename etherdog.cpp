
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
                        fmuVarType,
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

void WriteFmuDoubleToPdo(const EtherDOG::Mapping &m, double fmu_value)
{
    // This function is used to write a double value from the FMU variable to the PDO memory based on the mapping. It handles the necessary type conversion and byte copying based on the data type of the CoE entry.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        double value_to_write = fmu_value; // Assuming fmu_value is already a double
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        float value_to_write = static_cast<float>(fmu_value); // Convert double to float
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        bool value_to_write = (fmu_value != 0); // Convert int32_t to bool (non-zero is true)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        int8_t value_to_write = static_cast<int8_t>(fmu_value); // Convert double to int8_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        uint8_t value_to_write = static_cast<uint8_t>(fmu_value); // Convert double to uint8_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER16: // Also INT
    {
        // INTEGER16 type
        int16_t value_to_write = static_cast<int16_t>(fmu_value); // Convert double to int16_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        uint16_t value_to_write = static_cast<uint16_t>(fmu_value); // Convert double to uint16_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        int32_t value_to_write = static_cast<int32_t>(fmu_value); // Convert double to int32_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        uint32_t value_to_write = static_cast<uint32_t>(fmu_value); // Convert double to uint32_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping FMU variable '" + m.FMUname + "' to PDO variable '" + m.PDOname + "'.");
    }
    }
}

void WriteFmuIntToPdo(const EtherDOG::Mapping &m, int32_t fmu_value)
{
    // This function can be used to write an integer value from the FMU variable to the PDO memory based on the mapping. It should handle the necessary type conversion and byte copying based on the data type of the CoE entry.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        double value_to_write = static_cast<double>(fmu_value); // Convert int32_t to double
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        float value_to_write = static_cast<float>(fmu_value); // Convert int32_t to float
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        bool value_to_write = (fmu_value != 0); // Convert int32_t to bool (non-zero is true)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        int8_t value_to_write = static_cast<int8_t>(fmu_value); // Convert int32_t to int8_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        uint8_t value_to_write = static_cast<uint8_t>(fmu_value); // Convert int32_t to uint8_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER16: // Also INT
    {
        // INTEGER16 type
        int16_t value_to_write = static_cast<int16_t>(fmu_value); // Convert int32_t to int16_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        uint16_t value_to_write = static_cast<uint16_t>(fmu_value); // Convert int32_t to uint16_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        int32_t value_to_write = fmu_value; // Assuming fmu_value is already an int32_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        uint32_t value_to_write = static_cast<uint32_t>(fmu_value); // Convert int32_t to uint32_t
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping FMU variable '" + m.FMUname + "' to PDO variable '" + m.PDOname + "'.");
    }
    }
}

void WriteFmuBoolToPdo(const EtherDOG::Mapping &m, fmi2Boolean fmu_value)
{
    // This function can be used to write a boolean value from the FMU variable to the PDO memory based on the mapping. It should handle the necessary type conversion and byte copying based on the data type of the CoE entry.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        double value_to_write = fmu_value ? 1.0 : 0.0; // Convert bool to double (true=1.0, false=0.0)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        float value_to_write = fmu_value ? 1.0f : 0.0f; // Convert bool to float (true=1.0f, false=0.0f)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        bool value_to_write = fmu_value; // Assuming fmu_value is already a fmi2Boolean
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        int8_t value_to_write = fmu_value ? 1 : 0; // Convert bool to int8_t (true=1, false=0)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        uint8_t value_to_write = fmu_value ? 1 : 0; // Convert bool to uint8_t (true=1, false=0)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER16: // Also INT
    {
        // INTEGER16 type
        int16_t value_to_write = fmu_value ? 1 : 0; // Convert bool to int16_t (true=1, false=0)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        uint16_t value_to_write = fmu_value ? 1 : 0; // Convert bool to uint16_t (true=1, false=0)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        int32_t value_to_write = fmu_value ? 1 : 0; // Convert bool to int32_t (true=1, false=0)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        uint32_t value_to_write = fmu_value ? 1 : 0; // Convert bool to uint32_t (true=1, false=0)
        std::memcpy(m.entry.data, &value_to_write, sizeof(value_to_write));
        break;
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping FMU variable '" + m.FMUname + "' to PDO variable '" + m.PDOname + "'.");
    }
    }
}

double ReadPdoToFmuDouble(const EtherDOG::Mapping &m)
{
    // This function reads the value from the PDO memory based on the mapping and converts it to double to be written to the FMU variable.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        double value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return value;
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        float value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<double>(value); // Convert float to double
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        bool value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return value ? 1.0 : 0.0; // Convert bool to double (true=1.0, false=0.0)
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        int8_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<double>(value); // Convert int8_t to double
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        uint8_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<double>(value); // Convert uint8_t to double
    }

    case DataType::INTEGER16:
    {
        // INTEGER16 type
        int16_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<double>(value); // Convert int16_t to double
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        uint16_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<double>(value); // Convert uint16_t to double
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        int32_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<double>(value); // Convert int32_t to double
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        uint32_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<double>(value); // Convert uint32_t to double
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping PDO variable '" + m.PDOname + "' to FMU variable '" + m.FMUname + "'.");
        return 0.0;
    }
    }
}

int32_t ReadPdoToFmuInt(const EtherDOG::Mapping &m)
{
    // This function can be used to read the value from the PDO memory based on the mapping and convert it to int32_t to be written to the FMU variable.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        double value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<int32_t>(value); // Convert double to int32_t
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        float value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<int32_t>(value); // Convert float to int32_t
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        bool value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return value ? 1 : 0; // Convert bool to int32_t (true=1, false=0)
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        int8_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<int32_t>(value); // Convert int8_t to int32_t
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        uint8_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<int32_t>(value); // Convert uint8_t to int32_t
    }

    case DataType::INTEGER16:
    {
        // INTEGER16 type
        int16_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<int32_t>(value); // Convert int16_t to int32_t
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        uint16_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<int32_t>(value); // Convert uint16_t to int32_t
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        int32_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return value; // Assuming value is already an int32_t
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        uint32_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return static_cast<int32_t>(value); // Convert uint32_t to int32_t
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping PDO variable '" + m.PDOname + "' to FMU variable '" + m.FMUname + "'.");
        return 0; // Default to 0 if unsupported type
    }
    }
}

fmi2Boolean ReadPdoToFmuBool(const EtherDOG::Mapping &m)
{
    // This function can be used to read the value from the PDO memory based on the mapping and convert it to fmi2Boolean to be written to the FMU variable.

    using namespace kickcat::CoE;

    switch (m.entry.type)
    {
    case DataType::REAL64: // Also LREAL
    {
        // REAL64 type
        double value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return (value != 0.0) ? 1 : 0; // Convert double to fmi2Boolean (non-zero is true)
    }

    case DataType::REAL32: // Also REAL
    {
        // REAL32 type
        float value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return (value != 0.0f) ? 1 : 0; // Convert float to fmi2Boolean (non-zero is true)
    }

    case DataType::BOOLEAN:
    {
        // BOOLEAN type
        bool value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return value ? 1 : 0; // Assuming value is already a bool, convert to fmi2Boolean (true=1, false=0)
    }

    case DataType::INTEGER8: // Also SINT
    {
        // INTEGER8 type
        int8_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return (value != 0) ? 1 : 0; // Convert int8_t to fmi2Boolean (non-zero is true)
    }

    case DataType::UNSIGNED8:
    case DataType::BYTE:
    case DataType::BIT8:
    {
        // UNSIGNED8 type
        uint8_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return (value != 0) ? 1 : 0; // Convert uint8_t to fmi2Boolean (non-zero is true)
    }

    case DataType::INTEGER16:
    {
        // INTEGER16 type
        int16_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return (value != 0) ? 1 : 0; // Convert int16_t to fmi2Boolean (non-zero is true)
    }

    case DataType::UNSIGNED16:
    case DataType::WORD:
    {
        // UNSIGNED16 type
        uint16_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return (value != 0) ? 1 : 0; // Convert uint16_t to fmi2Boolean (non-zero is true)
    }

    case DataType::INTEGER32: // Also DINT
    {
        // INTEGER32 type
        int32_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return (value != 0) ? 1 : 0; // Convert int32_t to fmi2Boolean (non-zero is true)
    }

    case DataType::UNSIGNED32:
    case DataType::DWORD:
    {
        // UNSIGNED32 type
        uint32_t value;
        std::memcpy(&value, m.entry.data, sizeof(value));
        return (value != 0) ? 1 : 0; // Convert uint32_t to fmi2Boolean (non-zero is true)
    }

    default:
    {
        std::cerr << "Unsupported data type for mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "'. Data type: " << toString(m.entry.type) << std::endl;
        throw std::runtime_error("Unsupported data type for mapping PDO variable '" + m.PDOname + "' to FMU variable '" + m.FMUname + "'.");
        return 0; // Default to false if unsupported type
    }
    }
}

void EtherDOG::ExecutePdoInputMappings()
{
    //  This function can be called in Step() after stepping the FMU to update the PDO memory with the latest values from the FMU variables based on the mappings set up in SetupMapping.

    fmu_mutex.lock(); // Lock MUTEX here if needed to safely read FMU variable while it's being updated by FmuThread
    for (auto &m : input_mappings)
    {
        if (!m.entry.is_mapped)
        {
            std::cerr << "[Slave index " << m.SlaveIndex << "] " << "Warning: CoE entry for PDO '" << m.PDOname << "' is not mapped. Skipping mapping for this entry." << std::endl;
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
                    std::cerr << "Error reading FMU variable with VR " << m.vr << ": " << to_string(fmu_slave->last_status()) << std::endl;
                    continue;
                }
                WriteFmuDoubleToPdo(m, fmu_output);
                std::cout << "[Slave index " << m.SlaveIndex << "] " << "Mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "' value= " << fmu_output << std::endl;
                break;
            }

            case FmuVariableType::INTEGER32:
            {
                int32_t fmu_output;
                if (!fmu_slave->read_integer(m.vr, fmu_output))
                {
                    std::cerr << "Error reading FMU variable with VR " << m.vr << ": " << to_string(fmu_slave->last_status()) << std::endl;
                    continue;
                }
                WriteFmuIntToPdo(m, fmu_output);
                std::cout << "[Slave index " << m.SlaveIndex << "] " << "Mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "' value= " << fmu_output << std::endl;
                break;
            }

            case FmuVariableType::BOOLEAN:
            {
                fmi2Boolean fmu_output;
                if (!fmu_slave->read_boolean(m.vr, fmu_output))
                {
                    std::cerr << "Error reading FMU variable with VR " << m.vr << ": " << to_string(fmu_slave->last_status()) << std::endl;
                    continue;
                }
                WriteFmuBoolToPdo(m, fmu_output);
                std::cout << "[Slave index " << m.SlaveIndex << "] " << "Mapping FMU variable '" << m.FMUname << "' to PDO variable '" << m.PDOname << "' value= " << fmu_output << std::endl;
                break;
            }

            default:
            {
                std::cerr << "Unsupported FMU variable type for variable '" << m.FMUname << "'. Skipping writing to PDO for this variable." << std::endl;
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
        if (!m.entry.is_mapped)
        {
            std::cerr << "[Slave index " << m.SlaveIndex << "] " << "Warning: CoE entry for PDO '" << m.PDOname << "' is not mapped. Skipping mapping for this entry." << std::endl;
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
                    std::cerr << "Error writing FMU variable with VR " << m.vr << ": " << to_string(fmu_slave->last_status()) << std::endl;
                    continue;
                }
                std::cout << "[Slave index " << m.SlaveIndex << "] " << "Mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "' value= " << fmu_input << std::endl;
                break;
            }

            case FmuVariableType::INTEGER32:
            {
                int32_t fmu_input = ReadPdoToFmuInt(m);
                if (!fmu_slave->write_integer(m.vr, fmu_input))
                {
                    std::cerr << "Error writing FMU variable with VR " << m.vr << ": " << to_string(fmu_slave->last_status()) << std::endl;
                    continue;
                }
                std::cout << "[Slave index " << m.SlaveIndex << "] " << "Mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "' value= " << fmu_input << std::endl;
                break;
            }

            case FmuVariableType::BOOLEAN:
            {
                fmi2Boolean fmu_input = ReadPdoToFmuBool(m);
                if (!fmu_slave->write_boolean(m.vr, fmu_input))
                {
                    std::cerr << "Error writing FMU variable with VR " << m.vr << ": " << to_string(fmu_slave->last_status()) << std::endl;
                    continue;
                }
                std::cout << "[Slave index " << m.SlaveIndex << "] " << "Mapping PDO variable '" << m.PDOname << "' to FMU variable '" << m.FMUname << "' value= " << fmu_input << std::endl;
                break;
            }
            }
        }
        fmu_mutex.unlock(); // Unlock MUTEX here if it was locked before to allow FmuThread to update FMU variable again
    }
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
