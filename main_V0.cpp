#include "Particle.h"
#include "Adafruit_SSD1306/Adafruit_SSD1306.h"
#include "GFX/Adafruit_GFX.h"
#include "GFX/gfxfont.h"

STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));
SYSTEM_MODE(SEMI_AUTOMATIC);

#define VFD_ON    LOW
#define VFD_OFF   HIGH

#define LOGGING_ON TRUE

#if (LOGGING_ON)
   SerialLogHandler logHandler(LOG_LEVEL_ALL);
#endif

Adafruit_SSD1306 oled(D4);
FuelGauge fuel;

const int   SLEEP_INTERVAL   =  60 * 20; //defult sleep of 20 minutes - under the 23 minutes needed between mandatory pings to cell tower, otherwise full re-connection will occur
const int   VFD_PIN      =  D2;  //attached to AL1 on WJ200 VFD (NC)
const int   PULL_UP_PIN  =  D3;  //always inernally pulled-up; hardwired to D2

/*const uint32_t RETAINED_DATA_MAGIC =  0xa2c7206a;
const size_t   DEVICE_NAME_MAX_LEN =  31;*/
/*typedef struct {
	uint32_t magic;
	char deviceName[DEVICE_NAME_MAX_LEN + 1];
} RetainedData;*/

String deviceID;
String firmwareVersion;

/*retained RetainedData retainedData =  {0};*/
retained int          VFDState     =  0;
retained int          lastVFDState =  0;
retained int          VFDchanged   =  false;
retained int          lastDay      =  0;
retained int          SoC          =  0; //state of charge for battery pack 2000mAh/3.7v

void  getData();
void  updateOLED();
void  publishData();
void  syncTime();
/*void  getDeviceName();*/
/*void  deviceNameHandler(const char *topic, const char *data);*/

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
  pinMode(PULL_UP_PIN, INPUT_PULLUP);

  //OLED setup
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
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
  //if battery drops below 20%, sleep for one hour
  if (fuel.getSoC() < 20){
    System.sleep(SLEEP_MODE_DEEP, 3600);
  }

  getData();
  updateOLED();

  if (!Particle.connected()) Particle.connect();
  waitFor(Particle.connected, 300000);

  /*getDeviceName();*/
  syncTime();

  publishData();

  System.sleep(VFD_PIN, CHANGE, SLEEP_INTERVAL, SLEEP_NETWORK_STANDBY);
  //sleep for SLEEP_INTERVAL, wake on VFD_PIN change
}

void getData()
{
  //Get VFD state
  //VFD_PIN is
  VFDState = (digitalRead(VFD_PIN) == VFD_ON ? 1 : 0);
  if (VFDState != lastVFDState) {
    VFDchanged = true;
  }else {
    VFDchanged = false;
  }
  lastVFDState = VFDState;

  Log.info("VFD State: " + String(VFDState));

  //Get battery SoC
  SoC = map(fuel.getSoC(), 0, 84, 0, 100);
}

/*void getDeviceName()
{
  if (retainedData.magic != RETAINED_DATA_MAGIC || retainedData.deviceName[0] == 0)
  {
    memset(&retainedData, 0, sizeof(retainedData));
    retainedData.magic = RETAINED_DATA_MAGIC;

    Particle.subscribe("spark/",  deviceNameHandler);
    Particle.publish("spark/device/name");
  }
}*/
void publishData()
{
  //publish states to particle cloud -> webhook -> losant

  /*String data = String::format(
    "{\"v\": %d, \"s\": %d, \"n\": \"%s\"}",
    VFDState, SoC, retainedData.deviceName
  );*/
  String data = String::format(
    "{\"v\": %d, \"s\": %d}",
    VFDState, SoC
  );

  Particle.publish("d", data);
  Log.info("Publishing currrent state to Particle Cloud");
}

void syncTime()
{
  //sync time once every 24hrs
  if (Time.day() != lastDay)
  {   // a new day calls for a sync
    Particle.syncTime();
    for(uint32_t ms=millis(); millis()-ms < 1000; Particle.process());
    lastDay = Time.day();
  }

}

/*void deviceNameHandler(const char *topic, const char *data)
{
	if (strlen(data) <= DEVICE_NAME_MAX_LEN) {
		strcpy(retainedData.deviceName, data);
	}
	else {
		// Truncate name
		strncpy(retainedData.deviceName, data, DEVICE_NAME_MAX_LEN);
		retainedData.deviceName[DEVICE_NAME_MAX_LEN] = 0;
	}
}*/


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

  CellularSignal sig = Cellular.RSSI();

  oled.setCursor(0,20);
  oled.print("RSSI:");
  oled.print(sig.rssi);

  oled.print(" QUAL:");
  oled.print(sig.qual);

  oled.display();
  delay(200);
}
