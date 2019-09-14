#include "ESP8266WiFi.h"
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>

#include "OTA.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 2
#define VERSION_PATCH 0

#define NODE_TYPE_GATE 1

#define SENSOR_RSSI 0x80

// WiFi access point connection configuration
const char* ssid = "CarInSitu";
const char* password = "Roulez jeunesse !";

// Network
byte mac[6];

WiFiUDP Udp;
unsigned int localUdpPort = 4210;
char incomingPacket[256];
char outgoingPacket[256];
IPAddress cisServerIpAddress;
WiFiClient cisClient;

// IR
IRsend irsend(4);

// OTA
OTA ota;

void print_wifi_status(int status) {
  switch (status) {
  case WL_CONNECTED:
    Serial.print("Connected to a WiFi network");
    break;
  case WL_NO_SHIELD:
    Serial.print("No WiFi shield is present");
    break;
  case WL_IDLE_STATUS:
    Serial.print("Idle");
    break;
  case WL_NO_SSID_AVAIL:
    Serial.print("No SSID are available");
    break;
  case WL_SCAN_COMPLETED:
    Serial.print("Scan networks is completed");
    break;
  case WL_CONNECT_FAILED:
    Serial.print("Connection fails for all the attempts");
    break;
  case WL_CONNECTION_LOST:
    Serial.print("Connection is lost");
    break;
  case WL_DISCONNECTED:
    Serial.print("Disconnected");
    break;
  }
}

void setup() {
  // Init serial monitoring
  Serial.begin(76800);

  // Shutdown bultin LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(10);

  Serial.printf("\nGateNode version: %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

  // Init WiFi
  WiFi.disconnect();
  delay(10);

  WiFi.macAddress(mac);
  // Start WiFi server and wait for connection
  String hostname = String("GateNode-") + String(mac[5], HEX) + String(mac[4], HEX) + String(mac[3], HEX) + String(mac[2], HEX) + String(mac[1], HEX) + String(mac[0], HEX);
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  Serial.printf("WiFi: Hostname: %s\n", WiFi.hostname().c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.printf("WiFi: Connecting... (status: %d): ", WiFi.status());
    print_wifi_status(WiFi.status());
    Serial.print("\n");
  }
  // Poweron bultin LED
  digitalWrite(LED_BUILTIN, LOW);

  Serial.printf("WiFi: Connected to SSID: \"%s\" with IP: %s\n", ssid, WiFi.localIP().toString().c_str());

  // MDNS
  MDNS.begin(hostname);

  Udp.begin(localUdpPort);
  Serial.printf("UDP: Listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

  // IR
  irsend.begin();

  // OTA
  ota.begin();
}

// Prototypes
void processTcp();
void processUdp();
void sendSensorsData();
void processIncomingPackets(const int len);
void sendUdpPacket(const int len);

void loop() {
  if (cisClient.connected()) {
    processTcp();
    processUdp();
    sendSensorsData();
  } else {
    // FIXME: searchCisServer();
    irsend.sendSony(0xa90, 12, 2); // 12 bits & 2 repeats
    ota.process();
  }

  // Give some CPU to process internal things...
  // UDP is instable without this...
  yield();
}

void searchCisServer() {
  int n = MDNS.queryService("cisserver", "tcp");
  if (n) {
    // Connect to the first available CisServer
    cisServerIpAddress = MDNS.IP(0);
    Serial.printf("MDNS: CIS server discovered: %s\n", cisServerIpAddress.toString().c_str());
    if (!cisClient.connect(cisServerIpAddress, MDNS.port(0))) {
      Serial.println("TCP: Connection failed!");
      delay(1000);
    } else {
      Serial.println("TCP: Connection established.\n");
    }
  } else {
    Serial.printf("MDNS: Waiting for CIS server...\n");
    delay(1000);
  }
}

#define PING 0x00
#define VERSION 0x01
#define INVALID_COMMAND 0xff

byte incomingTcpFrame[64];
byte* incomingTcpFramePtr = incomingTcpFrame;

void processTcp() {
  static int remainingBytesForCommand = -1;
  static byte command;

  while (int availableBytes = cisClient.available()) {
    if (remainingBytesForCommand == -1) { // We are waiting for a command
      command = cisClient.read();
      incomingTcpFramePtr = incomingTcpFrame;
      switch (command) {
      case PING: {
        cisClient.write(PING);
        break;
      }
      case VERSION: {
        outgoingPacket[0] = VERSION;        // repeat command code
        outgoingPacket[1] = NODE_TYPE_GATE; // says im a gate node
        outgoingPacket[2] = VERSION_MAJOR;  // says my firmware version
        outgoingPacket[3] = VERSION_MINOR;
        outgoingPacket[4] = VERSION_PATCH;
        Serial.printf("Replied to VERSION request\n");
        cisClient.write(outgoingPacket, 5);
        break;
      }
      default: {
        // Unknown command
        Serial.printf("TCP: Unknown command: 0x%02x (%d available bytes)\n", command, availableBytes);
        remainingBytesForCommand = availableBytes - 1;
        command = INVALID_COMMAND;
      }
      }
    } else {
      *incomingTcpFramePtr = cisClient.read();
      remainingBytesForCommand--;

      if (command == INVALID_COMMAND) {
        // Trash any byte
        incomingTcpFramePtr = incomingTcpFrame;
      } else {
        ++incomingTcpFramePtr;
      }

      if (remainingBytesForCommand == 0) {
        switch (command) {
        default: {
          // Nothing to do with invalid command
        }
        }
        remainingBytesForCommand = -1;
      }
    }
  }
}

void processUdp() {
  // UDP: receive incoming packets
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    // Serial.printf("UDP: Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    int len = Udp.read(incomingPacket, 255);
    // Serial.printf("UDP packet contents: %s\n", incomingPacket);
    processIncomingPackets(len);
  }
}

void sendUdpSensorData(const uint8_t type, const void* data, const int lenght) {
  outgoingPacket[0] = type;
  memcpy(outgoingPacket + 1, data, lenght + 1);
  sendUdpPacket(lenght + 1);
}

void processIncomingPackets(const int len) {
  int16_t* int_value = (int16_t*)&incomingPacket[1];
  int value;
  switch (incomingPacket[0]) {
  default:
    Serial.printf("UDP Unknown command: 0x%02x\n", incomingPacket[0]);
  }
}

void sendUdpPacket(const int len) {
  Udp.beginPacket(cisServerIpAddress, 4200);
  Udp.write(outgoingPacket, len);
  Udp.endPacket();
  // Give some CPU to process internal things...
  yield();
}

void sendSensorsData() {
  if (!cisServerIpAddress)
    return;

  // Only send sensors data each 100 runs
  static int prescaler = 0;
  prescaler++;
  prescaler %= 100;
  if (prescaler)
    return;

  // RSSI
  int32_t rssi = WiFi.RSSI();
  sendUdpSensorData(SENSOR_RSSI, &rssi, sizeof(rssi));
}
