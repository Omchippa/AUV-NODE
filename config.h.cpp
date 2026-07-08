#ifndef CONFIG_H
#define CONFIG_H

// System Intervals (Deliberately separated)
#define SENSOR_SAMPLE_INTERVAL_MS      1000   // Sample every 1s
#define TELEMETRY_PUBLISH_INTERVAL_MS   5000   // Publish every 5s

// Hardware Pin Configurations
#define PIN_ONEWIRE_BUS                15     // DS18B20 Temp Probe
#define PIN_TURBIDITY_ADC              34     // Analog Turbidity
#define PIN_BATTERY_ADC                35     // Battery Divider Sense
#define PIN_LEAK_DIGITAL               4      // Active-low moisture probe
#define PIN_STATUS_LED                 2      // Onboard status LED

// RS485 Hardware UART Configurations (Expansion)
#define PIN_RS485_RX                   16     // UART2 RX
#define PIN_RS485_TX                   17     // UART2 TX
#define PIN_RS485_CTRL                 18     // DE/RE Flow Control Pin
#define RS485_BAUD_RATE                115200 // Link to Flight Controller

// Constant Operational Envelopes
#define DEPTH_MAX_M                    30.0f  // Rated physical envelope limit
#define WDT_TIMEOUT_S                  15     // Watchdog timer length

// Battery Thresholds (7.4V nominal 2S Pack)
#define BATTERY_LOW_V                  6.8f
#define BATTERY_CRITICAL_V             6.2f

// Log File Configurations
#define LOG_FILE_PATH                  "/events.log"
#define MAX_LOG_SIZE_BYTES             51200  // 50KB limit before rotation

#endif