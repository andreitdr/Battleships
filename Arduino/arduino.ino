#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <LiquidCrystal.h>
#include <avr/pgmspace.h>

#define LED_PIN 4
#define MATRIX_WIDTH  8
#define MATRIX_HEIGHT 8
#define NUM_LEDS      (MATRIX_WIDTH * MATRIX_HEIGHT)

#define JOYSTICK_X A0
#define JOYSTICK_Y A1
#define JOYSTICK_SW 2

#define LCD_LINES 2
#define LCD_COLUMNS 16

static const uint16_t LOW_THRESHOLD  = 400;  
static const uint16_t HIGH_THRESHOLD = 600;  

static const CRGB COLOR_CURSOR       = CRGB(255,255,255);
static const CRGB COLOR_NONE         = CRGB(0,0,0);
static const CRGB COLOR_TARGET_HIT   = CRGB(0,255,0);
static const CRGB COLOR_TARGET_MISS  = CRGB(255,0,0);
static const CRGB COLOR_TARGET_KILL  = CRGB(0,0,255);

static const CRGB COLOR_REVEAL       = CRGB(128,0,128); 

struct ShipSegment {
  int8_t x;
  int8_t y;
};

struct Ship {
  ShipSegment* segments;  
  uint8_t length; 
  bool isSunk;     
};

struct Shape {
  uint8_t length;           
  const int8_t coords[5][2];
};

static const Shape shapeL = {
  4, 
  { {0,0}, {1,0}, {2,0}, {2,1} }
};
static const Shape shapeT = {
  4, 
  { {1,0}, {0,1}, {1,1}, {2,1} }
};
static const Shape shapePlus = {
  5, 
  { {1,0}, {1,1}, {0,1}, {2,1}, {1,2} }
};
static const Shape shapeLine = {
  3, 
  { {0,0}, {0,1}, {0,2} }
};

static const Shape allShapes[] = {
  shapeL, shapeT, shapePlus, shapeLine
};

#define MAX_SHIPS_ALLOWED    4
#define MAX_SEGMENTS_ALLOWED 5

static int8_t  cursorX = 0;
static int8_t  cursorY = 0;
static int8_t  oldDirectionX = 0;
static int8_t  oldDirectionY = 0;

static uint8_t maxShips = 3; 
static uint8_t maxTries = 32; 
static uint8_t currentTries = 0;

static bool gameActive = false;

static Ship ships[MAX_SHIPS_ALLOWED];
static ShipSegment shipsSegments[MAX_SHIPS_ALLOWED * MAX_SEGMENTS_ALLOWED];

static uint8_t enemyBoard[MATRIX_HEIGHT][MATRIX_WIDTH];
static CRGB persistentColor[MATRIX_HEIGHT][MATRIX_WIDTH];

static CRGB matrixLeds[NUM_LEDS];
static FastLED_NeoMatrix* matrix = new FastLED_NeoMatrix(
  matrixLeds, 
  MATRIX_WIDTH, 
  MATRIX_HEIGHT,
  1, 1,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS
);

static LiquidCrystal lcd(7,8,9,10,11,12);

void printMessage(const __FlashStringHelper* text);

void printMessage(const char* text) {
  lcd.clear();
  uint8_t textLength = 0;
  
  for (uint8_t i = 0; text[i] != '\0'; i++) {
    textLength++;
  }

  if (textLength <= 16) {
    lcd.setCursor(0, 0);
    lcd.print(text);
  } else {
    char line0[17];
    for (uint8_t i = 0; i < 16; i++) {
      line0[i] = text[i];
    }
    line0[16] = '\0';
    lcd.setCursor(0, 0);
    lcd.print(line0);

    uint8_t remainingChars = textLength - 16;
    if (remainingChars > 16) {
      remainingChars = 16;
    }
    char line1[17];
    for (uint8_t i = 0; i < remainingChars; i++) {
      line1[i] = text[16 + i];
    }
    line1[remainingChars] = '\0';
    lcd.setCursor(0, 1);
    lcd.print(line1);
  }
}

void printMessage(const __FlashStringHelper* text) {
  char buffer[64];
  strcpy_P(buffer, (PGM_P)text); 
  printMessage(buffer);
}

static void sendColorUpdate(int8_t x, int8_t y, const char* cmd)
{
  Serial.print(cmd);
  Serial.print(" ");
  Serial.print(x);
  Serial.print(" ");
  Serial.println(y);
}

static void sendSerialMsg(const __FlashStringHelper* msg) {
  Serial.println(msg);
}
static void sendSerialMsg(const char* msg) {
  Serial.println(msg);
}

static void rotatePoint90(int8_t &dx, int8_t &dy) {
  int8_t temp = dx;
  dx = -dy;
  dy = temp;
}

static void pickRandomShape(uint8_t &outLength, int8_t outCoords[][2]) {
  uint8_t shapeIndex = random(0, 4);
  const Shape &chosen = allShapes[shapeIndex];

  uint8_t rotations = random(0, 4);
  outLength = chosen.length;

  for(uint8_t i = 0; i < outLength; i++) {
    outCoords[i][0] = chosen.coords[i][0];
    outCoords[i][1] = chosen.coords[i][1];
  }

  for(uint8_t r = 0; r < rotations; r++){
    for(uint8_t i = 0; i < outLength; i++){
      rotatePoint90(outCoords[i][0], outCoords[i][1]);
    }
  }
}

static bool placeShapeOnBoard(Ship &ship, uint8_t length, int8_t coords[][2], int8_t startX, int8_t startY) {
  for(uint8_t i = 0; i < length; i++){
    int8_t xx = startX + coords[i][0];
    int8_t yy = startY + coords[i][1];
    if(xx < 0 || xx >= MATRIX_WIDTH || yy < 0 || yy >= MATRIX_HEIGHT) {
      return false;
    }
    if(enemyBoard[yy][xx] == 1) {
      return false;
    }
  }

  for(uint8_t i = 0; i < length; i++){
    int8_t xx = startX + coords[i][0];
    int8_t yy = startY + coords[i][1];
    enemyBoard[yy][xx] = 1;
    ship.segments[i].x = xx;
    ship.segments[i].y = yy;
  }
  ship.length = length;
  ship.isSunk = false;
  return true;
}

static void placeOneShip(uint8_t shipIndex) {
  Ship &ship = ships[shipIndex];

  uint8_t shapeLen;
  int8_t shapeCoords[5][2];
  pickRandomShape(shapeLen, shapeCoords);

  ship.segments = &shipsSegments[shipIndex * MAX_SEGMENTS_ALLOWED];

  bool placed = false;
  while(!placed) {
    int8_t startX = random(0, MATRIX_WIDTH);
    int8_t startY = random(0, MATRIX_HEIGHT);
    placed = placeShapeOnBoard(ship, shapeLen, shapeCoords, startX, startY);
  }
}

static void clearShips() {
  for(uint8_t i = 0; i < MAX_SHIPS_ALLOWED; i++) {
    ships[i].segments = NULL;
    ships[i].isSunk   = false;
    ships[i].length   = 0;
  }
}

static void resetGame() {
  clearShips();

  for(uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for(uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      enemyBoard[y][x] = 0;
      persistentColor[y][x] = COLOR_NONE;
    }
  }

  randomSeed(analogRead(A5));

  for(uint8_t i = 0; i < maxShips; i++){
    placeOneShip(i);
  }

  cursorX = 0;
  cursorY = 0;
  oldDirectionX = 0;
  oldDirectionY = 0;
  currentTries  = 0;

  gameActive = true;

  printMessage(F("Jocul a inceput!"));
  sendSerialMsg(F("RESET"));
  Serial.print(F("TOTAL_SHIPS=")); 
  Serial.println(maxShips);
  Serial.print(F("TOTAL_ATTACKS="));
  Serial.println(maxTries);
}

static int8_t findShipByCoords(int8_t x, int8_t y) {
  for (uint8_t i = 0; i < maxShips; i++) {
    if (!ships[i].isSunk) {
      for(uint8_t s = 0; s < ships[i].length; s++) {
        if (ships[i].segments[s].x == x && ships[i].segments[s].y == y) {
          return i;
        }
      }
    }
  }
  return -1;
}

static bool checkIfShipSunk(uint8_t shipIndex) {
  for (uint8_t s = 0; s < ships[shipIndex].length; s++) {
    int8_t sx = ships[shipIndex].segments[s].x;
    int8_t sy = ships[shipIndex].segments[s].y;
    CRGB col = persistentColor[sy][sx];
    if (col != COLOR_TARGET_HIT && col != COLOR_TARGET_KILL) {
      return false; 
    }
  }
  return true;
}

static void killShip(uint8_t shipIndex) {
  for (uint8_t s = 0; s < ships[shipIndex].length; s++) {
    int8_t sx = ships[shipIndex].segments[s].x;
    int8_t sy = ships[shipIndex].segments[s].y;
    persistentColor[sy][sx] = COLOR_TARGET_KILL;

    sendColorUpdate(sx, sy, "KILL");
  }
  ships[shipIndex].isSunk = true;

  sendSerialMsg(F("SHIP_DESTROYED"));
}

static void checkAllShipsSunk() {
  for(uint8_t i = 0; i < maxShips; i++){
    if (!ships[i].isSunk) {
      return; 
    }
  }
  sendSerialMsg(F("WIN"));
  printMessage(F("Ai castigat!"));
  gameActive = false;
}

static void revealShips() {
  for (uint8_t i = 0; i < maxShips; i++) {
    if (!ships[i].isSunk) {
      for (uint8_t s = 0; s < ships[i].length; s++) {
        int8_t sx = ships[i].segments[s].x;
        int8_t sy = ships[i].segments[s].y;
        if (persistentColor[sy][sx] == COLOR_NONE) {
          persistentColor[sy][sx] = COLOR_REVEAL;
          sendColorUpdate(sx, sy, "REVEAL");
        }
      }
    }
  }
}

static void attackCell(int8_t x, int8_t y) {
  if (enemyBoard[y][x] == 1) {
    persistentColor[y][x] = COLOR_TARGET_HIT;
    sendColorUpdate(x, y, "HIT");

    int8_t shipIdx = findShipByCoords(x, y);
    if (shipIdx >= 0) {
      if (checkIfShipSunk((uint8_t)shipIdx)) {
        killShip((uint8_t)shipIdx);
      }
    }
  } else {
    persistentColor[y][x] = COLOR_TARGET_MISS;
    sendColorUpdate(x, y, "MISS");
  }
  checkAllShipsSunk();
}

static void setDifficulty(uint8_t difficulty) {
  switch(difficulty) {
    case 0:
      maxShips = 2;
      maxTries = 48;
      break;
    case 2:
      maxShips = 4;
      maxTries = 28;
      break;
    default:
      maxShips = 3;
      maxTries = 32;
      break;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "Dif=%u S=%u T=%u", difficulty, maxShips, maxTries);
  printMessage(buf);
}

static void parseSerialCommand(char* cmd) {
  if (strncmp(cmd, "START", 5) == 0) {
    resetGame();
  }
  else if (strncmp(cmd, "SET DIFFICULTY=", 15) == 0) {
    uint8_t diff = (uint8_t)(cmd[15] - '0');
    setDifficulty(diff);
  } 
  else {
    printMessage(F("CMD necunoscut"));
  }
}

void setup() {
  lcd.begin(LCD_COLUMNS, LCD_LINES);

  FastLED.addLeds<NEOPIXEL, LED_PIN>(matrixLeds, NUM_LEDS);
  matrix->begin();
  matrix->setBrightness(10);
  matrix->fillScreen(0);

  pinMode(JOYSTICK_X, INPUT);
  pinMode(JOYSTICK_Y, INPUT);
  pinMode(JOYSTICK_SW, INPUT_PULLUP);

  Serial.begin(9600);

  resetGame(); 
  printMessage(F("Jocul a inceput!"));
}

void loop() {
  if (Serial.available() > 0) {
    char buffer[32];
    int len = Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    if (len < 0) len = 0;
    buffer[len] = '\0';

    int pos = len - 1;
    while (pos >= 0 && (buffer[pos] == '\r' || buffer[pos] == '\n' || buffer[pos] == ' ')) {
      buffer[pos] = '\0';
      pos--;
    }
    for (int i = 0; i <= pos; i++) {
      buffer[i] = toupper(buffer[i]);
    }

    if (pos >= 0) {
      parseSerialCommand(buffer);
    }
  }

  if (!gameActive) {
    return;
  }

  uint16_t xVal = analogRead(JOYSTICK_X);
  uint16_t yVal = analogRead(JOYSTICK_Y);

  int8_t currentDirectionX = 0;
  int8_t currentDirectionY = 0;

  if (xVal < LOW_THRESHOLD)       currentDirectionX = -1; 
  else if (xVal > HIGH_THRESHOLD) currentDirectionX =  1; 

  if (yVal < LOW_THRESHOLD)       currentDirectionY = -1; 
  else if (yVal > HIGH_THRESHOLD) currentDirectionY =  1; 

  if (currentDirectionX != 0 && oldDirectionX == 0) {
    if (currentDirectionX == -1 && cursorX > 0) {
      cursorX--;
    } else if (currentDirectionX ==  1 && cursorX < (MATRIX_WIDTH - 1)) {
      cursorX++;
    }
  }
  if (currentDirectionY != 0 && oldDirectionY == 0) {
    if (currentDirectionY == -1 && cursorY > 0) {
      cursorY--;
    } else if (currentDirectionY == 1 && cursorY < (MATRIX_HEIGHT - 1)) {
      cursorY++;
    }
  }
  oldDirectionX = currentDirectionX;
  oldDirectionY = currentDirectionY;

  if (digitalRead(JOYSTICK_SW) == LOW) {
    currentTries++;
    Serial.println(F("ATTACK"));

    if (currentTries > maxTries) {
      revealShips();
      sendSerialMsg(F("LOSE"));
      printMessage(F("Ai pierdut!"));
      gameActive = false;
    } else {
      attackCell(cursorX, cursorY);
    }
    delay(150); 
  }

  matrix->fillScreen(0);
  for(uint8_t yy = 0; yy < MATRIX_HEIGHT; yy++) {
    for(uint8_t xx = 0; xx < MATRIX_WIDTH; xx++) {
      if (persistentColor[yy][xx] != COLOR_NONE) {
        matrix->drawPixel(xx, yy, persistentColor[yy][xx]);
      }
    }
  }
  
  if (gameActive) {
    matrix->drawPixel(cursorX, cursorY, COLOR_CURSOR);
  }
  matrix->show();

  delay(50);
}
