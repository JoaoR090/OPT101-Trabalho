#include <RF24.h>
#include <RF24_config.h>
#include <nRF24L01.h>
#include <printf.h>

#define CE_PIN 7
#define CSN_PIN 8
#define DATA 0
#define ACK 1
#define TIMEOUT 1000000
#define MYIP 23
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
    while (1) {}  // hold in infinite loop
  }

  // because these examples are likely run with nodes in close proximity to
  // each other.
  radio.setPALevel(RF24_PA_MAX);  // RF24_PA_MAX is default.
  radio.setChannel(100);
  // save on transmission time by setting the radio to only transmit the
  // number of bytes we need to transmit a float
  radio.setPayloadSize(5);  // float datatype occupies 4 bytes
  radio.setAutoAck(false);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setDataRate(RF24_250KBPS);
  // set the TX address of the RX node into the TX pipe
  radio.openWritingPipe(address[0]);  // always uses pipe 0

  // set the RX address of the TX node into a RX pipe
  radio.openReadingPipe(0,address[1]);  // using pipe 1

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
  for(int i=4;i<tamanho;i++){
    Serial.print(pac[i]);
    Serial.print(" ");
  }
  Serial.println();  // print the payload's value
}

// Configura o inicio do payload
void config_payload(byte* payload, byte destino, byte controle){
  payload[0] = destino;// Colocamos no byte de destino o destino do payload
  payload[1] = MYIP;// Colocamos no byte de origem a origem do pacote que queremos enviar(nós)
  payload[2] = controle;// Colocamos no byte de controle o valor que recebemos
}

// Retorna se recebemos uma resposta ACK do destino
bool confirmacao(byte destino){
  radio.startListening();// Enviamos um sinal para a antena começar a ouvir o meio
  delayMicroseconds(100);// Esperamos 100 microsegundos, para a antena fazer a ação pedida
  
  byte resposta[3];// Variável para receber a resposta ACK
  bool recebido = false;// Variável indicando se recebemos a resposta ACK
  bool timeout = false;// Variável indicando se o limite de tempo de espera da resposta ACK acabou
  size_t tempo = millis();// Variável que recebe o tempo que começamos a ver se a resposta ACK chegou

  while(!(timeout || recebido)) {// Enquanto não acabou o limite de tempo nem foi recebido a resposta ACK
    if(radio.available()) {// Verificamos se tem algo para ler no meio
      radio.read(&resposta[0], 3);// Lemos um determinado tamanho de bytes da rede para um vetor de bytes(resposta)

      if(resposta[0] == MYIP && resposta[1] == destino && resposta[2] == ACK) {// Se a resposta é para nós e foi enviada do destino e é um ACK
        Serial.println("Confirmacao recebida");// Imprimimos que a confirmação foi recebida
        Serial.println(" ");// Imprimimos um ' ' para pular uma linha
        recebido = true;// Colocamos que recebemos a resposta ACK
      }
    }
    timeout = millis()-tempo > TIMEOUT;// Colocamos que o limite de tempo de espera da resposta ACK acabou
    //quando o (tempo atual) - (tempo que começamos a ver se a resposta ACK chegou) for maior que o TIMEOUT(limite de tempo de espera)
  }
  return recebido;// Retornamos se recebemos resposta ACK
}

// Retorna se tem alguma coisa no buffer de entrada
bool carrier_sense(){
  radio.flush_rx(); // Limpa o buffer de entrada
  radio.startListening();// Enviamos um sinal para a antena começar a ouvir o meio
  delayMicroseconds(200);// Esperamos 200 microsegundos, para a antena fazer a ação pedida
  radio.stopListening();// Enviamos um sinal para a antena parar de ouvir o meio
  delayMicroseconds(200);// Esperamos 200 microsegundos, para a antena fazer a ação pedida
  return radio.testCarrier(); // Retornamos se tem alguma coisa nu buffer de entrada
}

// Envia um pacote de determinado tamanho para um certo destino
void envia(byte* pacote, int tamanho, byte destino, byte controle){
  // Configuramos os bits de controle do pacote
  config_payload(pacote, destino, controle);

  unsigned long int tempo_de_espera = 10;
  unsigned int tentativas = 0;// Variável de controle para saber quantas tentativas foram feitas
  do{
    if(!carrier_sense()){
      radio.write(&pacote[0], 5);// Colocamos o pacote que queremos enviar no buffer da antena
      delayMicroseconds(300);// Esperamos 300 microsegundos, para a antena enviar o pacote que queremos enviar
      Serial.print("Tentativa de envio ");// Imprimimos para dizer que esta sendo realizada uma tentativa de envio
    } else {
      delay(tempo_de_espera);
      tempo_de_espera *= 10;
      Serial.print("Tentativa de ver o meio ");// Imprimimos para dizer que esta sendo realizada uma tentativa de ver o meio
    }

    Serial.println(tentativas);// Imprimimos o numero de tentativas

    tentativas ++;// Incrementamos o número de tentativas feitas
  }while(!confirmacao(destino) && tentativas < 15);// Enquanto não foi confirmado o pacote e não estorou o limite de tentivas, continuamos tentando enviar o pacote

  Serial.println(" ");// Imprimimos um ' ' para pular uma linha
}

// Envia um pacote de ack para o destino determinado
void envia_ack(byte destino){
  byte resposta_ACK[3];// Vetor de bytes representando a resposta ACK

  config_payload(resposta_ACK, destino, ACK);// Configuramos os bits de controle da resposta

  unsigned long int tempo_de_espera = 10;

  if(!carrier_sense()){
    radio.write(&resposta_ACK[0], 3);// Colocamos a resposta ACK no buffer da antena
    delayMicroseconds(300);// Esperamos 300 microsegundos, para a antena enviar o pacote que queremos enviar
  } else {
    delay(tempo_de_espera);
    tempo_de_espera *= 10;
  }

  Serial.println("ACK enviado");// Imprimimos que o ACK foi enviado
  Serial.println(" ");// Imprimimos um ' ' para pular uma linha
};

// Recebe um pacote de dados de um determinado tamanho e envia um ack
void receber(byte* pacote, int tamanho){
  radio.startListening();// Enviamos um sinal para a antena começar a ouvir o meio
  delayMicroseconds(100);// Esperamos 100 microsegundos, para a antena fazer a ação pedida

  if(radio.available()) {// Verificamos se tem algo para ler no meio
    radio.read(&pacote[0], tamanho);// Lemos um determinado tamanho de bytes da rede para um vetor de bytes(pacote)
    if((pacote[0] == MYIP) && (pacote[2] == DATA)) {// Verficamos se o pacote recebido é para nós e se é dado
      envia_ack(pacote[1]);// Enviamos o ACK

      Serial.println("Pacote Recebido:");// Imprimimos na tela que o pacote foi recebido
      printPacote(&pacote[0], tamanho);// Imprimimos na tela o pacote recebido
    }
  }
}

void loop() {
  byte payload[5]; // teste
  envia(&payload[0], 5, 37, DATA);
  delay(1000);
  // receber(&payload[0], 5);

}  // loop
