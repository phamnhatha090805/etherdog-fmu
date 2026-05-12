graph TD

    A[LoadMapping Called] --> B[Loop Through JSON Mappings]

    B --> C[Read FMU Variable Name and PDO Name]
    C --> D[Get FMU Value Reference]
    D --> E[Set mapping_found = false]

    E --> F[Loop Through EtherCAT Objects]
    F --> G[Loop Through Object Entries]

    G --> H{PDO Entry Name Matches?}

    H -- Yes --> I[Create Mapping Struct]
    I --> J[Push Mapping into Mapping Vector]
    J --> K[Set mapping_found = true]
    K --> L[Break Entry Loop]

    H -- No --> G

    L --> M{mapping_found?}
    M -- Yes --> N[Break Object Loop]
    M -- No --> F

    N --> O{mapping_found == false?}

    O -- Yes --> P[Throw PDO Entry Not Found Error]
    O -- No --> Q{More JSON Mappings?}

    Q -- Yes --> B
    Q -- No --> R[Return]