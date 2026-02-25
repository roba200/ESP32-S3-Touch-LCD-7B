/*
* This example implements the LED flashing at a frequency of 500 milliseconds.
*/
#define LED 6

void setup() {
  // put your setup code here, to run once:
  pinMode(LED, OUTPUT); // sets the digital pin 6 as output
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(LED, HIGH); // sets the digital pin 6 off
  delay(500);            // waits for a second
  digitalWrite(LED, LOW);  // sets the digital pin 6 on
  delay(500);            // waits for a second
}
