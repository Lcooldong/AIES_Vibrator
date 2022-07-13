// 컴파일 에러 - wire 말고 i2c 통신 연결하기

#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <LIDARLite_v3HP.h>
#define FAST_I2C
LIDARLite_v3HP myLidarLite;

#include <SoftwareSerial.h>
#define UART2_TX 17
#define UART2_RX 16
SoftwareSerial UART2;

// Global copy of slave
esp_now_peer_info_t slave;
#define CHANNEL 1
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0
#define MAC_ADDR_SIZE 6
#define BUILTIN_LED 2

bool led_state = 0;

String compare_remote = "RemoteESP_";
String SSID;
int send_complete_flag = 0;
int reconnect_count = 0;

uint32_t current_time = 0;
uint32_t past_time = 0;
uint16_t interval = 100;


String success;
uint8_t incomingRGB[3];
int neopixel_Flag = 0;
uint8_t broadcast_mac_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t target_mac_addr[MAC_ADDR_SIZE] ={0,};


#pragma pack(push, 1)
typedef struct packet_
{
    uint8_t STX;  // 0x02    
    uint8_t seq_num;
    uint8_t device_id;
    uint8_t state;      // pairing 상태
    uint8_t magic;     // 고유번호 기능
    uint8_t checksum;
    uint8_t payload;
    uint8_t ETX;  // 0x03
}PACKET;
#pragma pack(pop)

typedef enum {
  oneColor = 1,
  CHASE,
  RAINBOW
}STYLE_Typedef;

//STYLE_Typedef _style;

PACKET serial_data;
PACKET incomingReadings;
PACKET serial_lidar;
PACKET sample_data1 = {0x02, 0, 0x10, 1, 15, 0, 120, 0x03};
PACKET sample_data2 = {0x02, 0, 0x10, 1, 16, 0, 120, 0x03};


uint8_t data = 0;
// send data


// callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if(status == ESP_NOW_SEND_SUCCESS)
  {
    reconnect_count = 0;
  }
  else
  {
    reconnect_count++;
    if(reconnect_count >= 10)
    {
      ESP.restart();
    }
  }
  
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  Serial.print("Last Packet Recv Data: "); Serial.println(*incomingData);
  Serial.println("");
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
//  memcpy(&sample_incomings, incomingData, sizeof(sample_incomings));
  uint8_t start_sign = incomingReadings.STX;
  uint8_t sequence = incomingReadings.seq_num;
  uint8_t target_board_led = incomingReadings.device_id;
  uint8_t _state = incomingReadings.state;
  uint8_t _magic = incomingReadings.magic;
  uint8_t _checksum = incomingReadings.checksum;
  uint8_t _payload = incomingReadings.payload;
  uint8_t end_sign = incomingReadings.ETX;

  Serial.println(start_sign);
  Serial.println(end_sign);
  Serial.println(sequence);
  Serial.println(target_board_led);
  Serial.println(_state);
  Serial.println(_magic);
  Serial.println(incomingReadings.checksum);
  Serial.println(incomingReadings.payload);
  incomingReadings.checksum = incomingReadings.checksum + 1;
  if (incomingReadings.checksum == 3)
  {
    led_state = !led_state;
    digitalWrite(BUILTIN_LED,led_state);
  }
  UART2.write((char*)&incomingReadings, sizeof(incomingReadings));
  delay(1);
  
}


void setup() {
  Serial.begin(115200);
   UART2.begin(115200, SWSERIAL_8N1, UART2_RX, UART2_TX, false);
  if (!UART2) { // If the object did not initialize, then its configuration is invalid
    UART2.println("Invalid SoftwareSerial pin configuration, check config"); 
    while (1) { // Don't continue with invalid configuration
    delay (1000);
    }
  } 
  pinMode(BUILTIN_LED, OUTPUT);
  
  //Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  UART2.println("ESPNow/Basic/Master Example");
  // This is the mac address of the Master in Station Mode
  UART2.print("STA MAC: "); UART2.println(WiFi.macAddress());
  // Init ESPNow with a fallback logic
  InitESPNOW();
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  while(true)
  {
      // In the loop we scan for slave
      ScanForSlave();
      // If Slave is found, it would be populate in `slave` variable
      // We will check if `slave` is defined and then we proceed further
      if (slave.channel == CHANNEL) { // check if slave channel is defined
        // `slave` is defined
        // Add slave as peer if it has not been added already
        if(SSID.startsWith(compare_remote) == 1)
        {
          bool isPaired = manageSlave();
          if (isPaired) {
            // pair success or already paired
            // Send data to device
            send_Data();
          } else {
            // slave pair failed
            UART2.println("Slave pair failed!");
          }
//          send_Data();
          if(send_complete_flag == 1)
          {
            UART2.println("------Remote ESP connected-------");
            break;
          }
    
        }
      }
      else {
        // No slave found to process
      }
    
      // wait for 2seconds to run the logic again
      delay(2000);
  }
  
  UART2.write("\r\nSetup_Done\r\n");

  Wire.begin();
  #ifdef FAST_I2C
      #if ARDUINO >= 157
          Wire.setClock(400000UL); // Set I2C frequency to 400kHz (for Arduino Due)
      #else
          TWBR = ((F_CPU / 400000UL) - 16) / 2; // Set I2C frequency to 400kHz
      #endif
  #endif
  myLidarLite.configure(0);
}

void loop() {
  if(UART2.available())
  {
      // packet 사이즈만큼 읽어옴
      UART2.readBytes((char*)&serial_data, sizeof(serial_data));
      delay(1);
      serial_data.checksum += 1;
      neopixel_Flag = 1;
      Serial.println("-----------------------");  
  }
  

  
  if( neopixel_Flag == 1 ){
    neopixel_Flag = 0;
    broadcast((uint8_t *) &serial_data, sizeof(serial_data));
  }
  uint16_t distance;
  uint8_t  newDistance = 0;
  current_time = millis();
  if(current_time - past_time >= interval)
  {
    past_time = current_time;
    newDistance = distanceFast(&distance);
    Serial.println(distance);

    if(distance < 50 && distance != 0)
    {
      UART2.write((char*)&serial_lidar, sizeof(serial_lidar));

      esp_err_t result = esp_now_send(slave.peer_addr, (uint8_t *)&sample_data1, sizeof(sample_data1));
      if(result == ESP_OK){
        Serial.println("Send Serial OK");
      }else{
        Serial.println("Send Serial Fail");
      } 
    }
  } 
}

void broadcast(const uint8_t * broadcastData, int dataSize)
{
  // this will broadcast a message to everyone in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress))
  {
    esp_now_add_peer(&peerInfo);
  }
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)broadcastData, dataSize);

}

uint8_t distanceFast(uint16_t * distance)
{
    myLidarLite.waitForBusy();
    myLidarLite.takeRange();
    *distance = myLidarLite.readDistance();

    return 1;
}


// Init ESP Now with fallback
void InitESPNOW() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

// Scan for slaves in AP mode
void ScanForSlave() {
  int8_t scanResults = WiFi.scanNetworks();
  // reset on each scan
  bool slaveFound = 0;
  memset(&slave, 0, sizeof(slave));

  UART2.println("");
  if (scanResults == 0) {
    UART2.println("No WiFi devices in AP Mode found");
  } else {
    UART2.print("Found "); UART2.print(scanResults); UART2.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
      
      if (PRINTSCANRESULTS) {
        UART2.print(i + 1);
        UART2.print(": ");
        UART2.print(SSID);
        UART2.print(" (");
        UART2.print(RSSI);
        UART2.print(")");
        UART2.println("");
      }
      delay(10);
      // Check if the current device starts with `Remote`
      if (SSID.indexOf("Remote") == 0) {
        // SSID of interest
        UART2.println("Found a Slave.");
        UART2.print(i + 1); UART2.print(": "); UART2.print(SSID); UART2.print(" ["); UART2.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        // Get BSSID => Mac Address of the Slave
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slave.peer_addr[ii] = (uint8_t) mac[ii];
          }
        }

        slave.channel = CHANNEL; // pick a channel
        slave.encrypt = 0; // no encryption
        slaveFound = 1;
        // we are planning to have only one slave in this example;
        // Hence, break after we find one, to be a bit efficient  
        break;
        
      }
    }
  }

  if (slaveFound) {
    UART2.println("Slave Found, processing..");    
  } else {
    UART2.println("Slave Not Found, trying again.");
  }
  
  

  // clean up ram
  WiFi.scanDelete();
}

// Check if the slave is already paired with the master.
// If not, pair the slave with master
bool manageSlave() {
  if (slave.channel == CHANNEL) {
    if (DELETEBEFOREPAIR) {
      deletePeer();
    }

    Serial.print("Slave Status: ");
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(slave.peer_addr);
    if ( exists) {
      // Slave already paired.
      Serial.println("Already Paired");
      return true;
    } else {
      // Slave not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(&slave);
      if (addStatus == ESP_OK) {
        // Pair success
        Serial.println("Pair success");
        return true;
      } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
        // How did we get so far!!
        Serial.println("ESPNOW Not Init");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
        Serial.println("Invalid Argument");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
        Serial.println("Peer list full");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("Out of memory");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Peer Exists");
        return true;
      } else {
        Serial.println("Not sure what happened");
        return false;
      }
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
    return false;
  }
}

void deletePeer() {
  esp_err_t delStatus = esp_now_del_peer(slave.peer_addr);
  Serial.print("Slave Delete Status: ");
  if (delStatus == ESP_OK) {
    // Delete success
    Serial.println("Success");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW Not Init");
  } else if (delStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}

void send_Data() {
  data++;
  const uint8_t *peer_addr = slave.peer_addr;
  Serial.print("Sending: "); Serial.println(data);
  esp_err_t result = esp_now_send(peer_addr, (uint8_t *)&serial_data, sizeof(serial_data));    // send data
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
    send_complete_flag = 1;
    Serial.println("Success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}
