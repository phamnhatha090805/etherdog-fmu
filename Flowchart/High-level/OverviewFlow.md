flowchart TD

    %% =====================
    %% MAIN FLOW
    %% =====================
    A[main] --> C[StartNetworks]

    C --> C1[Parse -f Config.json Argument]
    C1 --> C2[Read Main Config JSON]

    C2 --> C3[Read network interface and fmuPath from Config.json]
    C3 --> C4[Read slaves Array from Config.json]

    C4 --> C7[Create each EtherCAT Slaves from Config.json]

    C7 --> C8[Open Socket]
    C8 --> C9[Start Slaves]

    C9 --> D[Load FMU from fmuPath]

    D --> E[Setup FMU ↔ PDO Mappings]
    E --> E1[Loop Through Each Slave Config]
    E1 --> E2[Get Slave Dictionary]
    E2 --> E3[Load Input and Output Mappings per Slave]

    E3 --> F[Initialize FMU Simulation]

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
    G3 --> G4[Read PDO Outputs and Write to FMU Inputs]
    G4 --> G5[Unlock Mutex]

    G5 --> G6[Execute FMU Step]

    G6 --> G8[Execute PDO Input Mappings]
    G8 --> G9[Lock Mutex]
    G9 --> G10[Read FMU Outputs and Write to PDO Inputs]
    G10 --> G11[Unlock Mutex]

    G11 --> G12[Sleep Until Next Cycle]
    G12 --> G1

    %% =====================
    %% SHUTDOWN
    %% =====================
    H1 -. Program Stop .-> Z[Terminate FMU]