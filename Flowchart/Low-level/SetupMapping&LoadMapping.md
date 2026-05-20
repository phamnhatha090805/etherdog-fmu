graph TD

%% =========================
%% SETUP MAPPING FILE
%% =========================

subgraph SetupMappingFile [SetupMappingFile]
    E[SetupMappingFile] --> E1[Get slaves Array from main_config]

    E1 --> E2[Loop Through Each Slave Config by Index i]

    E2 --> E3{Mailbox Exists for Slave i?}

    E3 -- No --> E4[Print Warning and Continue]

    E3 -- Yes --> E5[Get EtherCAT Dictionary from Mailbox i]

    E5 --> E6{input-mappings Exists?}

    E6 -- Yes --> E7[Get input-mappings]
    E7 --> E8[LoadMapping for input_mappings]

    E6 -- No --> E9[Skip Input Mapping]

    E8 --> E10{output-mappings Exists?}
    E9 --> E10

    E10 -- Yes --> E11[Get output-mappings]
    E11 --> E12[LoadMapping for output_mappings]

    E10 -- No --> E13[Skip Output Mapping]

    E12 --> E14{More Slave Configs?}
    E13 --> E14
    E4 --> E14

    E14 -- Yes --> E2
    E14 -- No --> E15[Return]
end

%% =========================
%% LOAD MAPPING
%% =========================

subgraph LoadMapping [LoadMapping]
    E8 --> M1[Loop Through JSON Mapping Entries]
    E12 --> M1

    M1 --> M2[Read FMU Variable Name]
    M2 --> M3[Read PDO Entry Description Name]
    M3 --> M4[Get FMU Variable by Name]

    M4 --> M5{FMU Variable Type?}

    M5 -- Real --> M6[Store Value Reference and Type REAL]
    M5 -- Integer --> M7[Store Value Reference and Type INTEGER32]
    M5 -- Boolean --> M8[Store Value Reference and Type BOOLEAN]
    M5 -- Unsupported --> M9[Print Warning and Skip Mapping]

    M6 --> M10[Search PDO Entry in CoE Dictionary]
    M7 --> M10
    M8 --> M10

    M10 --> M11{PDO Entry Found?}

    M11 -- Yes --> M12[Store Mapping: VR Size PDO Name Slave Index FMU Name FMU Type PDO Entry]
    M11 -- No --> M13[Throw PDO Entry Not Found Error]

    M12 --> M14{More Mapping Entries?}
    M9 --> M14

    M14 -- Yes --> M1
    M14 -- No --> M15[Return to SetupMappingFile]
end