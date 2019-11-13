#include <string>
#include <EEPROM.h>
#include <PxMatrix.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define LENGTH(x) (strlen(x) + 1)
#define PANELS_COUNT 1
#define MATRIX_WIDTH 40*PANELS_COUNT
#define MATRIX_HEIGHT 20
#define EEPROM_START 0
#define SERIAL_BAUD_RATE 115200
#define SERVICE_UUID "085ac895-8339-4478-a19a-2160e37e5ffa"
#define CHARACTERISTIC_UUID "8173ba12-78f1-4a17-bd6e-ca2869d89372"

#define ROTATE_DEFAULT false
#define FLIP_DEFAULT false
#define SCROLLING_SPEED_DEFAULT "40"
#define BRIGHTNESS_DEFAULT "200"
#define FONT_COLOR_DEFAULT "255,0,255"
#define FONT_SIZE_DEFAULT "2"
#define TEXT_DEFAULT "PENDING CONNECTION..."

const String FIRMWARE_VERSION = "0.2";
const String HARDWARE = "DOIT ESP32 DEVKIT V1";
const String LED_TYPE = "P8 40X20 RGB";
const String LED_SERIAL = "P8-4020-2727-5S";
const String LED_INFO = "40X20,ABC,1/5,ZAGGIZ,BINARY";
const String BLUETOOTH_SSID = "LED Matrix 0.2";

static prog_uint32_t crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

struct eeprom_data_t {
  bool rotate;
  bool flip;
  char brightness[8];
  char scrollingSpeed[8];
  char text[512];
  char fontColor[16];
  char fontSize[4];
} eeprom_data;

uint8_t DISPLAY_DRAW_TIME = 50;
uint32_t memcrc; 
uint8_t *p_memcrc = (uint8_t*)&memcrc;
int xpos = 0;
int ypos = 3;
uint8_t r_color = 255;
uint8_t g_color = 0;
uint8_t b_color = 255;
int incoming;
String message_text = "";
String backup_message_text = "";
String message_for_client = "";
bool is_isset_message_for_client;
bool setEEPROM = false;
bool is_isset_message = true;
bool pre_control_mode = false;
bool control_mode = false;
bool is_setting_change = false;
bool deviceConnected = false;
bool oldDeviceConnected = false;

PxMATRIX display(MATRIX_WIDTH, MATRIX_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C);
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;


#ifdef ESP32
void IRAM_ATTR display_updater()
{
    portENTER_CRITICAL_ISR(&timerMux);
    display.display(DISPLAY_DRAW_TIME);
    portEXIT_CRITICAL_ISR(&timerMux);
}
#endif

void display_update_enable(bool is_enable)
{
  #ifdef ESP32
    if (is_enable)
    {
      timer = timerBegin(0, 80, true);
      timerAttachInterrupt(timer, &display_updater, true);
      timerAlarmWrite(timer, 2000, true);
      timerAlarmEnable(timer);
    }
    else
    {
      timerDetachInterrupt(timer);
      timerAlarmDisable(timer);
    }
  #endif
}

class MyServerCallbacks: public BLEServerCallbacks 
{
    void onConnect(BLEServer* pServer) 
    {
        deviceConnected = true;
        Serial.println("Device connected!");
        BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) 
    {
        Serial.println("Device disconnected!");
        deviceConnected = false;
    }
};

class MyRuntimeCallbacks: public BLECharacteristicCallbacks 
{
    void onWrite(BLECharacteristic *pCharacteristic) 
    {
        std::string value = pCharacteristic->getValue();
        Serial.println("Received new message!");
        if (value.length() > 0) 
        {
            backup_message_text = "";
            for (int i = 0; i < value.length(); i++)
            {
                backup_message_text = backup_message_text + value[i];
            }
            backup_message_text = backup_message_text.substring(1, backup_message_text.length());
            String command = backup_message_text.substring(1, backup_message_text.indexOf("="));
            String value = backup_message_text.substring(backup_message_text.indexOf("=") + 1, backup_message_text.length());
            Serial.println("Executing controll command '" + command + "' with value '" + value + "'");
            execute_control_command(command, value);
            backup_message_text = "";
        }
    }
};

void sendBleMessage(String message)
{
    message_for_client = message;
    is_isset_message_for_client = true;
}

void erase_eeprom()
{
    Serial.println("Erasing EEPROM memory...");
    display_update_enable(false);
    delay(25);
    EEPROM.begin(sizeof(eeprom_data) + sizeof(memcrc));
    int EEPROM_start = EEPROM_START;
    for (int i = EEPROM_start; i < EEPROM_start + sizeof(eeprom_data) + sizeof(memcrc); i++) 
    {
      EEPROM.write(i, 0);
      delay(4);
    }
    
    EEPROM.commit();
    delay(100);
    display_update_enable(true);
}

void write_settings_to_eeprom()
{
    Serial.println("Overriding EEPROM simulation FLASH...");
    display_update_enable(false);
    delay(25);
    int i;
    byte eeprom_data_tmp[sizeof(eeprom_data)];
  
    EEPROM.begin(sizeof(eeprom_data) + sizeof(memcrc));
    memcpy(eeprom_data_tmp, &eeprom_data, sizeof(eeprom_data));
    for (i = EEPROM_START; i < EEPROM_START + sizeof(eeprom_data); i++)
    {
      EEPROM.write(i, eeprom_data_tmp[i]);
      delay(2);
    }
    
    memcrc = crc_byte(eeprom_data_tmp, sizeof(eeprom_data_tmp));
  
    EEPROM.write(i++, p_memcrc[0]);delay(4);
    EEPROM.write(i++, p_memcrc[1]);delay(4);
    EEPROM.write(i++, p_memcrc[2]);delay(4);
    EEPROM.write(i++, p_memcrc[3]);delay(4);
  
    EEPROM.commit();
    delay(100);
    is_setting_change = true;
    display_update_enable(true);
}

void read_settings_from_eeprom()
{
    int i;
    uint32_t datacrc;
    byte eeprom_data_tmp[sizeof(eeprom_data)];
  
    EEPROM.begin(sizeof(eeprom_data) + sizeof(memcrc));
  
    for (i = EEPROM_START; i < EEPROM_START + sizeof(eeprom_data); i++)
    {
      eeprom_data_tmp[i] = EEPROM.read(i);
      delay(4);
    }
  
    p_memcrc[0] = EEPROM.read(i++);delay(4);
    p_memcrc[1] = EEPROM.read(i++);delay(4);
    p_memcrc[2] = EEPROM.read(i++);delay(4);
    p_memcrc[3] = EEPROM.read(i++);delay(4);
  
    datacrc = crc_byte(eeprom_data_tmp, sizeof(eeprom_data_tmp));
  
    if (memcrc == datacrc)
    {
        setEEPROM = true;
        memcpy(&eeprom_data, eeprom_data_tmp,  sizeof(eeprom_data));
    }
    else{
        eeprom_data.rotate = ROTATE_DEFAULT;
        eeprom_data.flip = FLIP_DEFAULT;
        strncpy(eeprom_data.text, TEXT_DEFAULT, sizeof(TEXT_DEFAULT));
        strncpy(eeprom_data.scrollingSpeed, SCROLLING_SPEED_DEFAULT, sizeof(SCROLLING_SPEED_DEFAULT));
        strncpy(eeprom_data.brightness, BRIGHTNESS_DEFAULT, sizeof(BRIGHTNESS_DEFAULT));
        strncpy(eeprom_data.fontColor, FONT_COLOR_DEFAULT, sizeof(FONT_COLOR_DEFAULT));
        strncpy(eeprom_data.fontSize, FONT_SIZE_DEFAULT, sizeof(FONT_SIZE_DEFAULT));
    }
}

unsigned long crc_update(unsigned long crc, byte data)
{
    byte tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    
    return crc;
}

unsigned long crc_byte(byte *b, int len)
{
    unsigned long crc = ~0L;
    uint16_t i;
  
    for (i = 0 ; i < len ; i++)
    {
      crc = crc_update(crc, *b++);
    }
    crc = ~crc;
    
    return crc;
}

void scroll_text(uint8_t ylpos, unsigned long scroll_delay, String text)
{
    uint16_t text_length = text.length();
    display.setTextWrap(false);  // we don't wrap text so it scrolls nicely
    display.setRotation(0);
    
    for (int xpos = MATRIX_WIDTH; xpos > -(MATRIX_WIDTH + text_length*7); xpos--)
    {
        if(is_setting_change)
        {
            is_setting_change = false; 
            return;
        }
        display.setTextColor(display.color565(r_color,g_color,b_color));
        display.clearDisplay();
        display.setCursor(xpos,ylpos);
        display.println(utf8rus(text));
        delay(scroll_delay);
        yield();

        display.setTextColor(display.color565(r_color/4,g_color/4,b_color/4));  
        display.setCursor(xpos-1,ylpos);
        display.println(utf8rus(text));
        delay(scroll_delay/5);
        yield();
    }
}

String utf8rus(String source)
{
  int i,k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length(); i = 0;

  while (i < k) {
    n = source[i]; i++;

    if (n >= 0xBF){
      switch (n) {
        case 0xD0: {
          n = source[i]; i++;
          if (n == 0x81) { n = 0xA8; break; }
          if (n >= 0x90 && n <= 0xBF) n = n + 0x2F;
          break;
        }
        case 0xD1: {
          n = source[i]; i++;
          if (n == 0x91) { n = 0xB7; break; }
          if (n >= 0x80 && n <= 0x8F) n = n + 0x6F;
          break;
        }
      }
    }
    m[0] = n; target = target + String(m);
  }
  return target;
}

bool execute_control_command(String command, String value)
{
    if(command.equals("set-text"))
    {
        message_text = value;
        if(!message_text.equals(eeprom_data.text))
        {
            strcpy(eeprom_data.text, value.c_str());
            write_settings_to_eeprom();
        }
        return false;
    }
    if(command.equals("get-text"))
    {
        String str_text((char*)eeprom_data.text);
        Serial.println("Transferring current text: '" + str_text + "' to client");
        sendBleMessage(str_text);
        return true;
    }
    
    if(command.equals("set-size"))
    {
        if(value.toInt() > 0 && value.toInt() < 5)
        {
            strcpy(eeprom_data.fontSize, value.c_str());
            write_settings_to_eeprom();
            display.setTextSize(value.toInt());
        }else{
            Serial.println("Invalid value! Specify a number from the range [1,4]");
        }
        return true;
    }
    if(command.equals("get-size"))
    {
        String str_fontSize((char*)eeprom_data.fontSize);
        Serial.println("Transferring current font size: '" + str_fontSize + "' to client");
        sendBleMessage(str_fontSize);
        return true;
    }
    
    if(command.equals("set-color"))
    {
        uint8_t r = value.substring(0, value.indexOf(",")).toInt();
        r_color = r;
        String tmp_value = value.substring(value.indexOf(",") + 1, value.length());
        uint8_t g = tmp_value.substring(0, tmp_value.indexOf(",")).toInt();
        g_color = g;
        uint8_t b = tmp_value.substring(tmp_value.indexOf(",") + 1, tmp_value.length()).toInt();
        b_color = b;
        if( (r >=0 && r <= 255) && (g >=0 && g <= 255) && (b >=0 && b <= 255) )
        {
            strcpy(eeprom_data.fontColor, value.c_str());
            write_settings_to_eeprom();
            display.setTextColor(display.color565(r, g, b));
        }else{
            Serial.println("Invalid color value! Specify a RGB colors in format r,g,b. All values must be in the range [0,255]");
        }
        return true;
    }
    if(command.equals("get-color"))
    {
        String str_fontColor((char*)eeprom_data.fontColor);
        Serial.println("Transferring current font color: '" + str_fontColor + "' to client");
        //ESP_BT.println(str_fontColor);
        sendBleMessage(str_fontColor);
        return true;
    }
    
    if(command.equals("set-brightness"))
    {
        if(value.toInt() >= 0 && value.toInt() <= 255)
        {
            display.setBrightness(value.toInt());
            strcpy(eeprom_data.brightness, value.c_str());
            write_settings_to_eeprom();
        }else{
            Serial.println("Invalid value! Specify a number from the range [0,255]");
        }
        return true;
    }
    if(command.equals("get-brightness"))
    {
        String str_brightness((char*)eeprom_data.brightness);
        Serial.println("Transferring current brightness: '" + str_brightness + "' to client");
        //ESP_BT.println(str_brightness);
        sendBleMessage(str_brightness);
        return true;
    }
    
    if(command.equals("set-ypos"))
    {
        if(value.toInt() >= -100 && value.toInt() <= 100)
        {
            ypos = value.toInt();
        }else{
            Serial.println("Invalid value! Specify a number from the range [0,16]");
        }
        return true;
    }
    
    if(command.equals("set-speed"))
    {
        if(value.toInt() >= 5 && value.toInt() <= 1000)
        {
            strcpy(eeprom_data.scrollingSpeed, value.c_str());
            write_settings_to_eeprom();
        }else{
            Serial.println("Invalid speed value! Specify a number from the range [5,1000]");
        }
        return true;
    }
    if(command.equals("get-speed"))
    {
        String str_speed((char*)eeprom_data.scrollingSpeed);
        Serial.println("Transferring current scrolling speed: '" + str_speed + "' to client");
        //ESP_BT.println(str_speed);
        sendBleMessage(str_speed);
        return true;
    }
    
    if(command.equals("set-flip"))
    {
        if(value.equals("false") && value.equals("true"))
        {
            Serial.println("Invalid value! Specify a true of false in string type");
            return true;
        } if(value.equals("true"))
        {
            eeprom_data.flip = true;
            display.setFlip(true);
        } if(value.equals("false"))
        {
            eeprom_data.flip = false;
            display.setFlip(false);
        }
        write_settings_to_eeprom();
        return true;
    }
    if(command.equals("get-flip"))
    {
        bool bool_flip((char*)eeprom_data.flip);
        String flip_text = "";
        if(bool_flip)
        {
            sendBleMessage("true");
            //ESP_BT.println(true);
            flip_text = "true";
        }else{
            sendBleMessage("false");
            //ESP_BT.println(false);
            flip_text = "false";
        }
        Serial.println("Transferring current flip flag: '" + flip_text + "' to client");
        return true;
    }
    
    if(command.equals("set-rotate"))
    {
        if(value.equals("false") && value.equals("true"))
        {
            Serial.println("Invalid value! Specify a true of false in string type");
            return true;
        } if(value.equals("true"))
        {
            eeprom_data.rotate = true;
            display.setRotate(true);
        } if(value.equals("false"))
        {
            eeprom_data.rotate = false;
            display.setRotate(false);
        }
        write_settings_to_eeprom();
        return true;
    }
    if(command.equals("get-rotate"))
    {
        bool bool_rotate((char*)eeprom_data.rotate);
        String rotate_text = "";
        if(bool_rotate)
        {
            sendBleMessage("true");
            //ESP_BT.println(true);
            rotate_text = "true";
        }else{
            sendBleMessage("false");
            //ESP_BT.println(false);
            rotate_text = "false";
        }
        Serial.println("Transferring current rotate flag: '" + rotate_text + "' to client");
        return true;
    }

    if(command.equals("get-info"))
    {
        String info = "FIRMWARE_VERSION=" + FIRMWARE_VERSION + ";\n";
        info += "HARDWARE=" + HARDWARE + ";\n";
        info += "LED_TYPE=" + LED_TYPE + ";\n";
        info += "LED_SERIAL=" + LED_SERIAL + ";\n";
        info += "LED_INFO=" + LED_INFO + ";";
        sendBleMessage(info);
        Serial.println("Transferring device information to client");
        return true;
    }

    if(command.equals("get-info-full"))
    {
        String str_text((char*)eeprom_data.text);
        String str_speed((char*)eeprom_data.scrollingSpeed);
        String str_brightness((char*)eeprom_data.brightness);
        String str_font_color((char*)eeprom_data.fontColor);
        String str_font_size((char*)eeprom_data.fontSize);
        bool bool_flip((char*)eeprom_data.flip);
        
        String info = "#_get-text=" + str_text + ";\n";
        info += "#_get-speed=" + str_speed + ";\n";
        info += "#_get-brightness=" + str_brightness + ";\n";
        info += "#_get-color=" + str_font_color + ";\n";
        info += "#_get-size=" + str_font_size + ";\n";

        String flip = "";
        if(bool_flip)
        {
          flip = "true";
        } else {
          flip = "false";
        }
        info += "#_get-flip=" + flip + ";";
        
        sendBleMessage(info);
        Serial.println("Transferring device full information to client");

        is_setting_change = true;
        return true;
    }
    
    if(command.equals("reboot"))
    {
        ESP.restart();
        if(value.equals("1"))
        {
            
        }
        return true;
    } 
    if(command.equals("reset"))
    {
        erase_eeprom();
        ESP.restart();
        if(value.equals("1"))
        {
            
        }
        return true;
    } 
    /*if(command.equals("erase"))
    {
        erase_eeprom();
        ESP.restart();
        if(value.equals("1"))
        {
            
        }
        return true;
    } */
}

void setupSerial(bool is_enable)
{
  if (is_enable)
  {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println("Serial enabled! Baud rate = " + SERIAL_BAUD_RATE);
  }
}

void setupEeprom(bool is_enable)
{
  if (is_enable)
  {
    read_settings_from_eeprom();

    String str_text((char*)eeprom_data.text);
    String str_speed((char*)eeprom_data.scrollingSpeed);
    String str_brightness((char*)eeprom_data.brightness);
    String str_font_color((char*)eeprom_data.fontColor);
    String str_font_size((char*)eeprom_data.fontSize);
    message_text = str_text;

    Serial.print("SDK version: "); Serial.println(ESP.getSdkVersion());
    Serial.print("Firmware compiled for flash: "); Serial.println(ESP.getFlashChipSize());
    Serial.println("EEPROM initialized. Allowed = 1024B");
    Serial.println("Reading EEPROM...");
    Serial.println("Text: " + str_text);
    Serial.println("Scrolling speed: " + str_speed);
    Serial.println("Brightness: " + str_brightness);
    Serial.println("Font color: " + str_font_color);
    Serial.println("Font size: " + str_font_size);
    Serial.println("Rotated: " + eeprom_data.rotate);
    Serial.println("Flipped: " + eeprom_data.flip);
  }
}

void setupBluetooth(bool is_enable)
{
    if (is_enable)
    {
        Serial.println("Enabling BT module...");
        BLEDevice::init("LED Matrix 0.2");
        // Create the BLE Server
        pServer = BLEDevice::createServer();
        
        pServer->setCallbacks(new MyServerCallbacks());
      
        BLEService *pService = pServer->createService(SERVICE_UUID);
      
        pCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID,
                            BLECharacteristic::PROPERTY_READ   |
                            BLECharacteristic::PROPERTY_WRITE  |
                            BLECharacteristic::PROPERTY_NOTIFY |
                            BLECharacteristic::PROPERTY_INDICATE
                          );
        // Create a BLE Descriptor                  
        pCharacteristic->addDescriptor(new BLE2902());
        
        pCharacteristic->setCallbacks(new MyRuntimeCallbacks());
        // Start the service
        pService->start();
      
        // Start advertising
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(false);
        pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
        BLEDevice::startAdvertising();
        Serial.println("Bluetooth module is ready to pair (SSID: " + BLUETOOTH_SSID + ")");
    }
}

void setupDisplay(bool is_enable)
{
  if (is_enable)
  {
    display.begin(5);
    display.setScanPattern(ZAGGIZ); // LINE, ZIGZAG, ZAGGIZ, WZAGZIG, VZAG
    display.setMuxPattern(BINARY);  // BINARY, STRAIGHT
    //display.setMuxDelay(1,1,1,0,0);
    display.setPanelsWidth(PANELS_COUNT);
    display.setRotate(eeprom_data.rotate);
    display.setFlip(eeprom_data.flip);
    //display.setFastUpdate(true);
    display.setColorOffset(0, 0, 0);
    display.flushDisplay();

    String str_font_color((char*)eeprom_data.fontColor);
    uint8_t r = str_font_color.substring(0, str_font_color.indexOf(",")).toInt();
    r_color = r;
    String tmp_value = str_font_color.substring(str_font_color.indexOf(",") + 1, str_font_color.length());
    uint8_t g = tmp_value.substring(0, tmp_value.indexOf(",")).toInt();
    g_color = g;
    uint8_t b = tmp_value.substring(tmp_value.indexOf(",") + 1, tmp_value.length()).toInt(); 
    b_color = b;   
    display.setTextColor(display.color565(r, g, b));
    display.setTextSize(atoi(eeprom_data.fontSize));
    display.setBrightness(atoi(eeprom_data.brightness));
    display.clearDisplay();
    
    display_update_enable(true);
  }
}

uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myGREEN = display.color565(0, 255, 0);

int8_t static apple_icon[MATRIX_WIDTH][MATRIX_HEIGHT]={
  {9,10,11}, 
  {7,8,9,10,11,12,13}, 
  {6,7,8,9,10,11,12,13,14}, 
  {5,6,7,8,9,10,11,12,13,14,15},
  {5,6,7,8,9,10,11,12,13,14,15,16},
  {5,6,7,8,9,10,11,12,13,14,15,16},
  {6,7,8,9,10,11,12,13,14,15},
  {3,4,6,7,8,9,10,11,12,13,14,15},
  {2,3,6,7,8,9,10,11,12,13,14,15,16},
  {5,6,7,8,9,10,11,12,13,14,15,16},
  {5,6,7,8,9,10,11,12,13,14,15},
  {5,6,7,11,12,13,14},
  {6,12,13}
};

int8_t static android_icon[MATRIX_WIDTH][MATRIX_HEIGHT]={
  {7,8,9,10,11},
  {7,8,9,10,11},
  {},
  {5,7,8,9,10,11,12,13},
  {1,4,5,7,8,9,10,11,12,13,14},
  {2,3,4,5,7,8,9,10,11,12,13,14,15,16,17},
  {3,4,5,7,8,9,10,11,12,13,14,15,16,17},
  {3,4,5,7,8,9,10,11,12,13,14},
  {3,4,5,7,8,9,10,11,12,13,14,15,16,17},
  {2,3,4,5,7,8,9,10,11,12,13,14,15,16,17},
  {1,4,5,7,8,9,10,11,12,13,14},
  {5,7,8,9,10,11,12,13},
  {},
  {7,8,9,10,11},
  {7,8,9,10,11}
};

void drawImage(int8_t image[MATRIX_WIDTH][MATRIX_HEIGHT], uint16_t offsetOX, uint16_t color)
{
    for (int xx = 0; xx < MATRIX_WIDTH; xx++)
    {
        for (int yy = 0; yy < MATRIX_HEIGHT; yy++)
        {
            if (image[xx][yy])
            {
                display.drawPixel(offsetOX + xx, image[xx][yy], color);
            }
        }
    }
}


void startScreen()
{
    display.drawRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT, display.color565(255, 0, 0));
    display.drawRect(2, 2, MATRIX_WIDTH-4, MATRIX_HEIGHT-4, display.color565(0, 255, 0));
    display.drawRect(4, 4, MATRIX_WIDTH-8, MATRIX_HEIGHT-8, display.color565(0, 0, 255));
    display.setTextSize(1);
    display.setCursor((MATRIX_WIDTH/2)-8,(MATRIX_HEIGHT/2)-3);
    display.println("LED");
    display.setTextSize(atoi(eeprom_data.fontSize));

    //drawImage(apple_icon, 3, myWHITE);
    //drawImage(android_icon, 22, myGREEN);
    
}

void setup() 
{
    setupSerial(true);
  
    setupEeprom(true);
    
    setupBluetooth(true);
    
    setupDisplay(true);

    startScreen();
    
    delay(1000);
}
union single_double{
  uint8_t two[2];
  uint16_t one;
} this_single_double;

void loop() 
{
    //  Send message to client.
    if (is_isset_message_for_client && deviceConnected) 
    {
        pCharacteristic->setValue(message_for_client.c_str());
        pCharacteristic->notify();
        is_isset_message_for_client = false;
    }
  
    // Disconnecting.
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    
    // Connecting.
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
    
    //  Display message.
    if (is_isset_message)
    {
        scroll_text(ypos, atoi(eeprom_data.scrollingSpeed), message_text);
    }
    
    display.clearDisplay();
    delay(20);
}
