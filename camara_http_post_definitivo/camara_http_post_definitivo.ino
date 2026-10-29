#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <base64.h> 
#include "soc/soc.h"           // Necesario para controlar el Brownout
#include "soc/rtc_cntl_reg.h"  // Necesario para controlar el Brownout

// ================= CONFIGURACIÓN DE TIEMPO =================
#define uS_TO_S_FACTOR 1000000ULL  // Factor de conversión de microsegundos a segundos
#define TIME_TO_SLEEP  3600        // TIEMPO EN SEGUNDOS (3600s = 1 Hora)

// ================= PINES (AI THINKER) =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define LED_PIN           4

// ================= DATOS DE RED =================
//const char* ssid = "infind";
//const char* password = "1518wifi";

const char* POST_URL = "https://nr11.iot-uma.es/camera";

const char* ssid = "DIGIFIBRA-TySA";
const char* password = "T7zKKUR4b5kS";

NetworkClientSecure cliente;

void setup() {
  // 1. Desactivar detector de caídas de tensión (Brownout)
  // Vital cuando se trabaja con baterías y picos de consumo (WiFi + Flash)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nDespertando...");

  // 2. CONFIGURACIÓN CÁMARA
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // QVGA para asegurar envío rápido y poco consumo
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15; 
  config.fb_count = 1;

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Asegurar flash apagado al inicio

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Error init camara. Durmiendo...");
    goToSleep(); // Si falla, dormimos para reintentar luego
  }

  sensor_t * s = esp_camera_sensor_get(); // Obtenemos el puntero al sensor

  if (s != NULL) {
    // 1. CORRECCIÓN PRINCIPAL PARA BAJAR EL VERDE
    // Habilitar el control automático de ganancia y balance de blancos
    s->set_whitebal(s, 1);   // 1 = Activar Balance de Blancos
    s->set_awb_gain(s, 1);   // 1 = Activar Ganancia AWB
    
    // Si sigue saliendo verde, CAMBIA este modo:
    // 0 = Auto, 1 = Soleado, 2 = Nublado, 3 = Oficina, 4 = Hogar
    // Prueba con 3 (Oficina) o 4 (Hogar) si estás en interiores.
    s->set_wb_mode(s, 0); 

    // 2. OTROS AJUSTES DE CALIDAD
    s->set_brightness(s, 0); // Brillo (-2 a 2)
    s->set_contrast(s, 0);   // Contraste (-2 a 2)
    s->set_saturation(s, 0); // Saturación (-2 a 2)
    
    // Opcional: Si la foto sale con "ruido" (puntitos de colores), activa esto:
    // s->set_lenc(s, 1);    // Corrección de lente
  }

  // 3. CONEXIÓN WIFI (Con Timeout)
  // Si no conecta en 10 segs, duerme para ahorrar batería
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    intentos++;
    if(intentos > 20) { // 10 segundos máx
      Serial.println("\nNo se pudo conectar al WiFi. Durmiendo...");
      goToSleep();
    }
  }
  Serial.println("\nWiFi Conectado");
  
  cliente.setInsecure();

  // 4. LÓGICA DE FOTO Y ENVÍO
  Serial.println("--- Capturando ---");
  
  // Pequeño delay antes de la foto para estabilizar voltaje
  delay(100); 

  digitalWrite(LED_PIN, HIGH);
  delay(150); 
  camera_fb_t * fb = esp_camera_fb_get();
  delay(150); 
  digitalWrite(LED_PIN, LOW);

  if (fb) {
    Serial.printf("Foto: %u bytes. Codificando...\n", fb->len);
    
    String imageFile = base64::encode(fb->buf, fb->len);
    String jsonPayload = "{\"image\":\"" + imageFile + "\"}";

    HTTPClient http;
    http.setTimeout(20000); 

    if (http.begin(cliente, POST_URL)) {
      http.addHeader("Content-Type", "application/json");
      
      int httpCode = http.POST(jsonPayload);

      if (httpCode > 0) {
        Serial.printf("ÉXITO. Código HTTP: %d\n", httpCode);
        Serial.println("Respuesta: " + http.getString());
      } else {
        Serial.printf("Error HTTP: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    } else {
      Serial.println("Error conexión servidor");
    }
    
    esp_camera_fb_return(fb);
  } else {
    Serial.println("Fallo al tomar foto");
  }

  // 5. FIN DEL TRABAJO -> A DORMIR
  Serial.println("Trabajo terminado.");
  goToSleep();
}

void goToSleep() {
  Serial.println("Entrando en Deep Sleep por 1 hora...");
  Serial.flush(); // Asegura que se imprima todo por el puerto serie antes de cortar
  
  // Apagar el LED interno o Flash por seguridad (para que no quede residual)
  digitalWrite(LED_PIN, LOW);
  
  // Configurar el despertar por temporizador
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  // Iniciar sueño profundo
  esp_deep_sleep_start();
}

void loop() {
  // En Deep Sleep, el loop NUNCA se ejecuta.
}