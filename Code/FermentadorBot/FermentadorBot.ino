#define USE_DISPLAY		0

#if USE_DISPLAY
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif
#include <Adafruit_Sensor.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <DHT.h>

#if defined(ESP8266)		// Use ESP8266 or ESP32
#include <ESP8266WiFi.h>        
#else
#include <WiFi.h>
#endif

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#if USE_DISPLAY
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#endif

#define DHTPIN 14     // Digital pin connected to the DHT sensor

//#define DHTTYPE    DHT11     // DHT 11
#define DHTTYPE    DHT22     // DHT 22 (AM2302)

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "josemar" // Seu usuario cadastrado na plataforma da Adafruit
#define AIO_KEY         "aio_YJCH99t5lqxK8EWqB98NNz356kJD"       // Sua key da dashboard
 
#define RELAY_PIN 5
 
//Intervalo entre as checagens de novas mensagens
#define INTERVAL 1000 *60
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60 * 1   /* Time ESP32 will go to sleep (in seconds) */
 
//Token do seu bot. Troque pela que o BotFather te mostrar
#define BOT_TOKEN "826920761:AAGdkMoiYhsaQIbQ6KLcLxp95txknAbnwLI"
 
//Troque pelo ssid e senha da sua rede WiFi
#define SSID "BeerPoint"
#define PASSWORD "tiragosto"

//Comandos aceitos
const String LIGHT_ON = "ligar a luz";
const String LIGHT_OFF = "desligar a luz";
const String CLIMATE = "clima";
const String STATS = "status";
const String START = "/start";

WiFiClient client;

//Cliente para conexões seguras
WiFiClientSecure clientSecure;
 
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/* feed responsavel por receber os dados da nossa dashboard */
Adafruit_MQTT_Publish Graph_Temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/DHT_Fermenter_Temperature", MQTT_QOS_1);

/* feed responsavel por receber os dados da nossa dashboard */
Adafruit_MQTT_Publish Gauge_Temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/DHT_Fermenter_Temperature", MQTT_QOS_1);
 
/* feed responsavel por enviar os dados do sensor para nossa dashboard */
Adafruit_MQTT_Publish Graph_Humid = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/DHT22_Fermenter_Humidity", MQTT_QOS_1);

/* feed responsavel por receber os dados da nossa dashboard */
Adafruit_MQTT_Publish Gauge_Humid = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/DHT22_Fermenter_Humidity", MQTT_QOS_1);

 
/* Observe em ambas declarações acima a composição do tópico mqtt
  --> AIO_USERNAME "/feeds/mcp9808"
  O mpc9808 será justamente o nome que foi dado la na nossa dashboard, portanto o mesmo nome atribuido la, terá de ser colocado aqui tambem
*/

String LastChatId;

#if defined(ESP8266)
int bootCount = 0;
#else
RTC_DATA_ATTR int bootCount = 0;
#endif
 
//Estado do relê
int relayStatus = HIGH;
 
//Objeto com os métodos para comunicarmos pelo Telegram
UniversalTelegramBot bot(BOT_TOKEN, clientSecure);
//Tempo em que foi feita a última checagem
uint32_t lastCheckTime = 0;
 
//Quantidade de usuários que podem interagir com o bot
#define SENDER_ID_COUNT 1
//Ids dos usuários que podem interagir com o bot. 
//É possível verificar seu id pelo monitor serial ao enviar uma mensagem para o bot
String validSenderIds[SENDER_ID_COUNT] = {"1060038499"};
const String validChatId = "1060038499";

void setupWiFi();

void handleNewMessages(int numNewMessages);

boolean validateSender(String senderId);

void handleStart(String chatId, String fromName);

//String getCommands();
void getCommands(String message);

void handleLightOn(String chatId);

void handleLightOff(String chatId);

void handleClimate(String chatId);

String getClimateMessage();

/*----------------------------------------------------------
* PostClimateTelegram: Post climate info on telegram
*-----------------------------------------------------------*/
void PostClimateTelegram(String p_sChatId);

/*----------------------------------------------------------
* PostClimateAdafruitIO: Post climate info on Adafruint IO
*-----------------------------------------------------------*/
void PostClimateAdafruitIO(void);

/*----------------------------------------------------------
* ShowClimateDisplay: Update climate info on Display
*-----------------------------------------------------------*/
#if USE_DISPLAY
void ShowClimateDisplay(void);
#endif

void handleStatus(String chatId);

void handleNotFound(String chatId);

void initMQTT();

void conectar_broker();

DHT dht(DHTPIN, DHTTYPE);


float m_fTemperature, m_fHumidity;

void setup() {
  Serial.begin(115200);

  dht.begin();  
  delay(500);
  
  m_fTemperature = dht.readTemperature();
  m_fHumidity = dht.readHumidity();
  if (isnan(m_fHumidity) || isnan(m_fTemperature))
  {
    Serial.println("Failed to read from DHT sensor!");
	Serial.println("Rebooting...");
	ESP.restart();
  } 

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  
#if USE_DISPLAY
    // Start I2C Communication SDA = 5 and SCL = 4 on Wemos Lolin32 ESP32 with built-in SSD1306 OLED
  Wire.begin(5, 4);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
#endif

  //Inicializa o WiFi e se conecta à rede
  setupWiFi();

  initMQTT();
  
  delay(2000);
 
#if USE_DISPLAY
  display.clearDisplay();
  display.setTextColor(WHITE);
#endif
}

void loop() {
	
  conectar_broker();
  mqtt.processPackets(5000);
  
  PostClimateAdafruitIO();
  PostClimateTelegram(validChatId);
  
  delay(500);

#if USE_DISPLAY
  ShowClimateDisplay();
#endif

#if !defined(ESP8266)
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
#endif  
  
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Minutos");

  Serial.println("Going to sleep now");
  delay(2000);
  Serial.flush();

#if defined(ESP8266)
  ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR);
#else   
  esp_deep_sleep_start();
#endif
  
  Serial.println("This will never be printed");
  
}

/* Configuração da conexão MQTT */
void initMQTT() {
//  _rele01.setCallback(rele01_callback);
//  mqtt.subscribe(&_rele01);
}

void conectar_broker() {
  int8_t ret;
 
  if (mqtt.connected()) {
    return;
  }
 
  Serial.println("Conectando-se ao broker mqtt...");
 
  uint8_t num_tentativas = 3;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Falha ao se conectar. Tentando se reconectar em 5 segundos.");
    mqtt.disconnect();
    delay(5000);
    num_tentativas--;
    if (num_tentativas == 0) {
      Serial.println("Seu ESP será resetado.");
      while (1);
    }
  }
 
  Serial.println("Conectado ao broker com sucesso.");
}

void setupWiFi()
{
  Serial.print("Connecting to SSID: ");
  Serial.println(SSID);
 
  //Inicia em modo station e se conecta à rede WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
 
  //Enquanto não estiver conectado à rede
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
 
  //Se chegou aqui está conectado
  Serial.println();
  Serial.println("Connected");
}

void handleNewMessages(int numNewMessages)
{
  for (int i=0; i<numNewMessages; i++) //para cada mensagem nova

  {
    String chatId = String(bot.messages[i].chat_id); //id do chat 
    String senderId = String(bot.messages[i].from_id); //id do contato

    LastChatId = String(bot.messages[i].chat_id); //id do chat
     
    Serial.println("senderId: " + senderId); //mostra no monitor serial o id de quem mandou a mensagem
    Serial.println("ChatId: " + chatId); //mostra no monitor serial o id de quem mandou a mensagem
 
    boolean validSender = validateSender(senderId); //verifica se é o id de um remetente da lista de remetentes válidos
 
    if(!validSender) //se não for um remetente válido
    {
      bot.sendMessage(chatId, "Desculpe mas você não tem permissão", "HTML"); //envia mensagem que não possui permissão e retorna sem fazer mais nada
      continue; //continua para a próxima iteração do for (vai para próxima mensgem, não executa o código abaixo)
    }
     
    String text = bot.messages[i].text; //texto que chegou

    //Serial.println(text);
 
    if (text.equalsIgnoreCase(START))
    {
      handleStart(chatId, bot.messages[i].from_name); //mostra as opções
    }
    else if (text.equalsIgnoreCase(LIGHT_ON))
    {
      handleLightOn(chatId); //liga o relê
    }
    else if(text.equalsIgnoreCase(LIGHT_OFF))
    {
     handleLightOff(chatId); //desliga o relê
    }
    else if(text.equalsIgnoreCase(CLIMATE))
    {
      handleClimate(chatId); //envia mensagem com a temperatura e umidade
    }
    else if (text.equalsIgnoreCase(STATS))
    {
      handleStatus(chatId); //envia mensagem com o estado do relê, temperatura e umidade
    }
    else
    {
      handleNotFound(chatId); //mostra mensagem que a opção não é válida e mostra as opções
    }
  }//for
}

boolean validateSender(String senderId)
{
  //Para cada id de usuário que pode interagir com este bot
  for(int i=0; i<SENDER_ID_COUNT; i++)

  {
    //Se o id do remetente faz parte do array retornamos que é válido
    if(senderId == validSenderIds[i])
    {
      return true;
    }
  }
 
  //Se chegou aqui significa que verificou todos os ids e não encontrou no array
  return false;
}

void handleStart(String chatId, String fromName)
{
  //Mostra Olá e o nome do contato seguido das mensagens válidas
  String message = "<b>Olá " + fromName + ".</b>\n";

  //message += getCommands();
  //getCommands(message);
    //String com a lista de mensagens que são válidas e explicação sobre o que faz
  message.concat("Os comandos disponíveis são:\n\n");
  message.concat("<b>" + LIGHT_ON + "</b>: Para ligar a luz\n");
  message.concat("<b>" + LIGHT_OFF + "</b>: Para desligar a luz\n");
  message.concat("<b>" + CLIMATE + "</b>: Para verificar o clima\n");
  message.concat("<b>" + STATS + "</b>: Para verificar o estado da luz e a temperatura");

  //bot.sendMessage(chatId, "Teste", "HTML");
  bot.sendMessage(chatId, message, "HTML");
  //Serial.println(chatId);
  Serial.println(message);
}


void getCommands(String message)
{
  //String com a lista de mensagens que são válidas e explicação sobre o que faz
  message.concat("Os comandos disponíveis são:\n\n");
  message.concat("<b>" + LIGHT_ON + "<b>: Para ligar a luz\n");
  message.concat("<b>" + LIGHT_OFF + "<b>: Para desligar a luz\n");
  message.concat("<b>" + CLIMATE + "<b>: Para verificar o clima\n");
  message.concat("<b>" + STATS + "<b>: Para verificar o estado da luz e a temperatura");
  return;
}

//String getCommands()
//{
//  //String com a lista de mensagens que são válidas e explicação sobre o que faz
//  String message = "Os comandos disponíveis são:\n\n";
//  message += "<b>" + LIGHT_ON + "<b>: Para ligar a luz\n";
//  message += "<b>" + LIGHT_OFF + "<b>: Para desligar a luz\n";
//  message += "<b>" + CLIMATE + "<b>: Para verificar o clima\n";
//  message += "<b>" + STATS + "<b>: Para verificar o estado da luz e a temperatura";
//  return message;
//}

void handleLightOn(String chatId)
{
  //Liga o relê e envia mensagem confirmando a operação
  relayStatus = LOW; //A lógica do nosso relê é invertida
  //digitalWrite(RELAY_PIN, relayStatus);  
  bot.sendMessage(chatId, "A luz está <b>acesa</b>", "HTML");
}

void handleLightOff(String chatId)
{
  //Desliga o relê e envia mensagem confirmando a operação 
  relayStatus = HIGH; //A lógica do nosso relê é invertida
  //digitalWrite(RELAY_PIN, relayStatus);  
  bot.sendMessage(chatId, "A luz está <b>apagada</b>", "HTML");
}

void handleClimate(String chatId)
{
  //Envia mensagem com o valor da temperatura e da umidade
  bot.sendMessage(chatId, getClimateMessage(), "");
}

String getClimateMessage()
{
  //Faz a leitura da temperatura e da umidade
  float temperature, humidity;
  
  //read temperature and humidity
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  } 
  //Se foi bem sucedido
  else
  {
    //Retorna uma string com os valores
    String message = "";
    message += "A temperatura é de " + String(temperature)+ " °C e ";
    message += "a umidade é de " + String(humidity) + "%";
    return message;
  }
   
  //Se não foi bem sucedido retorna um mensagem de erro
  return "Erro ao ler temperatura e umidade";
}

void PostClimateTelegram(String p_sChatId)
{
    String sMessage = "";
	
	if(p_sChatId == NULL)
	{
		return;
	}
	
    sMessage += "A temperatura é de " + String(m_fTemperature)+ " °C e ";
    sMessage += "a umidade é de " + String(m_fHumidity) + "%";
	
	bot.sendMessage(p_sChatId, sMessage, "");
}

void PostClimateAdafruitIO(void)
{
  if(mqtt.connected())
  {
	if (! Graph_Temp.publish(m_fTemperature))
    {
      Serial.println("Falha ao enviar o valor do sensor de temperatura.");
    }

    if (! Gauge_Temp.publish(m_fTemperature))
    {
      Serial.println("Falha ao enviar o valor do sensor de temperatura.");
    }
    
    if (! Graph_Humid.publish(m_fHumidity)) 
    {
      Serial.println("Falha ao enviar o valor do sensor de humudade.");
    }

    if (! Gauge_Humid.publish(m_fHumidity)) 
    {
      Serial.println("Falha ao enviar o valor do sensor de humudade.");
    }
  }
}

#if USE_DISPLAY
void ShowClimateDisplay(void)
{
	
  if (isnan(m_fHumidity) || isnan(m_fTemperature))
  {
    Serial.println("Failed to read from DHT sensor!");
	return;
  }
  
  display.clearDisplay();
  
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(m_fTemperature);
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(2);
  display.print("C");
  
  // display humidity
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Humidity: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(m_fHumidity);
  display.print(" %"); 
  
  display.display();	
}
#endif

void handleStatus(String chatId)
{
  String message = "";
 
  //Verifica se o relê está ligado ou desligado e gera a mensagem de acordo
  if(relayStatus == LOW) //A lógica do nosso relê é invertida
  {
    message += "A luz está acesa\n";
  }
  else
  {
    message += "A luz está apagada\n";
  }
 
  //Adiciona à mensagem o valor da temperatura e umidade
  message += getClimateMessage();
 
  //Envia a mensagem para o contato
  bot.sendMessage(chatId, message, "");
}

void handleNotFound(String chatId)
{
  //Envia mensagem dizendo que o comando não foi encontrado e mostra opções de comando válidos
  String message = "Comando não encontrado\n";
  //message += getCommands();
  getCommands(message);
  bot.sendMessage(chatId, message, "HTML");
}
