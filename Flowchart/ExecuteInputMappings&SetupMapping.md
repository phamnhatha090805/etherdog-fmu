graph TD

    subgraph Initialization [Step 1: Configuration Phase]
    S1[SetupMapping] --> S2[Parse JSON & Find PDO/FMU Links]
    S2 --> S3[Populate input_mappings Vector]
    S3 --> S4[Store Pointers & Value References]
    end

    subgraph Runtime [Step 2: Execution Phase]
    R1[FrameHandler or Main Loop] --> R2{Is Mapping Ready?}
    R2 -- Yes --> R3[ExecuteInputMappings]
    R3 --> R4[Lock Mutex]
    R4 --> R5[Read FMU Data]
    R5 --> R6[Write to EtherCAT PDO Memory]
    R6 --> R7[Unlock Mutex]
    end

    S4 -.-> |Provides Data Structures for| R3
    R7 --> R1