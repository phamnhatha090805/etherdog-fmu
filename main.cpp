#include <iostream> 
#include <fmi4cpp/fmi4cpp.hpp>

using namespace fmi4cpp;

const double stop = 10.0;
const double stepSize = 0.01;

int main() 
{
    fmi2::fmu fmu("/home/etherdog/fmu_test/Test2-1.fmu");

    auto cs_fmu = fmu.as_cs_fmu();
    //auto me_fmu = fmu.as_me_fmu();
    
    auto cs_md = cs_fmu->get_model_description(); //smart pointer to a cs_model_description instance
    std::cout << "model_identifier=" << cs_md->model_identifier << std::endl;
    
    auto var = cs_md->get_variable_by_name("Out1").as_real();
    auto var2 = cs_md->get_variable_by_name("Out2").as_real();
    std::cout << "Name=" << var.name() <<  ", start=" << var.start().value_or(0) << std::endl;
    std::cout << "Name=" << var2.name() <<  ", start=" << var2.start().value_or(0) << std::endl;
              
    auto fmu_slave = cs_fmu->new_instance();
    
    fmu_slave->setup_experiment();
    fmu_slave->enter_initialization_mode();
    fmu_slave->exit_initialization_mode();
    
    double t;
    double value;
    double value2;
    auto vr = var.valueReference();
    auto vr2 = var2.valueReference();
    while (true) {
        if (!fmu_slave->step(stepSize)) {
            std::cerr << "Error! step() returned with status: " << to_string(fmu_slave->last_status()) << std::endl;
            break;
        }
        
        if (!fmu_slave->read_real(vr, value)) {
            std::cerr << "Error! step() returned with status: " << to_string(fmu_slave->last_status()) << std::endl;
            break;
        }
        //std::cout << "t=" << t << ", " << var.name() << "=" << value << std::endl;
        std::cout << "t=" << t << ", " << var.name() << "=" << value << ", " << var2.name() << "=" << value2 << std::endl;
    }
    fmu_slave->terminate();
    
    return 0;
}