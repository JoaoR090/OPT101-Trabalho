#include <Arduino.h>

extern const byte MYIP;

void setupRadio();
void envia(byte* pacote, unsigned int tamanho, byte destino);
void receber(byte* pacote, int tamanho);
