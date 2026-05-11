flowchart TD

    %% =====================
    %% MAIN FLOW
    %% =====================
    A[main] --> C

    C[Initialize EtherCAT Network]
    C --> C1[Load Slave and PDO Configuration, configure ESC and Mailbox]
    C1 --> C2[Open Socket and Start Slaves]

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
    H4 --> H5[Run Slave Routines]
    H5 --> H6[Validate and Update PDO Data]
    H6 --> H7[Unlock Mutex]

    H7 --> H8[Send Ethernet Frames]
    H8 --> H1

    %% =====================
    %% FMU LOOP
    %% =====================
    G --> G1{{FMU Loop}}

    G1 --> G2[Lock Mutex]
    G2 --> G3[Read PDO Output Mappings]
    G3 --> G4[Execute FMU Step]
    G4 --> G5[Write FMU Inputs to PDO]
    G5 --> G6[Unlock Mutex]

    G6 --> G7[Update Simulation Time]
    G7 --> G8[Sleep Until Next Cycle]
    G8 --> G1

    %% =====================
    %% SHUTDOWN
    %% =====================
    H1 -. Program Stop .-> Z[Terminate FMU]