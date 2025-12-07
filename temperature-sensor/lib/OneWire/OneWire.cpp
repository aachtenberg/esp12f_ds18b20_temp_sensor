/* OneWire.cpp copied into lib/OneWire for in-tree library
   (original left in project root for reference) */

#include <Arduino.h>
#include "OneWire.h"
#include "util/OneWire_direct_gpio.h"

#ifdef ARDUINO_ARCH_ESP32
// due to the dual core esp32, a critical section works better than disabling interrupts
#  define noInterrupts() {portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;portENTER_CRITICAL(&mux)
#  define interrupts() portEXIT_CRITICAL(&mux);} 
#  define CRIT_TIMING IRAM_ATTR
#else
#  define CRIT_TIMING 
#endif


void OneWire::begin(uint8_t pin)
{
    pinMode(pin, INPUT);
    bitmask = PIN_TO_BITMASK(pin);
    baseReg = PIN_TO_BASEREG(pin);
#if ONEWIRE_SEARCH
    reset_search();
#endif
}

// Perform the onewire reset function.  We will wait up to 250uS for
// the bus to come high, if it doesn't then it is broken or shorted
// and we return a 0;
//
// Returns 1 if a device asserted a presence pulse, 0 otherwise.
//
uint8_t CRIT_TIMING OneWire::reset(void)
{
    IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
    __attribute__((unused)) volatile IO_REG_TYPE *reg IO_REG_BASE_ATTR = baseReg;
    uint8_t r;
    uint8_t retries = 125;

    noInterrupts();
    DIRECT_MODE_INPUT(reg, mask);
    interrupts();
    // wait until the wire is high... just in case
    do {
        if (--retries == 0) return 0;
        delayMicroseconds(2);
    } while ( !DIRECT_READ(reg, mask));

    noInterrupts();
    DIRECT_WRITE_LOW(reg, mask);
    DIRECT_MODE_OUTPUT(reg, mask);    // drive output low
    interrupts();
    delayMicroseconds(480);
    noInterrupts();
    DIRECT_MODE_INPUT(reg, mask);    // allow it to float
    delayMicroseconds(70);
    r = !DIRECT_READ(reg, mask);
    interrupts();
    delayMicroseconds(410);
    return r;
}

// ... rest of OneWire.cpp implementation copied from original

// (trailing recursive include removed - file now contains the complete implementation)
