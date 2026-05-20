graph TD

%% =========================
%% PROGRAM START
%% =========================

    A[Program Start] --> C[StartNetworks argc argv]

    C --> C1[Parse -f Config.json]
    C1 --> C2[Open Main Config File]

    C2 --> C3{Config File Opened?}

    C3 -- No --> C4[Print Error and Return]
    C3 -- Yes --> C5[Parse Main Config JSON]

    C5 --> C7[Read interface and fmuPath]
    C7 --> C8[Get slaves Array]

%% =========================
%% START NETWORKS
%% =========================

    subgraph StartNetworks [StartNetwork]
        C8 --> S1[Reserve ESC PDO Slave Mailbox Vectors]

        S1 --> S2[Loop Through slaves Array]

        S2 --> S3[Read EEPROM Path from Slave Config]
        S3 --> S4[Create EmulatedESC]
        S4 --> S5[Create PDO]
        S5 --> S6[Create Slave]

        S6 --> S7{coe_xml Exists?}

        S7 -- Yes --> S8[Read CoE XML Path]
        S8 --> S9[Create Mailbox]
        S9 --> S10[Load Devices from CoE XML]
        S10 --> S11[Read Vendor ID Product Code Revision Number]
        S11 --> S12[Find Matching Device]
        S12 --> S13[Enable CoE Dictionary]
        S13 --> S14[Attach Mailbox to Slave]
        S14 --> S15[Store Mailbox]

        S7 -- No --> S16[Skip Mailbox Setup]

        S15 --> S17[Create Input PDO Memory]
        S16 --> S17

        S17 --> S18[Initialize Input PDO Memory]
        S18 --> S19[Create Output PDO Memory]
        S19 --> S20[Initialize Output PDO Memory]
        S20 --> S21[Set PDO Input and Output Buffers]

        S21 --> S22[Store ESC PDO Slave]
        S22 --> S23{More Slaves?}

        S23 -- Yes --> S2
        S23 -- No --> S24[Configure ESC DL Status for Chain Ports]
        S24 --> S25[Open Socket Using interface]
        S25 --> S26[Set Socket Timeout]
        S26 --> S27[Start All Slaves]
    end

    S27 --> D4[Load FMU from fmuPath]

%% =========================
%% LOAD FMU
%% =========================

    subgraph LoadFMU [loadFMU]
        D4 --> D5[Create FMU Object]
        D5 --> D6[Get Co-Simulation FMU]
        D6 --> D7[Get Model Description]
        D7 --> D8[Create FMU Instance]
    end

    D8 --> E[SetupMappingFile]

%% =========================
%% SETUP MAPPING FILE
%% =========================

    subgraph SetupMappingFile [SetupMappingFile]
        E --> E1[Get slaves Array from main_config]

        E1 --> E2[Loop Through Each Slave Config by Index i]

        E2 --> E3{Mailbox Exists for Slave i?}

        E3 -- No --> E4[Print Warning and Continue]

        E3 -- Yes --> E5[Get EtherCAT Dictionary from Mailbox i]

        E5 --> E6{input-mappings Exists?}

        E6 -- Yes --> E7[Get input-mappings]
        E7 --> E8[LoadMapping for input_mappings]

        E6 -- No --> E9[Skip Input Mapping]

        E8 --> E10{output-mappings Exists?}
        E9 --> E10

        E10 -- Yes --> E11[Get output-mappings]
        E11 --> E12[LoadMapping for output_mappings]

        E10 -- No --> E13[Skip Output Mapping]

        E12 --> E14{More Slave Configs?}
        E13 --> E14
        E4 --> E14

        E14 -- Yes --> E2
        E14 -- No --> E15[Return]
    end

%% =========================
%% LOAD MAPPING
%% =========================

    subgraph LoadMapping [LoadMapping]
        E8 --> M1[Loop Through JSON Mapping Entries]
        E12 --> M1

        M1 --> M2[Read FMU Variable Name]
        M2 --> M3[Read PDO Entry Description Name]
        M3 --> M4[Get FMU Variable by Name]

        M4 --> M5{FMU Variable Type?}

        M5 -- Real --> M6[Store Value Reference and Type REAL]
        M5 -- Integer --> M7[Store Value Reference and Type INTEGER32]
        M5 -- Boolean --> M8[Store Value Reference and Type BOOLEAN]
        M5 -- Unsupported --> M9[Print Warning and Skip Mapping]

        M6 --> M10[Search PDO Entry in CoE Dictionary]
        M7 --> M10
        M8 --> M10

        M10 --> M11{PDO Entry Found?}

        M11 -- Yes --> M12[Store Mapping: VR Size PDO Name Slave Index FMU Name FMU Type PDO Entry]
        M11 -- No --> M13[Throw PDO Entry Not Found Error]

        M12 --> M14{More Mapping Entries?}
        M9 --> M14

        M14 -- Yes --> M1
        M14 -- No --> M15[Return to SetupMappingFile]
    end

%% =========================
%% START FMU AND RUNTIME
%% =========================

    E15 --> F3[Start FMU Simulation]

    subgraph StartFMU [start]
        F3 --> F4[Setup FMU Experiment]
        F4 --> F5[Enter Initialization Mode]
        F5 --> F6[Exit Initialization Mode]
    end

    F6 --> G[Create FMU Thread]
    G --> H[Start EtherCAT Main Loop]

%% =========================
%% ETHERCAT MAIN LOOP
%% =========================

    subgraph EtherCATMainLoop [EtherCAT Main Loop]
        H --> H2[FrameHandler Loop]

        H2 --> H3[Receive EtherCAT Frame]
        H3 --> H4[Start Timing Measurement]
        H4 --> H5[Lock Mutex]

        H5 --> H6[Peek EtherCAT Datagram]
        H6 --> H7{Datagram Exists?}

        H7 -- Yes --> H8[Process Datagram Through Each ESC]
        H8 --> H6

        H7 -- No --> H9[Run Slave Routine for Each Slave]

        H9 --> H10{Slave in SAFE_OP?}
        H10 -- Yes --> H11{Output PDO Changed?}
        H11 -- Yes --> H12[Validate Output Data]
        H11 -- No --> H13[Skip Validation]

        H10 -- No --> H13

        H12 --> H14{More Slaves?}
        H13 --> H14

        H14 -- Yes --> H9
        H14 -- No --> H15[Unlock Mutex]

        H15 --> H16[Send EtherCAT Response]
        H16 --> H2
    end

%% =========================
%% FMU THREAD LOOP
%% =========================

    subgraph FMUThreadLoop [FMU Thread Loop]
        G --> G1[Loop Forever]
        G1 --> G2[Enter step]

        G2 --> J[ExecutePdoOutputMappings]

        J --> J1[Lock Mutex]
        J1 --> J2[Loop Through output_mappings]

        J2 --> J3{PDO Entry Mapped?}

        J3 -- No --> J4[Print Warning and Skip Mapping]
        J3 -- Yes --> J5{FMU Variable Type?}

        J5 -- REAL --> J6[Read PDO as double using PdoFmuConverter]
        J5 -- INTEGER32 --> J7[Read PDO as int32 using PdoFmuConverter]
        J5 -- BOOLEAN --> J8[Read PDO as boolean using PdoFmuConverter]
        J5 -- Unsupported --> J9[Print Unsupported FMU Type Warning]

        J6 --> J10[Switch on PDO DataType]
        J7 --> J10
        J8 --> J10

        J10 --> J11[Read Raw PDO Memory]
        J11 --> J12[Convert PDO Value to FMU Value]
        J12 --> J13[Write Value to FMU Input]

        J13 --> J14{Write Successful?}
        J14 -- No --> J15[Print Error and Continue]
        J14 -- Yes --> J16[Print Mapping Info with Slave Index]

        J4 --> J17{More output_mappings?}
        J9 --> J17
        J15 --> J17
        J16 --> J17

        J17 -- Yes --> J2
        J17 -- No --> J18[Unlock Mutex]

        J18 --> K[FMU Step]

        K --> K1{FMU Step Successful?}

        K1 -- No --> K2[Print Error and Return from step]
        K1 -- Yes --> K3[Update Simulation Time]
        K3 --> K4[Print Simulation Time]

        K4 --> L[ExecutePdoInputMappings]

        L --> L1[Lock Mutex]
        L1 --> L2[Loop Through input_mappings]

        L2 --> L3{PDO Entry Mapped?}

        L3 -- No --> L4[Print Warning and Skip Mapping]
        L3 -- Yes --> L5{FMU Variable Type?}

        L5 -- REAL --> L6[Read FMU Output as double]
        L5 -- INTEGER32 --> L7[Read FMU Output as int32]
        L5 -- BOOLEAN --> L8[Read FMU Output as boolean]
        L5 -- Unsupported --> L9[Print Unsupported FMU Type Warning]

        L6 --> L10{Read Successful?}
        L7 --> L10
        L8 --> L10

        L10 -- No --> L11[Print Error and Continue]
        L10 -- Yes --> L12[Call PdoFmuConverter Write Function]

        L12 --> L13[Switch on PDO DataType]
        L13 --> L14[Convert FMU Value to PDO value]
        L14 --> L15[Write Value to PDO Memory]
        L15 --> L16[Print Mapping Info with Slave Index]

        L4 --> L17{More input_mappings?}
        L9 --> L17
        L11 --> L17
        L16 --> L17

        L17 -- Yes --> L2
        L17 -- No --> L18[Unlock Mutex]

        L18 --> L19[Sleep stepSize]
        L19 --> G1
    end

%% =========================
%% PDO FMU CONVERTER DETAILS
%% =========================

    subgraph PdoFmuConverter [PdoFmuConverter]
        J10 --> P1{PDO DataType?}
        L13 --> P1

        P1 -- REAL64 --> P2[Read or Write double]
        P1 -- REAL32 --> P3[Read or Write float]
        P1 -- BOOLEAN --> P4[Read or Write bool]
        P1 -- INTEGER8 --> P5[Read or Write int8]
        P1 -- UNSIGNED8 BYTE BIT8 --> P6[Read or Write uint8]
        P1 -- INTEGER16 --> P7[Read or Write int16]
        P1 -- UNSIGNED16 WORD --> P8[Read or Write uint16]
        P1 -- INTEGER32 --> P9[Read or Write int32]
        P1 -- UNSIGNED32 DWORD --> P10[Read or Write uint32]
        P1 -- Unsupported --> P11[Throw Unsupported DataType Error]
    end