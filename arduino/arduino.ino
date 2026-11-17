#include "radio.h"

const byte MYIP = 48;

void setup() {

  Serial.begin(115200);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  setupRadio();
}  // setup

byte payload[5] = {0, 0, 0, 0, 0};// Vetor de 5 bytes denominado payload

void loop() {
  envia(&payload[0], 5, 23);// Enviamos o payload com tamanho 5 para o endereço 37 com dados
  payload[3] += 1; // Incrementamos o 4 byte do payload para checagem de códia do outro lado
  delay(500); // Esperamos 500 milissegundos para enviar outro pacote
  //receber(&payload[0], 5);// Recebemos
}
