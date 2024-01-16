#include "ezButton.h"
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "WiFiS3.h"
#include "WiFiSSLClient.h"
#include "ArduinoMqttClient.h"
#include "secrets.h"

// Arduino Led Matrix
ArduinoLEDMatrix matrix;

// Pins of the Joystick; up/down, left/right, and presssing down
#define JOYSTICK_X_PIN  A0
#define JOYSTICK_Y_PIN  A1
#define JOYSTICK_PUSH_PIN   2
ezButton button(JOYSTICK_PUSH_PIN);

// Wifi connection variable
WiFiClient client;

// MQTT connection variable
MqttClient mqttClient(client); 

// Snake tail variables
int tailLength = 0;
// Max amount of leds is 96 so max score can only be 95 to keep functioning
const int maxTailLength = 95;
int tailX[maxTailLength];
int tailY[maxTailLength];

// Board/game variables   
byte matrixx[8][12];
int xValue = 0;
int yValue = 0;
int bValue = 0;
int currentpositionX = 3;
int currentpositionY = 5;
bool foodExists = false;
int foodXpos;
int foodYpos;
unsigned long previousMillis = 0;
const long interval = 150;
int directionX;
int directionY;
bool gamePaused = false;

// Score variables
int score;
int highscore;
bool scoreAdded = false;

void setup() {
  Serial.begin(9600);
  button.setDebounceTime(50);
  matrix.begin();
  matrix.renderBitmap(matrixx, 8, 12);

  // Internet connection
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to wifi network
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    DrawMatrixText("Con");
    delay(2500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    while (WiFi.gatewayIP() == "0.0.0.0");
  }

  // Set MQTT username and password
  mqttClient.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);
  // Connect to MQTT
  if (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    while (1);
  }
  // define function to execute when getting data
  mqttClient.onMessage(getHighscore);
  // Listen to highscore topic for new highscores
  mqttClient.subscribe("jbtest/highscore", 2);
  mqttClient.poll();
}

void loop() {
  button.loop();
  // Get MQTT data
  mqttClient.poll();
  // Check if the pauze button has been pressed to pauze or unpauze the game
  gamePauze();
  // If the game is not pauzed
  if (!gamePaused) {
    // Clear the board so it can be drawn again
    clearMatrix();

    // read analog X and Y values of the joystick
    xValue = analogRead(JOYSTICK_X_PIN);
    yValue = analogRead(JOYSTICK_Y_PIN);
    // Read the joystick button value
    bValue = button.getState();

    // If the snake has picked up a food item, make another one
    if (foodExists == false) {
      createFood();
    }

    // Check if the snake has collided with a food item
    foodCollision();
    
    // Keep track of and draw the snake's tail
    updateTail();
    drawTail();

    // Only update the direction if button press
    if (bValue == HIGH) {
      updateDirection(xValue, yValue);
    }
    // move snake in the direction its going
    moveSnake();
    // check if snake has collided with itself
    selfCollisionDetection();
    // light (the led) the current position
    matrixx[currentpositionX][currentpositionY] = 1;

    unsigned long currentMillis = millis();

    // Blink food every so often
    if (currentMillis - previousMillis >= interval) {
      // Save blink time
      previousMillis = currentMillis;
      // Toggle the LED at food position
      toggleLED(foodXpos, foodYpos);
    }
    // render the matrix
    matrix.renderBitmap(matrixx, 8, 12);
    // game update every 75ms
    delay(75);
  }
}

void Reset() {
  // This function completely resets the "game", every aspect of the game is reset
  clearMatrix();
  currentpositionX = 3;
  currentpositionY = 5;
  foodExists = false;
  foodXpos = 0;
  foodYpos = 0;
  directionX = NULL;
  directionY = NULL;
  tailLength = 0;
  score = 0;
}

void gamePauze() {
  // Toggle the game pauze state when the button is pressed
  if (digitalRead(JOYSTICK_PUSH_PIN) == LOW) {
    gamePaused = !gamePaused;
    while (digitalRead(JOYSTICK_PUSH_PIN) == LOW) {
      // Wait for button to be released
      delay(10);
    }
  }
}

void sendMQTTmessage(String sendTopic, String sendMessage, bool retained) {
  // Send to MQTT broker
  mqttClient.beginMessage(sendTopic, retained);
  mqttClient.print(sendMessage);
  mqttClient.endMessage();
}

void getHighscore(int messageSize) {
  // On recieving highscore from MQTT onMessage, save it
  String recievedData;
  // get mqtt data
  while (mqttClient.available()) {
    recievedData += (char)mqttClient.read();
  }
  // update local highscore variable (as integer)
  highscore = recievedData.toInt();
}

void updateDirection(int xValue, int yValue) {
  // Update the direction of the snake based on joystick x and y input
  // Also checks if the snake is trying to back into itself
  if (xValue > 1020 && directionX != 1) {
    directionX = -1;
    directionY = 0;
  } else if (xValue < 200 && directionX != -1) {
    directionX = 1;
    directionY = 0;
  } else if (yValue > 1020 && directionY != 1) {
    directionX = 0;
    directionY = -1;
  } else if (yValue < 200 && directionY != -1) {
    directionX = 0;
    directionY = 1;
  }
}

void moveSnake() {
  // Move the snake continuously in the current direction
  currentpositionX += directionX;
  currentpositionY += directionY;

  // Keep the snake in bounds of matrix check
  checkPositionXY();
}

void selfCollisionDetection() {
  for (int i = 0; i < tailLength; i++) {
    // check if the head of the snake is in the same position as a tail element
    if (currentpositionX == tailX[i] && currentpositionY == tailY[i]) {
      // Display text on the led matrix to show users their score and highscore
      String discordMessage = "Score > " + String(score);
      String textToDisplay;
      // Check if new highscore
      if (score > highscore) {
        // update highscore in mqtt
        sendMQTTmessage("jbtest/highscore", String(score), true);
        textToDisplay += "NEW HIGHSCORE > ";
        textToDisplay += score;
      } else {
        textToDisplay += "Score > ";
        textToDisplay += score;
        textToDisplay += "   Highscore > ";
        textToDisplay += highscore;
      }
      // Draw the text
      DrawMatrixTextMoving(textToDisplay);
      // Reset the game; clear the board, variables etc
      Reset();
    }
  }
}

void clearMatrix() {
  // Reset every led in the matrix for next "frame"
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 12; j++) {
      matrixx[i][j] = 0;
    }
  }
}

void DrawMatrixText(String text) {
  // Convert the text to char so it can be drawn
  const char* charToDisplay = text.c_str();
  // Some settings for the text to display
  // "Draw" the reached score and highscore on the led matrix 
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_4x6);
  matrix.beginText(1, 1, 0xFFFFFF);
  // Text to display
  matrix.println(charToDisplay);
  matrix.endText();
  matrix.endDraw();
}

void DrawMatrixTextMoving(String text) {
  // add empty space before and after (padding) for readability (and scrolling animation)
  String TextToDisplay = "     ";
  TextToDisplay       += text;
  TextToDisplay       += "     ";
  // Convert the text to char so it can be drawn
  const char* charToDisplay = TextToDisplay.c_str();
  // "Draw" the reached score and highscore on the led matrix 
  matrix.beginDraw();
  // Color (matrix only supports red)
  matrix.stroke(0xFFFFFFFF);
  // Text scroll speed
  matrix.textScrollSpeed(45);
  // Font (width and size) of the text
  matrix.textFont(Font_5x7);
  // Text displayed at 1 led from the top and left
  matrix.beginText(1, 1, 0xFFFFFF);
  // Text to display
  matrix.println(charToDisplay);
  // "Scroll" the text from right to left
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

bool checkMatrix(int x, int y) {
  // return if the given matrix led is off
  if (matrixx[x][y] == 0) {
    return true;
  } else {
    return false;
  }
}

bool isSnakeSegment(int x, int y) {
  // Check if the position is occupied by the snake's head
  if (x == currentpositionX && y == currentpositionY) {
    return true;
  }

  // Check if the position is occupied by any tail segment
  for (int i = 0; i < tailLength; i++) {
    if (tailX[i] == x && tailY[i] == y) {
      return true;
    }
  }
  // if occupied by neither, return false
  return false;
}

void checkPositionXY() {
  // Check if the snake has moved "outside of the led matrix grid", if so place them on the other side of the board back in
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

void createFood() {
  do {
    // Generate random position for the "food" to be placed within the matrix
    int randomX = random(7);
    int randomY = random(11);
    if (checkMatrix(randomX, randomY) && !isSnakeSegment(randomX, randomY)) {
      foodXpos = randomX;
      foodYpos = randomY;
      matrixx[randomX][randomY] = 1;
      foodExists = true;
      // exit loop when spot found
      break;
    }
  } while (true); // continue until a empty spot is found (not already occupied by the snake)
}

void foodCollision() {
  // Check if the head of the snake is on the same x and y position as the food item
  if (currentpositionX == foodXpos && currentpositionY == foodYpos) {
    if (!scoreAdded) {
      foodExists = false;
      // When snake collides with "food," add another tail element and score
      createTailSegment();
      addScore();
      // scoreAdded variable to prevent giving double score with fast loop times
      scoreAdded = true;
    }
  } else {
    // reset scoreAdded
    scoreAdded = false;
  }
}

void addScore() {
  // add 1 score to total score
  score++;
  // send current score to MQTT
  sendMQTTmessage("jbtest/currentScore", String(score), false);
}

void createTailSegment() {
  // check if not exceeding tail lenght limit
  if (tailLength < maxTailLength) {
    // Add one to tailLength (creating a tail element)
    tailLength++;
  }
}

void updateTail() {
  // Move every tail element to the position of the tail element infront of them (moving the snake forward)
  for (int i = tailLength - 1; i > 0; i--) {
    tailX[i] = tailX[i - 1];
    tailY[i] = tailY[i - 1];
  }
  // Set the position of the first tail element to the last position of the snake head
  tailX[0] = currentpositionX;
  tailY[0] = currentpositionY;
}

void drawTail() {
  for (int i = 0; i < tailLength; i++) {
    matrixx[tailX[i]][tailY[i]] = 1;
  }
}

void toggleLED(int x, int y) {
  // Toggle a led on or off (if on set off, if off set on)
  matrixx[x][y] = 1 - matrixx[x][y];
}