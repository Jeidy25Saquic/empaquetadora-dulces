#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BluetoothSerial.h>
#include "WebUI.h"

// Pines originales: conservar estas conexiones.
const int IN1 = 18;
const int IN2 = 19;
const int IN3 = 21;
const int IN4 = 22;
const int SENSOR_LLENADO = 27;
const int PIN_SERVO = 23;

const int PASOS_VUELTA = 4096;
const byte secuencia[8][4] = {
  {1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
  {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}
};

// Red de acceso local. Conéctate a ella para abrir el panel web.
const char *WIFI_SSID = "Llenadora-ESP32";
const char *WIFI_PASSWORD = "cajitas2026";
WebServer server(80);
BluetoothSerial SerialBT;

const unsigned long INTERVALO_PROBADO_US = 3000;
float velocidad = 60000000.0 / (PASOS_VUELTA * INTERVALO_PROBADO_US);
int direccion = 1;
int pasoActual = 0;
bool motorActivo = false;
bool giroContinuo = false;
long pasosRestantes = 0;
unsigned long ultimoPaso = 0;
unsigned long intervalo = 0;

Servo servoEmbudo;
const int ANGULO_CERRADO = 10;
const int ANGULO_ABIERTO = 80;
unsigned long tiempoLlenado = 2000;
const unsigned long TIEMPO_SERVO = 600;
const unsigned long DEBOUNCE_SENSOR = 60;

enum EstadoProceso { PREPARANDO_INICIO, ESPERANDO_CAJA, ABRIENDO_EMBUDO, LLENANDO_CAJA, CERRANDO_EMBUDO };
EstadoProceso estado = ESPERANDO_CAJA;
bool modoAutomatico = false;
bool sensorArmado = false;
bool lecturaAnterior = false;
bool deteccionEstable = false;
unsigned long cambioSensor = 0;
unsigned long inicioEstado = 0;
unsigned long cajasLlenadas = 0;

String comando = "";
bool comandoDesbordado = false;
String comandoBT = "";
bool comandoBTDesbordado = false;
unsigned long ultimaTelemetria = 0;

void mostrarMenu();

void informar(const String &mensaje) {
  Serial.println(mensaje);
  if (SerialBT.hasClient()) SerialBT.println(mensaje);
}

void actualizarVelocidad() {
  intervalo = (unsigned long)round(60000000.0 / (PASOS_VUELTA * velocidad));
}

void apagarMotor() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void realizarPaso() {
  pasoActual += direccion;
  if (pasoActual > 7) pasoActual = 0;
  if (pasoActual < 0) pasoActual = 7;
  digitalWrite(IN1, secuencia[pasoActual][0]); digitalWrite(IN2, secuencia[pasoActual][1]);
  digitalWrite(IN3, secuencia[pasoActual][2]); digitalWrite(IN4, secuencia[pasoActual][3]);
}

void detenerMotor() {
  motorActivo = false; giroContinuo = false; pasosRestantes = 0; apagarMotor();
  informar("Motor detenido");
}

void pausarBanda() { motorActivo = false; apagarMotor(); }
void continuarBanda() { motorActivo = true; giroContinuo = true; ultimoPaso = micros(); }

void actualizarMotor() {
  if (!motorActivo) return;
  unsigned long ahora = micros();
  if ((unsigned long)(ahora - ultimoPaso) >= intervalo) {
    ultimoPaso = ahora; realizarPaso();
    if (!giroContinuo && --pasosRestantes <= 0) {
      detenerMotor(); informar("Movimiento completado");
    }
  }
}

void actualizarSensor() {
  bool lectura = digitalRead(SENSOR_LLENADO) == LOW;
  unsigned long ahora = millis();
  if (lectura != lecturaAnterior) { lecturaAnterior = lectura; cambioSensor = ahora; }
  if ((unsigned long)(ahora - cambioSensor) >= DEBOUNCE_SENSOR) deteccionEstable = lectura;
}

void iniciarAutomatico() {
  if (!servoEmbudo.attached()) { informar("Error: servo sin inicializar"); return; }
  intervalo = INTERVALO_PROBADO_US;
  velocidad = 60000000.0 / (PASOS_VUELTA * intervalo);
  servoEmbudo.write(ANGULO_CERRADO); pausarBanda();
  estado = PREPARANDO_INICIO; inicioEstado = millis(); modoAutomatico = true;
  lecturaAnterior = digitalRead(SENSOR_LLENADO) == LOW;
  deteccionEstable = lecturaAnterior; cambioSensor = millis(); sensorArmado = false;
  informar("Cerrando compuerta antes de iniciar"); informar("Esperando llegada de cajita...");
}

void pararTodo() {
  modoAutomatico = false; sensorArmado = false; detenerMotor();
  estado = ESPERANDO_CAJA; servoEmbudo.write(ANGULO_CERRADO);
  informar("Proceso detenido. Embudo cerrando.");
}

void actualizarProceso() {
  if (!modoAutomatico) return;
  unsigned long ahora = millis();
  switch (estado) {
    case PREPARANDO_INICIO:
      if (ahora - inicioEstado >= TIEMPO_SERVO) {
        continuarBanda(); estado = ESPERANDO_CAJA; informar("Banda iniciada. Esperando una nueva cajita.");
      } break;
    case ESPERANDO_CAJA:
      if (!deteccionEstable) sensorArmado = true;
      if (sensorArmado && deteccionEstable) {
        sensorArmado = false; pausarBanda(); informar("CAJITA DETECTADA"); informar("Banda detenida");
        servoEmbudo.write(ANGULO_ABIERTO); informar("Abriendo embudo...");
        inicioEstado = ahora; estado = ABRIENDO_EMBUDO;
      } break;
    case ABRIENDO_EMBUDO:
      if (ahora - inicioEstado >= TIEMPO_SERVO) { inicioEstado = ahora; estado = LLENANDO_CAJA; informar("Llenando cajita..."); }
      break;
    case LLENANDO_CAJA:
      if (ahora - inicioEstado >= tiempoLlenado) {
        servoEmbudo.write(ANGULO_CERRADO); inicioEstado = ahora; estado = CERRANDO_EMBUDO; informar("Cerrando embudo...");
      } break;
    case CERRANDO_EMBUDO:
      if (ahora - inicioEstado >= TIEMPO_SERVO) {
        cajasLlenadas++; informar("Llenado terminado"); informar("Cajas llenadas: " + String(cajasLlenadas));
        continuarBanda(); estado = ESPERANDO_CAJA; sensorArmado = false; informar("Banda reanudada");
      } break;
  }
}

String nombreEstado() {
  if (!modoAutomatico) return motorActivo ? "Manual" : "Detenido";
  switch (estado) {
    case PREPARANDO_INICIO: return "Preparando";
    case ESPERANDO_CAJA: return "Esperando cajita";
    case ABRIENDO_EMBUDO: return "Abriendo embudo";
    case LLENANDO_CAJA: return "Llenando";
    case CERRANDO_EMBUDO: return "Cerrando embudo";
  }
  return "Desconocido";
}

String jsonEstado() {
  String j = "{\"motor\":"; j += motorActivo ? "true" : "false";
  j += ",\"automatico\":"; j += modoAutomatico ? "true" : "false";
  j += ",\"sensor\":"; j += deteccionEstable ? "true" : "false";
  j += ",\"servoAbierto\":"; j += (estado == ABRIENDO_EMBUDO || estado == LLENANDO_CAJA) ? "true" : "false";
  j += ",\"cajas\":" + String(cajasLlenadas);
  j += ",\"velocidad\":" + String(velocidad, 2);
  j += ",\"tiempo\":" + String(tiempoLlenado / 1000.0, 1);
  j += ",\"direccion\":\"" + String(direccion == 1 ? "Derecha" : "Izquierda") + "\"";
  j += ",\"estado\":\"" + nombreEstado() + "\"}";
  return j;
}

void enviarEstadoBT() { if (SerialBT.hasClient()) SerialBT.println("STATUS " + jsonEstado()); }

void procesarComando(String texto) {
  texto.trim(); texto.toUpperCase();
  if (texto == "INICIAR" || texto == "START") {
    if (!modoAutomatico) iniciarAutomatico();
  } else if (texto == "PARAR" || texto == "STOP") {
    pararTodo();
  } else if (texto == "DERECHA" || texto == "RIGHT") {
    if (motorActivo || modoAutomatico) { informar("Usa PARAR antes de cambiar direccion"); return; }
    direccion = 1; informar("Direccion: derecha");
  } else if (texto == "IZQUIERDA" || texto == "LEFT") {
    if (motorActivo || modoAutomatico) { informar("Usa PARAR antes de cambiar direccion"); return; }
    direccion = -1; informar("Direccion: izquierda");
  } else if (texto.startsWith("VELOCIDAD ") || texto.startsWith("SPEED ")) {
    int separador = texto.indexOf(' '); float nueva = texto.substring(separador + 1).toFloat();
    if (nueva >= 1 && nueva <= 15) { velocidad = nueva; actualizarVelocidad(); informar("Velocidad: " + String(velocidad, 2) + " RPM"); }
    else informar("Velocidad valida: 1 a 15 RPM");
  } else if (texto.startsWith("TIEMPO ") || texto.startsWith("TIME ")) {
    int separador = texto.indexOf(' '); float segundos = texto.substring(separador + 1).toFloat();
    if (segundos >= 0.5 && segundos <= 10) { tiempoLlenado = (unsigned long)(segundos * 1000); informar("Tiempo configurado: " + String(segundos, 1) + " segundos"); }
    else informar("Tiempo permitido: 0.5 a 10 segundos");
  } else if (texto.startsWith("GRADOS ") || texto.startsWith("DEGREES ")) {
    if (modoAutomatico) { informar("Usa PARAR antes de mover por grados"); return; }
    int separador = texto.indexOf(' '); float grados = texto.substring(separador + 1).toFloat();
    if (grados > 0 && grados <= 3600) {
      pasosRestantes = (long)round(grados * PASOS_VUELTA / 360.0); giroContinuo = false; motorActivo = true; ultimoPaso = micros();
      informar("Moviendo " + String(grados, 1) + " grados");
    } else informar("Grados validos: 1 a 3600");
  } else if (texto == "ESTADO" || texto == "STATUS") {
    informar("STATUS " + jsonEstado());
  } else if (texto == "SENSOR") {
    informar(deteccionEstable ? "Sensor: CAJITA DETECTADA" : "Sensor: LIBRE");
  } else if (texto == "AYUDA" || texto == "HELP") {
    mostrarMenu();
  } else if (texto.length() > 0 && texto != "POLL") {
    informar("Comando no reconocido");
  }
}

void leerLinea(Stream &puerto, String &buffer, bool &desbordado) {
  int caracteres = 0;
  while (puerto.available() > 0 && caracteres++ < 64) {
    char c = (char)puerto.read();
    if (c == '\n' || c == '\r') {
      if (desbordado) informar("Comando demasiado largo: descartado");
      else if (buffer.length()) procesarComando(buffer);
      buffer = ""; desbordado = false;
    } else if (buffer.length() < 60 && !desbordado) buffer += c;
    else desbordado = true;
  }
}

void configurarWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  server.on("/api/status", HTTP_GET, []() { server.send(200, "application/json", jsonEstado()); });
  server.on("/api/cmd", HTTP_POST, []() {
    if (!server.hasArg("c")) { server.send(400, "application/json", "{\"ok\":false}"); return; }
    procesarComando(server.arg("c"));
    server.send(200, "application/json", "{\"ok\":true,\"status\":" + jsonEstado() + "}");
  });
  server.onNotFound([]() { server.send(404, "text/plain", "No encontrado"); });
  server.begin();
}

void mostrarMenu() {
  informar("===== CONTROL DE PRODUCCION =====");
  informar("INICIAR / PARAR - Automatizacion; DERECHA / IZQUIERDA");
  informar("VELOCIDAD 4.88 - RPM; TIEMPO 2 - segundos; GRADOS 90");
  informar("SENSOR; ESTADO; AYUDA");
}

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(SENSOR_LLENADO, INPUT);
  apagarMotor(); actualizarVelocidad();
  lecturaAnterior = digitalRead(SENSOR_LLENADO) == LOW; deteccionEstable = lecturaAnterior; cambioSensor = millis();
  servoEmbudo.setPeriodHertz(50); servoEmbudo.attach(PIN_SERVO, 1000, 2000); servoEmbudo.write(ANGULO_CERRADO);
  delay(500);
  WiFi.mode(WIFI_AP); WiFi.softAP(WIFI_SSID, WIFI_PASSWORD); configurarWeb();
  bool btOk = SerialBT.begin("LlenadoraESP32");
  informar("Sistema de llenado preparado; pines originales conservados");
  informar("WiFi: Llenadora-ESP32 | clave: cajitas2026 | panel: http://192.168.4.1");
  informar(btOk ? "Bluetooth listo: LlenadoraESP32" : "Error al iniciar Bluetooth");
  informar("Motor detenido por seguridad"); mostrarMenu();
}

void loop() {
  leerLinea(Serial, comando, comandoDesbordado);
  if (SerialBT.hasClient()) leerLinea(SerialBT, comandoBT, comandoBTDesbordado);
  server.handleClient();
  actualizarSensor(); actualizarProceso(); actualizarMotor();
  if (millis() - ultimaTelemetria >= 1000) { ultimaTelemetria = millis(); enviarEstadoBT(); }
}
