flowchart TD

    A[Start EtherDOG Application] --> B[Load Configuration]
    B --> C[Create EtherCAT Slave Network]
    C --> D[Load and Initialize FMU]
    D --> E[Create FMU ↔ PDO Mapping]

    E --> F[Start Runtime]

    F --> G[EtherCAT Communication Loop]
    F --> H[FMU Simulation Loop]

    G --> G1[Receive and Process EtherCAT Frames]
    G1 --> G2[Update EtherCAT Slave States]
    G2 --> G3[Send EtherCAT Responses]
    G3 --> G

    H --> H1[Transfer PDO Outputs to FMU Inputs]
    H1 --> H2[Step FMU Simulation]
    H2 --> H3[Transfer FMU Outputs to PDO Inputs]
    H3 --> H

    G --> I[Stop Requested]
    H --> I
    I --> J[Terminate FMU and Stop Application]