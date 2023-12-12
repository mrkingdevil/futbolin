#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>

#define PIN_ALTAVOZ          2
#define PIN_LED_CELEBRACION  4
#define PIN_LED_CELEBRACION1 17
#define PIN_MATRIX           5
#define PIN_TRIGGER1         14
#define PIN_ECHO1            12
#define PIN_TRIGGER2         15
#define PIN_ECHO2            13
#define PIN_PULSADOR         16
#define SSID                 "nombrewifi"
#define PASS                 "passwifi"
#define TIEMPO_RESET_ADDR    0
#define DISTANCIA_ADDR       4
#define INTENSIDAD_ADDR      8  // Dirección en EEPROM para almacenar intensidadLEDs

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, PIN_MATRIX,
  NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

int contadorEquipo1 = 0;
int contadorEquipo2 = 0;
unsigned long tiempoInicioPulsacion = 0;
bool pulsacionLarga = false;
float distanciaDeteccion = 1.0;
unsigned long TIEMPO_RESET = 5000;
int intensidadLEDs = 50;  // Valor predeterminado
unsigned long tiempoUltimoGol = 0;
unsigned long intervaloGol = 2000;  // Intervalo en milisegundos para ignorar múltiples detecciones

AsyncWebServer server(80);

void setup() {
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(intensidadLEDs);  // Configurar la intensidad de los LEDs al inicio
  pinMode(PIN_TRIGGER1, OUTPUT);
  pinMode(PIN_ECHO1, INPUT);
  pinMode(PIN_TRIGGER2, OUTPUT);
  pinMode(PIN_ECHO2, INPUT);
  pinMode(PIN_PULSADOR, INPUT_PULLUP);
  pinMode(PIN_ALTAVOZ, OUTPUT);
  pinMode(PIN_LED_CELEBRACION, OUTPUT);
  pinMode(PIN_LED_CELEBRACION1, OUTPUT);

  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conexión exitosa");

  EEPROM.begin(512);
  EEPROM.get(TIEMPO_RESET_ADDR, TIEMPO_RESET);
  EEPROM.get(DISTANCIA_ADDR, distanciaDeteccion);
  EEPROM.get(INTENSIDAD_ADDR, intensidadLEDs);
  EEPROM.end();

  // Configurar rutas para el servidor web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><body>";
    html += "<h1>Resultado del Futbolin</h1>";
    html += "<p>Equipo 1: " + String(contadorEquipo1) + "</p>";
    html += "<p>Equipo 2: " + String(contadorEquipo2) + "</p>";
    html += "<p>Distancia de Detección: " + String(distanciaDeteccion) + " cm</p>";
    html += "<p>Tiempo de Reset: " + String(TIEMPO_RESET) + " ms</p>";
    html += "<p>Intensidad de LEDs: " + String(intensidadLEDs) + "</p>";
    html += "<br><a href='/configuracion'>Configuracion</a>";
    html += "<br><a href='/configuracion-intensidad'>Configuracion de Intensidad</a>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  // Agregar configuración de intensidad
  server.on("/configuracion-intensidad", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><body>";
    html += "<h1>Configuracion de Intensidad de LEDs</h1>";
    html += "<form method='POST' action='/guardar-intensidad'>";
    html += "Intensidad de LEDs: <input type='range' name='intensidad' min='0' max='255' value='" + String(intensidadLEDs) + "'><br>";
    html += "<input type='submit' value='Guardar'>";
    html += "</form>";
    html += "<br><a href='/'>Volver</a>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  // Manejar el formulario de configuración de intensidad
  server.on("/guardar-intensidad", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("intensidad", true)){
      intensidadLEDs = request->getParam("intensidad", true)->value().toInt();
      EEPROM.begin(512);
      EEPROM.put(INTENSIDAD_ADDR, intensidadLEDs);
      EEPROM.commit();
      EEPROM.end();
      matrix.setBrightness(intensidadLEDs);  // Configurar la intensidad de los LEDs en tiempo real
    }
    request->redirect("/");
  });

server.on("/configuracion", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><body>";
    html += "<h1>Configuracion del Futbolin</h1>";
    html += "<form method='POST' action='/guardar-configuracion'>";
    html += "Distancia de Detección: <input type='text' name='distancia' value='" + String(distanciaDeteccion) + "'><br>";
    html += "Tiempo de Reset: <input type='text' name='tiempo' value='" + String(TIEMPO_RESET) + "'><br>";
    html += "<input type='submit' value='Guardar'>";
    html += "</form>";
    html += "<br><a href='/'>Volver</a>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/guardar-configuracion", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("distancia", true)){
      distanciaDeteccion = request->getParam("distancia", true)->value().toFloat();
      EEPROM.begin(512);
      EEPROM.put(DISTANCIA_ADDR, distanciaDeteccion);
      EEPROM.commit();
      EEPROM.end();
    }
    if(request->hasParam("tiempo", true)){
      TIEMPO_RESET = request->getParam("tiempo", true)->value().toInt();
      EEPROM.begin(512);
      EEPROM.put(TIEMPO_RESET_ADDR, TIEMPO_RESET);
      EEPROM.commit();
      EEPROM.end();
    }
    request->redirect("/");
  });

  server.begin();
}

void loop() {
  // Verificar si se ha marcado un gol en el equipo 1
  if (detectarGol(PIN_TRIGGER1, PIN_ECHO1)) {
    golEquipo1();
  }

  // Verificar si se ha marcado un gol en el equipo 2
  if (detectarGol(PIN_TRIGGER2, PIN_ECHO2)) {
    golEquipo2();
  }

  // Verificar el estado del pulsador
  verificarPulsador();

  // Verificar si se ha presionado el pulsador para resetear el contador
  if (pulsacionLarga) {
    resetearContador();
  }
  actualizarDisplay();
}

bool detectarGol(int triggerPin, int echoPin) {
  // Verificar si ha pasado suficiente tiempo desde el último gol
  if (millis() - tiempoUltimoGol > intervaloGol) {
    // Generar un pulso corto en el pin de trigger
    digitalWrite(triggerPin, LOW);
    delayMicroseconds(2);
    digitalWrite(triggerPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(triggerPin, LOW);

    // Medir el tiempo de eco en microsegundos
    long duration = pulseIn(echoPin, HIGH);

    // Calcular la distancia en centímetros
    float distance = duration * 0.034 / 2;

    // Determinar si se ha marcado un gol (ajusta la distancia según sea necesario)
    if (distance < distanciaDeteccion) {
      // Actualizar el tiempo del último gol
      tiempoUltimoGol = millis();
      return true;
    }
  }

  return false;
}

void golEquipo1() {
  contadorEquipo1++;
  celebracion();
  actualizarDisplay();
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  tone(PIN_ALTAVOZ, 1000); // C
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  tone(PIN_ALTAVOZ, 1200); // D
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  tone(PIN_ALTAVOZ, 1400); // E
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  tone(PIN_ALTAVOZ, 1600); // F
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  tone(PIN_ALTAVOZ, 1800); // G
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  tone(PIN_ALTAVOZ, 2000); // A
  delay(50);
  noTone(PIN_ALTAVOZ);
}

void golEquipo2() {
  contadorEquipo2++;
  celebracion();
  actualizarDisplay();
  digitalWrite(PIN_LED_CELEBRACION, HIGH);  
  tone(PIN_ALTAVOZ, 1500); // D#
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  tone(PIN_ALTAVOZ, 1700); // F#
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  tone(PIN_ALTAVOZ, 1900); // A#
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  tone(PIN_ALTAVOZ, 1500); // D#
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  tone(PIN_ALTAVOZ, 1700); // F#
  delay(50);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  tone(PIN_ALTAVOZ, 1900); // A#
  delay(50);
  noTone(PIN_ALTAVOZ);
}

void celebracion() {
  fuegosArtificiales(matrix);
  matrix.fillScreen(0);
  matrix.setCursor(1, 1);  // Ajusta las coordenadas según sea necesario
    matrix.print("GOOOL");
  matrix.show();
  delay(100); 
  fuegosArtificiales(matrix);
  matrix.fillScreen(0);
  matrix.setCursor(1, 1);  // Ajusta las coordenadas según sea necesario
  matrix.print("GOOOL");
  matrix.show();
  delay(100); 
  matrix.fillScreen(0); // Apagar la matriz
  matrix.show();
  delay(10); // Puedes ajustar el tiempo de celebración según sea necesario
}

void resetearContador() {
  contadorEquipo1 = 0;
  contadorEquipo2 = 0;
  actualizarDisplay();
  nuevaPartida();
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  digitalWrite(PIN_LED_CELEBRACION1, HIGH);
  tone(PIN_ALTAVOZ, 800); // G
  delay(20);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  digitalWrite(PIN_LED_CELEBRACION1, LOW);
  tone(PIN_ALTAVOZ, 600); // E
  delay(20);
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  digitalWrite(PIN_LED_CELEBRACION1, HIGH);
  tone(PIN_ALTAVOZ, 400); // C
  delay(20);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  digitalWrite(PIN_LED_CELEBRACION1, LOW);
  tone(PIN_ALTAVOZ, 300); // B
  delay(20);
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  digitalWrite(PIN_LED_CELEBRACION1, HIGH);
  tone(PIN_ALTAVOZ, 200); // A
  delay(20);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  digitalWrite(PIN_LED_CELEBRACION1, LOW);
  tone(PIN_ALTAVOZ, 300); // B
  delay(20);
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  digitalWrite(PIN_LED_CELEBRACION1, HIGH);
  tone(PIN_ALTAVOZ, 400); // C
  delay(20);
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  digitalWrite(PIN_LED_CELEBRACION1, LOW);
  tone(PIN_ALTAVOZ, 600); // E
  delay(20);
  digitalWrite(PIN_LED_CELEBRACION, HIGH);
  digitalWrite(PIN_LED_CELEBRACION1, HIGH);
  tone(PIN_ALTAVOZ, 800); // G
  digitalWrite(PIN_LED_CELEBRACION, LOW);
  digitalWrite(PIN_LED_CELEBRACION1, LOW);
  delay(20);
  noTone(PIN_ALTAVOZ);
  pulsacionLarga = false;  // Restablecer el estado de la pulsación
}

void verificarPulsador() {
  // Verificar el estado del pulsador
  if (digitalRead(PIN_PULSADOR) == LOW) {
    if (tiempoInicioPulsacion == 0) {
      tiempoInicioPulsacion = millis();  // Iniciar temporizador al inicio de la pulsación
    }

    // Verificar si la pulsación ha sido larga
    if (millis() - tiempoInicioPulsacion > TIEMPO_RESET) {
      pulsacionLarga = true;
    }
  } else {
    // Restablecer el temporizador cuando el pulsador se suelta
    tiempoInicioPulsacion = 0;
  }
}

void actualizarDisplay() {
  // Actualizar la matriz de LED con los contadores actuales
  matrix.fillScreen(0); // Limpiar la pantalla
  matrix.setTextSize(1);
  matrix.setCursor(1, 0);
  matrix.print(contadorEquipo1);
  matrix.setCursor(17, 0);
  matrix.print(contadorEquipo2);
  matrix.show();
}

void nuevaPartida() {
  // Actualizar la matriz de LED con los contadores actuales
    matrix.fillScreen(0); // Apagar la matriz
    matrix.setCursor(1, 1); // Ajusta las coordenadas según sea necesario
    matrix.print("RESET");
    matrix.show();
    delay(500);
}

void fuegosArtificiales(Adafruit_NeoMatrix &matrix) {
  int numExplosiones = 75; // Número total de explosiones simultáneas
  int explosionDuration = 10; // Duración de cada explosión en milisegundos

  for (int i = 0; i < numExplosiones; ++i) {
    // Generar una posición aleatoria en la matriz
    int x = random(matrix.width());
    int y = random(matrix.height());

    // Generar un color aleatorio
    uint32_t color = matrix.Color(random(256), random(256), random(256));

    // Mostrar la explosión en la posición aleatoria con el color aleatorio
    matrix.drawPixel(x, y, color);
  }

  // Mostrar todas las explosiones simultáneamente
  matrix.show();
  delay(explosionDuration);

  // Apagar la matriz después de todas las explosiones
  matrix.fillScreen(0);
  matrix.show();
  delay(50); // Puedes ajustar el tiempo entre explosiones
}
