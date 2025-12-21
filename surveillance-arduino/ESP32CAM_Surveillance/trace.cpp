#include <Arduino.h>
#include "trace.h"
#include <sstream>
#include <iomanip>

namespace Trace {
  // Static variables: initialized once per device boot
  static std::string g_traceIdUuid = "";  // UUID format for backward compatibility
  static std::string g_traceIdHex = "";   // 32-char hex for W3C traceparent
  static std::string g_spanId = "";       // 16-char hex span ID for distributed tracing
  static uint32_t g_sequenceNumber = 0;

  void init() {
    // Generate trace ID in both formats:
    // 1. UUID format (for backward compatibility): {chipid}-{boot_time_ms}
    // 2. 32-char hex format (for W3C traceparent): continuous hex string
    // 3. 16-char hex span ID (derived from sequence counter initialization)
    
    uint64_t chipid = ESP.getEfuseMac();
    uint32_t boot_ms = millis();
    
    // Generate UUID format (backward compatible)
    char trace_buffer_uuid[40];
    snprintf(trace_buffer_uuid, sizeof(trace_buffer_uuid), "%04x%04x-%04x-%04x-%04x-%08x%04x",
      (uint16_t)(chipid >> 32), (uint16_t)(chipid >> 16), (uint16_t)chipid,
      (uint16_t)(boot_ms >> 16), (uint16_t)boot_ms,
      (uint32_t)(boot_ms << 16), (uint16_t)boot_ms);
    g_traceIdUuid = std::string(trace_buffer_uuid);
    
    // Generate 32-char hex format for W3C traceparent (continuous hex, no dashes)
    char trace_buffer_hex[33];
    snprintf(trace_buffer_hex, sizeof(trace_buffer_hex), "%016llx%016x",
      (unsigned long long)chipid, boot_ms);
    g_traceIdHex = std::string(trace_buffer_hex);
    
    // Generate 16-char hex span ID (derived from device uptime and additional entropy)
    char span_buffer[17];
    uint64_t span_seed = (uint64_t)chipid ^ (uint64_t)boot_ms;
    snprintf(span_buffer, sizeof(span_buffer), "%016llx", (unsigned long long)span_seed);
    g_spanId = std::string(span_buffer);
    
    g_sequenceNumber = 0;
    
    Serial.printf("[TRACE] Initialized trace ID (UUID): %s\n", g_traceIdUuid.c_str());
    Serial.printf("[TRACE] Initialized trace ID (W3C hex): %s\n", g_traceIdHex.c_str());
    Serial.printf("[TRACE] Initialized span ID: %s\n", g_spanId.c_str());
  }

  std::string getTraceId() {
    if (g_traceIdUuid.empty()) {
      return "uninitialized";
    }
    return g_traceIdUuid;
  }

  uint32_t getSequenceNumber() {
    return ++g_sequenceNumber;
  }

  std::string getTraceIdentifier() {
    std::stringstream ss;
    ss << getTraceId() << ":" << g_sequenceNumber;
    return ss.str();
  }

  std::string getTraceparent() {
    // W3C traceparent format: 00-{trace_id}-{span_id}-01
    // version (00) - trace_id (32 hex chars) - span_id (16 hex chars) - flags (01 for sampled)
    if (g_traceIdHex.empty() || g_spanId.empty()) {
      return "00-00000000000000000000000000000000-0000000000000000-01";
    }
    std::string traceparent = "00-" + g_traceIdHex + "-" + g_spanId + "-01";
    return traceparent;
  }

  std::string getSpanId() {
    if (g_spanId.empty()) {
      return "uninitialized";
    }
    return g_spanId;
  }
}
