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

    C5 --> C7[Read interface & Read fmuPath]
    C7 --> C8[Get slaves Array]

%% =========================
%% START NETWORKS
%% =========================

    subgraph StartNetworks [StartNetworks]
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
        S12 --> S14[Enable CoE Dictionary]
        S14 --> S15[Store Mailbox]

        S7 -- No --> S16[Skip Mailbox Setup]

        S15 --> S17[Create Input PDO Memory]
        S16 --> S17

        S17 --> S19[Create Output PDO Memory]

        S19 --> S20[Store ESC PDO Slave]
        S20 --> S21{More Slaves?}

        S21 -- Yes --> S2
        S21 -- No --> S23[Open Socket Using interface]
        S23 --> S24[Start All Slaves]
    end

    S24 --> D4[Load FMU from fmuPath]

    D4 --> E[SetupMappingFile]

%% =========================
%% SETUP MAPPING FILE
%% =========================

    subgraph SetupMappingFile [SetupMappingFile]
        E --> E1[Get slaves Array from main_config]

        E1 --> E2[Loop Through Each Slave Config by Index i]

        E2 --> E3{Mailbox Exists for Slave i?}

        E3 -- No --> E4[Print Warning and Continue]

        E3 -- Yes --> E5[Get EtherCAT Dictionary from mailboxes i]

        E5 --> E6{input-mappings Exists?}

        E6 -- Yes --> E7[Get input-mappings]
        E7 --> E8[LoadMapping for input_mappings with slave index i]

        E6 -- No --> E9[Skip Input Mapping]

        E8 --> E10{output-mappings Exists?}
        E9 --> E10

        E10 -- Yes --> E11[Get output-mappings]
        E11 --> E12[LoadMapping for output_mappings with slave index i]

        E10 -- No --> E13[Skip Output Mapping]

        E12 --> E14{More Slave Configs?}
        E13 --> E14

        E14 -- Yes --> E2
        E14 -- No --> E15[Return]
    end

%% =========================
%% START FMU AND RUNTIME
%% =========================

    E15 --> F3[Start FMU Simulation]

    F3 --> G[Create FMU Thread]
    G --> H[Start EtherCAT Main Loop]

%% =========================
%% ETHERCAT MAIN LOOP
%% =========================

    subgraph EtherCATMainLoop [EtherCAT Main Loop]
        H --> H2[FrameHandler Loop]

        H2 --> H3[Receive EtherCAT Frame]
        H3 --> H4[Lock Mutex]

        H4 --> H5[Process EtherCAT Datagrams]
        H5 --> H6[Run Slave Routine for Each Slave]

        H6 --> H7{Slave in SAFE_OP?}
        H7 -- Yes --> H8{Output PDO Valid?}
        H8 -- Yes --> H9[Validate Output Data]
        H8 -- No --> H10[Skip Validation]

        H7 -- No --> H10

        H9 --> H11[Unlock Mutex]
        H10 --> H11

        H11 --> H12[Send EtherCAT Response]
        H12 --> H2
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

        J3 -- Yes --> J4[Copy Output PDO Data to Local FMU Input Value]
        J3 -- No --> J5[Print Warning]

        J4 --> J6[Write Value to FMU Input]
        J5 --> J6

        J6 --> J7{Write Successful?}

        J7 -- No --> J8[Print Error and Continue]
        J7 -- Yes --> J9[Print Mapping Info with Slave Index]

        J8 --> J10{More output_mappings?}
        J9 --> J10

        J10 -- Yes --> J2
        J10 -- No --> J11[Unlock Mutex]

        J11 --> K[FMU Step]

        K --> K1{FMU Step Successful?}

        K1 -- No --> K2[Print Error and Return]
        K1 -- Yes --> K3[Update Simulation Time]

        K3 --> L[ExecutePdoInputMappings]

        L --> L20[Lock Mutex]
        L20 --> L21[Loop Through input_mappings]

        L21 --> L22[Read FMU Output Value]

        L22 --> L23{Read Successful?}

        L23 -- No --> L24[Print Error and Continue]
        L23 -- Yes --> L25{PDO Entry Mapped?}

        L25 -- Yes --> L26[Copy FMU Output to Input PDO Memory]
        L25 -- No --> L27[Print Warning]

        L26 --> L28[Print Mapping Info with Slave Index]
        L27 --> L29{More input_mappings?}
        L24 --> L29
        L28 --> L29

        L29 -- Yes --> L21
        L29 -- No --> L30[Unlock Mutex]

        L30 --> L31[Sleep stepSize]
        L31 --> G1
    end