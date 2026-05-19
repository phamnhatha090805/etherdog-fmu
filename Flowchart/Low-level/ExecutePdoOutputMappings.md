graph TD

    A[ExecutePdoOutputMappings Called] --> B[Lock fmu_mutex]
    B --> C[Loop Through output_mappings Vector]
    
    C --> D{PDO Entry is Mapped?}

    D -- No --> E[Print Warning: Entry not mapped]
    E --> H{More Mappings?}
    
    D -- Yes --> F{Check FMU Variable Type}

    F -- REAL --> G[Read PDO Value using ReadPdoToFmuDouble]
    F -- INTEGER32 --> I[Read PDO Value using ReadPdoToFmuInt]
    F -- BOOLEAN --> J[Read PDO Value using ReadPdoToFmuBool]

    G --> K[Write Value to FMU using write_real]
    I --> L[Write Value to FMU using write_integer]
    J --> M[Write Value to FMU using write_boolean]

    K --> N{Write Successful?}
    L --> N
    M --> N

    N -- No --> O[Print Write Error]
    O --> H

    N -- Yes --> P[Print Mapping Success Log]
    P --> H

    H -- Yes --> C
    H -- No --> Q[Unlock fmu_mutex]

    Q --> R[Function End]