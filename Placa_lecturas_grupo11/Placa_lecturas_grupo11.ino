#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

//Para leer la luminosidad
#include "Arduino.h"

// Pantalla LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// defino el pin de la luminosidad 
#define LDR_PIN 34
#define humedad_PIN 33
#define sensor_suelo_PIN 35
#define SEGUNDOS_DEEP_SLEEP 300 //queremos que publique cada 5 minutos

// definimos macro para indicar tarea, función y línea de código en los mensajes
#define DEBUG_STRING "["+String(pcTaskGetName(NULL))+" - "+String(__FUNCTION__)+"():"+String(__LINE__)+"]   "
DHTesp dht;
// --- WiFi/MQTT ---
WiFiClient wClient;
PubSubClient mqtt_client(wClient);

const String ssid = "Livebox7-D581";
const String password = "CTZC35CZxNv2";
const String mqtt_server = "iot.ac.uma.es";
const String mqtt_user = "II11";
const String mqtt_pass = "zSWgWId8";

String ID_PLACA;
String topic_PUBLICACION;

#define PERIODO_PUBlICACION 30000

// --- FreeRTOS ---
SemaphoreHandle_t semMqttReady;

//-----------------------------------------------------
// muestra información de la tarea
//-----------------------------------------------------
inline void info_tarea_actual() { 
 Serial.println(DEBUG_STRING+"Prioridad de tarea "+ String(pcTaskGetName(NULL))+": "+String(uxTaskPriorityGet(NULL)));
}

//-----------------------------------------------------
// Callback MQTT → enciende y apaga led
//-----------------------------------------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) { 
  String mensaje="";
  for(int i=0; i<length; i++) mensaje += (char)payload[i];
  Serial.println(DEBUG_STRING+"Mensaje recibido ["+ String(topic) +"] \"" + mensaje + "\"");
}

//-----------------------------------------------------
// Conexión con WiFi
//-----------------------------------------------------
void conecta_wifi() {
  Serial.println(DEBUG_STRING+"Connecting to " + ssid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(100));
    Serial.print(".");
  }
  Serial.println();
  Serial.println(DEBUG_STRING+"WiFi connected, IP address: " + WiFi.localIP().toString());
}

//-----------------------------------------------------
// Conexión con MQTT
//-----------------------------------------------------
void conecta_mqtt() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.println(DEBUG_STRING+"Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA.c_str(), mqtt_user.c_str(), mqtt_pass.c_str())) {
      Serial.println(DEBUG_STRING+" conectado a broker: " + mqtt_server);
    } else {
      Serial.println(DEBUG_STRING+"ERROR:"+ String(mqtt_client.state()) +" reintento en 5s" );
      // Wait 5 seconds before retrying
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

//-----------------------------------------------------
//   TAREAS FreeRTOS
//-----------------------------------------------------

//-----------------------------------------------------
// Mantener conexión y ejecutar loop MQTT
void taskMQTTService(void *pvParameters) { //tarea MQTT para conectar el wifi y mantenerse conectado
  info_tarea_actual();
  // Inicialización de WiFi
  conecta_wifi();
  // Preparar identificadores
  ID_PLACA = String(WiFi.getHostname());
  topic_PUBLICACION = "II11/sensores"; //topic de publicacion

  Serial.println(DEBUG_STRING+"Identificador placa : "+ ID_PLACA);
  Serial.println(DEBUG_STRING+"Topic publicacion : "+ topic_PUBLICACION);

  // Inicializar cliente MQTT


//--------------------------------------------------
  mqtt_client.setServer(mqtt_server.c_str(), 1883);
  mqtt_client.setBufferSize(512);
  mqtt_client.setCallback(procesa_mensaje);

  // Conectar a MQTT
  conecta_mqtt();

  Serial.println(DEBUG_STRING+"Semaforo abierto...");

  // Señalizar que MQTT ya está listo
  xSemaphoreGive(semMqttReady);

  // Bucle principal de servicio MQTT
  while(true) {
    if (!mqtt_client.connected()) conecta_mqtt();
    mqtt_client.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

//-----------------------------------------------------
// Publicar cada 30s
void taskPublisher(void *pvParameters) {
  info_tarea_actual();
  Serial.println(DEBUG_STRING+"Tarea publicadora esperando en semáforo...");
  // Esperar a que MQTT esté listo
  xSemaphoreTake(semMqttReady, portMAX_DELAY);
  Serial.println(DEBUG_STRING+"Tarea publicadora supera el semáforo");
  dht.setup(humedad_PIN , DHTesp::DHT11); // Connect DHT sensor to GPIO 5


  const TickType_t periodo = pdMS_TO_TICKS(PERIODO_PUBlICACION); // 30s
  while(true) {
    String mensaje="Mensaje enviado desde "+ ID_PLACA +" en "+ String(millis()) +" ms";
    Serial.println(DEBUG_STRING+"Publicando: " + mensaje);
    
    //valores humedad y temperatura
    float humedad = dht.getHumidity(); 
    float temperatura = dht.getTemperature();

    //valores de luminosidad
    digitalWrite(33, HIGH);
    delay(10);
    float luminosidad=analogRead(LDR_PIN);
    delay(10);
    digitalWrite(23, LOW);
    if (luminosidad<0){
      luminosidad=0;
    }
    float luminosidad_porcentaje=100.0*luminosidad/4095.0;

    //valores para humedad de suelo
    double humedad_suelo = analogRead(sensor_suelo_PIN);
    humedad_suelo=(4095-humedad_suelo)/40.95;

    //LCD: 
    lcd.clear();

    // Fila 1: Humedad suelo y aire
    lcd.setCursor(0,0);
    lcd.print("HS:");
    lcd.print(humedad_suelo,0);
    lcd.print("% ");

    lcd.print(" HA:");
    lcd.print(humedad,0);
    lcd.print("%");

    // Fila 2: Temperatura y luz
    lcd.setCursor(0,1);
    lcd.print("T:");
    lcd.print(temperatura,1);
    lcd.print("C ");

    lcd.print("Lum:");
    lcd.print(luminosidad_porcentaje,0);
    lcd.print("%");

    String mensaje_temp="{\"temperatura\": "+ String(temperatura) +", \"humedad_aire\": "+ String(humedad) +", \"luminosidad\":"+ String(luminosidad_porcentaje)+", \"humedad_suelo\":"+ String(humedad_suelo)+"}";
    mqtt_client.publish(topic_PUBLICACION.c_str(), mensaje_temp.c_str());
    
    gotoSleep();
  }
}

//-----------------------------------------------------
//   SETUP
//-----------------------------------------------------
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Wire.begin(21, 22);   // SDA = 21, SCL = 22
  lcd.begin();
  lcd.backlight();
  lcd.print("Iniciando...");

  Serial.println();
  // Crear semáforo
  semMqttReady = xSemaphoreCreateBinary();
  info_tarea_actual();

  // Arrancar primero la tarea MQTT (que inicializa conexión)
  xTaskCreate(taskMQTTService, "MQTT Service", 4096, NULL, 2, NULL);

  // Arrancar la otra tarea (esperará al semáforo de MQTT para publicar)
  xTaskCreate(taskPublisher,   "Publisher",   4096, NULL, 1, NULL);

  Serial.println(DEBUG_STRING+"Setup terminado, esperando conexión MQTT...");

  // --- Terminar la tarea loopTask ---
  vTaskDelete(NULL);
}

//-----------------------------------------------------
void loop() {
  // vacío: todo lo hacen las tareas FreeRTOS
  
}

void gotoSleep() {
  // add some randomness to avoid collisions with multiple devices
  int sleepSecs = SEGUNDOS_DEEP_SLEEP + random(2,6); 
  Serial.println(DEBUG_STRING+"Nos vamos a dormir ahora: "+String(millis())+" ms");
  Serial.println(DEBUG_STRING+"Placa suspendida durante "+String(sleepSecs)+" segundos...");
  Serial.flush();
  esp_deep_sleep(sleepSecs * 1000000);
}