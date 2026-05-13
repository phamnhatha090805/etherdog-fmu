graph TD

%% =========================
%% MAIN THREAD
%% =========================

    A[main.cpp Start]
    A --> B[StartNetworks]
    B --> C[Load FMU & SetupMappingFile]
    C --> E[Start FMU]

    E --> F[Create FMU Thread]
    F --> H[Run EtherCAT Main Loop]

%% =========================
%% THREAD SPLIT
%% =========================

    subgraph MainThread [Main Thread]
        H --> H1[etherdog.run]
        H1 --> H2[FrameHandler Loop]

        H2 --> H3[Receive EtherCAT Frame]
        H3 --> H4[Lock Mutex]
        H4 --> H5[Process Datagram and PDO Memory]
        H5 --> H6[Unlock Mutex]
        H6 --> H7[Send EtherCAT Response]

        H7 --> H2
    end

    subgraph FMUThread [FMU Thread]
        F --> F2[Loop Forever]

        F2 --> F3[step]

        F3 --> F4[ExecutePdoOutputMappings]
        F4 --> F5[Lock Mutex]
        F5 --> F6[Read Output PDOs & Write FMU Inputs]
        F6 --> F8[Unlock Mutex]

        F8 --> F9[FMU Step]

        F9 --> F10[ExecutePdoInputMappings]
        F10 --> F11[Lock Mutex]
        F11 --> F12[Read FMU Outputs & Write Input PDOs]
        F12 --> F14[Unlock Mutex]

        F14 --> F15[Sleep stepSize]
        F15 --> F2
    end