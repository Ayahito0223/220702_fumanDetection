#include "Ultrasonic.h"

Ultrasonic ultrasonic(7);
void setup() {
  Serial.begin(9600);
}

long RangeInCentimeters;
void loop() {
  RangeInCentimeters = ultrasonic.MeasureInCentimeters();
  Serial.print(RangeInCentimeters);
  Serial.println(" cm");
  delay(1000);
}
