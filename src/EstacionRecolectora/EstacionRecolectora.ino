/*
  Estacion recolectora
  Dispositivo conectado a internet por medio de wifi
  utilizando esp8266 y sensores de
  Temperatura dht22
  Humedad dht22
  Movimiento por infarrojo PIR
  Composicion de Gas MQ2

  Emite alertas cuando los valores esten arriba o abajo de los
  determinados, asi como el envio constante de datos para almacenamiento
  Yeffri J. Salazar
  Xibalba Hackerspace, nov 2018 

*/


/********* librerias ********/ 


#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
/*
   Sensor de temperatura
*/
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
/*
   Cliente NTP
*/
#include <NTPClient.h>
#include <WiFiUdp.h>


/*constantes*****************/
/*
   Access point
*/
#define botonModoAP D1
/*
   MQTT
*/
#define usuarioServidorMQTT "" // usuario para acceder al broker
#define claveServidorMQTT ""     // clave del usuario para acceder al broker
#define servidorMQTT ""
#define puertoMQTT 1883
#define topicoActualizacion "/update"
#define topicoConexion "/alive" //topico que utilizara la opción de birth de mqtt
#define mensajeConexion "1"
#define mensajeDesconexion "0"
#define topicoAlerta "/alert"
/*
   Tiempos de lectura y envio de datos
*/
#define tiempoLecturadht 2 // segundos
#define tiempoEnvioDatos 60 // segundos
/*
   Definicion de pines
*/
#define pinPIR D2
#define pinGas D3
#define DHTPIN D4
/*
  Definicion de tipo de sensor de temperatura
*/
#define DHTTYPE DHT22     // DHT 22 (AM2302)
/*
  Objetos de conexion WiFi y UDP
*/
WiFiClient clienteWifi;
PubSubClient client(clienteWifi);
WiFiUDP ntpUDP;
/*
   Objeto NTP, timestamp
*/
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", -3600 * 6, 60000);
/*
  Variables
*/

String nodeID = String(ESP.getChipId()); //identificador unico de cada dispositivo
boolean mensajeEnviado = false;

long tiempoPasadoEnvioDatos = 0;
long tiempoPasadoEnvioDatosDht = 0;
long intentoDeReconexion = 0;

int temperatura = 0;
int humedad = 0;
int temperatura2 = 0;
int humedad2 = 0;
DHT_Unified dht(DHTPIN, DHTTYPE);

int temValor1 = 30;
int temValor2 = 10;
int humValor1 = 90;
int humValor2 = 30;

int estadoPIR = LOW;
boolean val = 0;

int estadoGas = LOW;
boolean valGas = 0;



void portalCaptivo() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("HackerspaceX");
  Serial.println("connected...yeey :)");

}

void portalCaptivoBoton() {
  if ( digitalRead(botonModoAP) == LOW ) {
    //WiFiManager
    WiFiManager wifiManager;

    if (!wifiManager.startConfigPortal("HackerspaceX")) {
      delay(3000);
      ESP.reset();
      delay(5000);
    }
    Serial.println("connected...yeey :)");
  }

}


void callback(char* topic, byte* payload, unsigned int length) {

}


boolean reconnect() {
  String clienteString = "Cliente-" + nodeID;
  char clienteId[sizeof(clienteString)];
  clienteString.toCharArray(clienteId, sizeof(clienteId));
  if (client.connect(clienteId)) {
    Serial.println("Conectado");

    client.subscribe(topicoActualizacion);
    client.subscribe(topicoAlerta);
    client.publish(topicoConexion, mensajeConexion);
    Serial.println("Conectado al mqtt");
  } else {
    Serial.print("Fallo, error=");
    Serial.print(client.state());
    Serial.println(" Intentando en 5 segundos");
    delay(5000);
  }
  return client.connected();
}



void envioDatos() {
  unsigned long tiempoDatosActual = millis();
  // se multiplica por mil los segundos porque el tiempo esta dado en milisegundos
  if (tiempoDatosActual - tiempoPasadoEnvioDatos >= tiempoEnvioDatos * 1000) {   
    tiempoPasadoEnvioDatos = tiempoDatosActual;
    String datos = nodeID + ",";
    datos += "h" + String(humedad) + ",";
    datos += "t" + String(temperatura) + ",";
    Serial.println(datos);
    char datosChar[40];
    datos.toCharArray(datosChar, 40);
    //Serial.println(datosChar);
    client.publish(topicoActualizacion, datosChar);
  }
}

void alertas(char* mensaje, int valor ) {
  if (!mensajeEnviado) {
    String datoString = nodeID + "," + mensaje + "," + valor + "," + String(timeClient.getEpochTime());
    char dato [datoString.length()];
    datoString.toCharArray(dato, datoString.length());
    Serial.println(dato);
    client.publish(topicoAlerta, dato);
    mensajeEnviado = true;
  }
}


void loopDht() {

  unsigned long tiempoDatosActualDht = millis();
  // se multiplica por mil los segundos porque el tiempo esta dado en milisegundos
  if (tiempoDatosActualDht - tiempoPasadoEnvioDatosDht >= tiempoLecturadht * 1000) {
    tiempoPasadoEnvioDatosDht = tiempoDatosActualDht;
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      Serial.println("Error lectura de temperatura!");
    }
    else {
      Serial.print("Temperatura: ");
      Serial.print(event.temperature);
      Serial.println(" *C");
      temperatura = event.temperature;
    }
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println("Error leyendo humedad!");
    }
    else {
      Serial.print("Humedad: ");
      Serial.print(event.relative_humidity);
      Serial.println("%");
      humedad = event.relative_humidity;
    }
    if (temperatura > temValor1 ) {
      temperatura2 = temperatura;
      mensajeEnviado = false;
      alertas("temperatura Alta", temperatura);
    }
    if (temperatura < temValor2 ) {
      temperatura2 = temperatura;
      mensajeEnviado = false;
      alertas("temperatura Baja", temperatura);
    }
    if (humedad > humValor1 ) {
      humedad2 = humedad;
      mensajeEnviado = false;
      alertas("Humedad Alta", humedad);
    }
    if (humedad < humValor2 ) {
      humedad2 = humedad;
      mensajeEnviado = false;
      alertas("Humedad Baja", humedad);
    }
  }
}


boolean loopPir() {

  val = digitalRead(pinPIR);
  if (val == LOW) {
    digitalWrite(LED_BUILTIN, HIGH);
    if (estadoPIR == HIGH) {
      Serial.println("Movimiento Detectado");
      estadoPIR = LOW;
      mensajeEnviado = false;
      return true;
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    if (estadoPIR == LOW) {
      Serial.println("Movimiento terminado ");
      estadoPIR = HIGH;
      return false;
    }
  }
}
boolean loopGas() {
  valGas = digitalRead(pinGas);
  if (valGas == LOW) {
    digitalWrite(LED_BUILTIN, HIGH);
    if (estadoGas == HIGH) {
      Serial.println("Humo detectado");
      mensajeEnviado = false;
      estadoGas = LOW;
      return true;
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW); 
    if (estadoGas == LOW) {
      estadoGas = HIGH;
      Serial.println("Fin deteccion de humo");
      return false;
    }
  }
}


void setup()
{
  Serial.begin(115200);
  delay(2000);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  Serial.println("iniciando programa " );
  pinMode(botonModoAP, INPUT_PULLUP);
  pinMode(pinPIR, INPUT_PULLUP);
  pinMode(pinGas, INPUT_PULLUP);
  digitalWrite(botonModoAP, HIGH);
  portalCaptivo();
  client.setServer(servidorMQTT, puertoMQTT);
  client.setCallback(callback);
  intentoDeReconexion = 0;
  Serial.println(nodeID);
  dht.begin();
}


void loop()
{
  portalCaptivoBoton();
  if (!client.connected()) {
    long ahora = millis();
    if (ahora - intentoDeReconexion > 5000) {
      intentoDeReconexion = ahora;
      if (reconnect()) {
        intentoDeReconexion = 0;
      }
    }
  } else {
    client.loop();
    if (loopPir()) {
      alertas("PIR", 1);
    }
    if (loopGas()) {
      alertas("Gas", 1);
    }
    loopDht();
    envioDatos();
  }
}
