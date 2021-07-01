#include <ArduinoJson.h>
#include <ezTime.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "settings.h"

Timezone myTZ;
WiFiClientSecure mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

// 16 chars but we need 1 extra character as it's a 0-terminated string
char deviceId[17];

/*****************************************************************************/
/* Function declaration                                                      */
/*****************************************************************************/
void connectToWiFi();
void initDeviceId();
void initHardware();
void kaching(const int quantity, const double price);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttReconnect();
void registerWifiHandlers();
void setClock();


/*****************************************************************************/
/* Main processing                                                           */
/*****************************************************************************/
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  initHardware();

  registerWifiHandlers();

  mqttWifiClient.setCACert((const char *)USERTrust_certificate);

  initDeviceId();
  log_i("This device has id '%s'.", deviceId);

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  log_i("MQTT client successfully configured.");

  connectToWiFi();
  setClock();
}

void loop() {
  // attempt MQTT reconnect if connection lost but WiFi conection still ok
  // loop() returns false if client not connected
  if (!mqttClient.loop() && WiFi.status() == WL_CONNECTED) {
    mqttReconnect();
  }

  // TODO evaluate PWM/kaching params here and call ledcWrite(0, somevalue);
  // https://www.electronicshub.org/esp32-pwm-tutorial/
}


/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  log_i("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    log_i(".");
    delay(200);
  }
}

void initDeviceId() {
  // Note that getEfuseMac() uses little endian architecture (LSB comes first). Hence, you
  // effectively get the MAC address in reverse order. As it's only 6 bytes the highest 2 bytes of
  // the 64bit int are zero.
  uint64_t reverseMacAddress = ESP.getEfuseMac();
  // The device id is created by joining
  // - the high 2 bytes (least significant ones)
  // - the low 4 bytes
  // - "pseudo-checksum": the high 2 bytes (i.e. same ones as added at the beginning) shifted to
  //   the LEFT by 1
  uint16_t high2 = (uint16_t)(reverseMacAddress >> 32);
  uint32_t low4 = (uint32_t)reverseMacAddress;
  snprintf(deviceId, 17, "%04X%08X%04X",  high2, low4, (high2 << 1));
}

void initHardware() {
  ledcAttachPin(pwm_pin, 0);

  // Initialize channels
  // channels 0-15, resolution 1-16 bits, freq limits depend on resolution
  // ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolution_bits);
  ledcSetup(0, 4000, 8); // 12 kHz PWM, 8-bit resolution
}

void kaching(const int quantity, const double price) {
  // TODO set PWM/kaching values to then be evaluated in loop()
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  log_d("Message arrived in topic '%s'.", topic);
  if (CORE_DEBUG_LEVEL > 3) { // CORE_DEBUG_LEVEL is a PlatformIO build flag
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }

  log_i("Deserializing MQTT message body to JSON.");
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    log_e("Deserializing MQTT message body to JSON failed due to: %s", error.c_str());
  } else {
    // (minimally) validate JSON structure rather than blindly accessing properties
    JsonObject sale = doc["sale"];
    const char* seller = doc["seller"];
    const char* customer = doc["customer"];
    if (sale && seller && customer) {
      const char* item = sale["item"];
      const int quantity = sale["quantity"];
      const double price = sale["price"];
      log_i("%s sold %d %s for %.2f to %s.", seller, quantity, item, price, customer);
      kaching(price, quantity);
    } else {
      log_e("'sale' or 'seller' JSON properties missing.");
    }
  }
}

// Regardless of how often you call this it will attempt at most 1 reconnect attempt within
// MQTT_RECONNECT_ATTEMPT_INTERVAL
void mqttReconnect() {
  static ulong checkMqttTimeout = 0;
  static ulong currentMillis;
  currentMillis = millis();

  if ((currentMillis > checkMqttTimeout) || (checkMqttTimeout == 0)) {
    String clientId = String(mqtt_client_id_prefix) + String(deviceId);
    log_i("Establishing MQTT connection with client id %s.", clientId.c_str());
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      log_i("Connection to MQTT broker established.");
      // (re)subscribe
      mqttClient.subscribe(mqtt_topic);
      log_i("Subscribed to topic '%s'.", mqtt_topic);
    } else {
      log_e("MQTT connection failed with return code %d.", mqttClient.state());
    }
    checkMqttTimeout = currentMillis + MQTT_RECONNECT_ATTEMPT_INTERVAL_MILLIS;
  }
}

void onWiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  log_i("Connected to AP successfully");
}

void onWiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  log_i("Disconnected from WiFi, reason: %s", info.disconnected.reason);
  log_i("Trying to reconnect");
  connectToWiFi();
}

void onWiFiStationGotIp(WiFiEvent_t event, WiFiEventInfo_t info) {
  log_i("Got IP address %s, RSSI %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void onWiFiStationLostIp(WiFiEvent_t event, WiFiEventInfo_t info) {
  log_i("Lost IP address");
}

void registerWifiHandlers() {
  WiFi.onEvent(onWiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(onWiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.onEvent(onWiFiStationGotIp, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(onWiFiStationLostIp, SYSTEM_EVENT_STA_LOST_IP);
}

void setClock() {
  waitForSync();
  log_i("UTC time: %s", UTC.dateTime().c_str());

  if (myTZ.setLocation(F(timezone))) {
    log_i("Local time at %s: %s", timezone, myTZ.dateTime().c_str());
  } else {
    log_e("%s", errorString());
  }
}
