# lora_ctf_2025
Hardware CTF Challenge using Arduino Nano ESP32, Lora REYAX RYLR998, and OLED display. 

## Server: 
- Beacons FLAG2 on startup
- Listens for specific message
  - Responds to specific message with FLAG3
  - Must not be a broadcast, has to be addressed to server address
 

## Client: 
- Upon correct hardware wiring, FLAG1 is displayed on screen
- After setting correct NETWORKID, will collect FLAG2 from Server beacon
- If PIN5 is grounded ("pressed"), will send correct message to server for FLAG3

- 
