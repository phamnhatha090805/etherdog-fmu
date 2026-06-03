graph TD

A[Load FMU Path from Configuration] --> B[Create FMU Object]

B --> C[Get Co-Simulation FMU]
C --> D[Read Model Description]
D --> E[Create FMU Instance]

E --> F[Setup Experiment]
F --> G[Enter Initialization Mode]
G --> H[Exit Initialization Mode]

H --> I[FMU Ready]

I --> J[Start Simulation Loop]

J --> K[Receive Inputs from Mapping Layer]
K --> L[Write Values to FMU Inputs]

L --> M[Execute FMU Step]
M --> N{Step Successful?}

N -- No --> O[Report FMU Error]
N -- Yes --> P[Update Simulation Time]

P --> Q[Read FMU Outputs]
Q --> R[Send Outputs to Mapping Layer]

R --> S[Wait stepSize]
S --> J
