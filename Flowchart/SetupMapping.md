graph TD

    A[SetupMapping Called] --> B[Open MappedVar.json]
    B --> C{File Opened?}

    C -- No --> D[Print Error: Failed to open file]
    D --> E[Return]

    C -- Yes --> F[Parse JSON & Get EtherCAT Dictionary]

%% =========================
%% INPUT MAPPINGS
%% =========================

    F --> G[Get input-mappings JSON]
    G --> H[Loop Through input-mappings]

    H --> I[Read FMU Output Name & PDO Name]
    I --> J[Get FMU Value Reference vr]
    J --> K[Initialize mapping_found = false]

    K --> L[Loop Through EtherCAT Objects]
    L --> M[Loop Through Object Entries]

    M --> N{entry.description == PDO Name?}

    N -- Yes --> O[Create Mapping Struct]
    O --> P[Push Mapping into input_mappings]
    P --> Q[Set mapping_found = true]
    Q --> R[Break Entry Loop]

    N -- No --> M

    R --> S{mapping_found == true?}
    S -- Yes --> T[Break Object Loop]
    S -- No --> L

    T --> U{mapping_found == false?}

    U -- Yes --> V[Print Error: PDO entry not found]
    U -- No --> W{More input-mappings?}

    V --> W
    W -- Yes --> H

%% =========================
%% OUTPUT MAPPINGS
%% =========================

    W -- No --> X[Get output-mappings JSON]

    X --> Y[Loop Through output-mappings]

    Y --> Z[Read FMU Input Name & PDO Name]

    Z --> AA[Get FMU Value Reference vr]

    AA --> AB[Initialize mapping_found = false]

    AB --> AC[Loop Through EtherCAT Objects]

    AC --> AD[Loop Through Object Entries]

    AD --> AE{entry.description == PDO Name?}

    AE -- Yes --> AF[Create Mapping Struct]

    AF --> AG[Push Mapping into output_mappings]

    AG --> AH[Set mapping_found = true]

    AH --> AI[Break Entry Loop]

    AE -- No --> AD

    AI --> AJ{mapping_found == true?}

    AJ -- Yes --> AK[Break Object Loop]

    AJ -- No --> AC

    AK --> AL{mapping_found == false?}

    AL -- Yes --> AM[Print Error: PDO entry not found]

    AL -- No --> AN{More output-mappings?}

    AM --> AN

    AN -- Yes --> Y

    AN -- No --> AO[Setup Complete]