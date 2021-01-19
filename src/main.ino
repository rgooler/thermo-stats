#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#endif

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>


#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

// *****************************************************************************************************************
// TODO: EDIT YOUR PERSONAL DATA HERE
#define LOCATION "TEST"
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define INFLUXDB_URL ""
#define INFLUXDB_TOKEN ""
#define INFLUXDB_ORG ""
#define INFLUXDB_BUCKET ""
// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
#define TZ_INFO "PST8PDT"
unsigned long delayTime = 60000; // 1 minute
// END EDITING
// *****************************************************************************************************************


// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensor("bme280");
Adafruit_BME280 bme; // I2C

void setup() {
    Serial.begin(9600);
    while(!Serial);    // time to get serial running
    
    // The sensors I have live on 0x76 and not the default 0x77 address
    unsigned status = bme.begin(0x76);
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
        Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
        Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
        Serial.print("        ID of 0x60 represents a BME 280.\n");
        Serial.print("        ID of 0x61 represents a BME 680.\n");
        while (1) delay(10);
    }

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

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

  sensor.addTag("location", LOCATION);

}


void loop() { 
    // Clear fields for reusing the point. Tags will remain untouched
    sensor.clearFields();
  
    //Read the BME and load up the data
    sensor.addField("temperature", bme.readTemperature()); // in degrees C
    sensor.addField("pressure", bme.readPressure() / 100.0F); // in hPa
    sensor.addField("altitude", bme.readAltitude(SEALEVELPRESSURE_HPA)); // in meters
    sensor.addField("humidity", bme.readHumidity()); // in percentage
  
    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(sensor.toLineProtocol());
    Serial.println(WiFi.RSSI());
  
    // If no Wifi signal, try to reconnect it
    if ((WiFi.RSSI() == 0) && (wifiMulti.run() != WL_CONNECTED)) {
      Serial.println("Wifi connection lost");
      ESP.restart();
    }
  
    // Write point
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
    
    delay(delayTime);
}
