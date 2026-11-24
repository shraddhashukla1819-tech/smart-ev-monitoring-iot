# Smart EV Monitoring and Anti-Theft System using IoT

An ESP32-based IoT system simulated in Wokwi that monitors electric vehicle (EV) charging parameters, analyzes driver behaviour, and detects unauthorised vehicle movement when parked, transmitting real-time telemetry over Wi-Fi to the Blynk IoT dashboard.

## Live Simulation
- **Wokwi Project Link:** https://wokwi.com/projects/481610209913985025

## System Modules
1. **EV Charging Monitoring:** Tracks charging voltage, current, and battery/charger temperature (DHT22) to detect over-current, over-voltage, and overheating faults.
2. **Driver Behaviour Analysis:** Uses an MPU6050 accelerometer and motor load sensor to detect sudden acceleration, sudden braking, and excessive motor load while driving.
3. **Anti-Theft Detection:** Arms security mode when parked (slide switch) and triggers a local buzzer, red alert LED, LCD warning, and Blynk alert if PIR motion or MPU6050 vibration is detected.
4. **Offline Fallback Mode:** Continues local monitoring and displays live status on the 16x2 I2C LCD and Serial Monitor if Wi-Fi disconnects.

## Repository Contents
- `sketch.ino` – ESP32 Arduino firmware with Blynk IoT integration and offline fallback
- `diagram.json` – Wokwi circuit schematic and hardware connections
- `libraries.txt` – Required Arduino libraries for Wokwi compilation
