#include "sensor_distancia.h"

#define PINO_TRIGGER 4
#define PINO_ECHO 5

void setupSensor() {
   pinMode(PINO_TRIGGER, OUTPUT);
   pinMode(PINO_ECHO, INPUT);

}  // setup

float distancia_objeto() {

  // Envia um pulso de 10us no Trigger
  digitalWrite(PINO_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PINO_TRIGGER, LOW);

  // Lê o tempo de retorno do pulso no Echo
  long duracao = pulseIn(PINO_ECHO, HIGH);

  // Converte o tempo em distância (cm) * 10
  float distancia = duracao * 0.034 / 2 / 10;

  return distancia;
}