graph TD
    %% Base Setup
    Start([Run Script]) --> GetNet[get_interfaces]
    GetNet -->|Reads /sys/class/net/| CreateApp[Initialize QApplication & SimulatorGUI]
    
    subgraph UI Layout Initialization
        CreateApp --> InitUI[Setup Layouts & Elements]
        InitUI --> E[Executable Input Field]
        InitUI --> N[Network Interface Dropdown]
        InitUI --> C[Config JSON Input Field]
        InitUI --> R[Run Simulation Button]
        InitUI --> S[Stop Simulation Button]
        InitUI --> O[Read-only Output display]
    end

    %% Interactions
    E_Btn[Browse Executable Clicked] --> BrowseExe[browse_executable]
    BrowseExe -->|Select File| E
    
    C_Btn[Browse Config Clicked] --> BrowseConfig[browse_config]
    BrowseConfig -->|Select JSON| C