/**
 * VictronSmartShunt.cpp
 *
 * Implementation of Victron SmartShunt VE.Direct driver
 */

#include "VictronSmartShunt.h"

VictronSmartShunt::VictronSmartShunt(HardwareSerial* serial)
    : _serial(serial)
    , _voltage_mv(0)
    , _current_ma(0)
    , _soc_tenth(0)
    , _ttg_min(-1)
    , _consumed_mah(0)
    , _alarm(false)
    , _relay(false)
    , _min_voltage_mv(0)
    , _max_voltage_mv(0)
    , _charge_cycles(0)
    , _deepest_discharge_mah(0)
    , _last_discharge_mah(0)
    , _lineBuffer("")
    , _lastUpdate(0)
    , _dataValid(false)
    , _fieldsReceived(0)
{
}

void VictronSmartShunt::begin() {
    _lineBuffer.reserve(64);  // Pre-allocate buffer for efficiency
    Serial.println("[SmartShunt] Initialized, waiting for data...");
}

void VictronSmartShunt::update() {
    // Non-blocking read of available serial data
    while (_serial->available()) {
        char c = _serial->read();

        if (c == '\n') {
            // End of line - parse it
            if (_lineBuffer.length() > 0) {
                parseLine(_lineBuffer);
                _lineBuffer = "";
            }
        } else if (c == '\r') {
            // Ignore carriage return
        } else {
            // Add character to buffer (limit size to prevent overflow)
            if (_lineBuffer.length() < 64) {
                _lineBuffer += c;
            }
        }
    }
}

void VictronSmartShunt::parseLine(const String& line) {
    // VE.Direct format: Key<TAB>Value
    int tabIndex = line.indexOf('\t');
    if (tabIndex == -1) {
        return;  // Invalid line format
    }

    String key = line.substring(0, tabIndex);
    String value = line.substring(tabIndex + 1);

    // Parse known fields
    if (key == "V") {
        // Battery voltage in mV
        _voltage_mv = value.toInt();
        _fieldsReceived++;
    }
    else if (key == "I") {
        // Battery current in mA (signed: negative = discharge)
        _current_ma = value.toInt();
        _fieldsReceived++;
    }
    else if (key == "SOC") {
        // State of charge in 0.1%
        _soc_tenth = value.toInt();
        _fieldsReceived++;
    }
    else if (key == "TTG") {
        // Time to go in minutes (-1 = infinite)
        _ttg_min = value.toInt();
    }
    else if (key == "CE") {
        // Consumed Ah in mAh (negative value)
        _consumed_mah = value.toInt();
    }
    else if (key == "Alarm") {
        // Alarm state: ON/OFF
        _alarm = (value == "ON");
    }
    else if (key == "Relay") {
        // Relay state: ON/OFF
        _relay = (value == "ON");
    }
    else if (key == "H1") {
        // Depth of deepest discharge in mAh
        _deepest_discharge_mah = value.toInt();
    }
    else if (key == "H2") {
        // Depth of last discharge in mAh
        _last_discharge_mah = value.toInt();
    }
    else if (key == "H4") {
        // Number of charge cycles
        _charge_cycles = value.toInt();
    }
    else if (key == "H7") {
        // Minimum battery voltage in mV
        _min_voltage_mv = value.toInt();
    }
    else if (key == "H8") {
        // Maximum battery voltage in mV
        _max_voltage_mv = value.toInt();
    }
    else if (key == "Checksum") {
        // End of data block - mark as valid if we have enough fields
        if (_fieldsReceived >= MIN_FIELDS_FOR_VALID) {
            _dataValid = true;
            _lastUpdate = millis();
        }
        _fieldsReceived = 0;  // Reset for next block
    }
}

// Getters - convert raw values to user-friendly units

float VictronSmartShunt::getBatteryVoltage() const {
    return _voltage_mv / 1000.0f;  // mV to V
}

float VictronSmartShunt::getBatteryCurrent() const {
    return _current_ma / 1000.0f;  // mA to A
}

float VictronSmartShunt::getStateOfCharge() const {
    return _soc_tenth / 10.0f;  // 0.1% to %
}

int VictronSmartShunt::getTimeRemaining() const {
    return _ttg_min;
}

float VictronSmartShunt::getConsumedAh() const {
    return abs(_consumed_mah) / 1000.0f;  // mAh to Ah (return positive value)
}

bool VictronSmartShunt::getAlarmState() const {
    return _alarm;
}

bool VictronSmartShunt::getRelayState() const {
    return _relay;
}

float VictronSmartShunt::getMinVoltage() const {
    return _min_voltage_mv / 1000.0f;  // mV to V
}

float VictronSmartShunt::getMaxVoltage() const {
    return _max_voltage_mv / 1000.0f;  // mV to V
}

int VictronSmartShunt::getChargeCycles() const {
    return _charge_cycles;
}

float VictronSmartShunt::getDeepestDischarge() const {
    return abs(_deepest_discharge_mah) / 1000.0f;  // mAh to Ah
}

float VictronSmartShunt::getLastDischarge() const {
    return abs(_last_discharge_mah) / 1000.0f;  // mAh to Ah
}

bool VictronSmartShunt::isDataValid() const {
    // Data is valid if we've received data in the last 5 seconds
    return _dataValid && (millis() - _lastUpdate < 5000);
}

unsigned long VictronSmartShunt::getLastUpdate() const {
    return _lastUpdate;
}
