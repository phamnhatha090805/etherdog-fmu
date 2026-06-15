import sys
import json
import subprocess
import threading
import os
import signal

from PyQt5.QtWidgets import (
    QApplication,
    QWidget,
    QLabel,
    QLineEdit,
    QPushButton,
    QTextEdit,
    QVBoxLayout,
    QHBoxLayout,
    QFileDialog,
    QComboBox,
)

from PyQt5.QtCore import pyqtSignal


def get_interfaces():
    try:
        return os.listdir("/sys/class/net/")
    except Exception:
        return ["eth0"]


class SimulatorGUI(QWidget):
    output_signal = pyqtSignal(str)

    def __init__(self):
        super().__init__()

        self.process = None  # -- To keep track of the running simulation process
        self.output_signal.connect(
            self.append_output
        )  # -- Connect the output signal to the append_output method

        self.setWindowTitle("TwinDOG Simulator GUI")
        self.setGeometry(200, 200, 1200, 800)

        # --- Executable Selection ---
        self.executable_label = QLabel("TwinDOG Executable:")
        self.executable_input = QLineEdit()
        self.executable_input.setText("/home/etherdog/etherdog-fmu/build/etherdog-fmu")
        self.executable_browse_btn = QPushButton("Browse")
        self.executable_browse_btn.clicked.connect(self.browse_executable)

        # --- Network Interface Selection ---
        self.interface_label = QLabel("Network Interface:")
        self.interface_dropdown = QComboBox()
        self.interface_dropdown.addItems(get_interfaces())

        self.config_label = QLabel("Main Config JSON:")
        self.config_input = QLineEdit()
        self.config_browse_btn = QPushButton("Browse")
        self.config_browse_btn.clicked.connect(self.browse_config)

        # -- Run Button ---
        self.run_btn = QPushButton("Run Simulation")
        self.run_btn.clicked.connect(self.run_simulation)

        # -- Stop Button ---
        self.stop_btn = QPushButton("Stop Simulation")
        self.stop_btn.clicked.connect(self.stop_simulation)

        # -- Output Display ---
        self.output = QTextEdit()
        self.output.setReadOnly(True)

        # -- Layout ---
        layout = QVBoxLayout()  # -- Create a vertical layout for the main window

        exe_layout = (
            QHBoxLayout()
        )  # -- Create a horizontal layout for the executable input and browse button
        exe_layout.addWidget(self.executable_input)
        exe_layout.addWidget(self.executable_browse_btn)

        config_layout = (
            QHBoxLayout()
        )  # -- Create a horizontal layout for the config input and browse button
        config_layout.addWidget(self.config_input)
        config_layout.addWidget(self.config_browse_btn)

        layout.addWidget(
            self.executable_label
        )  # -- Add the executable label to the main layout
        layout.addLayout(
            exe_layout
        )  # -- Add the executable input and browse button layout to the main layout

        layout.addWidget(
            self.interface_label
        )  # -- Add the network interface label to the main layout
        layout.addWidget(
            self.interface_dropdown
        )  # -- Add the network interface dropdown to the main layout

        layout.addWidget(
            self.config_label
        )  # -- Add the config label to the main layout
        layout.addLayout(
            config_layout
        )  # -- Add the config input and browse button layout to the main layout

        layout.addWidget(self.run_btn)  # -- Add the run button to the main layout
        layout.addWidget(self.stop_btn)  # -- Add the stop button to the main layout
        layout.addWidget(self.output)  # -- Add the output display to the main layout

        self.setLayout(layout)  # -- Set the main layout for the window

    def browse_executable(
        self,
    ):  # -- Open a file dialog to select the TwinDOG executable
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Select etherdog-fmu executable", "", "Executable Files (*)"
        )
        if file_path:
            self.executable_input.setText(file_path)

    def browse_config(
        self,
    ):  # -- Open a file dialog to select the main Config.json file
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Select main Config.json", "", "JSON Files (*.json)"
        )
        if file_path:
            self.config_input.setText(file_path)

    def append_output(
        self, text
    ):  # -- Append text to the output display and scroll to the bottom
        self.output.append(text)
        scrollbar = self.output.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def validate_config_json(
        self, path
    ):  # -- Validate the structure of the main Config.json file and update the selected network interface
        try:
            with open(path, "r") as f:
                config = json.load(f)

            required_top_level = ["interface", "fmuPath", "slaves"]
            for key in required_top_level:
                if key not in config:
                    return False, f"Missing required field: {key}"

            if not isinstance(config["slaves"], list):
                return False, "'slaves' must be an array"

            for index, slave in enumerate(config["slaves"]):
                if "eeprom" not in slave:
                    return False, f"Slave {index} is missing required field: eeprom"

                if "coe_xml" not in slave:
                    self.append_output(
                        f"Warning: Slave {index} has no coe_xml, so PDO mapping dictionary may be unavailable"
                    )

                if "input-mappings" in slave and not isinstance(
                    slave["input-mappings"], dict
                ):
                    return False, f"Slave {index}: input-mappings must be an object"

                if "output-mappings" in slave and not isinstance(
                    slave["output-mappings"], dict
                ):
                    return False, f"Slave {index}: output-mappings must be an object"

            selected_interface = self.interface_dropdown.currentText()
            config["interface"] = selected_interface

            with open(path, "w") as f:
                json.dump(config, f, indent=4)

            return True, ""

        except Exception as e:
            return False, str(e)

    def run_simulation(
        self,
    ):  # -- Validate inputs, update the config JSON with the selected network interface, and run the TwinDOG simulation in a separate thread
        executable = self.executable_input.text().strip()
        config = self.config_input.text().strip()

        if not os.path.exists(executable):
            self.append_output("Executable not found")
            return

        if not os.path.exists(config):
            self.append_output("Config JSON file not found")
            return

        valid, error = self.validate_config_json(config)
        if not valid:
            self.append_output(f"Invalid Config JSON: {error}")
            return

        cmd = [executable, "-f", config]

        self.append_output(f"Running: {' '.join(cmd)}")

        thread = threading.Thread(target=self.execute_command, args=(cmd,))
        thread.daemon = True
        thread.start()

    def stop_simulation(
        self,
    ):  # -- Stop the running simulation by sending a SIGINT signal to the process, or display a message if no simulation is running
        if self.process is not None and self.process.poll() is None:
            self.append_output("Stopping simulation...")
            try:
                os.killpg(os.getpgid(self.process.pid), signal.SIGINT)
            except Exception as e:
                self.append_output(f"Error while stopping simulation: {e}")
        else:
            self.append_output("No simulation is currently running.")

    def execute_command(
        self, cmd
    ):  # -- Run the given command in a subprocess and capture its output to display in the GUI
        try:
            executable_dir = os.path.dirname(cmd[0])

            self.process = subprocess.Popen(
                cmd,
                cwd=executable_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                start_new_session=True,
            )

            for line in self.process.stdout:
                self.output_signal.emit(line.rstrip())

            self.process.wait()
            self.process = None
            self.output_signal.emit("Simulation finished")

        except Exception as e:
            self.output_signal.emit(f"Error while running simulation: {e}")


if (
    __name__ == "__main__"
):  # -- Main entry point to create the application and show the GUI
    app = QApplication(sys.argv)
    window = SimulatorGUI()

    # Maximized window with title bar
    window.showMaximized()

    # Use this instead if you want real fullscreen:
    # window.showFullScreen()

    sys.exit(app.exec_())
