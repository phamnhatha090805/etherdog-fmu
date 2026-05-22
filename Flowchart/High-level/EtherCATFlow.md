flowchart TD
    A[Start EtherCAT Communication Loop] --> B[Receive Ethernet Frame]
    B --> C[Process EtherCAT Datagrams]
    C --> D[Update Emulated ESCs]
    D --> E[Run Slave Routines]
    E --> F[Check Slave State]
    F --> G{Slave in SAFE_OP?}

    G -- Yes --> H[Validate Output PDO Data]
    G -- No --> I[Skip Output Validation]

    H --> J[Send EtherCAT Response Frame]
    I --> J

    J --> K{Stop Requested?}
    K -- No --> B
    K -- Yes --> L[Stop EtherCAT Loop]