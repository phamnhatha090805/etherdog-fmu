graph TD

%% =========================
%% PROGRAM START
%% =========================

A[Program Start] --> C[StartNetworks argc argv]

C --> C1[Parse -f Config.json]
C1 --> C2[Open Main Config File]

C2 --> C3{Config File Opened?}

C3 -- No --> C4[Print Error and Return]
C3 -- Yes --> C5[Parse Main Config JSON]

C5 --> C7[Read interface and fmuPath]
C7 --> C8[Get slaves Array]

%% =========================
%% START NETWORKS
%% =========================

subgraph StartNetworks [StartNetwork]
    C8 --> S1[Reserve ESC PDO Slave Mailbox Vectors]

    S1 --> S2[Loop Through slaves Array]

    S2 --> S3[Read EEPROM Path from Slave Config]
    S3 --> S4[Create EmulatedESC]
    S4 --> S5[Create PDO]
    S5 --> S6[Create Slave]

    S6 --> S7{coe_xml Exists?}

    S7 -- Yes --> S8[Read CoE XML Path]
    S8 --> S9[Create Mailbox]
    S9 --> S10[Load Devices from CoE XML]
    S10 --> S11[Read Vendor ID Product Code Revision Number]
    S11 --> S12[Find Matching Device]
    S12 --> S13[Enable CoE Dictionary]
    S13 --> S14[Attach Mailbox to Slave]
    S14 --> S15[Store Mailbox]

    S7 -- No --> S16[Skip Mailbox Setup]

    S15 --> S17[Create Input PDO Memory]
    S16 --> S17

    S17 --> S18[Initialize Input PDO Memory]
    S18 --> S19[Create Output PDO Memory]
    S19 --> S20[Initialize Output PDO Memory]
    S20 --> S21[Set PDO Input and Output Buffers]

    S21 --> S22[Store ESC PDO Slave]
    S22 --> S23{More Slaves?}

    S23 -- Yes --> S2
    S23 -- No --> S24[Configure ESC DL Status for Chain Ports]
    S24 --> S25[Open Socket Using interface]
    S25 --> S26[Set Socket Timeout]
    S26 --> S27[Start All Slaves]
end