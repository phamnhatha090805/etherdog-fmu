# EtherDOG-FMU
**EtherDOG** (**EtherCAT** **D**istributed **O**bject **G**enerator) is a software-defined EtherCAT device platform designed for scalable and flexible simulation of virtual EtherCAT networks.

This project focuses on creating simulation platform that can represent different virtual EtherCAT devices. The idea is to use the KickCAT project to emulate virtual EtherCAT devices, while FMU models serve as virtual systems. The PDO/SDO of the KickCAT stack are connected to the variables of the FMU model. In this way, the behavior of the virtual EtherCAT devices can be simulated and made visible.

By leveraging the EtherDOG platform, multiple virtual EtherCAT devices can be instantiated and dynamically configured, enabling rapid testing, validation, and development of control systems without requiring physical hardware.
