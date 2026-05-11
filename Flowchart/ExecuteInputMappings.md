graph TD

    A[ExecuteInputMappings Called] --> B[Lock fmu_mutex]
    B --> C[Loop Through input_mappings Vector]
    
    C --> D[Read FMU Real Value via m.vr]
    
    D --> E{Read Successful?}
    E -- No --> F[Print Read Error]
    F --> G{More Mappings?}
    
    E -- Yes --> H{m.entry.is_mapped?}
    
    H -- Yes --> I[memcpy fmu_output to m.entry.data]
    I --> J[Print Mapping Success Log]
    J --> G
    
    H -- No --> K[Print Warning: Entry not mapped]
    K --> G

    G -- Yes --> C
    G -- No --> L[Unlock fmu_mutex]
    L --> M[Function End]