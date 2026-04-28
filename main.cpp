#include <iostream>
#include <fmi4cpp/fmi4cpp.hpp>

#include <kickcat/KickCAT.h>
#include <kickcat/CoE/EsiParser.h>
#include "etherdog.hpp"

using namespace fmi4cpp;
using namespace kickcat;

int main()
{
    EtherDOG etherdog;

    const std::string fmu_path = "/home/etherdog/fmu_test/Test2-1.fmu";
    etherdog.loadFMU(fmu_path);

    CoE::EsiParser parser;
    // Load the first device's dictionary from your ESI file
    auto dictionary = parser.loadFirstDictionaryFromFile("/home/etherdog/KickCAT/sample_app_Analogio.xml");

    etherdog.start();
    etherdog.run();
    etherdog.stop();

    return 0;
}