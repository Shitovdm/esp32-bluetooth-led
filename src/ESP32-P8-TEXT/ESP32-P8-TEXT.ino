
#include <PxMatrix.h>

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

#ifdef ESP8266

#include <Ticker.h>
Ticker display_ticker;
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_D 12
#define P_E 0
#define P_OE 2

#endif
// Pins for LED MATRIX

#define matrix_width 40
#define matrix_height 16

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
uint8_t display_draw_time=10; //10-50 is usually fine

PxMATRIX display(matrix_width,matrix_height,P_LAT, P_OE,P_A,P_B,P_C);
//PxMATRIX display(64,32,P_LAT, P_OE,P_A,P_B,P_C,P_D);
//PxMATRIX display(64,64,P_LAT, P_OE,P_A,P_B,P_C,P_D,P_E);

// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);

uint16_t myCOLORS[8]={myRED,myGREEN,myBLUE,myWHITE,myYELLOW,myCYAN,myMAGENTA,myBLACK};


#ifdef ESP32
void IRAM_ATTR display_updater(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
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



void setup() {
  Serial.begin(9600);
  display.begin(4);
  display.setScanPattern(ZAGGIZ); // LINE, ZIGZAG, ZAGGIZ, WZAGZIG, VZAG
  display.setMuxPattern(BINARY);  // BINARY, STRAIGHT
  //display.setMuxDelay(0,1,0,0,0);
  display.setRotate(false);
  display.setFlip(false);
  display.setFastUpdate(true);
  display.setColorOffset(0, 0, 0);
  display.setPanelsWidth(1);
  display.setBrightness(255);
  
  display.clearDisplay();
  //display.setTextColor(myWHITE);
  display.setCursor(2,0);
  display.setTextSize(1);
  display.print("Init...");

  //display.setTextColor(display.color565(40,240,120));
  //display.print("Myc cyc");
  
  display_update_enable(true);
  delay(10000);

}
union single_double{
  uint8_t two[2];
  uint16_t one;
} this_single_double;


unsigned long last_draw=0;
void scroll_text(uint8_t ypos, unsigned long scroll_delay, String text, uint8_t colorR, uint8_t colorG, uint8_t colorB)
{
    uint16_t text_length = text.length();
    display.setTextWrap(false);  // we don't wrap text so it scrolls nicely
    display.setTextSize(1);
    display.setRotation(0);
    display.setTextColor(display.color565(colorR,colorG,colorB));

    // Asuming 5 pixel average character width
    for (int xpos=matrix_width; xpos>-(matrix_width+text_length*5); xpos--)
    {
      display.setTextColor(display.color565(colorR,colorG,colorB));
      display.clearDisplay();
      display.setCursor(xpos,ypos);
      display.println(text);
      delay(scroll_delay);
      yield();

      // This might smooth the transition a bit if we go slow
      // display.setTextColor(display.color565(colorR/4,colorG/4,colorB/4));
      // display.setCursor(xpos-1,ypos);
      // display.println(text);

      delay(scroll_delay/5);
      yield();

    }
}

uint8_t icon_index=0;
void loop() {
  scroll_text(0,30,"FUCK FUCK FUCK",40,240,120);
  display.clearDisplay();

}
