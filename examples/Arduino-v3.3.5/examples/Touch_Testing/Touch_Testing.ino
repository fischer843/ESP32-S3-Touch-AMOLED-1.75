#include <Wire.h>             // Provides I2C communication support
#include "pin_config.h"       // Contains pin definitions such as IIC_SDA, IIC_SCL, TP_RESET, TP_INT
#include "TouchDrvCSTXXX.hpp" // Library for the touch controller

// Create a touch controller object.
// We will use this object later to communicate with the touch hardware.
TouchDrvCST92xx touch;

// Arrays for touch coordinates.
// x[0], y[0] = first finger
// x[1], y[1] = second finger
// and so on.
// The arrays can store up to 5 touch points.
int16_t x[5], y[5];

// This variable remembers whether a touch interrupt was triggered.
// "volatile" is important because the variable can change outside
// the normal program flow, namely inside the ISR.
volatile bool isPressed = false;

// This is the Interrupt Service Routine (ISR).
// It is called automatically when the touch interrupt pin is triggered.
//
// IRAM_ATTR is commonly used on ESP32 interrupt functions
// so the function is placed where it can be executed reliably and quickly.
//
// Important:
// An ISR should do as little work as possible.
// That is why we only set a flag here.
void IRAM_ATTR touchInterrupt() {
  isPressed = true;
}

void setup() {
  // Start the serial interface at 115200 baud.
  // We will later see debug messages in the Serial Monitor.
  Serial.begin(115200);

  // Small delay so the Serial Monitor has time to connect.
  delay(1000);

  Serial.println("\n--- Minimal Touch Test ---");

  // ------------------------------------------------------------
  // 1. Hardware reset of the touch controller
  // ------------------------------------------------------------
  //
  // Many chips need a defined reset sequence after power-up
  // so they start in a clean and known state.
  //
  // Configure TP_RESET as an output pin.
  pinMode(TP_RESET, OUTPUT);

  // Activate reset:
  // LOW means the touch controller is forced into reset state.
  digitalWrite(TP_RESET, LOW);
  delay(30); // wait a short moment

  // Release reset:
  // HIGH means the controller may start normally.
  digitalWrite(TP_RESET, HIGH);
  delay(50); // wait again so the chip can boot up

  // ------------------------------------------------------------
  // 2. Start the I2C bus
  // ------------------------------------------------------------
  //
  // I2C is the communication link between ESP32 and touch controller.
  // SDA = data line
  // SCL = clock line
  //
  // The actual pin numbers come from pin_config.h
  Wire.begin(IIC_SDA, IIC_SCL);

  // ------------------------------------------------------------
  // 3. Initialize the touch sensor
  // ------------------------------------------------------------
  //
  // Tell the library which pins are used for reset and interrupt.
  touch.setPins(TP_RESET, TP_INT);

  // Now try to start the touch controller.
  //
  // Parameters:
  // Wire       -> the I2C bus to use
  // 0x5A       -> I2C address of the touch controller
  // IIC_SDA    -> SDA pin
  // IIC_SCL    -> SCL pin
  //
  // Return value:
  // true  = initialization successful
  // false = sensor not found or communication failed
  bool result = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);

  // If initialization failed:
  if (!result) {
    Serial.println("Error: Touch sensor not found! Please check the wiring.");

    // Endless loop:
    // Stop here because the rest of the sketch does not make sense
    // if the touch controller is not reachable.
    while (1) delay(1000);
  }

  // If initialization worked, print the detected model name.
  Serial.print("Successfully connected. Model: ");
  Serial.println(touch.getModelName());

  // ------------------------------------------------------------
  // Optional feature: screen cover detection
  // ------------------------------------------------------------
  //
  // Here we register a callback function.
  // "Callback" means:
  // The library will call this function later automatically
  // when a certain event is detected.
  //
  // [](void *ptr) { ... } is an anonymous function (lambda).
  // For beginners, it is enough to think of it like this:
  // "If screen cover is detected, run this code."
  touch.setCoverScreenCallback([](void *ptr) {
    Serial.println("Warning: Screen is covered!");
  }, NULL);

  // ------------------------------------------------------------
  // 4. Configure the interrupt
  // ------------------------------------------------------------
  //
  // TP_INT is the interrupt pin from the touch controller.
  //
  // INPUT_PULLUP means:
  // The pin is used as an input and internally pulled up to HIGH.
  // This prevents the input from floating.
  pinMode(TP_INT, INPUT_PULLUP);

  // attachInterrupt links the pin to our ISR.
  //
  // TP_INT          -> pin to monitor
  // touchInterrupt  -> function to call when the interrupt occurs
  // FALLING         -> trigger on a transition from HIGH to LOW
  //
  // This often matches touch interrupt lines,
  // because the controller actively pulls the line LOW on an event.
  attachInterrupt(TP_INT, touchInterrupt, FALLING);

  Serial.println("Setup complete. Waiting for touch...");
}

void loop() {
  // Check whether the ISR reported that a touch event happened.
  if (isPressed) {

    // Read the touch data.
    //
    // getPoint(...) writes the detected coordinates into x[] and y[].
    //
    // Return value:
    // Number of currently detected touch points.
    uint8_t touchedPoints = touch.getPoint(x, y, touch.getSupportTouchPoint());

    // Only continue if at least one touch point was detected.
    if (touchedPoints > 0) {
      Serial.print("Touch detected! Number of fingers: ");
      Serial.println(touchedPoints);

      // Loop through all detected fingers
      for (int i = 0; i < touchedPoints; i++) {
        // Print the coordinates of each finger
        //
        // i + 1 is used only for display,
        // so numbering starts at 1 for humans.
        Serial.printf("  Finger %d -> X: %d, Y: %d\n", i + 1, x[i], y[i]);
      }

      Serial.println("-------------------------");
    }

    // Reset the flag:
    // We are now ready to wait for the next interrupt event.
    isPressed = false;
  }

  // Small delay so the loop does not run unnecessarily at full speed.
  delay(10);
}