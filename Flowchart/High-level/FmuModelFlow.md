flowchart TD
    A[Start FMU Simulation Loop] --> B[Read Output PDO Data]
    B --> C[Convert PDO Data to FMU Input Values]
    C --> D[Write Values to FMU Inputs]

    D --> E[Execute FMU Simulation Step]
    E --> F[Read FMU Output Values]

    F --> G[Convert FMU Outputs to PDO Data]
    G --> H[Write Values to Input PDO Memory]

    H --> I[Wait Until Next Simulation Step]
    I --> J{Stop Requested?}

    J -- No --> B
    J -- Yes --> K[Terminate FMU Simulation]