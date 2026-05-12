graph TD

%% =========================
%% PROGRAM START
%% =========================

    A[Program Start] --> B[StartNetworks & Load FMU]
    B --> E[SetupMappingFile & Open Mapping JSON]

%% =========================
%% SETUP MAPPING FILE
%% =========================

    subgraph SetupMappingFile [SetupMappingFile]
        E --> E1{File Opened?}

        E1 -- No --> E2[Print Error]
        E2 --> E3[Return]

        E1 -- Yes --> E4[Parse JSON & Get EtherCAT Dictionary]

        E4 --> E5[Get input-mappings]
        E5 --> E6[LoadMapping for input_mappings]

        E5 --> E7[Get output-mappings]
        E7 --> E8[LoadMapping for output_mappings]
    end

%% =========================
%% LOAD MAPPING FUNCTION
%% =========================

    subgraph LoadMapping [LoadMapping]
        L1[Loop Through JSON Mappings] --> L2[Read FMU Variable Name and PDO Name & Get FMU Value Reference.]
        L2 --> L3[Set mapping_found = false]

        L3 --> L4[Loop Through EtherCAT Objects]
        L4 --> L5[Loop Through Object Entries]

        L5 --> L6{entry.description == PDO Name?}

        L6 -- Yes --> L7[Create Mapping Struct & Push Mapping into Mapping Vector & Set mapping_found = true]
        L7 --> L8[Break Entry Loop]

        L6 -- No --> L5

        L8 --> L9{mapping_found?}
        L9 -- Yes --> L10[Break Object Loop]
        L9 -- No --> L4

        L10 --> L11{mapping_found == false?}
        L11 -- Yes --> L12[Throw PDO Entry Not Found Error]
        L11 -- No --> L13{More JSON Mappings?}

        L13 -- Yes --> L1
        L13 -- No --> L15[Return]
    end

    E6 --> L1
    E8 --> L1

%% =========================
%% START RUNTIME
%% =========================

    E --> F[Start FMU]
    F --> G[Runtime Loop]

%% =========================
%% RUNTIME LOOP
%% =========================

    subgraph RuntimeLoop [Runtime Flow]
        G --> H[FrameHandler]
        H --> H1[Receive EtherCAT Frame]

        H1 --> H2[Lock Mutex]

        H2 --> H5[Process Datagram]

        H5 --> H13[Unlock Mutex]

        H13 --> H14[Send EtherCAT Response]

        H14 --> I[Step FMU Logic]

        I --> J[ExecutePdoOutputMappings]
        J --> J1[Lock Mutex]
        J1 --> J2[Loop Through output_mappings]
        J2 --> J3{PDO Entry Mapped?}

        J3 -- Yes --> J4[Copy Output PDO Data to FMU Inputs]
        J3 -- No --> J5[Print Warning & Throw error]

        J4 --> J6[Write Value to FMU Input]

        J6 --> J7{Write Successful?}
        J7 -- No --> J8[Print Error]
        J7 -- Yes --> J9[Print Mapping Info]

        J9 --> J10{More output_mappings?}
        J10 -- Yes --> J2
        J10 -- No --> J11[Unlock Mutex]

        J11 --> K[FMU Step]
        K --> K1{FMU Step Successful?}

        K1 -- No --> K2[Print Error and Return]
        K1 -- Yes --> K3[Update Simulation Time]

        K3 --> L[ExecutePdoInputMappings]
        L --> L20[Lock Mutex]
        L20 --> L21[Loop Through input_mappings]
        L21 --> L22[Read FMU Output Value]

        L22 --> L23{Read Successful?}
        L23 -- No --> L24[Print Error]
        L23 -- Yes --> L25{PDO Entry Mapped?}

        L25 -- Yes --> L26[Copy FMU Output to Input PDO Memory]
        L25 -- No --> L27[Print Warning]

        L26 --> L28{More input_mappings?}
        L27 --> L28

        L28 -- Yes --> L21
        L28 -- No --> L29[Unlock Mutex]

        L29 --> G
    end