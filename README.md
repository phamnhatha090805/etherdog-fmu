# EtherDOG-FMU

**EtherDOG** (**EtherCAT Dynamic Object Generator**) is a software-defined EtherCAT device simulation platform. It allows users to create and run virtual EtherCAT slave devices whose behaviour is controlled by FMU models.

The main goal of EtherDOG is to make EtherCAT device testing more flexible. Instead of always needing physical EtherCAT terminals, users can simulate a virtual EtherCAT network, connect it to an EtherCAT master, and exchange process data with FMU-based simulation models.

In EtherDOG:

* **KickCAT** is used to emulate the EtherCAT slave stack.
* **FMU models** are used to represent the behaviour of the virtual machine, plant, sensor, actuator, or system.
* **JSON configuration files** define which FMU variables are connected to which EtherCAT PDO entries.

This makes EtherDOG useful for rapid testing, validation, controller development, education, and demonstrations without requiring all physical EtherCAT hardware to be available.

---

## 1. What is KickCAT?

[KickCAT](https://github.com/leducp/KickCAT) is an open-source C++ EtherCAT stack. It provides EtherCAT master and slave functionality and includes features such as:

* EtherCAT state machine support: `INIT -> PRE-OP -> SAFE-OP -> OP`
* Process data exchange using PDOs
* CoE / CANopen over EtherCAT support
* SDO read/write support
* ESI XML parsing
* EtherCAT Slave Controller emulation
* Network simulation support
* Raw Ethernet frame handling

KickCAT is designed to be embedded inside other C++ applications. This is exactly how EtherDOG uses it.

EtherDOG does **not** re-implement EtherCAT from scratch. Instead, EtherDOG uses KickCAT as the EtherCAT communication layer and then adds FMU integration and dynamic PDO-to-FMU mapping on top.

---

## 2. How EtherDOG uses KickCAT

EtherDOG uses KickCAT as a C++ library.

In the CMake project, EtherDOG links against:

```cmake
KickCAT::kickcat
fmi4cpp::fmi4cpp
```

KickCAT is responsible for the EtherCAT side of the simulation:

1. EtherDOG loads an EEPROM BIN file for each virtual slave.
2. KickCAT creates an emulated EtherCAT Slave Controller from that EEPROM data.
3. EtherDOG loads the matching ESI XML file.
4. KickCAT parses the ESI XML and builds the CoE object dictionary.
5. KickCAT enables the CoE mailbox for SDO access.
6. KickCAT processes EtherCAT frames from the master.
7. KickCAT handles the slave state machine and EtherCAT datagrams.
8. EtherDOG connects KickCAT PDO memory to FMU variables.

The FMU side is handled by `fmi4cpp`. EtherDOG loads the FMU, reads and writes FMU variables, and steps the FMU simulation in parallel with EtherCAT frame processing.

The basic runtime architecture is:

```text
EtherCAT Master
      |
      | Raw Ethernet frames
      v
KickCAT inside EtherDOG
      |
      | Emulated ESC + PDO + CoE + SDO
      v
EtherDOG Mapping Layer
      |
      | PDO <-> FMU variable conversion
      v
FMU Simulation Model
```

---

## 3. Requirements

This project is intended to run on Linux.

Tested/developed environment:

```text
OS: Linux / Ubuntu / Raspberry Pi OS
Compiler: GCC with C++17 support
Build system: CMake
EtherCAT interface: Ethernet interface with raw socket access
```

Required software:

* `git`
* `cmake`
* `g++`
* `make`
* `python3`
* `pip`
* `conan`
* `fmi4cpp`
* `KickCAT`
* `spdlog`
* `nlohmann_json`
* `tinyxml2`

Install common system packages:

```bash
sudo apt update

sudo apt install -y \
    git \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    python3-venv \
    pkg-config \
    libspdlog-dev \
    nlohmann-json3-dev \
    libtinyxml2-dev \
    libzip-dev \
    libpugixml-dev
```

---

## 4. Install KickCAT

EtherDOG uses KickCAT as a library, so KickCAT must be built and installed before building EtherDOG.

Clone KickCAT:

```bash
cd ~
git clone https://github.com/leducp/KickCAT.git
cd KickCAT
```

Configure KickCAT:

```bash
./scripts/configure.sh build --with=simulation --with=tools --with=esi_parser -ni
```

Set up the KickCAT build environment:

```bash
./scripts/setup_build.sh build
```

Build KickCAT:

```bash
cmake --build build -j$(nproc)
```

Install KickCAT:

```bash
sudo cmake --install build
```

After installation, EtherDOG should be able to find KickCAT using:

```cmake
find_package(KickCAT CONFIG REQUIRED)
```

If CMake cannot find KickCAT later, add `/usr/local` to `CMAKE_PREFIX_PATH` when configuring EtherDOG:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local
```

---

## 5. Install FMI4cpp

EtherDOG uses `fmi4cpp` to load and run FMU models.

Clone and build FMI4cpp:

```bash
cd ~
git clone https://github.com/NTNU-IHB/FMI4cpp.git
cd FMI4cpp
```

Configure:

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DFMI4CPP_BUILD_EXAMPLES=OFF \
    -DFMI4CPP_BUILD_TESTS=OFF
```

Build:

```bash
cmake --build build -j$(nproc)
```

Install:

```bash
sudo cmake --install build
sudo ldconfig
```

After installation, EtherDOG should be able to find FMI4cpp using:

```cmake
find_package(fmi4cpp REQUIRED)
```

---

## 6. Clone and build EtherDOG-FMU

Clone this repository:

```bash
cd ~
git clone https://github.com/phamnhatha090805/etherdog-fmu.git
cd etherdog-fmu
```

Create a build folder:

```bash
mkdir -p build
cd build
```

Configure the project:

```bash
cmake ..
```

If CMake cannot find KickCAT or FMI4cpp, use:

```bash
cmake .. -DCMAKE_PREFIX_PATH="/usr/local"
```

Build EtherDOG:

```bash
cmake --build . -j$(nproc)
```

After a successful build, the executable should be available as:

```bash
./etherdog-fmu
```

---

## 7. Run the first example

From the build directory:

```bash
cd ~/etherdog-fmu/build
sudo ./etherdog-fmu -f ../Examples/SimConfigDemo.json
```

Expected startup behaviour:

1. EtherDOG starts.
2. The JSON configuration file is loaded.
3. The FMU path is read from the config file.
4. The virtual slaves are created.
5. Each EEPROM BIN file is loaded.
6. Each matching ESI XML file is loaded.
7. KickCAT creates the CoE object dictionary.
8. The FMU is loaded through FMI4cpp.
9. PDO-to-FMU mappings are resolved.
10. EtherDOG starts the simulation loop.

Example output may include messages such as:

```text
EtherDOG FMU simulation starting...
EEPROM info for slave 0
Found matching device in ESI XML
Loading FMU from path: ...
Load configuration successfully. Simulation has not started yet.
Starting simulation...
```

To stop the simulation, press:

```text
Ctrl + C
```

EtherDOG should stop gracefully.

---

## 8. Using the GUI

EtherDOG-FMU also includes a simple Python GUI called `TwinDOG.py`.

The GUI provides a more user-friendly way to select the EtherDOG executable, choose the network interface, load a simulation configuration file, start the simulation, stop the simulation, and view the runtime log output.

The GUI does not replace the JSON configuration file.
It uses the same JSON configuration files as the command-line version.

### 8.1 Install GUI dependencies

The GUI is written with PyQt5. Install PyQt5 before running the GUI:

```bash
sudo apt install -y python3-pyqt5
```

Alternatively, PyQt5 can be installed with pip:

```bash
python3 -m pip install PyQt5
```

### 8.2 Start the GUI

From the EtherDOG-FMU project folder, run:

```bash
cd ~/etherdog-fmu
python3 TwinDOG.py
```

If EtherDOG needs administrator permission to access the Ethernet interface, run the GUI with `sudo`:

```bash
cd ~/etherdog-fmu
sudo -E python3 TwinDOG.py
```

The `-E` option keeps the user environment, which can help when Python or PyQt5 is installed in the user environment.

### 8.3 GUI fields

When the GUI opens, it shows several fields.

| GUI field            | Description                                                   |
| -------------------- | ------------------------------------------------------------- |
| `TwinDOG Executable` | Path to the compiled `etherdog-fmu` executable                |
| `Network Interface`  | Ethernet interface used for EtherCAT communication            |
| `Main Config JSON`   | JSON simulation configuration file                            |
| `Run Simulation`     | Starts EtherDOG using the selected executable and config file |
| `Stop Simulation`    | Stops the running simulation                                  |
| Output window        | Shows the EtherDOG runtime log output                         |

### 8.4 Select the EtherDOG executable

In the `TwinDOG Executable` field, select the compiled EtherDOG executable.

Example path:

```text
/home/etherdog/etherdog-fmu/build/etherdog-fmu
```

If the path is wrong, click `Browse` and select the correct executable manually.

### 8.5 Select the network interface

Use the `Network Interface` dropdown to select the Ethernet interface connected to the EtherCAT master.

The GUI automatically lists the available network interfaces from the Linux system.
When the simulation starts, the GUI writes the selected interface into the JSON configuration file.

For example, if `eth0` is selected, the GUI updates the config file like this:

```json
"interface": "eth0"
```

This means users do not have to manually edit the interface field in the JSON file every time.

### 8.6 Select the JSON configuration file

In the `Main Config JSON` field, select one of the simulation configuration files.

Example:

```text
/home/etherdog/etherdog-fmu/Examples/SimConfigDemo.json
```

Or from the project folder:

```text
Examples/SimConfigDemo.json
```

The selected JSON file must contain the required EtherDOG configuration fields:

```json
{
  "interface": "eth0",
  "fmuPath": "../Examples/FmuModel/DemoModel-1.fmu",
  "slaves": [
    {
      "eeprom": "../Examples/EsiFiles/BIN/Box 1 (EtherDOG).bin",
      "coe_xml": "../Examples/EsiFiles/XML/EtherDOG.xml",
      "input-mappings": {
        "RoomTemperature": "AnalogIN0"
      },
      "output-mappings": {
        "HeaterGain": "AnalogOUT0"
      }
    }
  ]
}
```

Before starting the simulation, the GUI checks that:

1. The selected executable exists.
2. The selected JSON file exists.
3. The JSON file contains `interface`, `fmuPath`, and `slaves`.
4. The `slaves` field is an array.
5. Each slave contains an `eeprom` field.
6. The `input-mappings` and `output-mappings` fields are valid objects if they are used.

If the JSON file is invalid, the GUI prints an error message in the output window.

### 8.7 Load the configuration

Before starting the simulation, users can click:

Load Configuration

This button loads and checks the selected JSON configuration file without starting the EtherCAT simulation yet.

The purpose of this step is to let the user verify the complete simulation setup before the virtual EtherCAT slaves become active. This is useful for checking whether the correct FMU model, network interface, EEPROM files, ESI XML files, and PDO/FMU mappings are selected.

When Load Configuration is clicked, the GUI prints the loaded configuration information in the output window.

The Load Configuration button is only a preparation and verification step. It does not scan EtherCAT devices, does not start frame processing, and does not start FMU simulation stepping. The actual runtime simulation only begins when Run Simulation is clicked.

### 8.8 Start the simulation

After selecting the executable, interface, and JSON config file, click:

```text
Run Simulation
```

The GUI runs the same command as the command-line workflow:

```bash
./etherdog-fmu -f selected_config.json
```

The output window will show the runtime messages from EtherDOG, such as:

```text
Running: /home/etherdog/etherdog-fmu/build/etherdog-fmu -f /home/etherdog/etherdog-fmu/Examples/SimConfigDemo.json
EtherDOG FMU simulation starting...
EEPROM info for slave 0
Found matching device in ESI XML
Loading FMU from path: ...
Starting simulation...
```

Once EtherDOG is running, the virtual EtherCAT slaves can be scanned from TwinCAT or another EtherCAT master.

### 8.9 Stop the simulation

To stop the simulation, click:

```text
Stop Simulation
```

The GUI sends an interrupt signal to the running EtherDOG process.
This is similar to pressing:

```text
Ctrl + C
```

in the terminal.

If no simulation is running, the GUI prints:

```text
No simulation is currently running.
```

---

## 9. JSON configuration format

EtherDOG uses a JSON file to define the virtual EtherCAT network.

Example:

```json
{
  "interface": "eth0",
  "fmuPath": "../Examples/FmuModel/DemoModel-1.fmu",
  "slaves": [
    {
      "eeprom": "../Examples/EsiFiles/BIN/Box 1 (EtherDOG).bin",
      "coe_xml": "../Examples/EsiFiles/XML/EtherDOG.xml",
      "input-mappings": {
        "RoomTemperature": "AnalogIN0",
        "OutdoorSensor": "AnalogIN1"
      },
      "output-mappings": {
        "HeaterGain": "AnalogOUT0"
      }
    }
  ]
}
```

### Top-level fields

| Field       | Description                                              |
| ----------- | -------------------------------------------------------- |
| `interface` | Linux network interface used for EtherCAT frame exchange |
| `fmuPath`   | Path to the FMU model                                    |
| `slaves`    | Array of virtual EtherCAT slaves                         |

### Slave fields

| Field             | Description                                      |
| ----------------- | ------------------------------------------------ |
| `eeprom`          | Path to the EEPROM BIN file of the virtual slave |
| `coe_xml`         | Path to the ESI XML file of the virtual slave    |
| `input-mappings`  | Maps FMU variables to EtherCAT input PDOs        |
| `output-mappings` | Maps EtherCAT output PDOs to FMU variables       |

---

## 10. PDO naming rules

PDO names in the JSON file must match entries from the CoE object dictionary loaded from the ESI XML file.

For simple devices, the PDO name may be a direct entry name:

```json
"Out1": "AnalogIN0"
```

For Beckhoff terminals with repeated channel names, use the format:

```text
Channel name/Entry name
```

Example:

```json
"Power": "AO outputs Ch.1/Analog output"
```

This tells EtherDOG to look for:

```text
Object name: AO outputs Ch.1
Entry description: Analog output
```

This is useful for terminals such as EL4004, where multiple channels may have entries with the same description.

---

## 11. Add multiple virtual slaves

The `slaves` field is an array, so you can add multiple virtual devices.

Example:

```json
{
  "interface": "eth0",
  "fmuPath": "../Examples/FmuModel/DemoModel-1.fmu",
  "slaves": [
    {
      "eeprom": "../Examples/EsiFiles/BIN/Box 1 (EtherDOG).bin",
      "coe_xml": "../Examples/EsiFiles/XML/EtherDOG.xml",
      "input-mappings": {
        "RoomTemperature": "AnalogIN0"
      }
    },
    {
      "eeprom": "../Examples/EsiFiles/BIN/Official Beckhoff/Term 1 (EK1100).bin",
      "coe_xml": "../Examples/EsiFiles/XML/Official Beckhoff/Beckhoff EK11xx.xml"
    },
    {
      "eeprom": "../Examples/EsiFiles/BIN/Official Beckhoff/Term 2 (EL4004).bin",
      "coe_xml": "../Examples/EsiFiles/XML/Official Beckhoff/Beckhoff EL4xxx.xml",
      "output-mappings": {
        "Power": "AO outputs Ch.1/Analog output"
      }
    }
  ]
}
```

The order in the JSON file is the virtual EtherCAT slave order.

---

## 12. Runtime behaviour

EtherDOG runs two main runtime activities:

### EtherCAT frame processing

This receives EtherCAT frames from the master, passes the datagrams through each emulated slave, updates the KickCAT slave state machine, and sends the response frame back to the master.

### FMU simulation stepping

This executes the FMU model repeatedly using a fixed step size. During each FMU step:

1. EtherDOG reads EtherCAT output PDO values.
2. These values are written into mapped FMU input variables.
3. The FMU simulation advances by one step.
4. EtherDOG reads mapped FMU output variables.
5. These values are written into EtherCAT input PDO memory.

This creates a continuous data exchange loop between the EtherCAT master and the FMU model.

---

## 13. Troubleshooting

### CMake cannot find KickCAT

Error example:

```text
Could not find a package configuration file provided by "KickCAT"
```

Fix:

```bash
sudo cmake --install ~/KickCAT/build
sudo ldconfig
```

Then configure EtherDOG again:

```bash
cd ~/etherdog-fmu
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH="/usr/local"
```

### CMake cannot find fmi4cpp

Error example:

```text
Could not find a package configuration file provided by "fmi4cpp"
```

Fix:

```bash
sudo cmake --install ~/FMI4cpp/build
sudo ldconfig
```

Then configure EtherDOG again:

```bash
cmake .. -DCMAKE_PREFIX_PATH="/usr/local"
```

### Permission denied when opening the network interface

EtherDOG needs raw socket access.

Run with `sudo`:

```bash
sudo ./etherdog-fmu -f ../Examples/SimConfigDemo.json
```

### Wrong network interface

Check your available interfaces:

```bash
ip link show
```

Then update the JSON config:

```json
"interface": "your_interface_name"
```

### TwinCAT cannot find the virtual slaves

Check the following:

1. EtherDOG is already running before scanning in TwinCAT.
2. The correct network interface is selected in the JSON file.
3. The Ethernet cable is connected to the correct port.
4. The interface is not blocked by NetworkManager or another service.
5. You are running EtherDOG with `sudo`.
6. The ESI XML and EEPROM BIN file match.
7. TwinCAT is scanning the correct Ethernet adapter.

### ESI XML does not match EEPROM

EtherDOG checks that the ESI XML matches the EEPROM identity information.

If vendor ID, product code, or revision number do not match, EtherDOG will stop with an error.

Make sure the `eeprom` and `coe_xml` fields in the JSON file belong to the same device.

### PDO entry not found

Error example:

```text
PDO entry not found
```

This means the PDO name in the JSON file does not match the CoE dictionary entry from the ESI XML.

Fix:

1. Check the exact PDO entry name in the ESI XML.
2. Use `Channel/Entry` format if the same entry name exists in multiple channels.
3. Make sure spelling and capitalization match.
4. Make sure the selected ESI XML belongs to the selected device.

### CoE entry is not mapped

Warning example:

```text
Warning: CoE entry for PDO '...' is not mapped. Skipping mapping for this entry.
```

This means the PDO entry exists in the object dictionary, but it is not currently part of the active PDO assignment.

Fix:

1. Check the PDO assignment in the ESI XML.
2. Check whether the EtherCAT master enables that PDO in its configuration.
3. In TwinCAT, check the process data tab of the slave.
4. Make sure the mapping in JSON points to an actually mapped PDO entry.

---

## 14. Current limitations

EtherDOG is still a development/research platform. Current limitations may include:

* Not all EtherCAT device behaviours are fully simulated.
* Correct operation depends on the quality of the ESI XML and EEPROM BIN files.
* PDO mapping must match the active PDO assignment.
* Real-time performance depends on the host system and network interface.
* The FMU step size is currently fixed in the code.
* Advanced EtherCAT features may require additional implementation.

---

## 15. Basic workflow summary

```text
1. Install dependencies
2. Build and install KickCAT
3. Build and install FMI4cpp
4. Build EtherDOG-FMU
5. Select the correct network interface
6. Choose or create a JSON simulation config
7. Start EtherDOG with sudo
8. Scan the virtual EtherCAT network from TwinCAT or another master
9. Move the network to OP state
10. Exchange PDO data between the PLC and the FMU model
```

Quick command summary:

```bash
cd ~/etherdog-fmu
mkdir -p build
cd build
cmake ..
cmake --build . -j$(nproc)
sudo ./etherdog-fmu -f ../Examples/SimConfigDemo.json
```

---

## 16. License and acknowledgements

EtherDOG-FMU builds on top of several open-source projects:

* [KickCAT](https://github.com/leducp/KickCAT) for the EtherCAT stack, slave emulation, CoE support, ESI parsing, and EtherCAT frame handling.
* [FMI4cpp](https://github.com/NTNU-IHB/FMI4cpp) for loading and running FMU simulation models.
* [spdlog](https://github.com/gabime/spdlog) for logging.

Special thanks to the developers and maintainers of KickCAT and FMI4cpp.  
Their work made it possible for EtherDOG-FMU to focus on dynamic virtual EtherCAT device generation, PDO/FMU mapping, and simulation workflow development instead of re-implementing the complete EtherCAT and FMI foundations from scratch.

Please check the licenses of these projects before using EtherDOG-FMU in a commercial or redistributed system.