
// This is how many color levels the display shows - the more the slower the update
#define PxMATRIX_COLOR_DEPTH 8

// Defines the buffer height / the maximum height of the matrix
//#define PxMATRIX_MAX_HEIGHT 20

// Defines the buffer width / the maximum width of the matrix
//#define PxMATRIX_MAX_WIDTH 40

// Defines how long we display things by default
//#define PxMATRIX_DEFAULT_SHOWTIME 30

// Defines the speed of the SPI bus (reducing this may help if you experience noisy images)
//#define PxMATRIX_SPI_FREQEUNCY 20000000

// Creates a second buffer for backround drawing (doubles the required RAM)
//#define PxMATRIX_double_buffer true

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


#define matrix_width 40
#define matrix_height 20

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
uint8_t display_draw_time=20; //10-50 is usually fine

PxMATRIX display(matrix_width,matrix_height,P_LAT, P_OE,P_A,P_B,P_C);
//PxMATRIX display(matrix_width,matrix_height,P_LAT, P_OE,P_A,P_B,P_C,P_D);
//PxMATRIX display(matrix_width,matrix_height,P_LAT, P_OE,P_A,P_B,P_C,P_D,P_E);

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
  // Define your display layout here, e.g. 1/8 step, and optional SPI pins begin(row_pattern, CLK, MOSI, MISO, SS)
  display.begin(5);

  // Define multiplex implemention here {BINARY, STRAIGHT} (default is BINARY)
  display.setMuxPattern(BINARY);

  // Set the multiplex pattern {LINE, ZIGZAG,ZZAGG, ZAGGIZ, WZAGZIG, VZAG, ZAGZIG} (default is LINE)
  display.setScanPattern(ZAGGIZ);
  display.setColorOffset(0, 0, 0);
  // Set the color order {RRGGBB, RRBBGG, GGRRBB, GGBBRR, BBRRGG, BBGGRR} (default is RRGGBB)
  //display.setColorOrder(RRGGBB);
  display.setMuxDelay(1,1,1,0,0);
  display.setPanelsWidth(1);
  display.setBrightness(150);

  //display.setFastUpdate(true);
  display.clearDisplay();
  display_update_enable(true);

  delay(3000);

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
    display.setTextSize(2);
    display.setRotation(0);
    display.setTextColor(display.color565(colorR,colorG,colorB));

    // Asuming 5 pixel average character width
    for (int xpos=matrix_width; xpos>-(matrix_width+text_length*5); xpos--)
    {
      //display.setTextColor(display.color565(colorR,colorG,colorB));
      display.clearDisplay();
      display.setCursor(xpos,ypos);
      display.println(text);
      delay(scroll_delay);
      yield();

      // This might smooth the transition a bit if we go slow
      

      delay(scroll_delay/5);
      yield();

    }
}

void loop() {
  //display.drawRoundRect(0, 0, 4, 4, 4, display.color565(0, 255, 0));
  //display.drawRect(0, 0, 10, 20, display.color565(0, 255, 0));
  //display.drawRect(30, 0, 10, 20, display.color565(255, 255, 0));
  //display.fillRect(0, 0, 40, 6, display.color565(255, 255, 255));
  //display.fillRect(0, 7, 40, 7, display.color565(0, 0, 255));
  display.fillRect(0, 13, 40, 6, display.color565(255, 0, 0));
  display.fillRect(0, 19, 40, 1, display.color565(0, 0, 255));
  /*display.drawPixel(5,1,display.color565(255, 255, 0));
  display.drawPixel(5,2,display.color565(255, 255, 0));
  display.drawPixel(5,3,display.color565(255, 255, 0));
  display.drawPixel(5,4,display.color565(255, 255, 0));
  display.drawPixel(5,5,display.color565(255, 255, 0));
  display.drawPixel(5,6,display.color565(255, 255, 0));
  display.drawPixel(5,7,display.color565(255, 255, 0));*/
  //delay(100);
  
  //display.println("TEST");
  //display.setTextSize(2);
  //scroll_text(3,30,"TEST",0, 255, 0);
  //display.clearDisplay();
}
