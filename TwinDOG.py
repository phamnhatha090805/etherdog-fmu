import sys
import json
import subprocess
import threading
import os

from PyQt5.QtWidgets import (
    QApplication, QWidget, QLabel, QLineEdit, QPushButton,
    QTextEdit, QVBoxLayout, QHBoxLayout, QFileDialog, QComboBox
)


def get_interfaces():
    try:
        return os.listdir("/sys/class/net/")
    except Exception:
        return ["eth0"]


class SimulatorGUI(QWidget):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("TwinDOG Simulator GUI")
        self.setGeometry(200, 200, 1200, 800)

        self.executable_label = QLabel("TwinDOG Executable:")
        self.executable_input = QLineEdit()
        self.executable_input.setText("/home/etherdog/etherdog-fmu/build/etherdog-fmu")
        self.executable_browse_btn = QPushButton("Browse")
        self.executable_browse_btn.clicked.connect(self.browse_executable)

        self.interface_label = QLabel("Network Interface:")
        self.interface_dropdown = QComboBox()
        self.interface_dropdown.addItems(get_interfaces())

        self.config_label = QLabel("Main Config JSON:")
        self.config_input = QLineEdit()
        self.config_browse_btn = QPushButton("Browse")
        self.config_browse_btn.clicked.connect(self.browse_config)

        self.run_btn = QPushButton("Run Simulation")
        self.run_btn.clicked.connect(self.run_simulation)

        self.output = QTextEdit()
        self.output.setReadOnly(True)

        layout = QVBoxLayout()

        exe_layout = QHBoxLayout()
        exe_layout.addWidget(self.executable_input)
        exe_layout.addWidget(self.executable_browse_btn)

        config_layout = QHBoxLayout()
        config_layout.addWidget(self.config_input)
        config_layout.addWidget(self.config_browse_btn)

        layout.addWidget(self.executable_label)
        layout.addLayout(exe_layout)

        layout.addWidget(self.interface_label)
        layout.addWidget(self.interface_dropdown)

        layout.addWidget(self.config_label)
        layout.addLayout(config_layout)

        layout.addWidget(self.run_btn)
        layout.addWidget(self.output)

        self.setLayout(layout)

    def browse_executable(self):
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select etherdog-fmu executable",
            "",
            "Executable Files (*)"
        )
        if file_path:
            self.executable_input.setText(file_path)

    def browse_config(self):
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select main Config.json",
            "",
            "JSON Files (*.json)"
        )
        if file_path:
            self.config_input.setText(file_path)

    def append_output(self, text):
        self.output.append(text)
        scrollbar = self.output.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def validate_config_json(self, path):
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

                if "input-mappings" in slave and not isinstance(slave["input-mappings"], dict):
                    return False, f"Slave {index}: input-mappings must be an object"

                if "output-mappings" in slave and not isinstance(slave["output-mappings"], dict):
                    return False, f"Slave {index}: output-mappings must be an object"

            selected_interface = self.interface_dropdown.currentText()
            config["interface"] = selected_interface

            with open(path, "w") as f:
                json.dump(config, f, indent=4)

            return True, ""

        except Exception as e:
            return False, str(e)

    def run_simulation(self):
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

        cmd = [
            executable,
            "-f",
            config
        ]

        self.append_output(f"Running: {' '.join(cmd)}")

        thread = threading.Thread(target=self.execute_command, args=(cmd,))
        thread.daemon = True
        thread.start()

    def execute_command(self, cmd):
        try:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )

            for line in process.stdout:
                self.append_output(line.rstrip())

            process.wait()
            self.append_output("Simulation finished")

        except Exception as e:
            self.append_output(f"Error while running simulation: {e}")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SimulatorGUI()

    # Maximized window with title bar
    window.showMaximized()

    # Use this instead if you want real fullscreen:
    # window.showFullScreen()

    sys.exit(app.exec_())