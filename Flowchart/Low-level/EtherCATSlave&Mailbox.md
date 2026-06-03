graph TD
A[Start Network Initialization] --> B[Read interface, FMU path and slave list]
B --> C[Loop through configured slaves]
C --> D[Create EmulatedESC, PDO and Slave object]
D --> E{CoE XML available?}

E -- Yes --> F[Create mailbox]
F --> G[Load CoE dictionary from XML]
G --> H[Match device by Vendor ID, Product Code and Revision]
H --> I[Attach mailbox to slave]

E -- No --> J[Continue without mailbox]

I --> K[Allocate input and output PDO memory]
J --> K
K --> L[Initialize PDO memory]
L --> M[Set PDO input and output buffers]
M --> N[Store ESC PDO slave]
N --> O{More slaves?}

O -- Yes --> C
O -- No --> P[Configure ESC chain ports]
P --> Q[Open socket using network interface]
Q --> R[Set socket timeout]
R --> S[Start all slaves]
