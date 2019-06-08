#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "passwords.h"

// led
#define LED LED_BUILTIN // D4
#define PUMP_GPIO 0
#define PUMP_ON_TIME (5*60*1000UL) // 5 minutes
#define PUMP_OFF_TIME (3*60*1000L) // 3 minutes

WiFiClient espClient;
PubSubClient mqttClient(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

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
}

void turn_off_pump() {
  pump_state = false;
  pump_changed_since = millis();

  digitalWrite(LED, HIGH);  // Turn the LED off by making the voltage HIGH
  digitalWrite(PUMP_GPIO, HIGH);
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
  Serial.println(wifi_ssid);
  WiFi.mode(WIFI_STA);

  WiFi.begin(wifi_ssid, wifi_password);

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
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("flower/pump", "connected");
      // ... and resubscribe
      mqttClient.subscribe("flower/valve");
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
  long now = millis();
  if (now - lastMsg > 60*1000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 50, "#%d Enable: %d   Pump state: %d", value, pump_enabled, pump_state);
    Serial.print("Publish message: ");
    Serial.println(msg);
    mqttClient.publish("flower/pump", msg);
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
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  loop_mqtt();
  loop_pump();
  mqttSendState();
  
  delay(100);
}

