#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>
#include "time.h"
#include <Base64.h>
#include <ESP8266WiFi.h>

// Sensor Information
const char* PlumeID = "1d7b5c69-5355-4401-81de-857d30a62e79";
const char* category = "organic";
short int ipUnique = 85;

// Network settings
const char* ssid = "Hawkeye";
const char* password = "awooga123"; 
const char* http_username = "hemisphere";
const char* http_password = "fk&IUhqhVeG&18t";

// Configure HTTP authentication
String auth = String(http_username) + ":" + String(http_password);
String base64Auth = base64::encode(auth);

IPAddress local_IP(11, 11, 3, ipUnique);
IPAddress gateway(11, 11, 0, 1);
IPAddress subnet(255, 255, 252, 0);
IPAddress primaryDNS(8, 8, 8, 8);

// NTP server to request epoch time
const char* ntpServer = "pool.ntp.org";
unsigned long epochTime; 
int timezoneShift = 3600*7; // Used for shifting timezone to UTC + 7 (Surakarta Time)

// Set up ultrasonic sensor
const int echoPin = 14; // D5
const int trigPin = 12; // D6
long duration;
float fullness;
float distance;

const unsigned long ONE_HOUR = 3600000;  // 1 hour in milliseconds
const unsigned long TEN_MINUTES = 600000;  // 20 minutes in milliseconds
const unsigned long FIVE_MINUTES = 300000;  // 5 minutes in milliseconds
const unsigned long ONE_MINUTE = 60000;  // 5 minutes in milliseconds

// Variables to track time
unsigned long previousMillis = 0;  // Stores the last time the action was performed
bool firstAction = true;       // Flag to check if the action is performed

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}

void setup() {
  Serial.begin(115200); // Starts the serial communication
  delay(1000);

  Serial.println("Initializing...");

  // Ultrasonic Sensor
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
}

void loop() {
  unsigned long currentMillis = millis();

  if (((currentMillis - previousMillis) >= ONE_HOUR) || firstAction) {
    firstAction = false;
    previousMillis = currentMillis;
    // Configures static IP address
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
      Serial.println("STA Failed to configure");
    }

    // Connect to Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(1000);
    }
    Serial.println("\nConnected to Wi-Fi.");
    configTime(0, 0, ntpServer);

    // GET SENSOR DATA
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Calculating the distance in cm
    duration = pulseIn(echoPin, HIGH);
    distance = (duration * 340 / 10000) / 2;


    // HTTP REQUEST TO INFLUXDB
    if (WiFi.status() == WL_CONNECTED) {
      std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
      client->setInsecure();
      HTTPClient http;

      Serial.print("[HTTP] begin...\n");
      if (http.begin(*client, "https://hawkeye-data.hemisphereproject.com/telegraf")) {  // HTTPS
        // GET current epoch time
        epochTime = getTime();

        // Send a POST request
        Serial.println("Sending POST request...");
        http.addHeader("Content-type", "application/json");
        http.addHeader("Authorization", "Basic " + base64Auth);

      String httpRequestData = String("{\"measurement\" : ") + "\"" + String(category) + "\", " +
                              "\"plume_id\" : \"" + String(PlumeID) + "\", " +
                              "\"time\" : " + String(epochTime) +
                              ", \"distance\": " + String(distance) + "}";
        int httpResponseCode = http.POST(httpRequestData);

        if (httpResponseCode > 0) {
          Serial.printf("[HTTP] POST... code: %d\n", httpResponseCode);
          String payload = http.getString();
          Serial.println(payload);
        } else {
          Serial.printf("HTTP Error code: %d\n", httpResponseCode);
          Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
      } else {
        Serial.printf("[HTTP] Unable to connect\n");
      }
    }

    // Disconnect Wi-Fi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);  // Ensure the Wi-Fi disconnects properly before going to sleep
  }
}
