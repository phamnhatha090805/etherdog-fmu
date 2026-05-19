graph TD

    L1[Loop Through JSON Mappings] --> L2[Read FMU Variable Name]
    L2 --> L3[Read PDO Name]

    L3 --> L4[Get FMU Variable by Name]
    L4 --> L5{Detect FMU Variable Type}

    L5 -- Real --> L6[Set fmuVarType = REAL]
    L5 -- Integer --> L7[Set fmuVarType = INTEGER32]
    L5 -- Boolean --> L8[Set fmuVarType = BOOLEAN]
    L5 -- Unsupported --> L9[Print Warning and Skip Mapping]

    L6 --> L10[Get FMU Value Reference]
    L7 --> L10
    L8 --> L10

    L10 --> L11[Set mapping_found false]

    L11 --> L12[Loop Through EtherCAT Objects]
    L12 --> L13[Loop Through Object Entries]

    L13 --> L14{entry.description equals PDO Name?}

    L14 -- Yes --> L15[Create Mapping Struct]
    L15 --> L16[Store VR, Size, PDO Name, Slave Index, FMU Name, FMU Type, CoE Entry]
    L16 --> L17[Push Mapping into Mapping Vector]
    L17 --> L18[Set mapping_found true]
    L18 --> L19[Break Entry Loop]

    L14 -- No --> L13

    L19 --> L20{mapping_found?}

    L20 -- Yes --> L21[Break Object Loop]
    L20 -- No --> L12

    L21 --> L22{mapping_found false?}

    L22 -- Yes --> L23[Throw PDO Entry Not Found Error]
    L22 -- No --> L24{More JSON Mappings?}

    L9 --> L24

    L24 -- Yes --> L1
    L24 -- No --> L25[Return]