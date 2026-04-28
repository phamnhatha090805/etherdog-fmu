
#include "etherdog.hpp"

const double stopTime = 10.0;
const double stepSize = 0.01;

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

// void EtherDOG::handleFrames()
// {
// }

void EtherDOG::step()
{

    /*
       Frame frame;
       int32_t r = socket->read(frame.data(), ETH_MAX_SIZE);
       if (r < 0)
       {
           printf("Something wrong happened. Aborting...\n");
           return -1;
       }

       auto t1 = since_epoch();

       while (true)
       {
           auto [header, data, wkc] = frame.peekDatagram();
           if (header == nullptr)
           {
               break;
           }

           for (auto& esc : escs)
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
       */

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
