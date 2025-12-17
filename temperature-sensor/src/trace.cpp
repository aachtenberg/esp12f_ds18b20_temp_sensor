#include "trace.h"
#include <sstream>
#include <iomanip>

namespace Trace {
  // Static variables: initialized once per device boot
  static std::string g_traceId = "";
  static uint32_t g_sequenceNumber = 0;

  void init() {
    // Generate a UUID-like trace ID from device chip ID and millis() for uniqueness
    // Format: {chipid}-{boot_time_ms} (simplified UUID-like format for compatibility)
    #ifdef ESP32
      uint64_t chipid = ESP.getEfuseMac();
    #else
      uint32_t chipid = ESP.getChipId();
    #endif
    
    uint32_t boot_ms = millis();
    char trace_buffer[40];
    
    #ifdef ESP32
      snprintf(trace_buffer, sizeof(trace_buffer), "%04x%04x-%04x-%04x-%04x-%08x%04x",
        (uint16_t)(chipid >> 32), (uint16_t)(chipid >> 16), (uint16_t)chipid,
        (uint16_t)(boot_ms >> 16), (uint16_t)boot_ms,
        (uint32_t)(boot_ms << 16), (uint16_t)boot_ms);
    #else
      snprintf(trace_buffer, sizeof(trace_buffer), "%08x-%04x-%04x-%04x-%08x",
        (uint32_t)chipid, (uint16_t)(boot_ms >> 16), (uint16_t)boot_ms,
        (uint16_t)(boot_ms >> 8), (uint32_t)(boot_ms << 8));
    #endif
    
    g_traceId = std::string(trace_buffer);
    g_sequenceNumber = 0;
    
    Serial.printf("[TRACE] Initialized trace ID: %s\n", g_traceId.c_str());
  }

  std::string getTraceId() {
    if (g_traceId.empty()) {
      return "uninitialized";
    }
    return g_traceId;
  }

  uint32_t getSequenceNumber() {
    return ++g_sequenceNumber;
  }

  std::string getTraceIdentifier() {
    std::stringstream ss;
    ss << getTraceId() << ":" << g_sequenceNumber;
    return ss.str();
  }
}
