#include <esp_now.h>
#include <WiFi.h>

#include <SoftwareSerial.h>
#define UART2_TX 17
#define UART2_RX 16
SoftwareSerial uart2_port;

// Global copy of slave
esp_now_peer_info_t slave;
#define CHANNEL 1
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0
#define MAC_ADDR_SIZE 6


String compare_remote = "RemoteESP_";
//String compare_remote = "Slave_";
String SSID;
int send_complete_flag = 0;
int reconnect_count = 0;

uint32_t current_time = 0;
uint32_t past_time = 0;
uint16_t interval = 1000;


String success;
uint8_t incomingRGB[3];
int neopixel_Flag = 0;
uint8_t broadcast_mac_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//uint8_t target_mac_addr[] = {0x60, 0x55, 0xf9, 0x57, 0x48, 0xe9};
uint8_t target_mac_addr[MAC_ADDR_SIZE] ={0,};


#pragma pack(push, 1)
typedef struct packet_
{
    uint8_t STX;  // 0x02    
    uint32_t seq_num;
    uint8_t device_led;
    uint8_t state;      // pairing 상태
    uint16_t magic;     // 고유번호 기능
    uint8_t RGB[3]; 
    uint8_t brightness;
    uint8_t style;            
    uint8_t wait;
    uint8_t checksum;
    uint8_t payload;
    uint8_t ETX;  // 0x03
}PACKET;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct test_
{
    uint8_t STX;  // 0x02    
    uint8_t ETX;  // 0x03
}TEST_PACKET;
#pragma pack(pop)

TEST_PACKET sample_test;
TEST_PACKET sample_incomings;

typedef enum {
  oneColor = 1,
  CHASE,
  RAINBOW
}STYLE_Typedef;

//STYLE_Typedef _style;

PACKET serial_data = {0, };
PACKET incomingReadings;
PACKET sample_data1 = {0x02, 0, 0x10, 1, 15, {255, 40, 100}, 50, 1, 20, 0, 120,0x03};
PACKET sample_data2 = {0x02, 0, 0x10, 1, 16, {100, 255, 40}, 50, 1, 20, 0, 120,0x03};





// Init ESP Now with fallback
void InitESPNow() {
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

  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
      
      if (PRINTSCANRESULTS) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
      }
      delay(10);
      // Check if the current device starts with `Remote`
      if (SSID.indexOf("Remote") == 0) {
        // SSID of interest
        Serial.println("Found a Slave.");
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
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
    Serial.println("Slave Found, processing..");    
  } else {
    Serial.println("Slave Not Found, trying again.");
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

uint8_t data = 0;
// send data
void sendData() {
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
  uint32_t sequence = incomingReadings.seq_num;
  uint8_t target_board_led = incomingReadings.device_led;
  uint8_t _state = incomingReadings.state;
  uint16_t _magic = incomingReadings.magic;
  uint8_t R = incomingReadings.RGB[0];
  uint8_t G = incomingReadings.RGB[1];
  uint8_t B = incomingReadings.RGB[2];
  uint8_t _brightness = incomingReadings.brightness;
  uint8_t _style = incomingReadings.style;
  uint8_t waitORtimes = incomingReadings.wait;
  uint8_t _checksum = incomingReadings.checksum;
  uint8_t _payload = incomingReadings.payload;
  uint8_t end_sign = incomingReadings.ETX;

  Serial.println(start_sign);
  Serial.println(end_sign);
  
  Serial.println(sequence);
  Serial.println(target_board_led);
  Serial.println(_state);
  Serial.println(_magic);
  Serial.println(incomingReadings.RGB[0]);
  Serial.println(incomingReadings.RGB[1]);
  Serial.println(incomingReadings.RGB[2]);
  Serial.println(incomingReadings.brightness);
  Serial.println(incomingReadings.style);
  Serial.println(incomingReadings.wait);
  Serial.println(incomingReadings.checksum);
  Serial.println(incomingReadings.payload);
  
}


void setup() {
  Serial.begin(115200);
   uart2_port.begin(115200, SWSERIAL_8N1, UART2_RX, UART2_TX, false);
  if (!uart2_port) { // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid SoftwareSerial pin configuration, check config"); 
    while (1) { // Don't continue with invalid configuration
    delay (1000);
    }
  } 

  //Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  Serial.println("ESPNow/Basic/Master Example");
  // This is the mac address of the Master in Station Mode
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();
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
            sendData();
          } else {
            // slave pair failed
            Serial.println("Slave pair failed!");
          }
          
//          sendData();
          if(send_complete_flag == 1)
          {
            Serial.println("------Remote ESP connected-------");
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
  
  Serial.write("\r\nSetup_Done");
  
}

void loop() {
  if(Serial.available())
  {
      // packet 사이즈만큼 읽어옴
//      Serial.readBytes((char*)&serial_data, sizeof(serial_data));
      serial_data.checksum += 1;
//      neopixel_Flag = 1;
      Serial.println("-----------------------");      
      //Serial.write((char*)&serial_data, sizeof(serial_data));
      delay(1);

      char pressed = Serial.read();
      
      if(pressed == 'a'){
        Serial.println("a pressed");
//        sample_test.STX = 0x02;
//        sample_test.ETX = 0x03;
//        Serial.println(sample_test.STX);
//        Serial.println(sample_test.ETX);
        esp_err_t result = esp_now_send(slave.peer_addr, (uint8_t *)&sample_data2, sizeof(sample_data2));
        if(result == ESP_OK){
          Serial.println("Send Serial OK");
        }else{
          Serial.println("Send Serial Fail");
        }
        delay(10);
        
      }
  }
  
  current_time = millis();
  if(current_time - past_time >= interval)
  {
    past_time = current_time;
//    sample_test.STX = 0x02;
//    sample_test.ETX = 0x03;
//    Serial.println(sample_test.STX);
//    Serial.println(sample_test.ETX);
    esp_err_t result = esp_now_send(slave.peer_addr, (uint8_t *)&sample_data1, sizeof(sample_data1));
    if(result == ESP_OK){
      Serial.println("Send Serial OK");
    }else{
      Serial.println("Send Serial Fail");
    }    
  }
  


  if( neopixel_Flag == 1 ){
    neopixel_Flag = 0;
    broadcast((uint8_t *) &serial_data, sizeof(serial_data));
//    sendData();
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
