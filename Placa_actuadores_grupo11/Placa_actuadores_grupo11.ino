// definimos macro para indicar función y línea de código en los mensajes
#define DEBUG_STRING "["+String(__FUNCTION__)+"():"+String(__LINE__)+"]   "

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include <ESP32Servo.h>

//definimos datos, objetos y estructuras
DHTesp dht;

WiFiClient wClient;
PubSubClient mqtt_client(wClient);


// Update these with values suitable for your network.
const String ssid = "Livebox7-D581";
const String password = "CTZC35CZxNv2";
//const String ssid = "infind";
//const String password = "1518wifi";
const String mqtt_server = "iot.ac.uma.es";
const String mqtt_user = "II11";
const String mqtt_pass = "zSWgWId8";


// cadenas para topics e ID
String ID_PLACA;
String topic_SUSCRIPCION_REGAR;
String topic_SUSCRIPCION_VENTILAR;

//Tiempo de ventilacion yy de riego
const int tiempo_riego= 30000; //riega 30 segundos
const int tiempo_ventilo= 180000; // ventila 3 minutos
const int PIN_riego=33;
const int PIN_servo = 23;
const int PIN_ventilador = 21;
Servo miServo;  // Crea el objeto servo


//-----------------------------------------------------
void conecta_wifi() {
  Serial.println(DEBUG_STRING+"Connecting to " + ssid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(DEBUG_STRING+"WiFi connected, IP address: " + WiFi.localIP().toString());
}

//-----------------------------------------------------
void conecta_mqtt() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print(DEBUG_STRING+"Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA.c_str(), mqtt_user.c_str(), mqtt_pass.c_str())) {
      Serial.println(" conectado a broker: " + mqtt_server);
      mqtt_client.subscribe(topic_SUSCRIPCION_REGAR.c_str());
      mqtt_client.subscribe(topic_SUSCRIPCION_VENTILAR.c_str());
    }
    else {
      Serial.println(DEBUG_STRING+"ERROR:"+ String(mqtt_client.state()) +" reintento en 5s" );
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//-----------------------------------------------------
void regar(){
  //aqui va todo lo referente a la bomba activa
  Serial.println("\n \n Estoy reagando y esperando\n \n");
  digitalWrite(PIN_riego,HIGH);
  delay(tiempo_riego);
  digitalWrite(PIN_riego,LOW);
  Serial.println("\n \n acabe \n \n");
  //aqui va todo lo referente a la bomba inactiva
}

void ventilar(){
  //aqui va todo lo referente a la bomba activa
  miServo.write(180);              // Le dice al servo que vaya a la posición 'pos'
  //Pines del ventilador
  pinMode(PIN_ventilador, OUTPUT);
  Serial.println("\n \n Estoy ventilando y esperando\n \n");
  delay(tiempo_ventilo);
  pinMode(PIN_ventilador, INPUT);
  miServo.write(0);              
  Serial.println("\n \n acabe \n \n");
  //aqui va todo lo referente a la bomba inactiva
}

//-----------------------------------------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) { 
  String mensaje=""; // mejor String que el buffer de bytes
  for(int i=0; i<length; i++) mensaje+= (char)payload[i];
  Serial.println(DEBUG_STRING+"Mensaje recibido ["+ String(topic) +"] \"" + mensaje + "\" "+String(length)+" bytes" );
  // compruebo el topic
  if(String(topic)==topic_SUSCRIPCION_REGAR) 
  {
    if (mensaje[0] == '1') { //regamos
        Serial.println("\n \n Estoy regando \n \n");
        regar();
    } 
  }
  else if(String(topic)==topic_SUSCRIPCION_VENTILAR) 
  {
    if (mensaje[0] == '1') { //regamos
        Serial.println("\n \n Estoy ventilando \n \n");
        ventilar();
    } 
  }
}

//-----------------------------------------------------
//     SETUPPIN_riego
//-----------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(DEBUG_STRING+"Empieza setup...");
  // crea topics usando id único de la placa
  ID_PLACA= String(WiFi.getHostname());

  topic_SUSCRIPCION_REGAR="II11/actuadores/regar";
  topic_SUSCRIPCION_VENTILAR="II11/actuadores/ventilar";
  conecta_wifi();
  mqtt_client.setServer(mqtt_server.c_str(), 1883);
  mqtt_client.setBufferSize(512); // para poder enviar mensajes de hasta X bytes
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  Serial.println(DEBUG_STRING+"Identificador placa : "+ ID_PLACA);
  Serial.println(DEBUG_STRING+"Topics suscripcion : "+ topic_SUSCRIPCION_REGAR +", "+topic_SUSCRIPCION_VENTILAR);
  Serial.println(DEBUG_STRING+"Termina setup en " +  String(millis()) + " ms");
  pinMode(PIN_riego, OUTPUT);
  miServo.attach(PIN_servo);

}

//-----------------------------------------------------

unsigned long ultimo_mensaje=0;
//-----------------------------------------------------
//     LOOP
//-----------------------------------------------------
void loop() {
  if (!mqtt_client.connected()) conecta_mqtt();
  
  mqtt_client.loop(); // esta llamada para que la librería recupere el control
  

}
