#include <RF24.h>
#include <RF24_config.h>
#include <nRF24L01.h>
#include <printf.h>

#define CE_PIN 7
#define CSN_PIN 8
#define RTS 0
#define CTS 1
#define DATA 2
#define ACK 3
#define CTSACK_TIMEOUT_MS 20
#define MYIP 48
// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);

// Let these addresses be used for the pair
uint64_t address[2] = { 0x3030303030LL, 0x3030303030LL };

void setup() {

  Serial.begin(115200);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}// hold in infinite loop
  }

  // because these examples are likely run with nodes in close proximity to
  // each other.
  radio.setPALevel(RF24_PA_MAX);// RF24_PA_MAX is default.
  radio.setChannel(30);
  radio.setPayloadSize(5);
  radio.setAutoAck(false);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setDataRate(RF24_250KBPS);
  // set the TX address of the RX node into the TX pipe
  radio.openWritingPipe(address[0]);

  // set the RX address of the TX node into a RX pipe
  radio.openReadingPipe(0,address[1]);

  // For debugging info
   printf_begin();             // needed only once for printing details
   radio.printDetails();       // (smaller) function that prints raw register values
   radio.printPrettyDetails(); // (larger) function that prints human readable data

}  // setup

void printPacote(byte *pac, int tamanho){
  Serial.print(F("Rcvd "));
  Serial.print(tamanho);  // print the size of the payload
  Serial.print(F(" Destino: "));
  Serial.print(pac[0]);  // print the payload's value
  Serial.print(F(" Origem: "));
  Serial.print(pac[1]);  // print the payload's value
  Serial.print(F(" Controle: "));
  Serial.print(pac[2]);  // print the payload's value
  Serial.print(F(" Dados: "));
  Serial.print(pac[3]);  // print the payload's value
  Serial.print(F(" : "));
  for(unsigned int i=4;i<tamanho;i++){
    Serial.print(pac[i]);
    Serial.print(" ");
  }
  Serial.println();  // print the payload's value
}

// Configura o inicio do payload
void configPayload(byte* payload, byte destino, byte controle){
  payload[0] = destino;// Colocamos no byte de destino o destino do payload
  payload[1] = MYIP;// Colocamos no byte de origem a origem do pacote que queremos enviar(nós)
  payload[2] = controle;// Colocamos no byte de controle o valor que recebemos
}

// Retorna se tem alguma coisa no buffer de entrada
bool carrierSense(){
  radio.flush_rx();// Limpa o buffer de entrada
  radio.startListening();// Enviamos um sinal para a antena começar a ouvir o meio
  delayMicroseconds(500);// Esperamos 200 microsegundos, para a antena fazer a ação pedida
  radio.stopListening();// Enviamos um sinal para a antena parar de ouvir o meio
  return radio.testCarrier(); // Retornamos se tem onda de rádio na antena
}

// Retorna se recebemos uma resposta CTSACK do destino
bool confirmacaoCTSACK(byte destino, byte controle){
  radio.startListening();// Enviamos um sinal para a antena começar a ouvir o meio
  delayMicroseconds(100);// Esperamos 100 microsegundos, para a antena fazer a ação pedida
  
  byte respostaCTSACK[3];// Variável para receber a resposta CTSACK

  bool recebido = false;// Variável indicando se recebemos a resposta CTSACK
  bool timeout = false;// Variável indicando se o limite de tempo de espera da resposta CTSACK acabou
  unsigned long int tempo = millis();// Variável que recebe o tempo que começamos a ver se a resposta CTSACK chegou

  while(!(timeout || recebido)) {// Enquanto não acabou o limite de tempo nem foi recebido a resposta CTSACK
    if(radio.available()) {// Verificamos se tem algo para ler no meio
      radio.read(&respostaCTSACK[0], 3);// Lemos um determinado tamanho de bytes da rede para um vetor de bytes(resposta)

      if(respostaCTSACK[0] == MYIP && respostaCTSACK[1] == destino && respostaCTSACK[2] == controle) {// Se a resposta é para nós e foi enviada do destino e é um CTSACK
        Serial.println("Confirmacao recebida");// Imprimimos que a confirmação foi recebida
        recebido = true;// Colocamos que recebemos a resposta CTSACK
      }
    }
    timeout = millis()-tempo > CTSACK_TIMEOUT_MS;// Colocamos que o limite de tempo de espera da resposta CTSACK acabou
    //quando o (tempo atual) - (tempo que começamos a ver se a resposta CTSACK chegou) for maior que o CTSACK_TIMEOUT_MS(limite de tempo de espera)
  }
  return recebido;// Retornamos se recebemos resposta CTSACK
}

// Envia um pacote RTS e espera um CTS
byte enviaRTS(byte destino){
  byte respostaRTS[3];// Variável para enviar a solitação RTS
  unsigned int tentativas = 0;// Variável de controle para saber quantas tentativas foram feitas
  configPayload(respostaRTS, destino, RTS);// Configuramos os bits de controle do pacote
  byte chegou_CTS = 0;
  do{
    delayMicroseconds(200);// Esperamos 200 microsegundos, para enviar o RTS
    if(!carrierSense()){
      radio.stopListening();// Enviamos um sinal para a antena parar de ouvir o meio
      delayMicroseconds(100);// Esperamos 100 microsegundos, para a antena fazer a ação pedida
      radio.flush_tx();// Limpa o buffer de saída
      if(!radio.write(&respostaRTS[0], 3)){// Colocamos o pacote que queremos enviar no buffer da antena
        Serial.println("Falha no envio do RTS");// Imprimimos que ocorreu uma falha na transmissão
        continue;
      }
    }
    tentativas++;
    chegou_CTS = confirmacaoCTSACK(destino, CTS);
  }while(!chegou_CTS && tentativas < 3);
  return chegou_CTS;
}

// Envia um pacote CTS
void enviaCTS(byte destino){
  byte respostaCTS[3];// Variável para enviar a resposta CTS
  unsigned long int tempo_de_espera = 10;// Tempo base que se espera se tem alguém transmitindo
  unsigned int tentativas = 0;// Variável de controle para saber quantas tentativas foram feitas
  configPayload(respostaCTS, destino, CTS);// Configuramos os bits de controle da resposta

  do{
    if(!carrierSense()){
      radio.stopListening();// Enviamos um sinal para a antena parar de ouvir o meio
      delayMicroseconds(100);// Esperamos 200 microsegundos, para a antena fazer a ação pedida
      radio.flush_tx();// Limpa o buffer de saída
      if (!radio.write(&respostaCTS[0], 3)){// Colocamos a resposta CTS no buffer da antena para tranmissão
        Serial.println("Falha no envio");// Imprimimos que ocorreu uma falha na transmissão
        continue;
      }
      Serial.println("CTS enviado");// Imprimimos que o CTS foi enviado
      break;// Saimos do do while
    } else {
      Serial.println("Meio ocupado");// Imprimimos que o meio esta ocupado
      delay(tempo_de_espera);// Esperamos o tempo definido na variável 'tempo_de_espera'
      tempo_de_espera *= 6;// Multiplicamos a variável 'tempo_de_espera' por 10
    }
    tentativas++;// Incrementamos o número de tentativas feitas
  }while(tentativas < 3);// Enquanto não foi confirmado o pacote e não estorou o limite de tentivas, continuamos tentando enviar o pacote
}

byte enviaDADO(byte* pacote, unsigned int tamanho, byte destino){
  configPayload(pacote, destino, DATA);// Configuramos os bits de controle do pacote
  printPacote(pacote, tamanho);
  unsigned long int tempo_de_espera = 10;// Tempo base que se espera se tem alguém transmitindo
  unsigned int tentativas = 0;// Variável de controle para saber quantas tentativas foram feitas
  byte chegou_ACK = 0;
  do{
    if(!carrierSense()){
      radio.stopListening();// Enviamos um sinal para a antena parar de ouvir o meio
      delayMicroseconds(100);// Esperamos 100 microsegundos, para a antena fazer a ação pedida
      radio.flush_tx();// Limpa o buffer de saída
      if(!radio.write(&pacote[0], tamanho)){// Colocamos o pacote que queremos enviar no buffer da antena
        Serial.println("Falha no envio dos DADOS");// Imprimimos que ocorreu uma falha na transmissão
        continue;
      }
    } else {
      delay(tempo_de_espera);// Esperamos o tempo definido na variável 'tempo_de_espera'
      tempo_de_espera *= 6;// Multiplicamos a variável 'tempo_de_espera' por 6
      Serial.print("Tentativa de ver o meio ");// Imprimimos para dizer que esta sendo realizada uma tentativa de ver o meio
    }
    tentativas++;// Incrementamos o número de tentativas feitas
    chegou_ACK = confirmacaoCTSACK(destino, ACK);
  }while(!chegou_ACK && tentativas < 10);// Enquanto não foi confirmado o pacote e não estorou o limite de tentivas, continuamos tentando enviar o pacote
  Serial.print("Número de tentativas de envio: ");// Imprimimos para dizer que esta sendo realizada uma tentativa de envio
  Serial.println(tentativas);// Imprimimos o numero de tentativas
  Serial.println(" ");// Imprimimos um ' ' para pular uma linha

  return chegou_ACK;
}

// Envia um pacote de ack para o destino determinado
void enviaACK(byte destino){
  byte respostaACK[3];// Vetor de bytes representando a resposta ACK
  
  configPayload(respostaACK, destino, ACK);// Configuramos os bits de controle da resposta

  unsigned long int tempo_de_espera = 10;// Tempo base que se espera se tem alguém transmitindo
  unsigned int tentativas = 0;// Variável de controle para saber quantas tentativas foram feitas
  
  do{
    if(!carrierSense()){
      radio.stopListening();// Enviamos um sinal para a antena parar de ouvir o meio
      delayMicroseconds(100);// Esperamos 200 microsegundos, para a antena fazer a ação pedida
      radio.flush_tx();// Limpa o buffer de saída
      if (!radio.write(&respostaACK[0], 3)){// Colocamos a resposta ACK no buffer da antena para tranmissão
        Serial.println("Falha no envio");// Imprimimos que ocorreu uma falha na transmissão
        continue;
      }
      Serial.println("ACK enviado");// Imprimimos que o ACK foi enviado
      break;// Saimos do do while
    } else {
      Serial.println("Meio ocupado");// Imprimimos que o meio esta ocupado
      delay(tempo_de_espera);// Esperamos o tempo definido na variável 'tempo_de_espera'
      tempo_de_espera *= 6;// Multiplicamos a variável 'tempo_de_espera' por 10
    }
    tentativas++;// Incrementamos o número de tentativas feitas
  }while(tentativas < 3);// Enquanto não foi confirmado o pacote e não estorou o limite de tentivas, continuamos tentando enviar o pacote
};

// Envia um pacote de determinado tamanho para um certo destino
void envia(byte* pacote, unsigned int tamanho, byte destino){
  byte RTS_CTS = enviaRTS(destino);
  if(!RTS_CTS){
    Serial.println("Não recebemos o CTS");// Imprimimos que não recebemos o CTS
    return;
  }
  Serial.println("Recebemos o CTS");// Imprimimos que recebemos o CTS
  byte DADO_ACK = enviaDADO(pacote, tamanho, destino);
  if(!DADO_ACK){
    Serial.println("Não recebemos o ACK");// Imprimimos que não recebemos o ACK
    return;
  }
  Serial.println("Recebemos o ACK");// Imprimimos que não recebemos o ACK
}

// Recebe um pacote de dados de um determinado tamanho e envia um ack
void receber(byte* pacote, int tamanho){
  radio.startListening();// Enviamos um sinal para a antena começar a ouvir o meio
  delayMicroseconds(100);// Esperamos 100 microsegundos, para a antena fazer a ação pedida
  byte RTS_recebido = 0;

  unsigned int tentativas = 0;// Variável de controle para saber quantas tentativas foram feitas

  do{
    if(radio.available()) {// Verificamos se tem algo para ler no meio
      radio.read(&pacote[0], tamanho);// Lemos um determinado tamanho de bytes da rede para um vetor de bytes(pacote)
      radio.flush_rx();// Limpa o buffer de entrada
      if((pacote[0] == MYIP) && (pacote[2] == RTS)) {// Verficamos se o pacote recebido é para nós e se é RTS
        RTS_recebido = 1;
        enviaCTS(pacote[1]);// Enviamos o CTS

        Serial.println("Pacote Recebido:");// Imprimimos na tela que o pacote foi recebido
        printPacote(&pacote[0], tamanho);// Imprimimos na tela o pacote recebido
      }
    }
    tentativas++;// Incrementamos o número de tentativas feitas
  }while(tentativas < 3);

  if(!RTS_recebido)
    return;

  tentativas = 0;

  do{
    if(radio.available()) {// Verificamos se tem algo para ler no meio
      radio.read(&pacote[0], tamanho);// Lemos um determinado tamanho de bytes da rede para um vetor de bytes(pacote)
      radio.flush_rx();// Limpa o buffer de entrada
      if((pacote[0] == MYIP) && (pacote[2] == DATA)) {// Verficamos se o pacote recebido é para nós e se é dado
        enviaACK(pacote[1]);// Enviamos o ACK

        Serial.println("Pacote Recebido:");// Imprimimos na tela que o pacote foi recebido
        printPacote(&pacote[0], tamanho);// Imprimimos na tela o pacote recebido
      }
    }
    tentativas++;// Incrementamos o número de tentativas feitas
  }while(tentativas < 3);
}

byte payload[5] = {0, 0, 0, 0, 0};// Vetor de 5 bytes denominado payload

void loop() {
  // envia(&payload[0], 5, 37, DATA);// Enviamos o payload com tamanho 5 para o endereço 37 com dados
  // payload[3] += 1; // Incrementamos o 4 byte do payload para checagem de códia do outro lado
  // delay(500); // Esperamos 500 milissegundos para enviar outro pacote
  // receber(&payload[0], 5);// Recebemos
}
