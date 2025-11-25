# Changelog

All notable changes to the Beehive Monitor project will be documented in this file.

## [v27] - 2025-11-23

### Memory Optimizations
- **TINY_GSM_RX_BUFFER**: Reduced from 1024 to 512 bytes across the project to optimize memory usage
  - Updated in `BeehiveMonitor_26.ino` and `modem_manager.h`
  
- **Flash string storage (F() macro)**: Wrapped static ASCII string literals with F() macro to store them in flash memory instead of RAM
  - Applied to Serial.print/println calls in:
    - `BeehiveMonitor_26.ino`
    - `modem_manager.cpp`
    - `key_server.cpp`
    - `menu_manager.cpp`
    - `lcd_server_simple.cpp`
  
- **PROGMEM support for Greek text**: Added helper functions to support Greek static literals stored in flash
  - New functions: `uiPrint_P()` and `lcdPrintGreek_P()` in `ui.cpp`/`ui.h`
  - Copy flash strings into stack buffer (max 96 bytes) before calling existing display functions
  
- **HTML in PROGMEM**: Moved static HTML snippets in `key_server.cpp` to PROGMEM storage
  - WiFi setup form and index page now use `send_P()` for efficient flash-based serving
  
- **Removed std::vector**: Replaced dynamic `std::vector<String>` usage with lightweight fixed-size array structures
  - Modified `splitVisual()`, `joinVisualPad()`, and `getExistingLineCells()` in `ui.cpp`
  - Preserves UTF-8 multi-byte sequence handling with no runtime behavior changes

### Debug Control
- **ENABLE_DEBUG macro**: Added `#define ENABLE_DEBUG 0` in `config.h`
  - Set to 1 to enable debug output, 0 to disable and save memory
  - Wrapped large debug Serial prints with `#if ENABLE_DEBUG ... #endif`
  - All modem initialization, network status, and diagnostic prints are now conditional

### Code Quality
- Ensured modem helper functions (`modem_isNetworkRegistered`, `modem_getRSSI`, `modem_getOperator`) are defined only once in `modem_manager.cpp`
- No LTO (Link Time Optimization) changes - build flags remain unchanged
- All changes maintain backward compatibility and identical runtime behavior

### Version Update
- Updated splash screen from v25 to v26
- File renamed: `BeehiveMonitor_25.ino` → `BeehiveMonitor_26.ino`

## [v25] - Previous Version
- Base version before optimization pass
