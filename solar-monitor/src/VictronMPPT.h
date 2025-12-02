/**
 * VictronMPPT.h
 *
 * Driver for Victron SmartSolar MPPT charge controller via VE.Direct protocol
 *
 * Hardware: SmartSolar MPPT SCC110050210 (100V/50A)
 * Protocol: VE.Direct ASCII, 19200 baud, 8N1, 3.3V TTL
 *
 * Connection: ESP32 GPIO 19 (UART1 RX) <- MPPT TX
 */

#ifndef VICTRON_MPPT_H
#define VICTRON_MPPT_H

#include <Arduino.h>
#include <Stream.h>

// Charge state enumeration
enum class ChargeState : uint8_t {
    OFF = 0,
    FAULT = 2,
    BULK = 3,
    ABSORPTION = 4,
    FLOAT = 5,
    STORAGE = 6,
    EQUALIZE = 7,
    UNKNOWN = 255
};

class VictronMPPT {
public:
    /**
     * Constructor
     * @param serial Pointer to Stream instance (HardwareSerial or SoftwareSerial)
     */
    VictronMPPT(Stream* serial);

    /**
     * Initialize the MPPT connection
     * Call in setup() after Serial.begin()
     */
    void begin();

    /**
     * Process incoming VE.Direct data
     * Call in loop() - non-blocking
     */
    void update();

    // Device identification getters
    String getProductID() const;        // Returns product ID (e.g., "0xA060")
    String getSerialNumber() const;     // Returns serial number

    // Primary data getters
    float getBatteryVoltage() const;    // Returns volts
    float getChargeCurrent() const;     // Returns amps
    float getPanelVoltage() const;      // Returns volts
    float getPanelPower() const;        // Returns watts
    ChargeState getChargeStateEnum() const;  // Returns enum value
    String getChargeState() const;      // Returns string (OFF, BULK, etc.)
    int getErrorCode() const;           // Returns error code (0 = no error)
    String getErrorString() const;      // Returns human-readable error

    // Load output getters (for models with load output)
    String getLoadState() const;        // Returns "ON" or "OFF"
    float getLoadCurrent() const;       // Returns load current in amps

    // Yield data getters
    float getYieldToday() const;        // Returns kWh
    float getYieldYesterday() const;    // Returns kWh
    float getYieldTotal() const;        // Returns kWh
    int getMaxPowerToday() const;       // Returns watts
    int getMaxPowerYesterday() const;   // Returns watts

    // Status
    bool isDataValid() const;           // True if receiving valid data
    unsigned long getLastUpdate() const; // millis() of last valid update

private:
    Stream* _serial;

    // Parse a single VE.Direct line
    void parseLine(const String& line);

    // Convert charge state code to string
    static String chargeStateToString(ChargeState state);

    // Convert error code to string
    static String errorCodeToString(int code);

    // Device identification (captured once)
    String _product_id;         // PID: Product ID
    String _serial_number;      // SER#: Serial number

    // Data storage (raw values from device)
    int32_t _batt_voltage_mv;   // Battery voltage in mV
    int32_t _charge_current_ma; // Charge current in mA
    int32_t _pv_voltage_mv;     // Panel voltage in mV
    int32_t _pv_power_w;        // Panel power in W
    ChargeState _charge_state;  // Charge state
    int _error_code;            // Error code

    // Load output data (for models with load output)
    String _load_state;         // LOAD: ON or OFF
    int32_t _load_current_ma;   // IL: Load current in mA

    // Yield data (raw values)
    int32_t _yield_today;       // H20: Today's yield in 0.01 kWh
    int32_t _yield_yesterday;   // H22: Yesterday's yield in 0.01 kWh
    int32_t _yield_total;       // H19: Total yield in 0.01 kWh
    int32_t _max_power_today;   // H21: Max power today in W
    int32_t _max_power_yesterday; // H23: Max power yesterday in W

    // Parsing state
    String _lineBuffer;
    unsigned long _lastUpdate;
    bool _dataValid;
    uint8_t _fieldsReceived;

    // Constants
    static const uint8_t MIN_FIELDS_FOR_VALID = 3;
};

#endif // VICTRON_MPPT_H
