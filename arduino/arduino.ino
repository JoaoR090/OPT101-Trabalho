#include "radio.h"
#include "sensor_distancia.h"

const byte MYIP = 48;// IP da plaqunha temos as plaquinhas 23, 37 e 48
const byte MYID = 1;// 1 é o 48 e o 2 é o 37

void setup() {

  Serial.begin(115200);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  setupRadio();
  setupSensor();
}  // setup

byte payload[5] = {0, 0, 0, 0, 0};// Vetor de 5 bytes denominado payload

void envia_distancia(){
  float distancia_objeto = distancia_objeto()
  payload[3] = MYID
  payload[4] = (byte)distancia_objeto
  envia(&payload[0], 5, 23);// Enviamos o payload com tamanho 5 para o endereço 37 com dados
}

void loop() {
  //receber(&payload[0], 5);// Recebemos na plaquinha 23
  envia_distancia();
  delay(50);
}
