# Beehive-Monitor

A comprehensive beehive monitoring system using ESP32 with LTE connectivity, LCD display, and various sensors.

## Version

**Current Version: v27** (2025-11-23)

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

## Features

- **Multi-language support**: English and Greek
- **Dual connectivity**: LTE (A7670 modem) and WiFi with automatic failover
- **Sensor integration**: Weight, temperature, humidity, pressure, accelerometer
- **LCD display**: 20x4 I2C LCD with Greek character support
- **Web interface**: WiFi provisioning and LCD JSON endpoint
- **ThingSpeak integration**: Automatic data upload
- **Weather data**: Integration with Open-Meteo API
- **SD card logging**: Data persistence
- **Menu system**: Interactive button-driven navigation

## Memory Optimizations (v27)

Version 26 introduces several memory optimizations while maintaining full backward compatibility:

### Flash String Storage
Static string literals are stored in flash memory using the F() macro, reducing RAM usage:
- Serial debug messages
- LCD display strings (via PROGMEM helpers)
- HTML templates for web server

### PROGMEM Helpers for Greek Text
New helper functions allow Greek text to be stored in flash:
- `uiPrint_P(F("text"), col, row)` - Print flash string to display
- `lcdPrintGreek_P(F("Ελληνικά"), col, row)` - Print Greek flash string to LCD

### Debug Control
Debug output can be disabled to save memory and reduce serial traffic:
- Set `ENABLE_DEBUG 1` in `config.h` to enable debug output
- Set `ENABLE_DEBUG 0` to disable (default in v26)

### Buffer Optimization
- `TINY_GSM_RX_BUFFER` reduced from 1024 to 512 bytes
- Replaced `std::vector` with fixed-size arrays for UI rendering

## Hardware Requirements

- ESP32 development board
- A7670 LTE modem module
- 20x4 I2C LCD display (0x27)
- HX711 load cell amplifier
- SD card module
- BME280/BME680 environmental sensor
- SI7021 temperature/humidity sensor
- Accelerometer (optional)
- 4 push buttons (UP, DOWN, SELECT, BACK)

## Pin Configuration

See `config.h` for complete pin definitions:
- I2C (SDA: 21, SCL: 22)
- Modem UART (RX: 27, TX: 26, PWR: 4)
- SD Card (SPI on pins 2, 13, 14, 15)
- Buttons (23, 12, 33, 32)

## Building and Uploading

This is an Arduino/PlatformIO project for ESP32:

1. Install Arduino IDE or PlatformIO
2. Install required libraries:
   - TinyGSM
   - LiquidCrystal_I2C
   - WiFi (built-in)
   - SD (built-in)
   - Preferences (built-in)
3. Configure WiFi credentials in `config.h` (optional)
4. Upload `BeehiveMonitor_26.ino` to your ESP32

## Configuration

### WiFi Networks
Default WiFi credentials can be set in `config.h`:
- `WIFI_SSID1` / `WIFI_PASS1` - Primary network
- `WIFI_SSID2` / `WIFI_PASS2` - Secondary network

WiFi can also be configured via the web interface when connected.

### LTE/APN Settings
Configure in `config.h`:
- `MODEM_APN` - Default: "internet"
- `MODEM_GPRS_USER` - Usually empty
- `MODEM_GPRS_PASS` - Usually empty

### Debug Output
To enable detailed debug logging:
1. Edit `config.h`
2. Change `#define ENABLE_DEBUG 0` to `#define ENABLE_DEBUG 1`
3. Rebuild and upload

## Web Interface

When WiFi is connected, the device provides:
- **Port 80**: WiFi provisioning interface at `http://<device-ip>/wifi`
- **Port 80**: LCD JSON endpoint at `http://<device-ip>/lcd.json`
- **Port 8080**: Alternate LCD JSON server

## License

See LICENSE file for details.

## Contributors

- Original author
- GitHub Copilot (v26 optimizations)
