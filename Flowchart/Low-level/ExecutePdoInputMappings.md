graph TD

    A[ExecutePdoInputMappings Called] --> B[Lock fmu_mutex]
    B --> C[Loop Through input_mappings Vector]

    C --> D{PDO Entry is Mapped?}

    D -- No --> E[Print Warning: CoE Entry not mapped]
    E --> Z{More Mappings?}

    D -- Yes --> F{Check FMU Variable Type}

    F -- REAL --> G[Read FMU Real Value via m.vr]
    F -- INTEGER32 --> H[Read FMU Integer Value via m.vr]
    F -- BOOLEAN --> I[Read FMU Boolean Value via m.vr]

    G --> J{Read Successful?}
    H --> J
    I --> J

    J -- No --> K[Print Read Error]
    K --> Z

    J -- Yes --> L[Convert FMU Value to PDO Data Type]

    L --> M[Write Converted Value to m.entry.data]
    M --> N[Print Mapping Success Log]
    N --> Z

    Z -- Yes --> C
    Z -- No --> O[Unlock fmu_mutex]
    O --> P[Function End]