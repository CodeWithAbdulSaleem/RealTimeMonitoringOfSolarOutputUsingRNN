# ⚡ Real-Time Solar Output Monitoring & Prediction System V2.0

![Python](https://img.shields.io/badge/Python-3.8+-blue.svg)
![TensorFlow](https://img.shields.io/badge/TensorFlow-2.0+-orange.svg)
![Streamlit](https://img.shields.io/badge/Streamlit-1.0+-red.svg)
![Arduino](https://img.shields.io/badge/Arduino-ESP32-00979D.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

A professional-grade, AI-powered Solar Energy Monitoring and Prediction System designed to optimize the generation, consumption, and storage of solar power. Version 2.0 introduces advanced structural enhancements including an **RNN/LSTM predictive model**, **Fuzzy Logic** for intelligent load management, high-fidelity data visualization via **Streamlit**, and dual-display hardware architectures.

---

## 🎯 Project Overview

Managing and maximizing the efficiency of solar energy systems requires precise instrumentation and smart load balancing. This project provides an end-to-end framework starting from edge-device telemetry (ESP32) up to a centralized, predictive analytics dashboard.

The system leverages Machine Learning to forecast solar output based on environmental and historical data, empowering users or automated controllers to optimize usage and prevent critical battery depletion.

## 🚀 Key Features

* **🔋 Comprehensive Energy Telemetry:** Precision real-time tracking of Solar Panel Voltage, Current, Power Output, and State of Charge (SoC).
* **⚖️ Dynamic Load Monitoring:** Isolated energy tracking for connected operational devices (e.g., Motors & Cooling Fans) utilizing ACS712 Hall-effect current sensors.
* **🧠 AI-Driven Power Prediction:** Time-series forecasting implemented using Recurrent Neural Networks (RNN) and Long Short-Term Memory (LSTM) models for proactive energy management.
* **⚙️ Intelligent Fuzzy Logic Control:** Autonomous rule-based management engine. It actuates relay states based on real-time battery voltage, solar availability, and thermal indices to prevent irreversible deep discharge cycles.
* **📊 Professional Analytics Dashboard:** An interactive, low-latency web interface built in **Streamlit** for real-time telemetry visualization, dataset analysis, and ML inference comparisons.
* **🖥️ Edge Display Architecture:** Dual local monitoring outputs (128x64 OLED and 16x2 I2C LCD) ensuring data visibility independent of cloud connectivity.

---

## 🏗️ System Architecture

### 1. Hardware Stack

* **Microcontroller:** ESP32 (Wi-Fi Enabled 32-bit MCU)
* **Visual Displays:** 0.96" OLED (SSD1306) | 16x2 Character LCD (I2C)
* **Sensory Array:**
  * 3x ACS712 Current Sensors (5A/30A ratings)
  * Precision Voltage Divider Networks (Panel & Battery isolation)
  * DHT11/DHT22 Temperature & Humidity Sensor
* **Actuation (Control):** 2-Channel 5V Relay Module (handling high-current loads)

### 2. Software & AI Stack

* **Firmware:** C++ (Arduino Core)
* **Backend & Dashboard:** Python (Streamlit, Pandas, Plotly, NumPy)
* **Machine Learning:** TensorFlow / Keras (RNN, LSTM), Scikit-Learn (Data Preprocessing)

---

## 💻 Installation & Setup

### 1. ESP32 Firmware Configuration

1. Navigate to the `ArduinoIDE/` directory and open `esp32.ino` in the Arduino IDE.
2. Ensure you have the ESP32 board manager installed.
3. Install the following prerequisite libraries via the Library Manager:
   * `Adafruit SSD1306` & `Adafruit GFX Library`
   * `LiquidCrystal_I2C`
   * `DHT sensor library`
4. Modify the connection parameters:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
5. Compile and Flash the firmware to your ESP32 device.

### 2. Streamlit Analytics Dashboard

1. Ensure **Python 3.8+** is installed on your designated server or local machine.
2. Clone the repository and navigate to the project root.
3. Install the required Python dependencies:
   ```bash
   pip install streamlit pandas numpy requests plotly tensorflow scikit-learn matplotlib
   ```
4. Verify the ESP32 IP address matches the `ESP32_IP` configuration in `Website/dashboard.py`.
5. Launch the Streamlit server:
   ```bash
   streamlit run Website/dashboard.py
   ```

---

## 📈 Artificial Intelligence Integration

The platform incorporates both heuristic and deep learning approaches for robust energy management:

* **Fuzzy Inference Engine (Safety & Control):**
  * **Critical Mode:** Immediately shuts off auxiliary loads if Battery Voltage drops below 11.5V.
  * **Optimal Mode:** Engages relays dynamically based on solar power generation margins and healthy battery state.
* **Predictive Modeling (RNN vs LSTM):**
  Located in `Prediction/aiml.py`, the system trains on collected datasets, executing comparative analyses between SimpleRNN and LSTM models to accurately forecast future power generation trends over a 30-step sequence window.

---

## 📁 Repository Structure

```text
├── ArduinoIDE/               # ESP32 C++ firmware files
│   └── esp32.ino             # Main microcontroller code
├── Dataset/                  # Historical telemetry data
├── Prediction/               # Machine Learning model scripts
│   └── aiml.py               # RNN/LSTM training and inference logic
├── Website/                  # Streamlit web application
│   └── dashboard.py          # Dashboard UI and frontend logic
├── LICENSE                   # Open-source MIT License
└── README.md                 # Project documentation
```

---

## 📜 License

This software is distributed under the [MIT License](LICENSE).

*Developed for professional Real-Time Solar Energy Monitoring, Analytics, and Optimization.*

## 🏆 Recognition

🥉 **Consolation Prize Winner – SCIMIT’26 National Science Day Mega Project Contest**  
Manakula Vinayagar Institute of Technology, February 2026

# 👨‍💻 Author

Abdul Saleem
B.Tech – Electrical & Electronics Engineering (AI & ML)
GitHub: https://github.com/CodeWithAbdulSaleem
LinkedIn: https://www.linkedin.com/in/abdulsaleem03