// Sketch feito para a disciplina de DCC091
// Utilizando a placa NodeMCU
// Código baseado no programa: NodeMCU e MQTT - Controle e Monitoramento IoT
// Autor original: Pedro Bertoleti

#include <ESP8266WiFi.h>  // Biblioteca da placa
#include <PubSubClient.h> // Bibioteca do cliente do MQTT
#include <DHTesp.h>       // Bibioteca do sensor de temperatura

#define TOPICO_SENSORES "iot7-ambient"                    // node do tópico onde devem ser publicados os dados dos sensores
#define ID_MQTT "78ea88bc572c47718b6a6e6451d75431adda222" //identificador da sessão no MQTT

// mapeamento de pinos do NodeMCU
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define D9 3
#define D10 1

#define TEMPO_ENVIO_DADOS 2000

// WIFI
const char *SSID = "iot";
const char *PASSWORD = "netdascoisas#";

// MQTT
const char *BROKER_MQTT = "m2m.eclipse.org";
int BROKER_PORT = 1883;

WiFiClient espClient; // Cria o objeto espClient
DHTesp dht;
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient

//Prototypes
void initSerial();
void initWiFi();
void initMQTT();
void reconectWiFi();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
void VerificaConexoesWiFIEMQTT(void);
void InitOutput(void);

unsigned long tempoUltimoEnvio = 0;

void setup()
{
    //inicializações:
    //InitOutput();
    initSerial();
    initWiFi();
    initMQTT();

    dht.setup(16, DHTesp::DHT11);
    delay(1000);
}

//Função: inicializa comunicação serial com baudrate 115200 (para fins de monitorar no terminal serial
//        o que está acontecendo.
//Parâmetros: nenhum
//Retorno: nenhum
void initSerial()
{
    Serial.begin(115200);
}

//Função: inicializa e conecta-se na rede WI-FI desejada
//Parâmetros: nenhum
//Retorno: nenhum
void initWiFi()
{
    delay(10);
    Serial.println("------Conexao WI-FI------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde");

    reconectWiFi();
}

//Função: inicializa parâmetros de conexão MQTT
//Parâmetros: nenhum
//Retorno: nenhum
void initMQTT()
{
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);
}

//Função: reconecta-se ao broker MQTT (caso ainda não esteja conectado ou em caso de a conexão cair)
//        em caso de sucesso na conexão ou reconexão, o subscribe dos tópicos é refeito.
//Parâmetros: nenhum
//Retorno: nenhum
void reconnectMQTT()
{
    while (!MQTT.connected())
    {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT))
        {
            Serial.println("Conectado com sucesso ao broker MQTT!");
        }
        else
        {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Havera nova tentatica de conexao em 2s");
            delay(2000);
        }
    }
}

//Função: reconecta-se ao WiFi
//Parâmetros: nenhum
//Retorno: nenhum
void reconectWiFi()
{
    //se já está conectado a rede WI-FI, nada é feito.
    //Caso contrário, são efetuadas tentativas de conexão
    if (WiFi.status() == WL_CONNECTED)
        return;

    WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Conectado com sucesso na rede ");
    Serial.print(SSID);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
}

//Função: verifica o estado das conexões WiFI e ao broker MQTT.
//        Em caso de desconexão (qualquer uma das duas), a conexão
//        é refeita.
//Parâmetros: nenhum
//Retorno: nenhum
void VerificaConexoesWiFIEMQTT(void)
{
    if (!MQTT.connected())
        reconnectMQTT(); //se não há conexão com o Broker, a conexão é refeita

    reconectWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}

void EnviaDadosSensores()
{
    TempAndHumidity newValues = dht.getTempAndHumidity();
    if (dht.getStatus() != 0)
    {
        Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    }
    else
    {
        char buff[5], buff2[5], data[50];

        char *temp = dtostrf(newValues.temperature, 4, 1, buff);
        char *hum = dtostrf(newValues.humidity, 4, 1, buff2);

        sprintf(data, "{\"temperature\": %s, \"humidity\": %s}", temp, hum);
        Serial.println(data);

        MQTT.publish(TOPICO_SENSORES, data);
    }
}

//programa principal
void loop()
{
    //garante funcionamento das conexões WiFi e ao broker MQTT
    VerificaConexoesWiFIEMQTT();

    unsigned long tempoAtual = milis();

    if (tempoAtual - tempoUltimoEnvio > TEMPO_ENVIO_DADOS)
    {
        EnviaDadosSensores();
        tempoUltimoEnvio = milis();
    }
    //keep-alive da comunicação com broker MQTT
    MQTT.loop();
}