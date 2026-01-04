#ifndef VERSION_H
#define VERSION_H

/**
 * BME280 Sensor - Firmware Version
 * 
 * Version format: MAJOR.MINOR.PATCH-buildYYYYMMDD
 * Updated via update_version.sh before each build
 */

inline String getFirmwareVersion() {
  return String(FIRMWARE_VERSION_MAJOR) + "." + 
         String(FIRMWARE_VERSION_MINOR) + "." + 
         String(FIRMWARE_VERSION_PATCH) + "-build" + 
         String(BUILD_TIMESTAMP);
}

#endif // VERSION_H
