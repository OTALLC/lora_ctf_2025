#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AESLib.h>

// OLED display parameters
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 32     // OLED display height, in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

AESLib aesLib;
uint8_t key[] = { 'g', 'm', 't', 'Z', 'A', 'w', 's', 'j', 'i', 'k', 'P', 'V', '4', 'y', 'g', '9'};
uint8_t encrypted[] = { 0xb9, 0xcd, 0x01, 0xa1, 0x18, 0x94, 0x02, 0x69, 0xe0, 0xa1, 0x27, 0x91, 0x84, 0x76, 0x18, 0xd4 };
char decrypted[17]; // 16 bytes + null terminator for string
uint8_t iv[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

unsigned long lastBeaconTime = 0;
const unsigned long beaconInterval = 20000;  // 30 seconds in milliseconds
#define MAX_FLAGS 3
String flagArray[MAX_FLAGS];
int flagCount = 0;

#define BUTTON_PIN 5  //GPIO5 (D5)
// LoRa module serial communication
HardwareSerial loraSerial(1);

// LoRa variables
#define LORA_NETWORKID 37     //needs to be 73
#define LORA_ADDRESS_SRC 101  //this radio's address 0~65535
#define LORA_ADDRESS_DEST 44   // 0=broadcast, 4=server
#define LORA_BAND 915000000


void setup() {
  // Initialize serial communication with the PC
  Serial.begin(9600);
  unsigned int to = 60;
  while (!Serial && --to) { // Wait briefly for Serial Monitor to open, then continue.
    delay(1);
  }  //continue whether we have serial connection or not. 
  Serial.println("Serial communication initialized.");


  // Initialize SoftwareSerial communication with the RYLR998 at default baud rate
  loraSerial.begin(115200, SERIAL_8N1, 3, 2);
  Serial.println("SoftwareSerial initialized at 115200 baud.");

  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {  // 0x3C is the I2C address
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  Serial.println("OLED display initialized.");
  delay(1000);

  // Clear the display buffer
  display.clearDisplay();

  // Set text properties
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Display initial message
  display.setCursor(0, 0);
  display.println("Initializing RYLR998...");
  display.display();

  Serial.println("Initializing RYLR998 Module...");
  loraSerial.flush();
  // Test connection with AT command
  sendCommand("AT+FACTORY");
  delay(500);
  readLoRaResponse();

  // Change baud rate to 9600
  sendCommand("AT+IPR=9600");
  delay(500);
  readLoRaResponse();

  // Reinitialize SoftwareSerial at new baud rate
  loraSerial.end();
  loraSerial.begin(9600, SERIAL_8N1, 3, 2);
  Serial.println("Baud rate changed to 9600");
  displayMessage("Baud rate changed to 9600");

  // Set module parameters
  configureLoRaModule();

  Serial.println("RYLR998 configuration complete.");
  delay(5000);

  // Initialize display with FLAG1
  uint16_t encrypted_len = 16; // Length of encrypted data
  aesLib.decrypt(encrypted, encrypted_len, (uint8_t*)decrypted, key, 16, iv);  
  decrypted[13] = '\0'; // Null-terminate after 13 characters to skip padding

  flagArray[0] = decrypted;
  flagCount = 1;

  // Display the initial flag on the OLED
  updateDisplay();

  // Configure the button pin as input with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  unsigned long currentTime = millis();

  // Read the button state
  int buttonState = digitalRead(BUTTON_PIN);

  // Check if the button is pressed (LOW because of pull-up resistor)
  if (buttonState == LOW) {
    // Debounce the button
    delay(50);  // Wait for 50ms to avoid bouncing
    // Confirm the button is still pressed
    if (digitalRead(BUTTON_PIN) == LOW) {
      sendFlagRequest();

      // Wait until the button is released to avoid multiple sends
      while (digitalRead(BUTTON_PIN) == LOW) {
        // Do nothing, just wait
        delay(10);
      }
    }
  }


  // Handle incoming data
  readLoRaResponse();
}

void sendFlagRequest() { // Upon button press, send a request to the server for a flag.
  String message = "HELLOTHERE"; // Or maybe you could try...what the server needs? 
  int messageLength = message.length();
  String atCommand = "AT+SEND=" + String(LORA_ADDRESS_DEST) + "," + String(messageLength) + "," + message;
  sendCommand(atCommand);
  readLoRaResponse();
}


// Function to send a command to the LoRa module
void sendCommand(String command) {
  Serial.print("Command sent: \"");
  Serial.print(command);
  Serial.println("\"");
  loraSerial.println(command);
}

// Function to read responses from the LoRa module
void readLoRaResponse() {
  while (loraSerial.available()) {
    String response = loraSerial.readStringUntil('\n');
    Serial.println("LoRa Module Response: " + response);
    if (response.startsWith("+RCV=")) {
      parseMessage(response);
    }
  }
}

void parseMessage(String response) {
  if (response.startsWith("+RCV=")) {
    String data = response.substring(5);

    // Split the data into parts separated by commas
    int firstComma = data.indexOf(',');
    int secondComma = data.indexOf(',', firstComma + 1);
    int thirdComma = data.indexOf(',', secondComma + 1);

    // Extract sender address
    String senderAddressStr = data.substring(0, firstComma);
    int senderAddress = senderAddressStr.toInt();

    // Extract message length
    String messageLengthStr = data.substring(firstComma + 1, secondComma);
    int messageLength = messageLengthStr.toInt();

    // Extract message
    String message;
    if (thirdComma != -1) {
      message = data.substring(secondComma + 1, thirdComma);
    } else {
      message = data.substring(secondComma + 1);
    }
    message.replace("\r", "");
    message.replace("\n", "");
    message.trim();

    // Print extracted information
    Serial.println("Sender Address: " + String(senderAddress));
    Serial.println("Message Length: " + String(messageLength));
    Serial.println("Message: \"" + message + "\"");

    // Check if the message starts with "SKY_HDWR"
    if (message.startsWith("SKY_HDWR")) {
      // Check if the flag is already in the array
      bool flagExists = false;
      for (int i = 0; i < flagCount; i++) {
        if (flagArray[i] == message) {
          flagExists = true;
          break;
        }
      }

      // If the flag is not in the array and there's space, add it
      if (!flagExists && flagCount < MAX_FLAGS) {
        flagArray[flagCount] = message;
        flagCount++;
        Serial.println("New flag added: " + message);
        // Update the OLED display
        updateDisplay();
      } else if (flagExists) {
        Serial.println("Flag already exists: " + message);
      } else {
        Serial.println("Flag array is full. Cannot add new flag.");
      }
    }

    // Display the received message
    // (Optional) You can comment this out if the display is used only for flags
    // displayMessage("From: " + String(senderAddress) + '\n' + message);
  }
}
// Function to display a message on the OLED
void displayMessage(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}
void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);

  for (int i = 0; i < flagCount; i++) {
    display.println(flagArray[i]);
  }

  display.display();
}
// Function to configure the LoRa module
void configureLoRaModule() {
  // Set module address (e.g., 1)
  sendCommand("AT+ADDRESS=" + String(LORA_ADDRESS_SRC) + "");
  delay(500);
  readLoRaResponse();

  // Set network ID (e.g., 18)
  sendCommand("AT+NETWORKID=" + String(LORA_NETWORKID) + "");
  delay(500);
  readLoRaResponse();

  // Set frequency (e.g., 915 MHz)
  sendCommand("AT+BAND=" + String(LORA_BAND) + "");
  delay(500);
  readLoRaResponse();

  // Set output power (optional)
  sendCommand("AT+CRFOP=15");
  delay(500);
  readLoRaResponse();
}