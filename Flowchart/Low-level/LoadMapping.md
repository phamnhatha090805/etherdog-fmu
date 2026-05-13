graph TD

    L1[Loop Through JSON Mappings] --> L2[Read FMU Variable Name]
    L2 --> L3[Read PDO Name]

    L3 --> L4[Get FMU Variable by Name]
    L4 --> L5[Get FMU Value Reference]

    L5 --> L6[Set mapping_found false]

    L6 --> L7[Loop Through EtherCAT Objects]
    L7 --> L8[Loop Through Object Entries]

    L8 --> L9{entry.description equals PDO Name?}

    L9 -- Yes --> L10[Create Mapping Struct with Slave Index]
    L10 --> L11[Push Mapping into Mapping Vector]
    L11 --> L12[Set mapping_found true]
    L12 --> L13[Break Entry Loop]

    L9 -- No --> L8

    L13 --> L14{mapping_found?}

    L14 -- Yes --> L15[Break Object Loop]
    L14 -- No --> L7

    L15 --> L16{mapping_found false?}

    L16 -- Yes --> L17[Throw PDO Entry Not Found Error]
    L16 -- No --> L18{More JSON Mappings?}

    L18 -- Yes --> L1
    L18 -- No --> L19[Return]