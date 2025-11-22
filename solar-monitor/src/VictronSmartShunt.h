/**
 * VictronSmartShunt.h
 *
 * Driver for Victron SmartShunt battery monitor via VE.Direct protocol
 *
 * Hardware: SmartShunt SHU050150050 (500A/50mV)
 * Protocol: VE.Direct ASCII, 19200 baud, 8N1, 3.3V TTL
 *
 * Connection: ESP32 GPIO 16 (UART2 RX) <- SmartShunt TX
 */

#ifndef VICTRON_SMARTSHUNT_H
#define VICTRON_SMARTSHUNT_H

#include <Arduino.h>
#include <HardwareSerial.h>

class VictronSmartShunt {
public:
    /**
     * Constructor
     * @param serial Pointer to HardwareSerial instance (UART2 recommended)
     */
    VictronSmartShunt(HardwareSerial* serial);

    /**
     * Initialize the SmartShunt connection
     * Call in setup() after Serial.begin()
     */
    void begin();

    /**
     * Process incoming VE.Direct data
     * Call in loop() - non-blocking
     */
    void update();

    // Primary data getters
    float getBatteryVoltage() const;    // Returns volts
    float getBatteryCurrent() const;    // Returns amps (negative = discharge)
    float getStateOfCharge() const;     // Returns percentage (0-100)
    int getTimeRemaining() const;       // Returns minutes (-1 = infinite)
    float getConsumedAh() const;        // Returns amp-hours consumed
    bool getAlarmState() const;         // Returns true if alarm active
    bool getRelayState() const;         // Returns true if relay on

    // Historical data getters
    float getMinVoltage() const;        // Minimum voltage recorded
    float getMaxVoltage() const;        // Maximum voltage recorded
    int getChargeCycles() const;        // Number of charge cycles
    float getDeepestDischarge() const;  // Deepest discharge in Ah
    float getLastDischarge() const;     // Last discharge depth in Ah

    // Status
    bool isDataValid() const;           // True if receiving valid data
    unsigned long getLastUpdate() const; // millis() of last valid update

private:
    HardwareSerial* _serial;

    // Parse a single VE.Direct line
    void parseLine(const String& line);

    // Data storage (raw values from device)
    int32_t _voltage_mv;        // Battery voltage in mV
    int32_t _current_ma;        // Battery current in mA (signed)
    int16_t _soc_tenth;         // State of charge in 0.1%
    int16_t _ttg_min;           // Time to go in minutes
    int32_t _consumed_mah;      // Consumed energy in mAh
    bool _alarm;                // Alarm state
    bool _relay;                // Relay state

    // Historical data (raw values)
    int32_t _min_voltage_mv;    // H7: Minimum voltage in mV
    int32_t _max_voltage_mv;    // H8: Maximum voltage in mV
    int32_t _charge_cycles;     // H4: Number of charge cycles
    int32_t _deepest_discharge_mah;  // H1: Deepest discharge in mAh
    int32_t _last_discharge_mah;     // H2: Last discharge in mAh

    // Parsing state
    String _lineBuffer;
    unsigned long _lastUpdate;
    bool _dataValid;
    uint8_t _fieldsReceived;    // Track how many fields in current block

    // Constants
    static const uint8_t MIN_FIELDS_FOR_VALID = 3;  // Minimum fields to consider data valid
};

#endif // VICTRON_SMARTSHUNT_H
