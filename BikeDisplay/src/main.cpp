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

#include "secrets.h"
#include "types.h"
#include "config.h"


// InfluxDB client instance with preconfigured InfluxCloud certificate
// InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// InfluxDB client instance without preconfigured InfluxCloud certificate for insecure connection 
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_BUCKET);

// Data point
Point sensor("wifi_status");

MatrixPanel_I2S_DMA *display = nullptr;

//TODO global cleanup
uint16_t myBLACK = display->color565(0, 0, 0);
uint16_t myWHITE = display->color565(255, 255, 255);
uint16_t myRED = display->color565(255, 0, 0);
uint16_t myGREEN = display->color565(0, 255, 0);
uint16_t myBLUE = display->color565(0, 0, 255);

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
  mxconfig.gpio.e = 18;

  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  mxconfig.clkphase = false;

  // Some matrix panels use different ICs for driving them and some of them have strange quirks.
  // If the display is not working right, try this.
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();
}

void setup() {
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  // Add tags
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());

  // Alternatively, set insecure connection to skip server certificate validation 
  //client.setInsecure();

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  displaySetup();
  display->clearScreen();
}

bike_message query_status() {
  bike_message bm;

  FluxQueryResult cadence = client.query(CADENCE_QUERY);
  bm.cadence = cadence.getValueByIndex(0).getDouble();

  FluxQueryResult speed = client.query(SPEED_QUERY);
  bm.speed = speed.getValueByIndex(0).getDouble();

  FluxQueryResult distance = client.query(DISTANCE_QUERY);
  bm.distance = distance.getValueByIndex(0).getDouble();

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
  display->clearScreen();
  display->fillScreen(myBLACK);
  display->setTextWrap(false);
  
  display->setTextSize(1);     // size 1 == 8 pixels high
  display->setTextColor(myBLUE);
  display->setCursor(0, 0);
  display->print("Hello");

  display->setTextSize(2);     // size 2 == 16 pixels high
  display->setTextColor(myGREEN);
  display->setCursor(0, 8);
  display->print("Hello");

  display->setTextSize(3);     // size 3 == 24 pixels high
  display->setTextColor(myRED);
  display->setCursor(0, 24);
  display->print("Hello");
}

void loop() {
  // Store measured value into point
  sensor.clearFields();
  // Report RSSI of currently connected network
  sensor.addField("rssi", WiFi.RSSI());
  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(sensor));
  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  bike_message bm = query_status();
  print_bm(bm);
  render_bm(bm);
  

  //Wait 10s
  Serial.println("Wait 10s");
  delay(10000);
}
