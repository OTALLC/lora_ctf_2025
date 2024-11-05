#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Maximum number of addresses to store
#define MAX_ADDRESSES 10

int addressQueue[MAX_ADDRESSES];
int queueStart = 0;
int queueEnd = 0;

unsigned long lastBeaconTime = 0;
const unsigned long beaconInterval = 30000; // 30 seconds in milliseconds

// OLED display parameters
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// LoRa module serial communication
HardwareSerial loraSerial(1);

// LoRa variables
#define LORA_NETWORKID 73  //needs to be 73
#define LORA_ADDRESS_SRC 4 //this radio's address 0~65535
#define LORA_ADDRESS_DEST 0 // 0=broadcast, 4=server
#define LORA_BAND 915000000


void setup() {
  // Initialize serial communication with the PC
  Serial.begin(9600);
  while (!Serial); // Wait for Serial Monitor to open
  Serial.println("Serial communication initialized.");


  // Initialize SoftwareSerial communication with the RYLR998 at default baud rate
  loraSerial.begin(115200, SERIAL_8N1, 3, 2); //rx, tx
  Serial.println("SoftwareSerial initialized at 115200 baud.");

  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { // 0x3C is the I2C address
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
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
  delay(500);
  displayMessage("Lets get it.");  //Display flag 1 for correct wiring and setup. 
  delay(2000);
}

void loop() {
  unsigned long currentTime = millis();

  // Check if it's time to send the beacon message
  if (currentTime - lastBeaconTime >= beaconInterval) {
    // Send beacon message
    sendCommand("AT+SEND=0,13,SKY_HDWR_2222");
    readLoRaResponse();
    lastBeaconTime = currentTime;
  }

  // Handle incoming data
  readLoRaResponse();

  // Process address queue
  if (queueStart != queueEnd) {
    int address = addressQueue[queueStart];
    queueStart = (queueStart + 1) % MAX_ADDRESSES;

    String responseMessage = "SKY_HDWR_3333";
    responseMessage.trim();
    int responseLength = responseMessage.length();
    Serial.println("Sending response to address: " + String(address));

    String atCommand = ("AT+SEND=" + String(address) + "," + String(responseLength) + "," + responseMessage);
    sendCommand(atCommand);
    readLoRaResponse();
  }

  // No delay here, so the loop runs continuously
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
  delay(100);
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

    // Test for flag request
    if (message == "GIVEMEFLAG") {
      if (senderAddress > 0) {
        enqueueAddress(senderAddress);
        Serial.println("Enqueued address: " + String(senderAddress));
      } else {
        Serial.println("Invalid sender address. Cannot enqueue.");
      }
    }

    String fullMessage = "From: " + String(senderAddress) + '\n' + message;
    displayMessage(fullMessage);
  }
}

// Function to display a message on the OLED
void displayMessage(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

// Function to configure the LoRa module
void configureLoRaModule() {
  // Set module address (e.g., 1)
  sendCommand("AT+ADDRESS=" + String(LORA_ADDRESS_SRC) +"");
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

  // Set LoRa parameters (Spreading Factor, Bandwidth, Coding Rate, Preamble Length)
  // sendCommand("AT+PARAMETER=10,7,1,4");
  // delay(500);
  // readLoRaResponse();
}

void enqueueAddress(int address) {
  // Check if the address is already in the queue
  for (int i = queueStart; i != queueEnd; i = (i + 1) % MAX_ADDRESSES) {
    if (addressQueue[i] == address) {
      // Address is already in the queue
      return;
    }
  }

  // Add the address to the queue if there's space
  int nextEnd = (queueEnd + 1) % MAX_ADDRESSES;
  if (nextEnd != queueStart) {
    addressQueue[queueEnd] = address;
    queueEnd = nextEnd;
  } else {
    Serial.println("Address queue is full!");
  }
}