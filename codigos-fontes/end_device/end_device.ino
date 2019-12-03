/*
* End-device LoRa de medição de temperatura
* Projeto apresentado na palestra "O papel da tecnologia LoRa na Internet das Coisas",
* ministrada no LHC (Laboratório Hacker de Campinas) dia 05/12/2019
* Autor: Pedro Bertoleti
*/
#include <DHT.h>
#include <Wire.h>
#include <LoRa.h>
#include <SPI.h>

/*
 * Defines do projeto
 */
/* GPIO do módulo WiFi LoRa 32(V2) que o pino de comunicação do sensor está ligado. */
#define DHTPIN    13 /* (GPIO 13) */

/* Baudrate da serial de debug */
#define BAUDRATE_SERIAL_DEBUG       115200

/* Tempo entre medições de temperatura */
#define TEMPO_LEITURA_TEMP          5000//ms

/*
A biblioteca serve para os sensores DHT11, DHT22 e DHT21. 
No nosso caso, usaremos o DHT22, porém se você desejar utilizar 
algum dos outros disponíveis, basta descomentar a linha correspondente.
*/
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

/* Definicoes para comunicação com radio LoRa */
#define SCK_LORA           5
#define MISO_LORA          19
#define MOSI_LORA          27
#define RESET_PIN_LORA     14
#define SS_PIN_LORA        18
#define HIGH_GAIN_LORA     20  /* dBm */
#define BAND               915E6  /* 915MHz de frequencia */

/*
 * Variáveis e objetos globais
 */
/* objeto para comunicação com sensor DHT22 */
DHT dht(DHTPIN, DHTTYPE);

/* variáveis que armazenam os valores máximo e mínimo de temperatura registrados. */
float temperatura_max=0.0;
float temperatura_min=0.0;

/* typedefs */
typedef struct __attribute__((__packed__))  
{
  float temperatura;
  float umidade;
  float temperatura_min;
  float temperatura_max;
}TDadosLora;

/* prototypes */
void atualiza_temperatura_max_e_minima(float temp_lida);
void envia_medicoes_para_serial(float temp_lida, float umid_lida);
void envia_informacoes_lora(float temp_lida, float umid_lida);
bool init_comunicacao_lora(void);

/*
 * Implementações
 */

 
/* Função: verifica se os valores de temperatura máxima e mínima devem ser atualizados
 * Parâmetros: temperatura lida
 * Retorno: nenhum
 */  
void atualiza_temperatura_max_e_minima(float temp_lida)
{
  if (temp_lida > temperatura_max)
    temperatura_max = temp_lida;

  if (temp_lida < temperatura_min)
    temperatura_min = temp_lida;  
}
 
/* Função: envia, na forma de mensagens textuais, as medições para a serial
 * Parâmetros: - Temperatura lida
 *             - Umidade relativa do ar lida
 *             - Máxima temperatura registrada
 *             - Mínima temperatura registrada
 * Retorno: nenhum
*/ 
void envia_medicoes_para_serial(float temp_lida, float umid_lida) 
{
  char mensagem[200];
  char i;

  /* pula 80 linhas, de forma que no monitor serial seja exibida somente as mensagens atuais (impressao de refresh de tela) */
  for(i=0; i<80; i++)
      Serial.println(" ");

  /* constrói mensagens e as envia */
  /* - temperatura atual */
  memset(mensagem,0,sizeof(mensagem));
  sprintf(mensagem,"- Temperatura: %.2f C", temp_lida);
  Serial.println(mensagem);
  
  //- umidade relativa do ar atual
  memset(mensagem,0,sizeof(mensagem));
  sprintf(mensagem,"- Umidade atual: %.2f/100",umid_lida);
  Serial.println(mensagem);
  
  //- temperatura maxima
  memset(mensagem,0,sizeof(mensagem));
  sprintf(mensagem,"- Temperatura maxima: %.2f C", temperatura_max);
  Serial.println(mensagem); 
  
  //- temperatura minima
  memset(mensagem,0,sizeof(mensagem));
  sprintf(mensagem,"- Temperatura minima: %.2f C", temperatura_min);
  Serial.println(mensagem);
}

/* 
 * Função: envia por LoRa as informações de temperatura e umidade lidas, assim como as temperaturas máxima e mínima
 * Parâmetros: - Temperatura lida
 *             - Umidade relativa do ar lida
 * Retorno: nenhum
 */
void envia_informacoes_lora(float temp_lida, float umid_lida)
{
    TDadosLora dados_lora;

    dados_lora.temperatura = temp_lida;
    dados_lora.umidade = umid_lida;
    dados_lora.temperatura_min = temperatura_min;
    dados_lora.temperatura_max = temperatura_max;

    LoRa.beginPacket();
    LoRa.write((unsigned char *)&dados_lora, sizeof(TDadosLora));
    LoRa.endPacket();
}


/* Funcao: inicia comunicação com chip LoRa
 * Parametros: nenhum
 * Retorno: true: comunicacao ok
 *          false: falha na comunicacao
*/
bool init_comunicacao_lora(void)
{
    bool status_init = false;
    Serial.println("[LoRa Sender] Tentando iniciar comunicacao com o radio LoRa...");
    SPI.begin(SCK_LORA, MISO_LORA, MOSI_LORA, SS_PIN_LORA);
    LoRa.setPins(SS_PIN_LORA, RESET_PIN_LORA, LORA_DEFAULT_DIO0_PIN);
    
    if (!LoRa.begin(BAND)) 
    {
        Serial.println("[LoRa Sender] Comunicacao com o radio LoRa falhou. Nova tentativa em 1 segundo...");        
        delay(1000);
        status_init = false;
    }
    else
    {
        /* Configura o ganho do receptor LoRa para 20dBm, o maior ganho possível (visando maior alcance possível) */ 
        LoRa.setTxPower(HIGH_GAIN_LORA); 
        Serial.println("[LoRa Sender] Comunicacao com o radio LoRa ok");
        status_init = true;
    }

    return status_init;
}

void setup() 
{
    /* configura comunicação serial (para enviar mensgens com as medições) 
    e inicializa comunicação com o sensor. 
    */
    Serial.begin(BAUDRATE_SERIAL_DEBUG);  
    dht.begin();
    delay(500);

    /* inicializa temperaturas máxima e mínima com a leitura inicial do sensor */
    do
    {
        temperatura_max = dht.readTemperature();
        temperatura_min = temperatura_max;
    }while (isnan(temperatura_max));

    /* Tenta, até obter sucesso, comunicacao com o chip LoRa */
    while(init_comunicacao_lora() == false);    
}

/*
 * Programa principal
 */
void loop() 
{
    float temperatura_lida;
    float umidade_lida;
  
    /* Faz a leitura de temperatura e umidade do sensor */
    temperatura_lida = dht.readTemperature();
    umidade_lida = dht.readHumidity();

    /* se houve falha na leitura do sensor, escreve mensagem de erro na serial */
    if ( isnan(temperatura_lida) || isnan(umidade_lida) ) 
        Serial.println("Erro ao ler sensor DHT22!");
    else
    {
        /*Se a leitura foi bem sucedida, ocorre o seguinte:
         - Os valores mínimos e máximos são verificados e comparados à medição atual de temperatura
           se a temperatura atual for menor que a mínima ou maior que a máxima até então
           registrada, os limites máximo ou mínimo são atualizados.
         - As medições (temperatura, umidade, máxima temperatura e mínima temperatura) são
           enviados pela serial na forma de mensagem textual. Tais mensagens podem ser vistas
           no monitor serial.
         - As medições (temperatura, umidade, máxima temperatura e mínima temperatura) são
           enviadas por LoRa para um módulo receptor
        */
        atualiza_temperatura_max_e_minima(temperatura_lida);
        envia_medicoes_para_serial(temperatura_lida, umidade_lida);
        envia_informacoes_lora(temperatura_lida, umidade_lida);
    }  

    /* Aguarda até a próxima leitura  */
    delay(TEMPO_LEITURA_TEMP);
}

