graph TD

%% =========================
%% ETHERCAT THREAD
%% =========================

    subgraph EtherCATThread [Thread 1: EtherCAT Communication]

    B1[run] --> B2[FrameHandler Loop]

    B2 --> B3[Receive EtherCAT Frames]

    B3 --> B4[Lock Mutex]

    B4 --> B5[Process ESC Datagram]

    B5 --> B6[Slave Routine & PDO Validation]

    B6 --> B7[Unlock Mutex]

    B7 --> B8[Send EtherCAT Response]

    B8 --> B2

    end

%% =========================
%% FMU THREAD
%% =========================

    subgraph FMUThread [Thread 2: FMU Simulation]

    C1[FmuThread Loop]

    C1 --> C2[Execute step]

%% OUTPUT MAPPINGS

    C2 --> C3[ExecuteOutputMappings]

    C3 --> C4[Lock Mutex]

    C4 --> C5[Read EtherCAT Output PDO Memory]

    C5 --> C6[Write Values into FMU Inputs]

    C6 --> C7[Unlock Mutex]

%% FMU STEP

    C7 --> C8[FMU step]

%% INPUT MAPPINGS

    C8 --> C9[ExecuteInputMappings]

    C9 --> C10[Lock Mutex]

    C10 --> C11[Read FMU Outputs]

    C11 --> C12[Write Values into EtherCAT Input PDO Memory]

    C12 --> C13[Unlock Mutex]

    C13 --> C14[Sleep stepSize]

    C14 --> C1

    end

%% =========================
%% SHARED DATA
%% =========================

    B5 -. accesses shared PDO memory .- C5
    C12 -. updates shared PDO memory .- B6