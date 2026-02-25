/*
* This example implements CAN loopback test. Before using it, 
* you need to control IO5 of IO EXTENSION to output low level, 
* otherwise CAN will not be used.
*/
#include "twai.h"

void setup() {
  // Start Serial 
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  
  // TWAI configuration settings
  static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();                                                 // Set CAN bus speed to 500 kbps
  static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();                                              // Accept all incoming CAN messages
  static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL); // General configuration, set TX/RX GPIOs and mode
  
  DEV_I2C_Init(); 
  IO_EXTENSION_Init();
  delay(10);
  IO_EXTENSION_Output(IO_EXTENSION_IO_5,1);  // Select communication interface: 0 for USB, 1 for CAN

  // Initialize the CAN communication interface
  can_init(t_config, f_config, g_config);  // Initialize CAN with specified configurations
}

uint32_t alerts_triggered; 
twai_message_t message;

void loop() {
  alerts_triggered = can_read_alerts();  // Check for any triggered CAN bus alerts
        
  // If new CAN data is received, read and transmit it
  if (alerts_triggered & TWAI_ALERT_RX_DATA)
  {
      message = can_read_Byte();      // Read the received CAN message
      can_write_Byte(message);        // Re-transmit the received message
  }
  delay(10);
}
