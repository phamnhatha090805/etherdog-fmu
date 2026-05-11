graph TD

    A[ExecuteOutputMappings Called] --> B[Lock fmu_mutex]
    B --> C[Loop Through output_mappings Vector]
    
    C --> D{m.entry.is_mapped?}

    D -- No --> E[Print Warning: Entry not mapped]
    E --> H
    
    D -- Yes --> F[memcpy m.entry.data to fmu_input]
    F --> G[Write PDO outputs values to m.vr]
    
    G --> H{More Mappings}

    H -- Yes --> C
    H -- No --> I[Unlock fmu_mutex]

    I --> J[End]