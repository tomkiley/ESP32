/**
 * Secure Write Example code for InfluxDBClient library for Arduino
 * Enter WiFi and InfluxDB parameters below
 *
 * Demonstrates connection to any InfluxDB instance accesible via:
 *  - unsecured http://...
 *  - secure https://... (appropriate certificate is required)
 *  - InfluxDB 2 Cloud at https://cloud2.influxdata.com/ (certificate is preconfigured)
 * Measures signal level of the actually connected WiFi network
 * This example demonstrates time handling, secure connection and measurement writing into InfluxDB
 * Data can be immediately seen in a InfluxDB 2 Cloud UI - measurement wifi_status
 * 
 * Complete project details at our blog: https://RandomNerdTutorials.com/
 * 
 **/



#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif

#include <mutex>

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ArduinoOTA.h>
#include <Fonts/Picopixel.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "secrets.h"
#include "types.h"
#include "config.h"
#include "ota.h"


// InfluxDB client instance with preconfigured InfluxCloud certificate
// InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// InfluxDB client instance without preconfigured InfluxCloud certificate for insecure connection 
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_BUCKET);
MatrixPanel_I2S_DMA *display = nullptr;
TaskHandle_t query_task;
std::mutex display_lock;

//TODO global cleanup
uint16_t myBLACK = display->color565(0, 0, 0);
uint16_t myWHITE = display->color565(255, 255, 255);
uint16_t myRED = display->color565(255, 0, 0);
uint16_t myGREEN = display->color565(0, 255, 0);
uint16_t myBLUE = display->color565(0, 0, 255);

uint8_t GLOBAL_COUNTER = 0;

time_t base_time = 0;
int media_position = 0;
int state = 0;

void displaySetup()
{
  HUB75_I2S_CFG mxconfig(
      panelResX,  // module width
      panelResY,  // module height
      panel_chain // Chain length
  );

  // If you are using a 64x64 matrix you need to pass a value for the E pin
  // The trinity connects GPIO 18 to E.
  // This can be commented out for any smaller displays (but should work fine with it)
  // mxconfig.gpio.e = 18;

  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  // mxconfig.clkphase = false;

  // Some matrix panels use different ICs for driving them and some of them have strange quirks.
  // If the display is not working right, try this.
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();

  display->setFont(&Picopixel);
  display->setTextWrap(false);
  display->setBrightness(30); //255 range
  
  display->clearScreen();
  display->fillScreen(myBLACK);
  
  display->setTextSize(1); 
  display->setTextColor(myRED, myBLACK);

}

void setup_wifi() {
  WiFi.disconnect();

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  display->setCursor(4,30);
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    display->print(".");
    delay(500);
  }
  Serial.println();

  // Alternatively, set insecure connection to skip server certificate validation 
  //client.setInsecure();

  sleep(5);

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  client.setHTTPOptions(HTTPOptions().httpReadTimeout(20000).connectionReuse(true));
  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

time_t timegm(struct tm *t)
{
    return (mktime(t) - _timezone) - ((t->tm_isdst > 0) * 3600);
} 

bike_message query_status() {
  bike_message bm;
  int i;
  FluxQueryResult combined = NULL;

  bm.speed = bm.cadence = bm.distance = 0;
  i = 0;
  do {
    i++;
    combined = client.query(COMBINED);
    while(combined.next()) {
      String tag = combined.getValueByName("tag").getString();
      
      if (tag == "cadence") bm.cadence = combined.getValueByName("_value").getDouble();
      else if (tag == "speed") bm.speed = combined.getValueByName("_value").getDouble();
      else if (tag == "distance") bm.distance = combined.getValueByName("_value").getDouble();
      else if (tag == "mp_pos") {
        media_position = (int) combined.getValueByName("_value").getDouble();
        struct tm t = combined.getValueByName("_time").getDateTime().value;
        base_time = timegm(&t);
      }
      else if (tag == "mp_state") {
        String current_state = combined.getValueByName("_value").getString();
        if (current_state == "playing")  state = 1;
        else if (current_state == "paused")  state = 2;
        else if (current_state == "idle") state = 3;
        else state = 0;
      }

    }
  } while (combined.getError() != "" && i < 3);


  return bm;
}

void print_bm(const bike_message &bm) {
  Serial.print("Cadence ");
  Serial.println(bm.cadence);

  Serial.print("Speed ");
  Serial.println(bm.speed);

  Serial.print("Distance ");
  Serial.println(bm.distance);

}

void render_bm(const bike_message &bm) {

  if (bm.cadence == -1 || bm.speed == -1 || bm.distance == -1) return;

  //positions are lower left of char
  // chars ate 5 high, 3 wide

  const std::lock_guard<std::mutex> lock(display_lock);

  if (bm.cadence != 0 || bm.distance != 0 || state != 0 ){
    display->setTextSize(2);

    // display->fillRect(1, 9, 33, 7, myBLACK);
    display->fillRect(1, 9, 62, 14, myBLACK);
    display->setCursor(5, 18);
    display->printf(" %3.0f RPM", bm.cadence);

    // display->fillRect(35, 9, 33, 7, myBLACK);
    // display->setCursor(35, 14);
    // display->printf("%2.1f MPH", bm.speed);

    // display->fillRect(1, 17, 34, 6, myBLACK);
    // display->setCursor(5, 30);
    // display->printf("%1d:%02d MIN", 0,1);

    // display->fillRect(35, 17, 34, 6, myBLACK);
    // display->setCursor(35, 22);
    // display->printf("%2.1f MI", bm.distance);
  } else {
    display->fillRect(1, 9, 62, 24, myBLACK);
  }

}

void print_time() {
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [9];
  
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  strftime (buffer,9,"%I:%M:%S",timeinfo);
  Serial.println(buffer);
}

void render_time() {
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [9];
  
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  strftime (buffer,9,"%I:%M:%S",timeinfo);

  int current_position;

  
  switch (state) {
    case 0: //off
      current_position = 0;
      break;
    case 1: //playing
      {
        double diff = difftime(rawtime, base_time);
        current_position = media_position + diff;
        break;
      }
    case 2: //paused
    case 3: //idle
      current_position = media_position;
      break;
  }

  const std::lock_guard<std::mutex> lock(display_lock);
  display->setTextSize(1);
  display->fillRect(35, 1, 34, 6, myBLACK);
  display->setCursor(35,6);
  display->printf(buffer);

  if (state !=0) {
    display->setTextSize(2);
    display->fillRect(0, 20, 64, 12, myBLACK);
    display->setCursor(5, 30);
    display->printf("%02d:%02d", current_position / 60, current_position % 60);
  }
}

void query_function( void * pvParameters ) {
  while(1) {
    if (GLOBAL_COUNTER == 0) {
      bike_message bm = query_status();
      // print_bm(bm);
      render_bm(bm);
    }
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);

  displaySetup();
  setup_wifi();
  setup_ota();
  display->clearScreen();

  xTaskCreatePinnedToCore(
      query_function, /* Function to implement the task */
      "Queries", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &query_task,  /* Task handle. */
      1); /* Core where the task should run */

}

void loop() {
  // Store measured value into point
  // sensor.clearFields();
  // // Report RSSI of currently connected network
  // sensor.addField("rssi", WiFi.RSSI());
  // // Print what are we exactly writing
  // Serial.print("Writing: ");
  // Serial.println(client.pointToLineProtocol(sensor));
  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
    setup_wifi();
  }
  // Write point
  // if (!client.writePoint(sensor)) {
  //   Serial.print("InfluxDB write failed: ");
  //   Serial.println(client.getLastErrorMessage());
  // }

  // print_time();
  render_time();
  
  check_ota();
  
  delay(100);
  GLOBAL_COUNTER = (GLOBAL_COUNTER + 1) % 5;
}
