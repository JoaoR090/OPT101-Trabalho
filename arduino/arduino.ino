#include "radio.h"
#include "sensor_distancia.h"

const byte MYIP = 48;
const byte MYID = 1;

void setup() {

  Serial.begin(115200);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  setupRadio();
  setupSensor();
}  // setup

byte payload[5] = {0, 0, 0, 0, 0};// Vetor de 5 bytes denominado payload

void loop() {
  //receber(&payload[0], 5);// Recebemos
  float distancia_objeto = distancia_objeto()
  if (distancia_objeto < 100){
    payload[3] = MYID
    payload[4] = (byte)distancia_objeto
    envia(&payload[0], 5, 23);// Enviamos o payload com tamanho 5 para o endereço 37 com dados
  }
  delay(50);
}
