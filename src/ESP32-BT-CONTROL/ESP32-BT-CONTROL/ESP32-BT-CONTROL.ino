/**
 * 
 * 
 * 
 */

#include <string>
#include <EEPROM.h>
#include <PxMatrix.h>
#include "BluetoothSerial.h"

#ifdef ESP32
#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
#endif


#define LENGTH(x) (strlen(x) + 1)
#define MATRIX_WIDTH 40
#define MATRIX_HEIGHT 20
#define EEPROM_START 0
#define SERIAL_BAUD_RATE 115200

#define ROTATE_DEFAULT false
#define FLIP_DEFAULT false
#define SCROLLING_SPEED_DEFAULT "40"
#define BRIGHTNESS_DEFAULT "200"
#define FONT_COLOR_DEFAULT "255,0,255"
#define FONT_SIZE_DEFAULT "1"
#define TEXT_DEFAULT "PENDING CONNECTION..."

const String FIRMWARE_VERSION = "0.2";
const String HARDWARE = "DOIT ESP32 DEVKIT V1";
const String LED_TYPE = "P8 32X16 RGB";
const String LED_SERIAL = "P10-1R-V706";
const String LED_INFO = "40X20,ABC,1/5,ZAGGIZ,BINARY";

const String BLUETOOTH_SSID = "RED LED Matrix 0.1";
uint8_t DISPLAY_DRAW_TIME = 10;

static prog_uint32_t crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

struct eeprom_data_t {
  bool rotate;
  bool flip;
  char brightness[8];
  char scrollingSpeed[8];
  char text[128];
  char fontColor[16];
  char fontSize[4];
} eeprom_data;

boolean setEEPROM = false;
uint32_t memcrc; 
uint8_t *p_memcrc = (uint8_t*)&memcrc;
int xpos = 0;
int ypos = 0;
int incoming;
String message_text = "";
String backup_message_text = "";
bool is_isset_message = true; // Тут будет false и true будет проставляться в процесске инициализации, если есть в памяти текст.
bool pre_control_mode = false;
bool control_mode = false;
bool is_setting_change = false;

PxMATRIX display(MATRIX_WIDTH, MATRIX_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C);
BluetoothSerial ESP_BT;



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

/*void IRAM_ATTR writeTextToEEPROM()
{
    int address = 100;
    if (!EEPROM.begin(256))
    {
        Serial.println("Failed to initialise EEPROM");
        Serial.println("Restarting...");
        delay(1000);
        ESP.restart();
    }
    EEPROM.writeString(address, message_text);
    EEPROM.commit();
    delay(10);
}*/

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
    
      
    for (int xpos = MATRIX_WIDTH; xpos > -(MATRIX_WIDTH + text_length*5); xpos--)
    {
        if(is_setting_change)
        {
            is_setting_change = false; 
            return;
        }
        display.clearDisplay();
        display.setCursor(xpos,ylpos);
        display.println(text);
        delay(scroll_delay);
        yield();
  
        // This might smooth the transition a bit if we go slow
        display.setCursor(xpos-1,ylpos);
        display.println(text);
  
        delay(scroll_delay/5);
        yield();
    }
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
        ESP_BT.println(str_text);
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
        ESP_BT.println(str_fontSize);
        return true;
    }
    
    if(command.equals("set-color"))
    {
        uint8_t r = value.substring(0, value.indexOf(",")).toInt();
        String tmp_value = value.substring(value.indexOf(",") + 1, value.length());
        uint8_t g = tmp_value.substring(0, tmp_value.indexOf(",")).toInt();
        uint8_t b = tmp_value.substring(tmp_value.indexOf(",") + 1, tmp_value.length()).toInt();
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
        ESP_BT.println(str_fontColor);
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
        ESP_BT.println(str_brightness);
        return true;
    }
    
    if(command.equals("set-ypos"))
    {
        if(value.toInt() >= -100 && value.toInt() <= 100)
        {
            ypos = value.toInt();
            //display.setBrightness(value.toInt());
            //strcpy(eeprom_data.brightness, value.c_str());
            //write_settings_to_eeprom();
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
        ESP_BT.println(str_speed);
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
            ESP_BT.println(true);
            flip_text = "true";
        }else{
            ESP_BT.println(false);
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
            ESP_BT.println(true);
            rotate_text = "true";
        }else{
            ESP_BT.println(false);
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
        ESP_BT.println(info);
        Serial.println("Transferring device information to client");
        return true;
    }
    
    if(command.equals("reboot"))
    {
        if(value.equals("1"))
        {
            ESP.restart(); 
        }
        return true;
    } 
    if(command.equals("reset"))
    {
        if(value.equals("1"))
        {
            erase_eeprom();
            ESP.restart();
        }
        return true;
    } 
    if(command.equals("erase"))
    {
        if(value.equals("1"))
        {
            erase_eeprom();
        }
        return true;
    } 
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
    ESP_BT.begin(BLUETOOTH_SSID);
    Serial.println("Enabling BT module...");
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
    //display.setMuxDelay(0,1,0,0,0);
    display.setPanelsWidth(1);
    display.setRotate(eeprom_data.rotate);
    display.setFlip(eeprom_data.flip);
    display.setFastUpdate(true);
    display.setColorOffset(0, 0, 0);
    display.flushDisplay();

    String str_font_color((char*)eeprom_data.fontColor);
    uint8_t r = str_font_color.substring(0, str_font_color.indexOf(",")).toInt();
    String tmp_value = str_font_color.substring(str_font_color.indexOf(",") + 1, str_font_color.length());
    uint8_t g = tmp_value.substring(0, tmp_value.indexOf(",")).toInt();
    uint8_t b = tmp_value.substring(tmp_value.indexOf(",") + 1, tmp_value.length()).toInt();    
    display.setTextColor(display.color565(r, g, b));
    display.setTextSize(atoi(eeprom_data.fontSize));
    display.setBrightness(atoi(eeprom_data.brightness));
    display.clearDisplay();
    
    display_update_enable(true);
  }
}

void setup() 
{
  setupSerial(true);

  setupEeprom(true);
  
  setupBluetooth(true);
  
  setupDisplay(true);
  
  delay(200);
}
union single_double{
  uint8_t two[2];
  uint16_t one;
} this_single_double;

void loop() 
{
    if (ESP_BT.available())
    {
        if(is_isset_message)
        {
          backup_message_text = message_text;
          message_text = "";
        }
        is_isset_message = false;
        incoming = ESP_BT.read();
    
        if (incoming != 13 && incoming != 10)
        {
            message_text = message_text + char(incoming);
            if(incoming == 35 || incoming == 95) //
            {
                if(incoming == 35)
                {
                    pre_control_mode = true;
                }else{ 
                    if(pre_control_mode)
                    { 
                      control_mode = true; 
                    }
                }
            }
        }
        if (incoming == 10)
        {
            Serial.println("Received message: " + message_text);
            if(control_mode)
            {
                message_text = message_text.substring(1, message_text.length());
                String command = message_text.substring(1, message_text.indexOf("="));
                String value = message_text.substring(message_text.indexOf("=") + 1, message_text.length());
  
                Serial.println("Executing controll command '" + command + "' with value '" + value + "'");
                if(execute_control_command(command, value))
                {
                    message_text = backup_message_text;
                }
            }else{
                //  Оставляем текст как был.
                message_text = backup_message_text;
            }
            backup_message_text = "";
            is_isset_message = true;
        }
    }
  
    if (is_isset_message)
    {
        scroll_text(ypos, atoi(eeprom_data.scrollingSpeed), message_text);
    }
    
    display.clearDisplay();
    delay(20);
}
