// board : https://github.com/01Space/ESP32-C3-0.42LCD
//#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <U8g2lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define INFO_SCREEN_DELAY 3000
#define SDA_PIN 5
#define SCL_PIN 6
#define LED_PIN 2
#define LED_COUNT 60
#define CHANNEL 1
#define BTN_PIN 3
#define MAC_ADDR_SIZE 6

// Debug용 LED
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);   // EastRising 0.42" OLED

esp_now_peer_info_t master;   // peer 등록하기 꼭
///////////////////////////////////////////////////////////////
String success;
int device_id = 1;    // check
const char *SSID = "RemoteESP_1";
////////////////////////////////////////////////////////////////
uint8_t target_mac_addr[MAC_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#pragma pack(push, 1)
typedef struct packet_
{
    uint8_t STX;
    uint8_t seq_num;
    uint8_t device_id;
    uint8_t state;      // pairing 상태
    uint8_t magic;     // 고유번호 기능
    uint8_t checksum;
    uint8_t payload;
    uint8_t ETX;
}PACKET;
#pragma pack(pop)


typedef enum {
  NORMAL = 1,
  START,
  WARNING,
  END
}STYLE_Typedef;

PACKET serial_data = {0, };
PACKET incomingReadings;

//esp_now_peer_info_t peerInfo;
int btn_count = 0;
int reconnect_count = 0;
bool btn_flag = false;
bool pair_flag = false;


// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// config AP SSID
void configDeviceAP() {
  bool result = WiFi.softAP(SSID, "Slave_1_Password", CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESPNow/Basic/Slave Example");
  pinMode(BTN_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  init_Neopixel(50);
  

  while(btn_flag == false)
  {
    if(digitalRead(BTN_PIN) == 0)
    {
      btn_count++;
      Serial.println(btn_count);
      delay(5);
    }else
    {
      btn_count = 0;
    }
    delay(20);
    if(btn_count >= 100)
    {
      btn_flag = true;
      break;
    }
  }
    

  if(btn_flag == true)
  {
      pickOneLED(0, strip.Color(255, 10, 140), 50, 10);
      //Set device in AP mode to begin with
      WiFi.mode(WIFI_AP_STA);
      // configure device AP mode
      configDeviceAP();
      // This is the mac address of the Slave in AP Mode
      Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
      // Init ESPNow with a fallback logic
      InitESPNow();
      // Once ESPNow is successfully Init, we will register for recv CB to
      // get recv packer info.
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);

  }
}

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

// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  Serial.print("Last Packet Recv Data: "); Serial.println(*data);
  Serial.println("");
  
  memcpy(&incomingReadings, data, sizeof(incomingReadings));  

//  for(int i=0; i< MAC_ADDR_SIZE; i++){
//    target_mac_addr[i] = mac_addr[i];
//  }
  uint8_t start_sign = incomingReadings.STX;
  uint32_t sequence = incomingReadings.seq_num;
  uint8_t target_board_led = incomingReadings.device_id;
  uint8_t _state = incomingReadings.state;
  uint16_t _magic = incomingReadings.magic;
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

  
  
  if(start_sign == 2 && end_sign == 3 && pair_flag == false)
  {
      memset(&master, 0, sizeof(master));
      master.channel = CHANNEL;
      master.encrypt = 0;
      
      for (int ii = 0; ii < 6; ++ii ) {
        master.peer_addr[ii] = (uint8_t) mac_addr[ii];
        Serial.print(master.peer_addr[ii], HEX);
        if(ii < 5) Serial.print(":");
      }
      Serial.println("");
      
      esp_err_t addStatus = esp_now_add_peer(&master);
       if (addStatus == ESP_OK) {
        // Pair success
        Serial.println("Pair success");
        pair_flag = true;
        

      } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
        // How did we get so far!!
        Serial.println("ESPNOW Not Init");

      } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
        Serial.println("Invalid Argument");

      } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
        Serial.println("Peer list full");

      } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("Out of memory");

      } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Peer Exists");

      } else {
        Serial.println("Not sure what happened");

      }
  }
  
  incomingReadings.checksum = incomingReadings.checksum + 1;
    // target_board_led >> 4
  if((device_id == (target_board_led / 16)) && start_sign == 2 && end_sign == 3 && incomingReadings.checksum == 2)
  {
    const uint8_t *peer_addr = master.peer_addr;
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *)&incomingReadings, sizeof(incomingReadings));
    if(result == ESP_OK){
      Serial.println("Return Success");
    }else{
      Serial.println("Return Fail");
    }
    
    switch(incomingReadings.state)
    {
      case NORMAL:
        pickOneLED(0, strip.Color(0, 255, 0), 50, 10);
        break;
        
      case START:
        pickOneLED(0, strip.Color(0, 0, 255), 50, 10);
        break;
        
      case WARNING:
        pickOneLED(0, strip.Color(255, 0, 0), 50, 10);
        break;
        
      case END:
        resetNeopixel();
        break;

      default:
        resetNeopixel();
        break;
    }
  } 

  
}

void loop() {
  // Chill
}



/////////////////////////////////////////////////////////////////////////////

void init_Neopixel(uint8_t brightness){
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(brightness); // Set BRIGHTNESS to about 1/5 (max = 255)
}

void pickOneLED(uint8_t ledNum, uint32_t color, uint8_t brightness, int wait){
    strip.setBrightness(brightness);
    strip.setPixelColor(ledNum, color);  
    strip.show();                                               
    delay(wait);
}

void blinkNeopixel(uint32_t color, int times, int delays){
  for(int i = 0; i < times; i++){
    pickOneLED(0, color, 50, delays);
    pickOneLED(0, strip.Color(0, 0, 0), 0, delays);
  }
}

void resetNeopixel(){
  for(int i=0; i < 256; i++){
    pickOneLED(i, strip.Color(0, 0, 0), 0, 0 );
  } 
}


// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
