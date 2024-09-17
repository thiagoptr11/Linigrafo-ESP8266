#include <Arduino.h>
#include <Wire.h> //INCLUSÃO DE BIBLIOTECA
#include <Adafruit_GFX.h> //INCLUSÃO DE BIBLIOTECA
#include <Adafruit_SSD1306.h> //INCLUSÃO DE BIBLIOTECA
#include <Adafruit_AHTX0.h>   //BIBLIOTECA AHT
#include <ThingSpeak.h>

const int trigPin = 13;
const int echoPin = 12;

long duration;
float distanceCm;
float media_temp = 0.0;
float media_umid = 0.0;
float soma_temp = 0.0;
float soma_umid = 0.0;
float amostra = 0.0;
float soma_distance=0.0;
unsigned long last_update;
unsigned int tela = 1;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Configurações do ThingSpeak
const char apn[] = "zap.vivo.com.br"; // APN do seu provedor de dados móveis
const char user[] = "vivo"; // Usuário do APN (geralmente não é necessário)
const char pass[] = "vivo"; // Senha do APN (geralmente não é necessário)

const char* thingSpeakAPIKey = "RZLWKKYDXI871Z6A";
const char* thingSpeakServer = "api.thingspeak.com";
const int thingSpeakPort = 80;

// Configurações do módulo SIM800L
//HardwareSerial sim800Serial(1); // RX, TX


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_AHTX0 aht; //Cria o Objeto aht

void centralizeMessage(String message, uint8_t textSize) {
  // Defina o tamanho do texto
  display.setTextSize(textSize);
  // Defina a cor do texto
  display.setTextColor(SSD1306_WHITE);

  // Calcula a largura e altura do texto
  int16_t x1, y1;
  uint16_t width, height;
  display.getTextBounds(message.c_str(), 0, 0, &x1, &y1, &width, &height);

  // Calcula a posição do cursor para centralizar o texto
  int16_t x = (SCREEN_WIDTH - width) / 2;
  int16_t y = (SCREEN_HEIGHT - height) / 2;

  // Limpa o buffer do display
  display.clearDisplay();

  // Defina a posição do cursor e imprima o texto
  display.setCursor(x, y);
  display.println(message);

  // Atualiza o display com o texto impresso
  display.display();
}

bool sendATCommand(const char* cmd, const int timeout, const char* expectedResponse) {
  Serial.println(cmd);
  long int time = millis();
  while ((time + timeout) > millis()) {
    while (Serial.available()) {
      String response = Serial.readString();
      Serial.println(response);
      if (response.indexOf(expectedResponse) != -1) {
        return true;
      }
    }
  }
  return false;
}

void setup() {
  Serial.begin(9600); // RX: GPIO13, TX: GPIO2
  delay(3000);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Wire.begin(D2,D1);
  // Adiciona um delay para garantir que o display esteja pronto
  delay(2000);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    for(;;); // Para a execução se houver falha
  }
  display.setTextColor(WHITE); //DEFINE A COR DO TEXTO
  display.setTextSize(2); //DEFINE O TAMANHO DA FONTE DO TEXTO
  display.setRotation(0);
  display.clearDisplay(); //LIMPA AS INFORMAÇÕES DO DISPLAY

  centralizeMessage("LINIGRAFO",2);
  delay(2000);
  display.clearDisplay(); //LIMPA AS INFORMAÇÕES DO DISPLAY

  //Inicia o AHT10
  if (! aht.begin()) {
    centralizeMessage("AHT FAIL",2);
    delay(1000);
    display.clearDisplay(); //LIMPA AS INFORMAÇÕES DO DISPLAY
    while (1) delay(10);
  }
  centralizeMessage("AHT OK",2);
  delay(1000);
  display.clearDisplay(); //LIMPA AS INFORMAÇÕES DO DISPLAY

  // Inicializa o módulo SIM800L
  sendATCommand("AT", 1000, "OK");
  sendATCommand("AT+CSQ", 1000, "OK");
  sendATCommand("AT+CGATT=1", 1000, "OK");
  sendATCommand("AT+CIPSHUT", 1000, "SHUT OK");
  sendATCommand("AT+CIPSTATUS", 1000, "OK");
  sendATCommand("AT+CIPMUX=0", 1000, "OK");

  // Conecta à rede
  String cmd = "AT+CSTT=\"" + String(apn) + "\",\"" + String(user) + "\",\"" + String(pass) + "\"";
  sendATCommand(cmd.c_str(), 1000, "OK");
  sendATCommand("AT+CIICR", 3000, "OK");
  sendATCommand("AT+CIFSR", 1000, ".");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Conectado à rede!");
  display.display();
}

void loop() {

  //Inicia a medição AHT
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  soma_temp += temp.temperature;
  soma_umid += humidity.relative_humidity;

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
  
    duration = pulseIn(echoPin, HIGH);

    float divisor =10000/(sqrt(((media_temp)+273.15)/273.15) * 331.45) ;
    soma_distance += (duration/divisor/2);
    amostra += 1.0;
    delay(100);

    //Atualiza valores
  if(millis()-last_update>30000){
    last_update=millis();
    media_temp= soma_temp/amostra;
    media_umid=soma_umid/amostra;
    distanceCm = soma_distance/amostra;
    soma_distance = 0.0;
    soma_temp = 0.0;
    soma_umid = 0.0;
    amostra = 0.0;
    if(tela<3){
      tela++;
    }else{
      tela=1;
    }

     // Dados a serem enviados
  String data = "field1=" + String(distanceCm) + "&field2=" + String(media_temp) + "&field3=" + String(media_umid);
  
  String cmd = "AT+CIPSTART=\"TCP\",\"" + String(thingSpeakServer) + "\"," + String(thingSpeakPort);
  if (sendATCommand(cmd.c_str(), 3000, "CONNECT OK")) {
    cmd = "POST /update HTTP/1.1\r\n";
    cmd += "Host: " + String(thingSpeakServer) + "\r\n";
    cmd += "Connection: close\r\n";
    cmd += "X-THINGSPEAKAPIKEY: " + String(thingSpeakAPIKey) + "\r\n";
    cmd += "Content-Type: application/x-www-form-urlencoded\r\n";
    cmd += "Content-Length: " + String(data.length()) + "\r\n";
    cmd += "\r\n";
    cmd += data;
    
    sendATCommand("AT+CIPSEND", 1000, ">");
    Serial.print(cmd);

    if (sendATCommand("", 3000, "SEND OK")) {
      Serial.println("Atualização bem-sucedida.");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Atualização bem-sucedida.");
      display.display();
    }
  }

  sendATCommand("AT+CIPCLOSE", 1000, "CLOSE OK");

  }

  if(tela==1){

    centralizeMessage((String(media_temp)+" C"),2);
    display.clearDisplay(); //LIMPA AS INFORMAÇÕES DO DISPLAY

  }else if(tela ==2){
    
    centralizeMessage((String(media_umid)+" %"),2);
    display.clearDisplay(); //LIMPA AS INFORMAÇÕES DO DISPLAY

  }else if(tela==3){
    centralizeMessage((String(distanceCm)+" cm"),2);
    display.clearDisplay(); //LIMPA AS INFORMAÇÕES DO DISPLAY

  }

  //yield();
}