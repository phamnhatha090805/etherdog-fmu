flowchart TD

    %% =====================
    %% MAIN FLOW
    %% =====================
    A[main] --> C

    C[Initialize EtherCAT Network]
    C --> C2[Open Socket and Start Slaves]

    C2 --> D[Load FMU]

    D --> E[Setup FMU ↔ PDO Mapping]
    E --> F[Initialize FMU Simulation]

    F --> G[Start FMU Thread]
    F --> H[Start EtherCAT Main Loop]

    %% =====================
    %% ETHERCAT LOOP
    %% =====================
    H --> H1{{EtherCAT Loop}}

    H1 --> H2[Receive Ethernet Frames]

    H2 --> H3[Lock Mutex]
    H3 --> H4[Process EtherCAT Datagrams]
    H4 --> H7[Unlock Mutex]

    H7 --> H8[Send Ethernet Frames]
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