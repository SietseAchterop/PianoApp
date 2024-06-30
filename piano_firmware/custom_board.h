// Custom configure for nano 33 ble board
//    see components/boards for examples

#ifndef NANO_33_BLE_H
#define NANO_33_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// LEDs definitions for Nano 33 BLE (5, not used now, saves 0.6mA))
#define LEDS_NUMBER    0

#define LED_DL1         NRF_GPIO_PIN_MAP(1,9)
#define LED_DL2         NRF_GPIO_PIN_MAP(0,13)
  // rgb led work the other way around
#define LED_DL3_RED     NRF_GPIO_PIN_MAP(0,24)
#define LED_DL3_GRN     NRF_GPIO_PIN_MAP(0,16)
#define LED_DL3_BLU     NRF_GPIO_PIN_MAP(0,6)

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST { LED_DL1, LED_DL2, LED_DL3_RED, LED_DL3_GRN, LED_DL3_BLU }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      LED_DL1
#define BSP_LED_1      LED_DL2

#define BUTTONS_NUMBER 1

#define BUTTON_START   ARDUINO_NOT_PIN
#define BUTTON_1       ARDUINO_NOT_PIN
#define BUTTON_STOP    ARDUINO_NOT_PIN
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_LIST { BUTTON_1 }
#define BUTTONS_ACTIVE_STATE NRF_GPIO_PIN_SENSE_LOW

#define BSP_BUTTON_0   BUTTON_1

#define RX_PIN_NUMBER  NRF_GPIO_PIN_MAP(1,10)
#define TX_PIN_NUMBER  NRF_GPIO_PIN_MAP(1,3)
#define CTS_PIN_NUMBER 19
#define RTS_PIN_NUMBER 20
#define HWFC APP_UART_FLOW_CONTROL_DISABLED

#define BSP_QSPI_SCK_PIN   NRF_GPIO_PIN_MAP(0, 19)
#define BSP_QSPI_CSN_PIN   NRF_GPIO_PIN_MAP(0, 17)
#define BSP_QSPI_IO0_PIN   NRF_GPIO_PIN_MAP(0, 20)
#define BSP_QSPI_IO1_PIN   NRF_GPIO_PIN_MAP(0, 22)
#define BSP_QSPI_IO2_PIN   NRF_GPIO_PIN_MAP(0, 21)
#define BSP_QSPI_IO3_PIN   NRF_GPIO_PIN_MAP(0, 23)

#define SPIS_MISO_PIN   NRF_GPIO_PIN_MAP(1,8)   // SPI MISO signal.
#define SPIS_CSN_PIN    NRF_GPIO_PIN_MAP(1,2)   // SPI CSN signal.
#define SPIS_MOSI_PIN   NRF_GPIO_PIN_MAP(1,1)    // SPI MOSI signal.
#define SPIS_SCK_PIN    NRF_GPIO_PIN_MAP(0,13)   // SPI SCK signal.

// Arduino board mappings
#define ARDUINO_SCL_PIN             NRF_GPIO_PIN_MAP(0,2)   // SCL signal pin
#define ARDUINO_SDA_PIN             NRF_GPIO_PIN_MAP(0,31)  // SDA signal pin
#define ARDUINO_SCL1_PIN            NRF_GPIO_PIN_MAP(0,15)  // SCL1 signal pin
#define ARDUINO_SDA1_PIN            NRF_GPIO_PIN_MAP(0,14)  // SDA1 signal pin
#define ARDUINO_AREF_PIN            21    									// Aref pin

#define ARDUINO_13_PIN              NRF_GPIO_PIN_MAP(0,13) // Digital pin 13
#define ARDUINO_12_PIN              NRF_GPIO_PIN_MAP(1,8)  // Digital pin 12
#define ARDUINO_11_PIN              NRF_GPIO_PIN_MAP(1,1)  // Digital pin 11
#define ARDUINO_10_PIN              NRF_GPIO_PIN_MAP(1,2)  // Digital pin 10
#define ARDUINO_9_PIN               NRF_GPIO_PIN_MAP(0,27) // Digital pin 9
#define ARDUINO_8_PIN               NRF_GPIO_PIN_MAP(0,21) // Digital pin 8  was fout

#define ARDUINO_7_PIN               NRF_GPIO_PIN_MAP(0,23)  // Digital pin 7 was fout
#define ARDUINO_6_PIN               NRF_GPIO_PIN_MAP(1,14) // Digital pin 6
#define ARDUINO_5_PIN               NRF_GPIO_PIN_MAP(1,13) // Digital pin 5
#define ARDUINO_4_PIN               NRF_GPIO_PIN_MAP(1,15) // Digital pin 4
#define ARDUINO_3_PIN               NRF_GPIO_PIN_MAP(1,12) // Digital pin 3
#define ARDUINO_2_PIN               NRF_GPIO_PIN_MAP(1,11) // Digital pin 2
#define ARDUINO_1_PIN               NRF_GPIO_PIN_MAP(1,10) // Digital pin 1
#define ARDUINO_0_PIN               NRF_GPIO_PIN_MAP(1,3)  // Digital pin 0

#define ARDUINO_A0_PIN              NRF_GPIO_PIN_MAP(0,4)     // Analog channel 0
#define ARDUINO_A1_PIN              NRF_GPIO_PIN_MAP(0,5)     // Analog channel 1
#define ARDUINO_A2_PIN              NRF_GPIO_PIN_MAP(0,30)    // Analog channel 2
#define ARDUINO_A3_PIN              NRF_GPIO_PIN_MAP(0,29)    // Analog channel 3
#define ARDUINO_A4_PIN              NRF_GPIO_PIN_MAP(0,31)    // Analog channel 4
#define ARDUINO_A5_PIN              NRF_GPIO_PIN_MAP(0,2)     // Analog channel 5
#define ARDUINO_A6_PIN              NRF_GPIO_PIN_MAP(0,28)    // Analog channel 6
#define ARDUINO_A7_PIN              NRF_GPIO_PIN_MAP(0,3)     // Analog channel 7

// dummy, not used. hopefully helps, see button_leds_init()
#define ARDUINO_NOT_PIN             NRF_GPIO_PIN_MAP(0,12)  // Digital pin not connected.  GPIO46

#define PIN_ENABLE_SENSORS_3V3      NRF_GPIO_PIN_MAP(0,22)     // 3.3V Sensors
#define PIN_ENABLE_I2C_PULLUP       NRF_GPIO_PIN_MAP(1,0)     // I2C Pullup  R_Pullup in schematic



#ifdef __cplusplus
}
#endif

#endif // NANO_33_BLE_H
