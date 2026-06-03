graph TD
    %% Running Simulation Sequence
    RunClick[Click 'Run Simulation'] --> CheckExe{Executable Path<br>Exists?}
    
    CheckExe -->|No| Err1[Log: 'Executable not found'] --> PrintUI
    CheckExe -->|Yes| CheckCfg{Config Path<br>Exists?}
    
    CheckCfg -->|No| Err2[Log: 'Config JSON file not found'] --> PrintUI
    CheckCfg -->|Yes| ValJSON[validate_config_json]
    
    %% Validation Block
    subgraph JSON Validation Loop
        ValJSON --> FieldCheck{Has interface, fmuPath,<br>and slaves list?}
        FieldCheck -->|No| RetFalse[Return False + Error Message]
        FieldCheck -->|Yes| SlaveLoop{Check all slaves<br>for eeprom & mappings}
        SlaveLoop -->|Invalid| RetFalse
        SlaveLoop -->|Valid| UpdateInt[Overwrite JSON interface key with <br> selected GUI dropdown value]
        UpdateInt --> RetTrue[Return True]
    end
    
    RetFalse --> Err3[Log: 'Invalid Config JSON'] --> PrintUI
    RetTrue --> BuildCmd[Build Command Array]
    
    %% Threading Worker
    BuildCmd --> StartThread[Spawn Daemon Thread:<br>execute_command]
    StartThread --> SubProc[subprocess.Popen<br>start_new_session=True]
    
    subgraph Background Thread Active
        SubProc --> ReadStream{Read lines from<br>stdout/stderr}
        ReadStream -->|Line available| EmitSig[output_signal.emit line]
        EmitSig -->|PyQtSignal Connection| PrintUI[append_output to display]
        PrintUI --> Scroll[Scroll text window to bottom]
        ReadStream -->|Stream closed| WaitProc[process.wait]
        WaitProc --> EndThread[Emit: 'Simulation finished'] --> PrintUI
    end

    %% Stopping Simulation Sequence
    StopClick[Click 'Stop Simulation'] --> CheckActive{Is process running?}
    CheckActive -->|No| Err4[Log: 'No simulation is running'] --> PrintUI
    CheckActive -->|Yes| Terminate[os.killpg with signal.SIGINT]
    Terminate -->|Process dies| ReadStream
