#include <Arduino.h>

const char* timezone = "Europe/Zurich";

// hardware settings
const byte pwm_pin = 32;

// WiFi settings
const char* ssid = "SSID";
const char* password = "password";

// MQTT settings
const char* mqtt_server = "";
const int mqtt_port = 8883; // non-SSL port 1883
const char* mqtt_user = "";
const char* mqtt_password = "";
const char* mqtt_client_id_prefix = "KaChing-";
const char* mqtt_topic = "KaChing";
const int MQTT_RECONNECT_ATTEMPT_INTERVAL_MILLIS = 5000;

// JSON settings
const int JSON_DOC_SIZE = 2 * MQTT_MAX_PACKET_SIZE; // MQTT_MAX_PACKET_SIZE is PlatformIO build flag

// USERTrust Root CA in x509.pem format
// Source: https://www.cloudmqtt.com/docs/faq.html#TLS_SSL
// To verify: openssl s_client -connect <instance>.cloudmqtt.com:<port> -servername <instance>.cloudmqtt.com -showcerts
extern const uint8_t USERTrust_certificate[] asm("_binary_src_ca_USERTrust_pem_start");
