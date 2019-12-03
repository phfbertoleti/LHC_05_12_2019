/*
* Gateway LoRa de medição de temperatura
* Projeto apresentado na palestra "O papel da tecnologia LoRa na Internet das Coisas",
* ministrada no LHC (Laboratório Hacker de Campinas) dia 05/12/2019
* Autor: Pedro Bertoleti
*/
#include <WiFi.h>
#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* Definicoes para comunicação com radio LoRa */
#define SCK_LORA           5
#define MISO_LORA          19
#define MOSI_LORA          27
#define RESET_PIN_LORA     14
#define SS_PIN_LORA        18

#define HIGH_GAIN_LORA     20  /* dBm */
#define BAND               915E6  /* 915MHz de frequencia */

/* Definicoes do OLED */
#define OLED_SDA_PIN    4
#define OLED_SCL_PIN    15
#define SCREEN_WIDTH    128 
#define SCREEN_HEIGHT   64  
#define OLED_ADDR       0x3C 
#define OLED_RESET      16

/* Offset de linhas no display OLED */
#define OLED_LINE1     0
#define OLED_LINE2     10
#define OLED_LINE3     20
#define OLED_LINE4     30
#define OLED_LINE5     40
#define OLED_LINE6     50

/* Definicoes gerais */
#define DEBUG_SERIAL_BAUDRATE    115200

/* Definições - comunicação com Tago.io */
#define MQTT_PUB_TOPIC "tago/data/post"
#define MQTT_ID        "lhc_demo_iot_device_id"       
#define MQTT_USERNAME  "lhc_demo_iot_device"  /* Coloque aqui qualquer valor */
#define MQTT_PASSWORD  " "  /* coloque aqui o Device Token do seu dispositivo no Tago.io */

/* Variaveis e objetos globais */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* WIFI */
const char* ssid_wifi = " ";     /*  WI-FI network SSID (name) you want to connect */
const char* password_wifi = " "; /*  WI-FI network password */
WiFiClient espClient;     

/* MQTT */
const char* broker_mqtt = "mqtt.tago.io"; /* MQTT broker URL */
int broker_port = 1883;                      /* MQTT broker port */
PubSubClient MQTT(espClient); 

/* Prototypes */
void display_init(void);
bool init_comunicacao_lora(void);
void init_wifi(void);
void init_MQTT(void);
void connect_MQTT(void);
void connect_wifi(void);
void verify_wifi_MQTT_connection(void);
void send_data_iot_platform(float temp_atual, float temp_max, float temp_min, float umid);

/* typedefs */
typedef struct __attribute__((__packed__))  
{
  float temperatura;
  float umidade;
  float temperatura_min;
  float temperatura_max;
}TDadosLora;

/* Funcao: inicializa comunicacao com o display OLED
 * Parametros: nenhnum
 * Retorno: nenhnum
*/ 
void display_init(void)
{
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) 
    {
        Serial.println("[LoRa Receiver] Falha ao inicializar comunicacao com OLED");        
    }
    else
    {
        Serial.println("[LoRa Receiver] Comunicacao com OLED inicializada com sucesso");
    
        /* Limpa display e configura tamanho de fonte */
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
    }
}

/* Funcao: inicia comunicação com chip LoRa
 * Parametros: nenhum
 * Retorno: true: comunicacao ok
 *          false: falha na comunicacao
*/
bool init_comunicacao_lora(void)
{
    bool status_init = false;
    Serial.println("[LoRa Receiver] Tentando iniciar comunicacao com o radio LoRa...");
    SPI.begin(SCK_LORA, MISO_LORA, MOSI_LORA, SS_PIN_LORA);
    LoRa.setPins(SS_PIN_LORA, RESET_PIN_LORA, LORA_DEFAULT_DIO0_PIN);
    
    if (!LoRa.begin(BAND)) 
    {
        Serial.println("[LoRa Receiver] Comunicacao com o radio LoRa falhou. Nova tentativa em 1 segundo...");        
        delay(1000);
        status_init = false;
    }
    else
    {
        /* Configura o ganho do receptor LoRa para 20dBm, o maior ganho possível (visando maior alcance possível) */ 
        LoRa.setTxPower(HIGH_GAIN_LORA); 
        Serial.println("[LoRa Receiver] Comunicacao com o radio LoRa ok");
        status_init = true;
    }

    return status_init;
}

/* Funcao: inicializa conexao wi-fi
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void init_wifi(void) 
{
    delay(10);
    Serial.println("------WI-FI -----");
    Serial.print("Tentando se conectar a rede wi-fi ");
    Serial.println(ssid_wifi);
    Serial.println("Aguardando conexao");    
    connect_wifi();
}

/* Funcao: inicializa variaveis do MQTT para conexao com broker
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void init_MQTT(void)
{
    MQTT.setServer(broker_mqtt, broker_port);
}

/* Funcao: conecta com broker MQTT (se nao ha conexao ativa)
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void connect_MQTT(void) 
{
    while (!MQTT.connected()) 
    {
        Serial.print("* Tentando se conectar ao broker MQTT: ");
        Serial.println(broker_mqtt);
        
        if (MQTT.connect(MQTT_ID, MQTT_USERNAME, MQTT_PASSWORD)) 
        {
            Serial.println("Conectado ao broker MQTT com sucesso!");
        } 
        else 
        {
            Serial.println("Falha na tentativa de conexao com broker MQTT.");
            Serial.println("Nova tentativa em 2s...");
            delay(2000);
        }
    }
}

/* Funcao: conexao a uma rede wi-fi
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void connect_wifi(void) 
{
    if (WiFi.status() == WL_CONNECTED)
        return;
        
    WiFi.begin(ssid_wifi, password_wifi);
    
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(100);
        Serial.print(".");
    }
  
    Serial.println();
    Serial.print("Conectado com sucesso a rede wi-fi: ");
    Serial.println(ssid_wifi);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

/* Funcao: verifica e garante conexao wi-fi e MQTT
 * Parametros: nenhum
 * Retorno: nenhum 
 */
void verify_wifi_MQTT_connection(void)
{
    connect_wifi(); 
    connect_MQTT();
}

/* Funcao: envia informacoes para plataforma IoT (Tago.io) via MQTT
 * Parametros: temperaturas (atual, max e min) e umidade relativa do ar atual
 * Retorno: nenhum
 */
void send_data_iot_platform(float temp_atual, float temp_max, float temp_min, float umid)
{
   StaticJsonDocument<250> tago_json_temperature;
   StaticJsonDocument<250> tago_json_temperature_max;
   StaticJsonDocument<250> tago_json_temperature_min;
   StaticJsonDocument<250> tago_json_humidity;
   char json_string[250] = {0};
   int i;

   /* Envio da temperatura */
   tago_json_temperature["variable"] = "temperatura";
   tago_json_temperature["unit"] = "C";
   tago_json_temperature["value"] = temp_atual;
   memset(json_string, 0, sizeof(json_string));
   serializeJson(tago_json_temperature, json_string);
   MQTT.publish(MQTT_PUB_TOPIC, json_string);

   /* Envio da temperatura máxima */
   tago_json_temperature_max["variable"] = "temperatura_max";
   tago_json_temperature_max["unit"] = "C";
   tago_json_temperature_max["value"] = temp_max;
   memset(json_string, 0, sizeof(json_string));
   serializeJson(tago_json_temperature_max, json_string);
   MQTT.publish(MQTT_PUB_TOPIC, json_string);

   /* Envio da temperatura mínima */
   tago_json_temperature_min["variable"] = "temperatura_min";
   tago_json_temperature_min["unit"] = "C";
   tago_json_temperature_min["value"] = temp_min;
   memset(json_string, 0, sizeof(json_string));
   serializeJson(tago_json_temperature_min, json_string);
   MQTT.publish(MQTT_PUB_TOPIC, json_string);

   /* Envio da umidade */
   tago_json_humidity["variable"] = "umidade";
   tago_json_humidity["unit"] = "%";
   tago_json_humidity["value"] = umid;
   memset(json_string, 0, sizeof(json_string));
   serializeJson(tago_json_humidity, json_string);
   MQTT.publish(MQTT_PUB_TOPIC, json_string);
}

/* Funcao de setup */
void setup() 
{
    /* Configuracao da I²C para o display OLED */
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    /* Display init */
    display_init();

    /* Print message telling to wait */
    display.clearDisplay();    
    display.setCursor(0, OLED_LINE1);
    display.print("Aguarde...");
    display.display();
    
    Serial.begin(DEBUG_SERIAL_BAUDRATE);
    while (!Serial);

    /* Tenta, até obter sucesso, comunicacao com o chip LoRa */
    while(init_comunicacao_lora() == false);     

    /* Inicializa wi-fi */
    init_wifi();

    /* Inicializa MQTT e faz conexao ao broker MQTT */
    init_MQTT();
    connect_MQTT();
}

/* Programa principal */
void loop() 
{
    char byte_recebido;
    int packet_size = 0;
    int lora_rssi = 0;
    TDadosLora dados_lora;
    char * ptInformaraoRecebida = NULL;
  
    /* Verifica se chegou alguma informação do tamanho esperado */
    packet_size = LoRa.parsePacket();
    
    if (packet_size == sizeof(TDadosLora)) 
    {
        Serial.println("[LoRa Receiver] Há dados a serem lidos");
        
        /* Recebe os dados conforme protocolo */               
        ptInformaraoRecebida = (char *)&dados_lora;  
        while (LoRa.available()) 
        {
            byte_recebido = (char)LoRa.read();
            *ptInformaraoRecebida = byte_recebido;
            ptInformaraoRecebida++;
        }

        /* Escreve RSSI de recepção e informação recebida */
        lora_rssi = LoRa.packetRssi();
        display.clearDisplay();   
        display.setCursor(0, OLED_LINE1);
        display.print("Temp: ");
        display.println(dados_lora.temperatura);

        display.setCursor(0, OLED_LINE2);
        display.print("Umid: ");
        display.println(dados_lora.umidade);

        display.setCursor(0, OLED_LINE3);
        display.println("Temp min/max:");
        display.setCursor(0, OLED_LINE4);
        display.print(dados_lora.temperatura_min);
        display.print("/");
        display.print(dados_lora.temperatura_max);

        display.display();      

        /* Garante conectividade wi-fi e MQTT */
        verify_wifi_MQTT_connection();
    
        /* Faz o envio das temperaturas e umidade para a plataforma IoT (Tago.io) */
        send_data_iot_platform(dados_lora.temperatura, 
                               dados_lora.temperatura_max, 
                               dados_lora.temperatura_min, 
                               dados_lora.umidade);
    }
}

