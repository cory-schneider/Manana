#include <AccelStepper.h>

#define STEP_PIN 2     // X-STEP on CNC Shield
#define DIR_PIN 5      // X-DIR on CNC Shield
#define ENABLE_PIN 8   // X-ENABLE on CNC Shield
#define ENDSTOP_PIN 9  // End stop switch input (Normally Closed - NC)

#define STEPS_PER_REV 1600    // 200 full steps * 8 for 1/8 microstepping
#define BELT_REDUCTION 6
#define MAX_TRAVEL_STEPS (STEPS_PER_REV * BELT_REDUCTION * (300.0 / 360.0))  // 300 degrees of travel
#define HALF_REV_STEPS (STEPS_PER_REV / 2) * BELT_REDUCTION  // Steps for 180 degrees
#define STEPS_PER_DEGREE (STEPS_PER_REV * BELT_REDUCTION / 360.0)  // Steps per degree

// Define max speed and default speeds
#define MAX_SPEED 6000.0      // Maximum speed in steps/sec
#define DEFAULT_ACCEL 4000.0  // Default acceleration in steps/sec^2

// Define stepper (1 = driver type, STEP_PIN, DIR_PIN)
AccelStepper stepper(1, STEP_PIN, DIR_PIN);

// Jogging state
bool isJogging = false;
int jogDirection = 0; // 1 for CW, -1 for CCW, 0 for stopped
float currentSpeed = MAX_SPEED; // Default speed in steps/sec
int currentSpeedPercentage = 100; // Default 100% of max speed
unsigned long lastPositionUpdate = 0;
const unsigned long POSITION_UPDATE_INTERVAL = 50; // Update position every 50ms

// Function to detect endstop with debouncing
bool endstopTriggered() {
  static int stableHits = 0;
  if (digitalRead(ENDSTOP_PIN) == HIGH) {
    stableHits++;
  } else {
    stableHits = 0;
  }
  return (stableHits >= 3);  // Require 3 stable reads before accepting
}

// Function for incremental homing approach
void incrementalApproach(long stepChunk, float speed) {
  stepper.setMaxSpeed(speed);
  stepper.setAcceleration(1200);  // HOMING_ACCELERATION
  
  while (true) {
    stepper.move(-stepChunk);
    while (stepper.distanceToGo() != 0) {
      stepper.run();
      if (endstopTriggered()) {
        stepper.stop(); // Smooth decelerate
        while (stepper.isRunning()) { stepper.run(); }
        return;
      }
    }
  }
}

// Function to home the motor and set zero position
void homeAndZero() {
  Serial.println("Starting incremental homing...");

  // Fast incremental approach
  incrementalApproach(1000, 1200);  // HOMING_SPEED_FAST (150 * 8)
  Serial.println("Endstop detected. Backing off...");

  // Back off
  stepper.setMaxSpeed(480);  // HOMING_SPEED_FAST
  stepper.setAcceleration(1200);  // HOMING_ACCELERATION
  stepper.move(400);  // HOMING_BACKOFF_STEPS
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }

  // Slow precision approach
  Serial.println("Slow precision approach...");
  incrementalApproach(400, 480);  // HOMING_SPEED_SLOW (60 * 8)
  Serial.println("Endstop confirmed at low speed.");

  // Set current position as zero
  stepper.setCurrentPosition(0);
  Serial.println("Homing complete. Position zeroed.");

  // Restore normal speed settings
  stepper.setMaxSpeed(currentSpeed);
  stepper.setAcceleration(DEFAULT_ACCEL);
}

// Function to send position updates to the server
void updatePosition() {
  unsigned long currentTime = millis();
  if (currentTime - lastPositionUpdate >= POSITION_UPDATE_INTERVAL) {
    Serial.print("POS:");
    Serial.println(stepper.currentPosition());
    lastPositionUpdate = currentTime;
  }
}

// Function to send speed updates to the server
void updateSpeed() {
  Serial.print("SPEED:");
  Serial.println(currentSpeedPercentage);
}

void setup() {
  Serial.begin(9600);
  
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(ENDSTOP_PIN, INPUT_PULLUP);
  
  digitalWrite(ENABLE_PIN, LOW);  // Enable stepper driver (LOW = ON)
  
  stepper.setMaxSpeed(currentSpeed);
  stepper.setAcceleration(DEFAULT_ACCEL);

  Serial.println("Starting homing process...");
  homeAndZero();  // Run homing on startup

  // Move to center position (150 degrees) after homing
  Serial.println("Moving to center position after homing...");
  long centerPosition = MAX_TRAVEL_STEPS / 2;
  stepper.moveTo(centerPosition);
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  Serial.print("At center position: ");
  Serial.println(stepper.currentPosition());

  // Send initial position
  Serial.print("POS:");
  Serial.println(stepper.currentPosition());

  // Send initial speed
  updateSpeed();

  Serial.println("Waiting for commands...");
}

void loop() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    
    if (data == "RESET") {
      Serial.println("Reset command received. Homing...");
      isJogging = false; // Stop any jogging
      jogDirection = 0;
      currentSpeed = MAX_SPEED; // Reset speed to max
      currentSpeedPercentage = 100;
      stepper.setMaxSpeed(currentSpeed);
      homeAndZero();

      Serial.println("Moving to center position...");
      long centerPosition = MAX_TRAVEL_STEPS / 2;
      stepper.moveTo(centerPosition);
      while (stepper.distanceToGo() != 0) {
        stepper.run();
        updatePosition(); // Update position during movement
      }
      Serial.print("Reset complete. At position: ");
      Serial.println(stepper.currentPosition());

      Serial.print("POS:");
      Serial.println(stepper.currentPosition());
      updateSpeed();
      
    } else if (data.startsWith("JOG_CW")) {
      // Check if speed parameter is provided
      int speedValue = 100; // Default to 100% if not specified
      
      // Parse speed parameter if present
      int spaceIndex = data.indexOf(' ');
      if (spaceIndex > 0) {
        String speedStr = data.substring(spaceIndex + 1);
        speedValue = speedStr.toInt();
        // Ensure speed is within range
        speedValue = constrain(speedValue, 1, 100);
      }
      
      // Calculate actual speed based on percentage
      currentSpeedPercentage = speedValue;
      currentSpeed = (MAX_SPEED * speedValue) / 100.0;
      
      Serial.print("Starting clockwise jog at ");
      Serial.print(currentSpeedPercentage);
      Serial.println("% speed...");
      
      isJogging = true;
      jogDirection = 1;
      stepper.setMaxSpeed(currentSpeed);
      stepper.moveTo(MAX_TRAVEL_STEPS); // Move to max position (will be constrained)
      
      // Send speed update
      updateSpeed();
      
    } else if (data.startsWith("JOG_CCW")) {
      // Check if speed parameter is provided
      int speedValue = 100; // Default to 100% if not specified
      
      // Parse speed parameter if present
      int spaceIndex = data.indexOf(' ');
      if (spaceIndex > 0) {
        String speedStr = data.substring(spaceIndex + 1);
        speedValue = speedStr.toInt();
        // Ensure speed is within range
        speedValue = constrain(speedValue, 1, 100);
      }
      
      // Calculate actual speed based on percentage
      currentSpeedPercentage = speedValue;
      currentSpeed = (MAX_SPEED * speedValue) / 100.0;
      
      Serial.print("Starting counterclockwise jog at ");
      Serial.print(currentSpeedPercentage);
      Serial.println("% speed...");
      
      isJogging = true;
      jogDirection = -1;
      stepper.setMaxSpeed(currentSpeed);
      stepper.moveTo(0); // Move to min position (will be constrained)
      
      // Send speed update
      updateSpeed();
      
    } else if (data == "STOP_JOG") {
      Serial.println("Stopping jog...");
      isJogging = false;
      jogDirection = 0;
      stepper.stop(); // Smooth decelerate
      while (stepper.isRunning()) {
        stepper.run();
        updatePosition(); // Update position during deceleration
      }
      
      // Reset speed display
      currentSpeedPercentage = 0;
      updateSpeed();
      
      Serial.print("POS:");
      Serial.println(stepper.currentPosition());
      
    } else if (data.startsWith("MOVE ")) {
      String positionStr = data.substring(5);
      long targetPosition = positionStr.toInt();

      targetPosition = constrain(targetPosition, 0, MAX_TRAVEL_STEPS);

      Serial.print("Moving to position: ");
      Serial.println(targetPosition);
      isJogging = false; // Stop any jogging
      jogDirection = 0;
      
      // Reset to max speed for precise movements
      currentSpeed = MAX_SPEED;
      currentSpeedPercentage = 100;
      stepper.setMaxSpeed(currentSpeed);
      
      stepper.moveTo(targetPosition);
      while (stepper.distanceToGo() != 0) {
        stepper.run();
        updatePosition(); // Update position during movement
      }

      Serial.print("POS:");
      Serial.println(stepper.currentPosition());
      updateSpeed();
      
    } else if (data.startsWith("SET_SPEED ")) {
      String speedStr = data.substring(10);
      float speedValue = speedStr.toFloat();
      speedValue = constrain(speedValue, 0, 6000); // Limit speed to max
      
      currentSpeed = speedValue;
      currentSpeedPercentage = (currentSpeed / MAX_SPEED) * 100;
      
      Serial.print("Setting speed to: ");
      Serial.print(currentSpeed);
      Serial.print(" (");
      Serial.print(currentSpeedPercentage);
      Serial.println("%)");
      
      stepper.setMaxSpeed(currentSpeed);
      updateSpeed();
    }
  }

  // Handle jogging
  if (isJogging) {
    // Constrain movement within limits
    long currentPos = stepper.currentPosition();
    if (jogDirection == 1 && currentPos >= MAX_TRAVEL_STEPS) {
      stepper.stop();
      isJogging = false;
      jogDirection = 0;
      // Reset speed display
      currentSpeedPercentage = 0;
      updateSpeed();
      Serial.print("POS:");
      Serial.println(stepper.currentPosition());
    } else if (jogDirection == -1 && currentPos <= 0) {
      stepper.stop();
      isJogging = false;
      jogDirection = 0;
      // Reset speed display
      currentSpeedPercentage = 0;
      updateSpeed();
      Serial.print("POS:");
      Serial.println(stepper.currentPosition());
    }
  }

  stepper.run();
  updatePosition(); // Continuously update position
}