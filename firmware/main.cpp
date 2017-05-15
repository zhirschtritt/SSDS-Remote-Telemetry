#include "Particle.h"
#include "Adafruit_SSD1306/Adafruit_SSD1306.h"
#include "GFX/Adafruit_GFX.h"
#include "GFX/gfxfont.h"

#define VFD_PIN 7

#define LOGGING_ON TRUE

#if (LOGGING_ON)
   SerialLogHandler logHandler(LOG_LEVEL_ALL);
#endif

/* ============== MAIN =====================*/

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

Adafruit_SSD1306 oled(D4);

FuelGauge fuel;

unsigned long checkTS;
unsigned long lastVFDRead;
unsigned long checkVFD;
unsigned long checkBattery;

bool updateScreen = false;
int batteryStatus = 0;

volatile int VFDState = 1; //assume VFD is online at time of restart
int previousState;

String deviceID;
String firmwareVersion;

void syncClock();
void printDisplay();

/* system will reset if firmware does not checkin in 120 seconds */
ApplicationWatchdog wd(120000, System.reset);

void setup() // Put setup code here to run once
{
  Serial.begin(14400);

  // SPI
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  delay(50);
  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  delay(100);
  fuel.quickStart(); // Start Fuel Gauge
  delay(200);

  deviceID = System.deviceID();
  firmwareVersion = System.version();
  Log.info("Wake up");

  delay(200);

  //Particle.connect();

  Log.info("Setup battery");
  batteryStatus = fuel.getSoC();
  batteryStatus = map(batteryStatus, 0, 84, 0, 100);

  updateScreen = true;
  checkBattery = checkTS = checkVFD = millis();
}


void loop() // Put code here to loop forever
{
  /* sync clock once a day */

  // --------------------------------------------------------- Update VFD status every minute

  if ((millis() - checkVFD > 1000)) {
      updateScreen = true;
      //check VFD pin with debounce
      VFDState = digitalRead(VFD_PIN);
      //check for pin state change
      if (VFDState != previousState){
        //if vfd is 'on'
        if (VFDState == 1){
          Log.info("sending RESTART message");
          //do send restart message function
        }
        //if vfd is 'off'
        else {
          Log.info("sending SHUTDOWN message");
          //do send shutdown message function
        }
        previousState = VFDState;
      }
      checkVFD = millis();
    }

  // --------------------------------------------------------- Update battery SoC every 10 minutes
  if ((millis() - checkBattery > 10*60*1000)) {
      updateScreen = true;
      batteryStatus = fuel.getSoC();
      batteryStatus = map(batteryStatus, 0, 84, 0, 100);
      checkBattery = millis();
  }


  if (updateScreen) {
      printDisplay();
      updateScreen = false;
  }
}

void registerParticleEvents() {
  //Particle.subscribe("arm_device", myEventHandler);
}

// --------------------------------------------------------- Print onto OLED
void printDisplay(){
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.drawLine(0, 8, 128, 8, WHITE);

    oled.setCursor(0,0);
    if (batteryStatus > 100) {
        batteryStatus = 100;
    }

    oled.print(batteryStatus);
    oled.print("%");
    oled.print(" BAT RAW: ");
    oled.print(fuel.getSoC());

    oled.setCursor(0, 11);
    oled.print("VFD: ");
    oled.print(VFDState ? "ON" : "OFF");

    oled.display();
}
