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

//TODO global cleanup
uint16_t myBLACK = display->color565(0, 0, 0);
uint16_t myWHITE = display->color565(255, 255, 255);
uint16_t myRED = display->color565(255, 0, 0);
uint16_t myGREEN = display->color565(0, 255, 0);
uint16_t myBLUE = display->color565(0, 0, 255);

uint8_t GLOBAL_COUNTER = 0;

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
  display->setBrightness(60); //255 range
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

void setup() {
  Serial.begin(115200);

  displaySetup();
  setup_wifi();
  setup_ota();
  display->clearScreen();
}

bike_message query_status() {
  bike_message bm;
  int i;
  FluxQueryResult cadence = NULL;
  FluxQueryResult distance = NULL;
  FluxQueryResult speed = NULL;

  i = 0;
  do {
    i++;
    cadence = client.query(CADENCE_QUERY);
    cadence.next();
    bm.cadence = cadence.getValueByName("_value").getDouble();
  } while (cadence.getError() != "" && i < 3);

  if (cadence.getError() != "") {
    Serial.println(cadence.getError());
    bm.cadence = -1;
  }

  
  i=0;
  do {
    i++;
    speed = client.query(SPEED_QUERY);
    speed.next();
    bm.speed = speed.getValueByName("_value").getDouble();
  } while (speed.getError() != "" && i < 3);

  if (speed.getError() != "") {
    Serial.println(speed.getError());
    bm.speed = -1;
  }

  i = 0;
  do {
    i++;
    distance = client.query(DISTANCE_QUERY);
    distance.next();
    bm.distance = distance.getValueByName("_value").getDouble();
  } while (distance.getError() != "" && i < 3);

  if (distance.getError() != "") {
    Serial.println(distance.getError());
    bm.distance = -1;
  }

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

  display->clearScreen();
  display->fillScreen(myBLACK);
  
  display->setTextSize(1); 
  display->setTextColor(myRED);

  //positions are lower left of char
  // chars ate 5 high, 3 wide

  if (bm.cadence != 0 || bm.distance != 0 ){
    display->setCursor(1, 14);
    display->printf(" %3.0f RPM", bm.cadence);

    display->setCursor(35, 14);
    display->printf("%2.1f MPH", bm.speed);

    display->setCursor(1, 22);
    display->printf("%1d:%02d MIN", 0,1);

    display->setCursor(35, 22);
    display->printf("%2.1f MI", bm.distance);
  }

}

void print_time() {
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [9];
  
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  strftime (buffer,9,"%I:%M:%S",timeinfo);
  serial->println(buffer);
}

void render_time() {
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [9];
  
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  strftime (buffer,9,"%I:%M:%S",timeinfo);

  display->setCursor(35,6);
  display->printf("         ");
  display->setCursor(35,6);
  display->printf(buffer);
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

  if (GLOBAL_COUNTER == 0) {
    bike_message bm = query_status();
    print_bm(bm);
    render_bm(bm);
  }

  print_time();
  render_time();
  
  check_ota();
  
  delay(100);
  GLOBAL_COUNTER = (GLOBAL_COUNTER + 1) % 5;
}
