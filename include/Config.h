// Compile-time settings (SPEC 5.2). Platform-free: also included by the host tests.
#pragma once

#define AIS_BAUD            38400   // WatchMate AIS output; 4800 for standard 0183
#define FORWARD_OWN_SHIP    false   // Also convert AIVDO
#define LED_BRIGHTNESS      20      // 0 = LED off
#define SERIAL_RX_BUFFER    2048
#define CAN_RX_QUEUE        32      // TWAI RX queue (SetN2kCANReceiveFrameBufSize)
#define N2K_SEND_FRAME_BUF  64      // Send frames in total: TWAI TX queue = this - 4, library keeps 4
                                    // (NMEA2000_esp32_idf_twai sizing; covers fast-packet bursts)
#define STATS_LOG_MS        60000   // Periodic stats log line

// Web console (SPEC 6)
#define WEB_PORT            80
#define LOG_LINES           150     // log lines kept for the console
#define LOG_LINE_LEN        200     // per line, incl. NUL (the stats line can pass 160 characters)
#define LOG_JSON_MAX_LINES  40      // lines per /api/log response; the page asks again if more wait

// Pins: M5Stack Atom Lite + CAN Unit + Tail485
#define CAN_TX_PIN          GPIO_NUM_22
#define CAN_RX_PIN          GPIO_NUM_19
#define RS485_RX_PIN        26
#define RS485_TX_PIN        32
#define LED_PIN             27

// Timeouts
#define NO_AIS_MS           (5UL * 60 * 1000)  // LED "no AIS" after this long without a sentence
#define HOTSPOT_AFTER_MS    (5UL * 60 * 1000)  // Fallback hotspot after this long without the boat network
#define ROLLBACK_MIN_UPTIME_MS (60UL * 1000)   // New image: healthy for at least this long before marked valid
#define ROLLBACK_TIMEOUT_MS (5UL * 60 * 1000)  // New image: restart (roll back) if not valid by then

#define HOSTNAME            "ais-gateway"
