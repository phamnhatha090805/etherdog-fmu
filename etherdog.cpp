
#include "etherdog.hpp"

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

    program.add_argument("-i", "--interface")
        .help("network interface name")
        .required()
        .store_into(interface);

    program.add_argument("-n", "--count")
        .help("Number of slaves to simulate")
        .default_value(0)
        .scan<'i', int>()
        .store_into(slave_number);

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

    escs.reserve(slave_count);
    pdos.reserve(slave_count);
    slaves.reserve(slave_count);
    mailboxes.reserve(slave_count);
    input_pdo.reserve(slave_count);
    output_pdo.reserve(slave_count);

    constexpr uint32_t PDO_MAX_SIZE = 32;
    CoE::EsiParser parser;

    for (const auto &config_path : expanded_slave_configs)
    {
        fs::path p(config_path);
        fs::path config_dir = p.parent_path();

        std::ifstream f(config_path);
        if (not f.is_open())
        {
            std::cerr << "Failed to open config file: " << config_path << std::endl;
            return 1;
        }

        json config;
        try
        {
            f >> config;
        }
        catch (const json::parse_error &e)
        {
            std::cerr << "Failed to parse JSON in " << config_path << ": " << e.what() << std::endl;
            return 1;
        }

        if (not config.contains("eeprom"))
        {
            std::cerr << "Config file " << config_path << " missing 'eeprom' field" << std::endl;
            return 1;
        }

        std::string eeprom_path = config["eeprom"];
        fs::path eeprom_full_path = config_dir / eeprom_path;
        auto esc = std::make_unique<EmulatedESC>(eeprom_full_path.string().c_str());
        auto pdo = std::make_unique<PDO>(esc.get());
        auto slave = std::make_unique<Slave>(esc.get(), pdo.get());

        if (config.contains("coe_xml"))
        {
            std::string coe_xml_path = config["coe_xml"];
            fs::path coe_xml_full_path = config_dir / coe_xml_path;
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

    auto [socket, _] = createSockets(interface, "");
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
    uint8_t current_value = 0x11;
    auto var = cs_md->get_variable_by_name("Out1").as_real();
    auto vr = var.valueReference();
    double fmu_output;

    fmu_slave->read_real(vr, fmu_output);
    float rx_value = static_cast<float>(fmu_output);
    std::memcpy(input_pdo[0].data(), &rx_value, sizeof(float));
    while (true)
    {
        Frame frame;
        int32_t received = socket->read(frame.data(), ETH_MAX_SIZE);
        if (received < 0)
        {
            printf("Something wrong happened. Aborting...\n");
            return;
        }

        while (true)
        {
            auto [header, data, wkc] = frame.peekDatagram();
            if (header == nullptr)
            {
                return;
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

            std::fill(input_pdo[i].begin(), input_pdo[i].end(), current_value);
        }
        int32_t written = socket->write(frame.data(), received);
    }
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
    while (true)
    {
        step();
    }
}

void EtherDOG::step()
{
    if (!fmu_slave->step(stepSize))
    {
        std::cerr << "Error! step() returned with status: " << to_string(fmu_slave->last_status()) << std::endl;
        return;
    }

    /*if (!fmu_slave->read_real(vr, fmu_output))
    {
        std::cerr << "Error! step() returned with status: " << to_string(fmu_slave->last_status()) << std::endl;
        return;
    }*/

    FrameHandler();
    // std::cout << "t=" << t << ", " << var.name() << "=" << fmu_output << std::endl;
    // std::cout << "t=" << t << ", " << var.name() << "=" << value << ", " << var2.name() << "=" << value2 << std::endl;
}

void EtherDOG::stop()
{
    fmu_slave->terminate();
}
