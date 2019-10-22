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
#define MATRIX_HEIGHT 16
#define EEPROM_START 0
#define SERIAL_BAUD_RATE 115200

#define ROTATE_DEFAULT false
#define FLIP_DEFAULT false
#define SPEED_DEFAULT "40"
#define BRIGHTNESS_DEFAULT "200"
#define TEXT_DEFAULT "Start greeting :-)"


const String BLUETOOTH_SSID = "LED Matrix 0.1";
const uint8_t DISPLAY_DRAW_TIME = 10;

static  prog_uint32_t crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

struct eeprom_data_t {
  bool rotate;
  bool flip;
  char brightness[3];
  char speed[4];
  char text[255];
} eeprom_data;

boolean setEEPROM = false;
uint32_t memcrc; uint8_t *p_memcrc = (uint8_t*)&memcrc;
int incoming;
String message_text = "";
String backup_message_text = "";
bool is_isset_message = true; // Тут будет false и true будет проставляться в процесске инициализации, если есть в памяти текст.
bool pre_control_mode = false;
bool control_mode = false;
bool allow_ = false;

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
    EEPROM.begin(sizeof(eeprom_data) + sizeof(memcrc));
    for (int i = EEPROM_START; i < EEPROM_START + sizeof(eeprom_data) + sizeof(memcrc); i++) 
    {
      EEPROM.write(i, 0);
    }
    EEPROM.end();
    ESP.restart();
}

void write_settings_to_eeprom()
{
    int i;
    byte eeprom_data_tmp[sizeof(eeprom_data)];
  
    EEPROM.begin(sizeof(eeprom_data) + sizeof(memcrc));
  
    memcpy(eeprom_data_tmp, &eeprom_data, sizeof(eeprom_data));
  
    for (i = EEPROM_START; i < EEPROM_START+sizeof(eeprom_data); i++)
    {
      EEPROM.write(i, eeprom_data_tmp[i]);
    }
    memcrc = crc_byte(eeprom_data_tmp, sizeof(eeprom_data_tmp));
  
    EEPROM.write(i++, p_memcrc[0]);
    EEPROM.write(i++, p_memcrc[1]);
    EEPROM.write(i++, p_memcrc[2]);
    EEPROM.write(i++, p_memcrc[3]);
  
    EEPROM.commit();
}

void read_settings_from_eeprom()
{
    /*int i;
    uint32_t datacrc;
    byte eeprom_data_tmp[sizeof(eeprom_data)];
  
    EEPROM.begin(sizeof(eeprom_data) + sizeof(memcrc));
  
    for (i = EEPROM_START; i < EEPROM_START+sizeof(eeprom_data); i++)
    {
      eeprom_data_tmp[i] = EEPROM.read(i);
    }
  
    p_memcrc[0] = EEPROM.read(i++);
    p_memcrc[1] = EEPROM.read(i++);
    p_memcrc[2] = EEPROM.read(i++);
    p_memcrc[3] = EEPROM.read(i++);
  
    datacrc = crc_byte(eeprom_data_tmp, sizeof(eeprom_data_tmp));
  
    if (memcrc == datacrc)
    {
        setEEPROM = true;
        memcpy(&eeprom_data, eeprom_data_tmp,  sizeof(eeprom_data));
    }
    else{*/
        eeprom_data.rotate = ROTATE_DEFAULT;
        eeprom_data.flip = FLIP_DEFAULT;
        strncpy(eeprom_data.text, TEXT_DEFAULT, sizeof(TEXT_DEFAULT));
        strncpy(eeprom_data.speed, SPEED_DEFAULT, sizeof(SPEED_DEFAULT));
        strncpy(eeprom_data.brightness, BRIGHTNESS_DEFAULT, sizeof(BRIGHTNESS_DEFAULT));
      
    //}

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

void scroll_text(uint8_t ypos, unsigned long scroll_delay, String text)
{
    uint16_t text_length = text.length();
    display.setTextWrap(false);  // we don't wrap text so it scrolls nicely
    display.setTextSize(1);
    display.setRotation(0);
    
    for (int xpos = MATRIX_WIDTH; xpos > -(MATRIX_WIDTH + text_length*5); xpos--)
    {
        display.clearDisplay();
        display.setCursor(xpos,ypos);
        display.println(text);
        delay(scroll_delay);
        yield();
  
        // This might smooth the transition a bit if we go slow
        display.setCursor(xpos-1,ypos);
        display.println(text);
  
        delay(scroll_delay/5);
        yield();
    }
}

bool execute_control_command(String command, String value)
{
    if(command.equals("text"))
    {
        message_text = value;
        return false;
    }
    if(command.equals("font-size"))
    {
        if(value.toInt() > 0 && value.toInt() < 5)
        {
            display.setTextSize(value.toInt());
        }else{
            Serial.println("Invalid value! Specify a number from the range [1,4]");
        }
        return true;
    }
    if(command.equals("font-color"))
    {
        uint8_t r = value.substring(0, value.indexOf(",")).toInt();
        value = value.substring(value.indexOf(",") + 1, value.length());
        uint8_t g = value.substring(0, value.indexOf(",")).toInt();
        uint8_t b = value.substring(value.indexOf(",") + 1, value.length()).toInt();
        if( (r >=0 && r <= 255) && (g >=0 && g <= 255) && (b >=0 && b <= 255) )
        {
            display.setTextColor(display.color565(r, g, b));
        }else{
            Serial.println("Invalid color value! Specify a RGB colors in format r,g,b. All values must be in the range [0,255]");
        }
        return true;
    }
    if(command.equals("brightness"))
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
    if(command.equals("speed"))
    {
        if(value.toInt() >= 5 && value.toInt() <= 1000)
        {
            strcpy(eeprom_data.speed, value.c_str());
            write_settings_to_eeprom();
        }else{
            Serial.println("Invalid speed value! Specify a number from the range [5,1000]");
        }
        return true;
    }
    if(command.equals("flip"))
    {
        if(value.equals("false") && value.equals("true"))
        {
            Serial.println("Invalid value! Specify a true of false in string type");
            return true;
        } if(value.equals("true"))
        {
            display.setFlip(true);
        } if(value.equals("false"))
        {
            display.setFlip(false);
        }
        write_settings_to_eeprom();
        return true;
    }
    if(command.equals("rotate"))
    {
        if(value.equals("false") && value.equals("true"))
        {
            Serial.println("Invalid value! Specify a true of false in string type");
            return true;
        } if(value.equals("true"))
        {
            display.setRotate(true);
        } if(value.equals("false"))
        {
            display.setRotate(false);
        }
        write_settings_to_eeprom();
        return true;
    }
    if(command.equals("restart"))
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
    String str_speed((char*)eeprom_data.speed);
    String str_brightness((char*)eeprom_data.brightness);
    message_text = str_text;
    
    Serial.println("EEPROM initialized. Allowed = 1024B");
    Serial.println("Reading EEPROM...");
    Serial.println("Text: " + str_text);
    Serial.println("Scrolling speed: " + str_speed);
    Serial.println("Brightness: " + str_brightness);
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
    display.begin(4);
    display.setScanPattern(ZAGGIZ); // LINE, ZIGZAG, ZAGGIZ, WZAGZIG, VZAG
    display.setMuxPattern(BINARY);  // BINARY, STRAIGHT
    //display.setMuxDelay(0,1,0,0,0);
    display.setPanelsWidth(1);
    display.setRotate(eeprom_data.rotate);
    display.setFlip(eeprom_data.flip);
    display.setFastUpdate(true);
    display.setColorOffset(0, 0, 0);
    display.setTextColor(display.color565(255, 0, 255));
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
      if(!message_text.equals(eeprom_data.text))
      {
          message_text.toCharArray(eeprom_data.text, sizeof(eeprom_data.text));
          write_settings_to_eeprom();
      }
      
  }

  if (is_isset_message)
  {
    scroll_text(0, atoi(eeprom_data.speed), message_text);
  }
  
  display.clearDisplay();
  delay(20);
}
