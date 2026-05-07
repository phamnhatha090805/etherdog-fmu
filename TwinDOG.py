import sys
import json
import subprocess
import threading
import os

from PyQt5.QtWidgets import (
    QApplication, QWidget, QLabel, QLineEdit, QPushButton,
    QTextEdit, QVBoxLayout, QHBoxLayout, QFileDialog, QComboBox
)

# --- Helper: detect network interfaces ---
def get_interfaces():
    try:
        interfaces = os.listdir('/sys/class/net/')
        return interfaces
    except Exception:
        return ["eth0"]


class SimulatorGUI(QWidget):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("KickCAT Network Simulator GUI")
        self.setGeometry(200, 200, 600, 500)

        # --- Interface dropdown ---
        self.interface_label = QLabel("Network Interface:")
        self.interface_dropdown = QComboBox()
        self.interface_dropdown.addItems(get_interfaces())

        # --- Slave count ---
        self.slave_label = QLabel("Number of Slaves:")
        self.slave_input = QLineEdit()
        self.slave_input.setPlaceholderText("e.g. 5")

        # --- JSON config ---
        self.config_label = QLabel("Slave Config (.json):")
        self.config_input = QLineEdit()
        self.browse_btn = QPushButton("Browse")
        self.browse_btn.clicked.connect(self.browse_file)

        # --- Run button ---
        self.run_btn = QPushButton("Run Simulation")
        self.run_btn.clicked.connect(self.run_simulation)

        # --- Output ---
        self.output = QTextEdit()
        self.output.setReadOnly(True)

        # --- Layouts ---
        layout = QVBoxLayout()

        layout.addWidget(self.interface_label)
        layout.addWidget(self.interface_dropdown)

        layout.addWidget(self.slave_label)
        layout.addWidget(self.slave_input)

        config_layout = QHBoxLayout()
        config_layout.addWidget(self.config_input)
        config_layout.addWidget(self.browse_btn)

        layout.addWidget(self.config_label)
        layout.addLayout(config_layout)

        layout.addWidget(self.run_btn)
        layout.addWidget(self.output)

        self.setLayout(layout)

    # --- File picker ---
    def browse_file(self):
        file_path, _ = QFileDialog.getOpenFileName(self, "Select JSON file", "", "JSON Files (*.json)")
        if file_path:
            self.config_input.setText(file_path)

    # --- JSON validation ---
    def validate_json(self, path):
        try:
            with open(path, 'r') as f:
                json.load(f)
            return True, ""
        except Exception as e:
            return False, str(e)

    # --- Run simulator ---
    def run_simulation(self):
        interface = self.interface_dropdown.currentText()
        slaves = self.slave_input.text()
        config = self.config_input.text()

        # --- Input validation ---
        if not slaves.isdigit():
            self.output.append("Number of slaves must be an integer\n")
            return

        if not os.path.exists(config):
            self.output.append("JSON file not found\n")
            return

        valid, error = self.validate_json(config)
        if not valid:
            self.output.append(f"Invalid JSON: {error}\n")
            return

        # --- Command ---
        cmd = [

            "/home/etherdog/etherdog-fmu/build/etherdog-fmu",
            "-i", interface,
            "-n", slaves,
            "-s", config
        ]

        self.output.append(f"Running: {' '.join(cmd)}\n")

        # Run in separate thread to avoid UI freeze
        thread = threading.Thread(target=self.execute_command, args=(cmd,))
        thread.start()

    # --- Execute command + live output ---
    def execute_command(self, cmd):
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )

        for line in process.stdout:
            self.output.append(line)

        process.wait()
        self.output.append("\nSimulation finished\n")


# --- Run app ---
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SimulatorGUI()
    window.show()
    sys.exit(app.exec_())