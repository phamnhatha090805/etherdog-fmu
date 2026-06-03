graph TD
A[Start Mapping Setup] --> B[Loop through slave configs]
B --> C{Mailbox and CoE dictionary exist?}

C -- No --> D[Skip mapping for this slave]
C -- Yes --> E[Read input-mappings and output-mappings]

E --> F[Loop through mapping entries]
F --> G[Read FMU variable name]
G --> H[Read PDO entry name]
H --> I[Find FMU variable and type]
I --> J{FMU type supported?}

J -- No --> K[Print warning and skip mapping]
J -- Yes --> L[Search PDO entry in CoE dictionary]

L --> M{PDO entry found?}
M -- No --> N[Report mapping error]
M -- Yes --> O[Store mapping with slave index, PDO entry, FMU value reference and type]

O --> P{More mappings?}
K --> P
N --> P

P -- Yes --> F
P -- No --> Q{More slave configs?}
D --> Q

Q -- Yes --> B
Q -- No --> R[Mapping setup complete]
