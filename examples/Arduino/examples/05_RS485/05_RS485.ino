// Define 485 communication pins
#define RS485_RX_PIN  15
#define RS485_TX_PIN  16

// Redefine serial port name
#define RS485 Serial1

void setup() {
  // Initialize 485 device
  Serial.begin(115200); // Initialize the default serial port for debugging
  RS485.begin(115200, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN); // Initialize the RS485 serial port
  while (!RS485) {
    delay(10); // Wait for initialization to succeed
  }
}

void loop() {
  // Waiting for 485 data, cannot exceed 120 characters
  static String receivedData = ""; // Buffer to store received data
  if (RS485.available()) { // Check if data is available on RS485
    // Send the received data back
    char incomingByte = RS485.read(); // Read a single byte from RS485
    // Check if it is a newline character
    if (incomingByte == '\n') {
      // If it is a newline character, send the received content
      RS485.write(receivedData.c_str()); // Send the received data back
      RS485.write("\n"); // Manually add a newline character
      // Clear the receive buffer, ready to receive the next piece of data
      receivedData = "";
    } else {
      // If it is not a newline character, add the character to the receive buffer
      receivedData += incomingByte;
    }
  }
}