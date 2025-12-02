/**
 * VictronMPPT.cpp
 *
 * Implementation of Victron SmartSolar MPPT VE.Direct driver
 */

#include "VictronMPPT.h"

VictronMPPT::VictronMPPT(Stream* serial)
    : _serial(serial)
    , _product_id("")
    , _serial_number("")
    , _batt_voltage_mv(0)
    , _charge_current_ma(0)
    , _pv_voltage_mv(0)
    , _pv_power_w(0)
    , _charge_state(ChargeState::UNKNOWN)
    , _error_code(0)
    , _load_state("OFF")
    , _load_current_ma(0)
    , _yield_today(0)
    , _yield_yesterday(0)
    , _yield_total(0)
    , _max_power_today(0)
    , _max_power_yesterday(0)
    , _lineBuffer("")
    , _lastUpdate(0)
    , _dataValid(false)
    , _fieldsReceived(0)
{
}

void VictronMPPT::begin() {
    _lineBuffer.reserve(64);  // Pre-allocate buffer for efficiency
    Serial.println("[MPPT] Initialized, waiting for data...");
}

void VictronMPPT::update() {
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

void VictronMPPT::parseLine(const String& line) {
    // VE.Direct format: Key<TAB>Value
    int tabIndex = line.indexOf('\t');
    if (tabIndex == -1) {
        return;  // Invalid line format
    }

    String key = line.substring(0, tabIndex);
    String value = line.substring(tabIndex + 1);

    // Parse known fields
    if (key == "PID") {
        // Product ID (device identification)
        _product_id = value;
    }
    else if (key == "SER#") {
        // Serial number (device identification)
        _serial_number = value;
    }
    else if (key == "V") {
        // Battery voltage in mV
        _batt_voltage_mv = value.toInt();
        _fieldsReceived++;
    }
    else if (key == "I") {
        // Charge current in mA
        _charge_current_ma = value.toInt();
        _fieldsReceived++;
    }
    else if (key == "VPV") {
        // Panel voltage in mV
        _pv_voltage_mv = value.toInt();
        _fieldsReceived++;
    }
    else if (key == "PPV") {
        // Panel power in W
        _pv_power_w = value.toInt();
    }
    else if (key == "CS") {
        // Charge state
        int state = value.toInt();
        switch (state) {
            case 0: _charge_state = ChargeState::OFF; break;
            case 2: _charge_state = ChargeState::FAULT; break;
            case 3: _charge_state = ChargeState::BULK; break;
            case 4: _charge_state = ChargeState::ABSORPTION; break;
            case 5: _charge_state = ChargeState::FLOAT; break;
            case 6: _charge_state = ChargeState::STORAGE; break;
            case 7: _charge_state = ChargeState::EQUALIZE; break;
            default: _charge_state = ChargeState::UNKNOWN; break;
        }
    }
    else if (key == "ERR") {
        // Error code
        _error_code = value.toInt();
    }
    else if (key == "LOAD") {
        // Load output state (ON/OFF)
        _load_state = value;
    }
    else if (key == "IL") {
        // Load current in mA
        _load_current_ma = value.toInt();
    }
    else if (key == "H19") {
        // Total yield in 0.01 kWh
        _yield_total = value.toInt();
    }
    else if (key == "H20") {
        // Yield today in 0.01 kWh
        _yield_today = value.toInt();
    }
    else if (key == "H21") {
        // Max power today in W
        _max_power_today = value.toInt();
    }
    else if (key == "H22") {
        // Yield yesterday in 0.01 kWh
        _yield_yesterday = value.toInt();
    }
    else if (key == "H23") {
        // Max power yesterday in W
        _max_power_yesterday = value.toInt();
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

// Static helper: Convert charge state to string
String VictronMPPT::chargeStateToString(ChargeState state) {
    switch (state) {
        case ChargeState::OFF: return "OFF";
        case ChargeState::FAULT: return "FAULT";
        case ChargeState::BULK: return "BULK";
        case ChargeState::ABSORPTION: return "ABSORPTION";
        case ChargeState::FLOAT: return "FLOAT";
        case ChargeState::STORAGE: return "STORAGE";
        case ChargeState::EQUALIZE: return "EQUALIZE";
        default: return "UNKNOWN";
    }
}

// Static helper: Convert error code to string
String VictronMPPT::errorCodeToString(int code) {
    switch (code) {
        case 0: return "No error";
        case 2: return "Battery voltage too high";
        case 17: return "Charger temperature too high";
        case 18: return "Charger over current";
        case 19: return "Charger current reversed";
        case 20: return "Bulk time limit exceeded";
        case 33: return "Input voltage too high (solar)";
        case 34: return "Input current too high (solar)";
        default: return "Unknown error (" + String(code) + ")";
    }
}

// Getters - convert raw values to user-friendly units

String VictronMPPT::getProductID() const {
    return _product_id;
}

String VictronMPPT::getSerialNumber() const {
    return _serial_number;
}

float VictronMPPT::getBatteryVoltage() const {
    return _batt_voltage_mv / 1000.0f;  // mV to V
}

float VictronMPPT::getChargeCurrent() const {
    return _charge_current_ma / 1000.0f;  // mA to A
}

float VictronMPPT::getPanelVoltage() const {
    return _pv_voltage_mv / 1000.0f;  // mV to V
}

float VictronMPPT::getPanelPower() const {
    return (float)_pv_power_w;  // Already in W
}

ChargeState VictronMPPT::getChargeStateEnum() const {
    return _charge_state;
}

String VictronMPPT::getChargeState() const {
    return chargeStateToString(_charge_state);
}

int VictronMPPT::getErrorCode() const {
    return _error_code;
}

String VictronMPPT::getErrorString() const {
    return errorCodeToString(_error_code);
}

String VictronMPPT::getLoadState() const {
    return _load_state;
}

float VictronMPPT::getLoadCurrent() const {
    return _load_current_ma / 1000.0f;  // mA to A
}

float VictronMPPT::getYieldToday() const {
    return _yield_today * 0.01f;  // 0.01 kWh to kWh
}

float VictronMPPT::getYieldYesterday() const {
    return _yield_yesterday * 0.01f;  // 0.01 kWh to kWh
}

float VictronMPPT::getYieldTotal() const {
    return _yield_total * 0.01f;  // 0.01 kWh to kWh
}

int VictronMPPT::getMaxPowerToday() const {
    return _max_power_today;
}

int VictronMPPT::getMaxPowerYesterday() const {
    return _max_power_yesterday;
}

bool VictronMPPT::isDataValid() const {
    // Data is valid if we've received data in the last 5 seconds
    return _dataValid && (millis() - _lastUpdate < 5000);
}

unsigned long VictronMPPT::getLastUpdate() const {
    return _lastUpdate;
}
