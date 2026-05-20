flowchart TD

    %% =====================
    %% MAIN FLOW
    %% =====================
    A[main] --> C[StartNetworks]

    C --> C1[Parse -f Config.json Argument]
    C1 --> C2[Read Main Config JSON]

    C2 --> C3[Read network interface and fmuPath from Config.json]
    C3 --> C4[Read slaves Array from Config.json]

    C4 --> C7[Create each EtherCAT Slave from Config.json]

    C7 --> C8[Open Socket]
    C8 --> C9[Start Slaves]

    C9 --> D[Load FMU from fmuPath]

    D --> E[Setup FMU ↔ PDO Mappings]

    E --> E1[Loop Through Each Slave Config]
    E1 --> E2[Get Slave Dictionary]
    E2 --> E3[Load Input and Output Mappings]

    E3 --> E4[Detect FMU Variable Type]
    E4 --> E5[Store Mapping with FMU Type and PDO Type]

    E5 --> F[Initialize FMU Simulation]

    F --> G[Start FMU Thread]
    F --> H[Start EtherCAT Main Loop]

    %% =====================
    %% ETHERCAT LOOP
    %% =====================
    H --> H1{{EtherCAT Loop}}

    H1 --> H2[Receive Ethernet Frame]
    H2 --> H3[Lock Mutex]
    H3 --> H4[Process EtherCAT Datagrams]
    H4 --> H5[Run Slave Routine]
    H5 --> H6[Validate Output Data if SAFE_OP]
    H6 --> H7[Unlock Mutex]

    H7 --> H8[Send Ethernet Frame]
    H8 --> H1

    %% =====================
    %% FMU LOOP
    %% =====================
    G --> G1{{FMU Loop}}

    G1 --> G2[Execute PDO Output Mappings]

    G2 --> G3[Lock Mutex]
    G3 --> G4[Read PDO Values]
    G4 --> G5[Convert PDO Data Type to FMU Variable Type]
    G5 --> G6[Write Values to FMU Inputs]
    G6 --> G7[Unlock Mutex]

    G7 --> G8[Execute FMU Step]

    G8 --> G9[Execute PDO Input Mappings]

    G9 --> G10[Lock Mutex]
    G10 --> G11[Read FMU Outputs]
    G11 --> G12[Convert FMU Variable Type to PDO Data Type]
    G12 --> G13[Write Values to PDO Memory]
    G13 --> G14[Unlock Mutex]

    G14 --> G15[Sleep Until Next Cycle]
    G15 --> G1

    %% =====================
    %% SHUTDOWN
    %% =====================
    H1 -. Program Stop .-> Z[Terminate FMU]