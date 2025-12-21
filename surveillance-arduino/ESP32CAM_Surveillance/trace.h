#ifndef TRACE_H
#define TRACE_H

#include <Arduino.h>
#include <string>

/**
 * @brief Trace and instrumentation utilities for MQTT payload correlation.
 * 
 * Generates a single UUID v4 trace ID at device boot and maintains a monotonic
 * sequence number for each published message. Used to correlate related events
 * across MQTT topics and backend systems.
 */

namespace Trace {
  /**
   * @brief Initialize trace system. Must be called once at startup.
   * Generates a new UUID v4 trace ID for this device session.
   */
  void init();

  /**
   * @brief Get the current trace ID (UUID v4).
   * @return String containing the trace ID in UUID format (e.g., "550e8400-e29b-41d4-a716-446655440000")
   */
  std::string getTraceId();

  /**
   * @brief Get the next sequence number for this message.
   * Increments and returns a monotonic counter starting from 1.
   * @return Unsigned integer sequence number
   */
  uint32_t getSequenceNumber();

  /**
   * @brief Get human-readable trace identifier combining trace ID and sequence.
   * Useful for logs and debugging.
   * @return String in format "trace_id:seq_num"
   */
  std::string getTraceIdentifier();

  /**
   * @brief Get the W3C traceparent header value for this message.
   * Format: 00-{trace_id}-{span_id}-01
   * @return String in W3C traceparent format
   */
  std::string getTraceparent();

  /**
   * @brief Get the span ID (16 hex characters) for distributed tracing.
   * @return String containing the span ID
   */
  std::string getSpanId();
}

#endif // TRACE_H
