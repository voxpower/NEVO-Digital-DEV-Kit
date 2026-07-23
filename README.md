# NEVO_Digital_DevKit

## Project Overview

The NEVO_Digital_DevKit is an open-source development kit for power management bus (PMBus) applications, built on the ESP-IDF framework (v5.4). It provides a flexible platform for controlling and monitoring PMBus-compatible devices, offering various communication interfaces including USBTMC, TCP/IP, and a web interface.

This project is developed by Vox-Power Ltd in collaboration with SOLINGEL TECH.

## Features

*   **PMBus Communication:** Control and monitor PMBus devices.
*   **Multiple Interfaces:**
    *   **USBTMC (USB Test and Measurement Class):** Standardized USB interface for test and measurement instruments.
    *   **TCP/IP (SCPI over Sockets):** Remote control and monitoring via Ethernet/Wi-Fi.
    *   **Web Interface:** User-friendly web-based control and visualization.
*   **SoftAP Mode:** Easily set up a Wi-Fi access point for device configuration.
*   **Configurable Device Support:** Supports multiple PMBus devices.
*   **FreeRTOS Integration:** Efficient task management for real-time operations.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

*   **ESP-IDF v5.4:** Follow the official Espressif IoT Development Framework (ESP-IDF) installation guide for your operating system.
    *   [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/get-started/index.html)
*   **CMake:** Ensure CMake is installed and configured.
*   **Python 3.x:** Required by ESP-IDF.

### Installation

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/voxpower/NEVO-Digital-DEV-Kit.git
    cd NEVO-Digital-DEV-Kit
    ```

2.  **Set up ESP-IDF environment:**
    Follow the ESP-IDF "Get Started" guide to set up your environment variables (e.g., `IDF_PATH`). This typically involves running `export.bat` (Windows) or `export.sh` (Linux/macOS) from your ESP-IDF installation directory.

3.  **Configure the project:**
    ```bash
    idf.py set-target esp32s3 # Or your target chip
    idf.py menuconfig
    ```
    In `menuconfig`, you can configure various project settings, including Wi-Fi credentials (if not using SoftAP), logging levels, etc.

4.  **Build the project:**
    ```bash
    idf.py build
    ```

5.  **Flash to device:**
    Connect your ESP32-S3 development board (or chosen target) and flash the firmware:
    ```bash
    idf.py -p (YOUR_SERIAL_PORT) flash monitor
    ```
    Replace `(YOUR_SERIAL_PORT)` with the actual serial port of your device.

## Usage

Once flashed, the device will start in SoftAP mode (default SSID: "VOXPOWER SOFTAP").

### Web Interface

1.  Connect to the Wi-Fi network named "VOXPOWER SOFTAP" (or whatever you configured).
2.  Open a web browser and navigate to `http://192.168.4.1` (default SoftAP IP).
3.  You can monitor and control PMBus devices through the web interface.

### USBTMC

1.  Connect the device to your PC via USB.
2.  The device should enumerate as a USBTMC instrument.
3.  Use any compatible test and measurement software (e.g., LabVIEW, Python with `PyVISA`) to send SCPI commands.

### TCP/IP (SCPI over Sockets)

1.  If the device is configured to connect to an existing Wi-Fi network or Ethernet, find its IP address (e.g., via your router or serial monitor).
2.  Connect to the device's IP address on port `5025` (standard SCPI port) using a telnet client or custom software.
3.  Send SCPI commands over the TCP connection.

## Getting Started in an IDE

### Espressif-IDE (Eclipse-based)

IDE project files (`.project`, `.cproject`) are not stored in the repository and are generated locally on first import.

1.  Open Espressif-IDE.
2.  Go to **File → Import → Espressif → Import IDF Project**.
3.  Click **Browse** and select the cloned `NEVO_Digital_DevKit` folder.
4.  Click **Finish**. The IDE will generate the project files automatically from `CMakeLists.txt`.
5.  The generated `.project` and `.cproject` files are covered by `.gitignore` and will not be committed.

### VS Code with ESP-IDF Extension

1.  Install the [ESP-IDF Extension for VS Code](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).
2.  Open VS Code and select **File → Open Folder**, then select the cloned `NEVO_Digital_DevKit` folder.
3.  The extension will automatically detect the `CMakeLists.txt` and configure the project.
4.  Use the status bar buttons or the **ESP-IDF: Build, Flash and Monitor** command to build and flash.

---

## SCPI Commands

Refer to the `scpi-def.c` and `scpi-def.h` files for a list of supported SCPI commands and their syntax.

## Contributing

We welcome contributions to the NEVO_Digital_DevKit project! Please follow these guidelines:

1.  **Fork the repository.**
2.  **Create a new branch** for your feature or bug fix: `git checkout -b feature/your-feature-name` or `git checkout -b bugfix/your-bug-fix`.
3.  **Make your changes.**
4.  **Ensure your code adheres to the project's coding style** (run `clang-format` if applicable).
5.  **Write clear, concise commit messages.**
6.  **Push your branch** to your forked repository.
7.  **Open a Pull Request** to the `main` branch of the original repository, describing your changes in detail.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contact

For questions or support, please open an issue on the GitHub repository.

---
Developed by:
*   Francisco Almendros / SOLINGEL TECH.
*   Sergi Gomis / SOLINGEL TECH.
*   Brian McDonald / Vox-Power Ltd.