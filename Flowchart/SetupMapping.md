graph TD

    A[SetupMapping Called] --> B[Open MappedVar.json]
    B --> C{File Opened?}

    C -- No --> D[Print Error: Failed to open file]
    D --> E[Return]

    C -- Yes --> F[Parse JSON & Get EtherCAT Dictionary]
    F --> G[Loop Through input-mappings JSON]

    G --> H[Read FMU name & PDO name]
    H --> I[Get FMU Value Reference vr]
    I --> J[Initialize mapping_found = false]

    J --> K[Loop Through EtherCAT Objects]
    K --> L[Loop Through Object Entries]

    L --> M{entry.description == PDO Name?}

    M -- Yes --> N[Create Mapping Struct & Push to input_mappings]
    N --> O[Set mapping_found = true]
    O --> P[Break Entry Loop]

    M -- No --> L

    P --> Q{mapping_found == true?}
    Q -- Yes --> R[Break Object Loop]
    Q -- No --> K

    R --> S{mapping_found == false?}
    S -- Yes --> T[Print Error: PDO entry not found]
    S -- No --> U{More JSON Mappings?}

    T --> U
    U -- Yes --> G
    U -- No --> V[Setup Complete]