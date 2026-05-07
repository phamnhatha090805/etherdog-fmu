graph TD
    A([Start: SetupMapping]) --> B[Open JSON config file]
    B --> C{File open?}
    
    C -- No --> D[Print error & Return]
    D --> Z
    C -- Yes --> E[Parse JSON into 'config']
    
    E --> F[Access EtherCAT dictionary & JSON file]
    F --> G[Loop through each mapping pair from JSON]
    
    G --> J[Extract FMU variable name & PDO name]
    J --> K[Get FMU Value Reference]
    K --> L[Read current FMU real value into 'fmu_output']
    
    L --> M{Read Success?}
    M -- No --> N[Print error & Return]
    N --> Z
    
    M -- Yes --> O[Loop through EtherCAT Dictionary Objects]
    
    O --> P{mapping_found?}
    P -- No --> Q[Print 'PDO entry not found']
    Q --> G
    
    P -- Yes --> R[Loop through Entries in Object]
    
    R --> S{Entry matches pdo_name?}
    S -- No --> R
    S -- Yes --> U[Lock fmu_mutex]
    
    U --> V[memcpy fmu_output to entry.data]
    V --> W[Unlock fmu_mutex]
    
    W --> X[Print mapping confirmation]
    X --> Y[mapping_found = true and break] 

    Y --> Z[End]