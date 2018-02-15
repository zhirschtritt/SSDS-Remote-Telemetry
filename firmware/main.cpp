#include "Particle.h"
PRODUCT_ID(4282);
PRODUCT_VERSION(2);

#include "Adafruit_SSD1306/Adafruit_SSD1306.h"
#include "GFX/Adafruit_GFX.h"
#include "GFX/gfxfont.h"

STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));
SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

#define VFD_ON    LOW
#define VFD_OFF   HIGH
#define FLOW_ON   LOW
#define FLOW_OFF  HIGH

#define LOGGING_ON TRUE

#if (LOGGING_ON)
   SerialLogHandler logHandler(LOG_LEVEL_ALL);
#endif

String deviceID;
String firmwareVersion;
Adafruit_SSD1306 oled(-1); //oled reset pin not used w/i2c but needed by library
FuelGauge fuel;

const    int    SLEEP_INTERVAL  =  60 * 20; //defult sleep of 20 minutes - under the 23 minutes needed between mandatory pings to cell tower, otherwise full re-connection will occur
const    int    VFD_PIN         =  D2;  //attached to AL1 on WJ200 VFD (NC)
const    int    VFD_PULLUP_PIN  =  D3;  //always inernally pulled-up; hardwired to D2
const    int    FLOW_PIN        =  D4;  //attached to common connection of flow switch
const    int    FLOW_PULLUP_PIN =  D5;  //always inernally pulled-up; hardwired to D4

retained int    VFDState        =  0;
retained int    FlowState       =  0;
retained int    lastDay         =  0;
retained int    SoC             =  0; //state of charge for battery pack 2000mAh/3.7v

void  getData();
void  updateOLED();
void  publishData();
void  syncTime();

ApplicationWatchdog wd(660000, System.reset); //This Watchdog code will reset the processor if the dog is not kicked every 11 mins which gives time for 2 modem reset's.

void setup()
{
  Serial.begin(14400);

  #if (PLATFORM_ID == PLATFORM_ELECTRON_PRODUCTION)
    Cellular.on();
    Cellular.connect();
  #else
    WiFi.on();
    WiFi.connect();
  #endif

  pinMode(VFD_PIN, INPUT);
  pinMode(VFD_PULLUP_PIN, INPUT_PULLUP);
  pinMode(FLOW_PIN, INPUT);
  pinMode(FLOW_PULLUP_PIN, INPUT_PULLUP);

  //OLED setup
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C, FALSE); //no reset pin required, hence "FALSE"
  delay(50);
  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  //set deviceID and firmwareVersion
  deviceID = System.deviceID();
  firmwareVersion = System.version();
  Log.info("Wake up");

  delay(200);

  //battery setup
  fuel.quickStart();
  delay(200);
}


void loop()
{
  /*SleepResult r = System.sleepResult();
  if (r.wokenUpByPin()) {
    Log.info("The device was woken up by pin %d", r.pin());
  } else {
    Log.info("The device was woken up by something other then pins");
  }*/

  //if battery drops below 20%, sleep for one hour
  if (fuel.getSoC() < 20)
  {
    System.sleep(SLEEP_MODE_DEEP, 3600);
  }

  getData();
  updateOLED();

  if (!Particle.connected()) Particle.connect();
  waitFor(Particle.connected, 300000);

  /*getDeviceName();*/
  syncTime();
  publishData();

  System.sleep({FLOW_PIN, VFD_PIN}, CHANGE, SLEEP_INTERVAL, SLEEP_NETWORK_STANDBY);
  //sleep for SLEEP_INTERVAL, wake on VFD_PIN or FLOW_PIN change
}

void getData()
{
  //Get VFD state
  VFDState = digitalRead(VFD_PIN) == VFD_ON ? 1 : 0;
  FlowState = digitalRead(FLOW_PIN) == FLOW_ON ? 1 : 0;

  Log.info("VFD State: " + String(VFDState));
  Log.info("Flow State: " + String(FlowState));

  //Get battery SoC
  SoC = map((int) fuel.getSoC(), 0, 84, 0, 100);
  /*SoC = fuel.getSoC();*/
}

void publishData()
{
  //publish states to particle cloud -> webhook -> losant
  String data = String::format(
    "{\"v\": %d, \"f\": %d, \"s\": %d}",
    VFDState, FlowState, SoC
  );

  Particle.publish("test", data);
  Log.info("Publishing currrent state to Particle Cloud");
}

void syncTime()
{
  //sync time once every 24hrs
  if (Time.day() != lastDay)
  {   // a new day calls for a sync
    Particle.syncTime();
    for(uint32_t ms = millis(); millis()-ms < 1000; Particle.process());
    lastDay = Time.day();
  }
}

//Print to OLED
void updateOLED()
{
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.drawLine(0, 8, 128, 8, WHITE);

  oled.setCursor(0,0);
  oled.print("SSDS MONITOR");

  if (SoC > 100) {
      SoC = 100;
  }

  oled.print(" BAT:");
  oled.print(SoC);
  oled.print("%");

  oled.setCursor(0,10);
  oled.print("VFD: ");
  oled.print(VFDState ? "ON" : "OFF");

  oled.setCursor(60,10);
  oled.print("FLOW: ");
  oled.print(FlowState ? "ON" : "OFF");

  CellularSignal sig = Cellular.RSSI();

  oled.setCursor(0,20);
  oled.print("RSSI:");
  oled.print(sig.rssi);

  oled.print(" QUAL:");
  oled.print(sig.qual);

  oled.display();
  delay(400);
}
