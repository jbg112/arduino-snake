#include "ezButton.h"
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "secrets.h"

// Arduino Led Matrix
ArduinoLEDMatrix matrix;

// Pins of the Joystick; up/down, left/right, and press
#define VRX_PIN  A0
#define VRY_PIN  A1
#define SW_PIN   2
ezButton button(SW_PIN);

// Board variables  
byte matrixx[8][12];
int xValue = 0;
int yValue = 0;
int bValue = 0;
int currentpositionX = 3;
int currentpositionY = 5;
int directionX;
int directionY;

void setup() {
  Serial.begin(9600);
  matrix.begin();
  matrixx[3][5] = 1;
  matrix.renderBitmap(matrixx, 8, 12);
}

void loop() {
  // Clear the board so it can be drawn again
  clearMatrix();

  // read analog X and Y values of the joystick
  xValue = analogRead(VRX_PIN);
  yValue = analogRead(VRY_PIN);
  // Read the joystick button value
  bValue = button.getState();

  // Only update the direction if button press
  if (bValue == HIGH) {
    updateDirection(xValue, yValue);
  }
  // move the snake
  moveSnake();
  // light (the led of) the current position
  matrixx[currentpositionX][currentpositionY] = 1;
  // render the matrix
  matrix.renderBitmap(matrixx, 8, 12);
  // add game delay, a game update every 85 ms, so the snake moves and everything else
  delay(85);
}

void updateDirection(int xValue, int yValue) {
  // Update direction based on joystick input
  if (xValue > 1020) {
    directionX = -1;
  } else if (xValue < 200) {
    directionX = 1;
  } else {
    directionX = 0;
  }

  if (yValue > 1020) {
    directionY = -1;
  } else if (yValue < 200) {
    directionY = 1;
  } else {
    directionY = 0;
  }
}
void moveSnake() {
  // Move the snake continuously in the current direction
  currentpositionX += directionX;
  currentpositionY += directionY;
  // Keep the snake in bounds of matrix check
  checkPositionXY();
}

void clearMatrix() {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 12; j++) {
      matrixx[i][j] = 0;
    }
  }
  matrix.renderBitmap(matrixx, 8, 12);
}

bool checkMatrix(int x, int y) {
  if (matrixx[x][y] == 0) {
    return true;
  } else {
    return false;
  }
}

void checkPositionXY() {
  // Check if the snake has moved "outside of the led matrix grid", if so place them on the other side of the board
  if (currentpositionX > 7) {
    currentpositionX = 0;
  } else if (currentpositionX < 0) {
    currentpositionX = 7;
  }
  if (currentpositionY > 11) {
    currentpositionY = 0;
  } else if (currentpositionY < 0) {
    currentpositionY = 11;
  }
}

void toggleLED(int x, int y) {
  // Toggle on or off
  matrixx[x][y] = 1 - matrixx[x][y];

  matrix.renderBitmap(matrixx, 8, 12);
}