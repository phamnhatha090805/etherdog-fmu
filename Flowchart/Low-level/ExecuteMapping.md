graph TD

%% =========================
%% FMU THREAD LOOP
%% =========================

subgraph FMUThreadLoop [FMU Thread Loop]
    G[FMU Thread Loop] --> G1[Loop Forever]
    G1 --> G2[Enter step]

    G2 --> J[ExecutePdoOutputMappings]

    J --> J1[Lock Mutex]
    J1 --> J2[Loop Through output_mappings]

    J2 --> J3{PDO Entry Mapped?}

    J3 -- No --> J4[Print Warning and Skip Mapping]
    J3 -- Yes --> J5{FMU Variable Type?}

    J5 -- REAL --> J6[Read PDO as double using PdoFmuConverter]
    J5 -- INTEGER32 --> J7[Read PDO as int32 using PdoFmuConverter]
    J5 -- BOOLEAN --> J8[Read PDO as boolean using PdoFmuConverter]
    J5 -- Unsupported --> J9[Print Unsupported FMU Type Warning]

    J6 --> J10[Switch on PDO DataType]
    J7 --> J10
    J8 --> J10

    J10 --> J11[Read Raw PDO Memory]
    J11 --> J12[Convert PDO Value to FMU Value]
    J12 --> J13[Write Value to FMU Input]

    J13 --> J14{Write Successful?}
    J14 -- No --> J15[Print Error and Continue]
    J14 -- Yes --> J16[Print Mapping Info with Slave Index]

    J4 --> J17{More output_mappings?}
    J9 --> J17
    J15 --> J17
    J16 --> J17

    J17 -- Yes --> J2
    J17 -- No --> J18[Unlock Mutex]

    J18 --> K[FMU Step]

    K --> K1{FMU Step Successful?}

    K1 -- No --> K2[Print Error and Return from step]
    K1 -- Yes --> K3[Update Simulation Time]
    K3 --> K4[Print Simulation Time]

    K4 --> L[ExecutePdoInputMappings]

    L --> L1[Lock Mutex]
    L1 --> L2[Loop Through input_mappings]

    L2 --> L3{PDO Entry Mapped?}

    L3 -- No --> L4[Print Warning and Skip Mapping]
    L3 -- Yes --> L5{FMU Variable Type?}

    L5 -- REAL --> L6[Read FMU Output as double]
    L5 -- INTEGER32 --> L7[Read FMU Output as int32]
    L5 -- BOOLEAN --> L8[Read FMU Output as boolean]
    L5 -- Unsupported --> L9[Print Unsupported FMU Type Warning]

    L6 --> L10{Read Successful?}
    L7 --> L10
    L8 --> L10

    L10 -- No --> L11[Print Error and Continue]
    L10 -- Yes --> L12[Call PdoFmuConverter Write Function]

    L12 --> L13[Switch on PDO DataType]
    L13 --> L14[Convert FMU Value to PDO value]
    L14 --> L15[Write Value to PDO Memory]
    L15 --> L16[Print Mapping Info with Slave Index]

    L4 --> L17{More input_mappings?}
    L9 --> L17
    L11 --> L17
    L16 --> L17

    L17 -- Yes --> L2
    L17 -- No --> L18[Unlock Mutex]

    L18 --> L19[Sleep stepSize]
    L19 --> G1
end

%% =========================
%% PDO FMU CONVERTER DETAILS
%% =========================

subgraph PdoFmuConverter [PdoFmuConverter]
    J10 --> P1{PDO DataType?}
    L13 --> P1

    P1 -- REAL64 --> P2[Read or Write double]
    P1 -- REAL32 --> P3[Read or Write float]
    P1 -- BOOLEAN --> P4[Read or Write bool]
    P1 -- INTEGER8 --> P5[Read or Write int8]
    P1 -- UNSIGNED8 BYTE BIT8 --> P6[Read or Write uint8]
    P1 -- INTEGER16 --> P7[Read or Write int16]
    P1 -- UNSIGNED16 WORD --> P8[Read or Write uint16]
    P1 -- INTEGER32 --> P9[Read or Write int32]
    P1 -- UNSIGNED32 DWORD --> P10[Read or Write uint32]
    P1 -- Unsupported --> P11[Throw Unsupported DataType Error]
end