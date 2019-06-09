#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "passwords.h"

/* led and pump configuration */
#define LED LED_BUILTIN // D4
#define PUMP_GPIO 0
#define PUMP_ON_TIME (5*60*1000UL) // 5 minutes
#define PUMP_OFF_TIME (3*60*1000L) // 3 minutes

/* mqtt channels */
#define MQTT_CONTROL "flower/valve"
#define MQTT_UPTIME_MINUTES "flower/uptime_minutes"
#define MQTT_STATE "flower/current_state"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
long lastMsg = 0;
char msg_buffer[50];

int pump_state = 0;
int pump_enabled = 0;
long pump_changed_since;

void enable_pump(void) {
  pump_enabled = true;
  turn_on_pump();
}

void disable_pump(void) {
  pump_enabled = false;
  turn_off_pump();
}

void turn_on_pump() {
  pump_state = true;
  pump_changed_since = millis();

  digitalWrite(LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because
  // it is active low on the ESP-01)
  digitalWrite(PUMP_GPIO, LOW);
  mqttClient.publish(MQTT_STATE, "on", 2);
}

void turn_off_pump() {
  pump_state = false;
  pump_changed_since = millis();

  digitalWrite(LED, HIGH);  // Turn the LED off by making the voltage HIGH
  digitalWrite(PUMP_GPIO, HIGH);
  mqttClient.publish(MQTT_STATE, "off", 2);
}

void loop_pump() {
  if (pump_enabled) {
    if (pump_state) {
      if (millis() - pump_changed_since > PUMP_ON_TIME) {
        turn_off_pump();
      }
    } else {
      if (millis() - pump_changed_since > PUMP_OFF_TIME) {
        turn_on_pump();
      }
    }
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print(LED);
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    enable_pump();
  } else {
    disable_pump();
  }
}


void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client";
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD, MQTT_STATE, 0, true, "disconnected")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish(MQTT_STATE, "connected");
      // ... and resubscribe
      mqttClient.subscribe(MQTT_CONTROL);
    } else {
      disable_pump();
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void mqttSendState(void) {
  static unsigned long uptime_minutes = 0U;
  long now = millis();
  if (now - lastMsg > 60*1000) {
    lastMsg = now;
    ++uptime_minutes;
    snprintf (msg_buffer, 50, "%lu", uptime_minutes);
    Serial.print("Publish message: ");
    Serial.println(msg_buffer);
    mqttClient.publish(MQTT_UPTIME_MINUTES, msg_buffer);
  }
}

void loop_mqtt(void) {
  mqttReconnect();
  mqttClient.loop();
}

void setup() {
  pinMode(LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(PUMP_GPIO, OUTPUT);
  turn_off_pump();
  Serial.begin(115200);
  setup_wifi();
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  loop_mqtt();
  loop_pump();
  mqttSendState();
  
  delay(100);
}
