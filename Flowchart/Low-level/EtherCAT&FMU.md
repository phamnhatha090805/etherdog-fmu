graph TD
A[Start Runtime] --> B[Initialize FMU]
B --> C[Create FMU Thread]
C --> D[Start EtherCAT Frame Loop]

subgraph EtherCAT_Frame_Loop [EtherCAT Frame Loop]
    D --> E[Receive EtherCAT frame]
    E --> F[Lock shared PDO memory]
    F --> G[Process all datagrams through ESCs]
    G --> H[Run slave routines]
    H --> I[Validate output PDO data if changed]
    I --> J[Unlock shared PDO memory]
    J --> K[Send EtherCAT response]
    K --> E
end

subgraph FMU_Thread_Loop [FMU Thread Loop]
    C --> L[Wait for next simulation step]
    L --> M[Lock shared PDO memory]
    M --> N[Copy EtherCAT output PDO values to FMU inputs]
    N --> O[Unlock shared PDO memory]
    O --> P[Execute FMU step]
    P --> Q{FMU step successful?}
    Q -- No --> R[Print error and stop FMU step]
    Q -- Yes --> S[Lock shared PDO memory]
    S --> T[Copy FMU outputs to EtherCAT input PDO values]
    T --> U[Unlock shared PDO memory]
    U --> L
end
