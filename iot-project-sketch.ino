// Sketch feito para a disciplina de DCC091
// Utilizando a placa NodeMCU
// Código baseado no programa: NodeMCU e MQTT - Controle e Monitoramento IoT
// Autor original: Pedro Bertoleti

#include <ESP8266WiFi.h>  // Biblioteca da placa
#include <PubSubClient.h> // Bibioteca do cliente do MQTT
#include <DHTesp.h>       // Bibioteca do sensor de temperatura
#include <Keypad.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

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
#define SENSOR_TEMPERATURA_PINO D4

#define HTTP_SECRET "capivara"
#define HTTP_AUTH_URL "http://192.168.1.118:3000/arduino/auth/"

#define RST_PIN D1
#define SS_PIN D2

int potenciaArCondicionado = 2;

// WIFI
const char *SSID = "LAB3";
const char *PASSWORD = "0203634280";

// MQTT
const char *BROKER_MQTT = "m2m.eclipse.org";
int BROKER_PORT = 1883;

// // Keymap
// const byte ROWS = 4;
// const byte COLS = 4;

// char keys[ROWS][COLS] = {
//   {
//     '1','2','3','A',  }
//   ,
//   {
//     '4','5','6','B',  }
//   ,
//   {
//     '7','8','9','C',  }
//   ,
//   {
//     '*','0','#','D',  }
// };

// // Pinos do Keymap
// byte colPins[ROWS] = {
//   D3, D2, D1, D0}; //connect to the row pinouts of the keypad
// byte rowPins[COLS] = {
//   D7, D5, D6, D4}; //connect to the column pinouts of the keypad

WiFiClient espClient;             // Cria o objeto espClient
DHTesp dht;                       // Instancia do sensor de temperatura
PubSubClient MQTT(espClient);     // Instancia o Cliente MQTT passando o objeto espClient
MFRC522 mfrc522(SS_PIN, RST_PIN); //Instancia o cliente do leitor de RFID

//Prototypes
void initMFRC();
void initSerial();
void initWiFi();
void initMQTT();
void reconectWiFi();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
void VerificaConexoesWiFIEMQTT(void);
void InitOutput(void);
void LeituraRFID();

unsigned long tempoUltimoEnvio = 0;

void setup()
{
    //inicializações:
    //InitOutput();
    initSerial();
    initWiFi();
    initMQTT();
    initMFRC();
    pinMode(D0, OUTPUT);

    pinMode(D3, OUTPUT);
    digitalWrite(D0, LOW);

    dht.setup(SENSOR_TEMPERATURA_PINO, DHTesp::DHT11);
    delay(1000);
}

void initMFRC()
{
    SPI.begin();
    mfrc522.PCD_Init();
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
    MQTT.setCallback(callback);
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
            MQTT.subscribe("iot7-ac");
            MQTT.subscribe("iot7-alert");
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
    Serial.println(SSID);
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

bool verificaAcesso(String codigo)
{
    HTTPClient http;
    String url = HTTP_AUTH_URL;
    url.concat(codigo);
    
    Serial.println("Acessando " + url);
    http.begin(url);
    http.addHeader("x-arduino-secret", HTTP_SECRET);

    int httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK)
    {
        String response = http.getString();
        
        Serial.println(codigo + " => " + response);
        //http.end();
        if (response == "1")
            return true;
    }
    else
    {
        Serial.println("Erro");
        Serial.println(http.errorToString(httpCode).c_str());
    }

    return false;
}

void callback(char *topic, byte *payload, unsigned int length)
{

    Serial.println(topic);
    Serial.println((char)payload[0]);

    if (strcmp("iot7-ac", topic) == 0)
    {
        if ((char)payload[0] == '1')
            potenciaArCondicionado = 1;
        else if ((char)payload[0] == '2')
            potenciaArCondicionado = 2;
        else if ((char)payload[0] == '3')
            potenciaArCondicionado = 3;
    }

    else if (strcmp("iot7-alert", topic) == 0)
    {
        if ((char)payload[0] == '0')
            digitalWrite(D3, LOW);
        else if ((char)payload[0] == '1')
            digitalWrite(D3, HIGH);
    }
}

void piscaLuzArCondicionado()
{
    int i;
    for (i = 0; i < potenciaArCondicionado; i++)
    {
        digitalWrite(D0, LOW);
        delay(200);
        digitalWrite(D0, HIGH);
        delay(200);
    }
}

void LeituraRFID()
{
    if (!mfrc522.PICC_IsNewCardPresent()) {
        delay(100);
        return;
    }

    if (!mfrc522.PICC_ReadCardSerial()) {
        delay(100);
        return;
    }

    String content = "";
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        //content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        content.concat(String(mfrc522.uid.uidByte[i], HEX));
    }

    content.toUpperCase();

    Serial.println("Lido o cartão " + content);

    if(verificaAcesso(content))
        Serial.println("Acesso liberado");
    else
        Serial.println("Acesso negado");

    delay(1000);
}

//programa principal
void loop()
{
    //garante funcionamento das conexões WiFi e ao broker MQTT
    VerificaConexoesWiFIEMQTT();

    unsigned long tempoAtual = millis();

    if (tempoAtual - tempoUltimoEnvio > TEMPO_ENVIO_DADOS)
    {
        EnviaDadosSensores();
        tempoUltimoEnvio = millis();

        piscaLuzArCondicionado();
        //potenciaArCondicionado++;
    }

    LeituraRFID();
    //keep-alive da comunicação com broker MQTT
    MQTT.loop();
}