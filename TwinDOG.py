import sys
import json
import subprocess
import threading
import os
import signal
import zipfile
import base64

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
    QFrame,
)

from PyQt5.QtCore import pyqtSignal, Qt
from PyQt5.QtGui import QPixmap


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
        self.load_process = (
            None  # -- Separate process for loading/printing the configuration only
        )
        self.configuration_loaded = False
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

        # -- Load Configuration Button ---
        self.load_config_btn = QPushButton("Load Configuration")
        self.load_config_btn.clicked.connect(self.load_configuration)

        # -- Run Button ---
        self.run_btn = QPushButton("Run Simulation")
        self.run_btn.clicked.connect(self.run_simulation)

        # -- Stop Button ---
        self.stop_btn = QPushButton("Stop Simulation")
        self.stop_btn.clicked.connect(self.stop_simulation)

        # -- FMU Model Image Display ---
        self.model_image_original = None
        self.model_image_label = QLabel(
            "FMU model image will appear here after Load Configuration."
        )
        self.model_image_label.setAlignment(Qt.AlignCenter)
        self.model_image_label.setMinimumHeight(260)
        self.model_image_label.setFrameShape(QFrame.StyledPanel)
        self.model_image_label.setScaledContents(False)

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

        layout.addWidget(
            self.load_config_btn
        )  # -- Add the load configuration button to the main layout
        layout.addWidget(self.run_btn)  # -- Add the run button to the main layout
        layout.addWidget(self.stop_btn)  # -- Add the stop button to the main layout
        layout.addWidget(QLabel("FMU Model Preview:"))
        layout.addWidget(self.model_image_label)
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

    def clear_fmu_model_image(self, message="No FMU model image found."):
        # -- Clear the current FMU image preview and show a readable status message.
        self.model_image_original = None
        self.model_image_label.setPixmap(QPixmap())
        self.model_image_label.setText(message)
        self.model_image_label.setToolTip("")

    def resizeEvent(self, event):
        # -- Keep the FMU image preview scaled when the window is resized.
        super().resizeEvent(event)
        self.update_fmu_model_image_size()

    def update_fmu_model_image_size(self):
        # -- Scale the original image to fit the QLabel while preserving aspect ratio.
        if self.model_image_original is None or self.model_image_original.isNull():
            return

        target_size = self.model_image_label.size()
        if target_size.width() <= 0 or target_size.height() <= 0:
            return

        scaled = self.model_image_original.scaled(
            target_size,
            Qt.KeepAspectRatio,
            Qt.SmoothTransformation,
        )
        self.model_image_label.setPixmap(scaled)

    def resolve_fmu_path_from_config(self, config_path):
        # -- Read fmuPath from the selected JSON config and resolve relative paths.
        try:
            with open(config_path, "r") as f:
                config = json.load(f)
        except Exception as e:
            return None, f"Could not read config JSON: {e}"

        fmu_path = config.get("fmuPath")
        if not fmu_path:
            return None, "Config JSON has no fmuPath field."

        fmu_path = os.path.expanduser(fmu_path)

        if os.path.isabs(fmu_path):
            return os.path.normpath(fmu_path), ""

        config_dir = os.path.dirname(os.path.abspath(config_path))
        executable_dir = os.path.dirname(
            os.path.abspath(self.executable_input.text().strip())
        )

        candidates = [
            os.path.normpath(os.path.join(config_dir, fmu_path)),
            os.path.normpath(os.path.join(executable_dir, fmu_path)),
            os.path.normpath(os.path.abspath(fmu_path)),
        ]

        for candidate in candidates:
            if os.path.exists(candidate):
                return candidate, ""

        return candidates[0], "FMU file does not exist at the resolved path."

    def show_fmu_model_image(self):
        # -- Show model.png from inside the FMU, similar to how FMPy does it.
        config_path = self.config_input.text().strip()
        fmu_path, error = self.resolve_fmu_path_from_config(config_path)

        if error:
            self.clear_fmu_model_image(error)
            self.append_output(f"FMU image preview: {error}")
            return

        try:
            with zipfile.ZipFile(fmu_path, "r") as fmu_zip:
                # FMPy expects the FMU diagram at the root of the FMU as model.png.
                model_png_name = None
                for name in fmu_zip.namelist():
                    if name.lower() == "model.png":
                        model_png_name = name
                        break

                if model_png_name is None:
                    self.clear_fmu_model_image("This FMU does not contain model.png.")
                    self.append_output(
                        "FMU image preview: no model.png found inside the FMU."
                    )
                    return

                image_bytes = fmu_zip.read(model_png_name)

            pixmap = QPixmap()
            if not pixmap.loadFromData(image_bytes, "PNG"):
                self.clear_fmu_model_image(
                    "model.png exists, but Qt could not load it."
                )
                self.append_output(
                    "FMU image preview: model.png could not be loaded as a PNG image."
                )
                return

            self.model_image_original = pixmap
            self.model_image_label.setText("")
            self.update_fmu_model_image_size()

            # Show the full unscaled image in the tooltip, like FMPy does.
            encoded = base64.b64encode(image_bytes).decode("ascii")
            self.model_image_label.setToolTip(
                f'<img src="data:image/png;base64,{encoded}">'
            )

            self.append_output(f"FMU image preview loaded: {fmu_path}")

        except zipfile.BadZipFile:
            self.clear_fmu_model_image("Selected FMU is not a valid ZIP/FMU file.")
            self.append_output(
                "FMU image preview: selected FMU is not a valid ZIP/FMU file."
            )
        except Exception as e:
            self.clear_fmu_model_image(f"Could not load FMU image: {e}")
            self.append_output(f"FMU image preview error: {e}")

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

    def get_validated_command_base(self):
        executable = self.executable_input.text().strip()
        config = self.config_input.text().strip()

        if not os.path.exists(executable):
            self.append_output("Executable not found")
            return None

        if not os.path.exists(config):
            self.append_output("Config JSON file not found")
            return None

        valid, error = self.validate_config_json(config)
        if not valid:
            self.append_output(f"Invalid Config JSON: {error}")
            return None

        return [executable, "-f", config]

    def load_configuration(self):
        # -- Load and print all configuration information, but do not start the simulation.
        if self.process is not None and self.process.poll() is None:
            self.append_output(
                "Simulation is already running. Stop it before loading configuration again."
            )
            return

        if self.load_process is not None and self.load_process.poll() is None:
            self.append_output("Configuration is already being loaded.")
            return

        cmd = self.get_validated_command_base()
        if cmd is None:
            return

        self.show_fmu_model_image()

        cmd.append("--load-config-only")
        self.configuration_loaded = False
        self.append_output(f"Loading configuration: {' '.join(cmd)}")

        thread = threading.Thread(target=self.execute_command, args=(cmd, "load"))
        thread.daemon = True
        thread.start()

    def run_simulation(
        self,
    ):  # -- Validate inputs, update the config JSON with the selected network interface, and run the TwinDOG simulation in a separate thread
        if self.process is not None and self.process.poll() is None:
            self.append_output("Simulation is already running.")
            return

        cmd = self.get_validated_command_base()
        if cmd is None:
            return

        self.show_fmu_model_image()
        self.append_output(f"Running: {' '.join(cmd)}")

        thread = threading.Thread(target=self.execute_command, args=(cmd, "simulation"))
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
        self, cmd, mode="simulation"
    ):  # -- Run the given command in a subprocess and capture its output to display in the GUI
        try:
            executable_dir = os.path.dirname(cmd[0])

            process = subprocess.Popen(
                cmd,
                cwd=executable_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                start_new_session=True,
            )

            if mode == "load":
                self.load_process = process
            else:
                self.process = process

            for line in process.stdout:
                self.output_signal.emit(line.rstrip())

            return_code = process.wait()

            if mode == "load":
                self.load_process = None
                if return_code == 0:
                    self.configuration_loaded = True
                    self.output_signal.emit(
                        "Configuration loaded. Simulation has not started yet."
                    )
                else:
                    self.output_signal.emit(
                        f"Configuration load failed with exit code {return_code}"
                    )
            else:
                self.process = None
                self.output_signal.emit("Simulation finished")

        except Exception as e:
            self.output_signal.emit(f"Error while running command: {e}")


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
