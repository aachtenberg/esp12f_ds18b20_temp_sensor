#include "trace.h"
#include <sstream>

namespace Trace {
  // Static variables: initialized once per device boot
  static std::string g_traceId = "";
  static uint32_t g_sequenceNumber = 0;

  void init() {
    // Generate a UUID-like trace ID from ESP32 chip ID and millis() for uniqueness
    uint64_t chipid = ESP.getEfuseMac();
    uint32_t boot_ms = millis();
    char trace_buffer[40];
    
    snprintf(trace_buffer, sizeof(trace_buffer), "%04x%04x-%04x-%04x-%04x-%08x%04x",
      (uint16_t)(chipid >> 32), (uint16_t)(chipid >> 16), (uint16_t)chipid,
      (uint16_t)(boot_ms >> 16), (uint16_t)boot_ms,
      (uint32_t)(boot_ms << 16), (uint16_t)boot_ms);
    
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
