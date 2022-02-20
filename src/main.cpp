
#include <dummy.h>
/**
   Created by K. Suwatchai (Mobizt)

   Email: k_suwatchai@hotmail.com

   Github: https://github.com/mobizt

   Copyright (c) 2021 mobizt

*/

#if defined(ESP32)
#include <WiFi.h>
#include <FirebaseESP32.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#endif
#include <FireConfig.h>

/**********************************************/

#define LIGA LOW
#define DESLIGA HIGH

#define NIVEL_ALTO D0
#define NIVEL_MEDIO D1
#define NIVEL_BAIXO D2

#define MOTOR D5
#define DRENO D6
#define AGUA D7
#define BUZZER D8
boolean P0[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
boolean P1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
boolean P2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
boolean P3[4] = {0, 0, 0, 0};

// Define Firebase Data object
FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;

// Definindo algumas variáveis globais
String nivel, comando, resposta, programa;
bool dreno = false;
volatile bool dataChanged = false;
int count = 0;
volatile bool finalizado = false;
volatile bool confirmaSegunda = false;

// alguns prototipos de funções

boolean SeMexe(String x);
void buzina();

void streamCallback(StreamData data)
{
  /* Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                 data.streamPath().c_str(),
                 data.dataPath().c_str(),
                 data.dataType().c_str(),
                 data.eventType().c_str());
                 */
  printResult(data); // see addons/RTDBHelper.h
  Serial.println();

  // This is the size of stream payload received (current and max value)
  // Max payload size is the payload size under the stream path since the stream connected
  // and read once and will not update until stream reconnection takes place.
  // This max value will be zero as no payload received in case of ESP8266 which
  // BearSSL reserved Rx buffer size is less than the actual stream payload.
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());

  // Due to limited of stack memory, do not perform any task that used large memory here especially starting connect to server.
  // Just set this flag and check it status later.
  dataChanged = true;
}
void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void setup()
{

  pinMode(BUZZER, OUTPUT);
  pinMode(MOTOR, OUTPUT);
  digitalWrite(MOTOR, DESLIGA);
  pinMode(DRENO, OUTPUT);
  digitalWrite(DRENO, DESLIGA);
  pinMode(AGUA, OUTPUT);
  digitalWrite(AGUA, DESLIGA);
  pinMode(NIVEL_ALTO, INPUT);
  pinMode(NIVEL_MEDIO, INPUT);
  pinMode(NIVEL_BAIXO, INPUT);
  pinMode(BUZZER, OUTPUT);

  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  stream.setBSSLBufferSize(2048 /* Rx in bytes, 512 - 16384 */, 512 /* Tx in bytes, 512 - 16384 */);
  if (!Firebase.beginStream(stream, "/Comunica/comando"))
    Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());

  Firebase.setStreamCallback(stream, streamCallback, streamTimeoutCallback);
  Firebase.setDoubleDigits(5);
  if (Firebase.ready())
  {
    sendDataPrevMillis = millis();

    Firebase.getString(fbdo, "/Comunica/comando");
    comando = fbdo.to<String>();
    Firebase.getString(fbdo, "/Comunica/resposta");
    resposta = fbdo.to<String>();
    Serial.printf("Comando:  %s    ---  Resposta:  %s\n", comando.c_str(), resposta.c_str());

    Firebase.getString(fbdo, "/Modo/nivel");
    nivel = fbdo.to<String>();
    Firebase.getString(fbdo, "/Modo/programa");
    programa = fbdo.to<String>();
    Serial.printf("Nivel:  %s    ---  Programa:  %s\n", nivel.c_str(), programa.c_str());

    delay(100);
  }
}

void loop()
{

  if (dataChanged)
  {
    dataChanged = false;
    Firebase.getString(fbdo, "/Comunica/comando");
    String comando = fbdo.to<String>();
    if (comando.compareTo("LIGAR") == 0)
    {
      Firebase.set(fbdo, "/Comunica/resposta", "LIGAR: SIM ou NÃO ?");
      confirmaSegunda = true;
    }
    else if (comando.compareTo("SIM") == 0 && confirmaSegunda)
    {
      confirmaSegunda = false;
      Firebase.setString(fbdo, "/Comunica/resposta", "LIGADO");
      Firebase.getString(fbdo, "/Modo/nivel");
      nivel = fbdo.to<String>();
      Firebase.getString(fbdo, "/Modo/programa");
      programa = fbdo.to<String>();
      SeMexe(programa);
      Firebase.setString(fbdo, "/Comunica/resposta", "CICLO ENCERRADO");
      Firebase.setString(fbdo, "/Comunica/comando", "AGUARDANDO COMANDO!!");
     }
  }
}

////////////////////////    ENCHER   //////////////////////

boolean Encher(String nivel)
{
  /*
      1. Quando o nível for atingido o sensor de nível retorna zero.
      1.1 - Acima do nível sensor=0 e Abaixo do nivel sensor = 1 .
      2. Adotou-se a correspondencia (string=> valor): "ALTO"=1 ; "MEDIO"=2 ; "BAIXO"=3.
  */
  byte Nivel;
  digitalWrite(DRENO, DESLIGA);
  digitalWrite(MOTOR, DESLIGA);

  if (nivel.equals(String("ALTO")))
  {
    Nivel = 1;
  }
  else if (nivel.equals(String("MEDIO")))
  {
    Nivel = 2;
  }
  else
  {
    Nivel = 3;
  }

  switch (Nivel)
  {
  case 1:
  {
    while (digitalRead(NIVEL_ALTO))
    {
      delay(10);
      digitalWrite(AGUA, LIGA);
      delay(10);
      Serial.println("Enchendo");
    }
  }
  break;
  case 2:
  {
    while (digitalRead(NIVEL_MEDIO))
    {
      delay(10);
      digitalWrite(AGUA, LIGA);
      delay(10);
      Serial.println("Enchendo");
    }
  }
  break;
  case 3:
  {
    while (digitalRead(NIVEL_BAIXO))
    {
      delay(10);
      digitalWrite(AGUA, LIGA);
      delay(10);
      Serial.println("Enchendo");
    }
  }
  break;
  }
  digitalWrite(AGUA, DESLIGA);
  return true;
}

////////////////////////////////   DRENAR   ////////////////////////

boolean Drenar()
{
  /*
     Ao se entrar nesta função:

    i)   Se o nivel da agua estiver acima do nível "BAIXO", o dreno funcionará até que
         se tenha transcorrido DT segundos depois do nível "BAIXO" ter sido alcançado.
    ii)  Se o nivel da agua estiver abaixo do nível "BAIXO" o dreno ficará ligado por mais DT segundos.
     1.1 - Acima do nível sensor=0 e Abaixo do nivel sensor = 1

  */
  long unsigned int timeDreno;
  int DT = 40; // tempo que fica ligado depois de atingir o nível zero.
  digitalWrite(AGUA, DESLIGA);
  digitalWrite(MOTOR, DESLIGA);
  digitalWrite(DRENO, LIGA);
  Serial.println("Drenando");
  while (!digitalRead(NIVEL_BAIXO))
  {
    delay(100);
  }
  timeDreno = millis() / 1000 + DT;
  while (millis() / 1000 < timeDreno)
  {
    delay(1000);
  }
  digitalWrite(DRENO, DESLIGA);
  return true;
}

////////////////////////////////   MOLHO   ////////////////////////

boolean Molho(int DT)
{
  /*
     DT - tempo do molho em minutos
  */
  Encher(nivel);
  Serial.println("Inicia o molho");
  long unsigned int Time_Molho = millis() / 1000 + DT * 60;
  while (millis() / 1000 < Time_Molho)
  {
    delay(100);
  }
  return true;
}

////////////////////////////////   BATER   ////////////////////////

boolean Bater(int DT)
{
  /*
      DT - tempo de bater em minutos
  */
  Encher(nivel);
  Serial.println("Inicia Bater ...");
  long unsigned int Time_Bater = millis() / 1000 + DT * 60;
  digitalWrite(MOTOR, LIGA);
  while (millis() / 1000 < Time_Bater)
  {
    delay(10);
  }
  digitalWrite(MOTOR, DESLIGA);
  return true;
}

////////////////////////////////   MAIN PRPGRAM   (SeMexe) ////////////////////////

boolean SeMexe(String programa)
{
  boolean drenado = false, cheio = false;

  byte Programa = 10;

  if (programa.equals(String("CICLO_LONGO")))
  {
    Programa = 0;
  }
  else if (programa.equals(String("CICLO_MEDIO")))
  {
    Programa = 1;
  }
  else if (programa.equals(String("CICLO_CURTO")))
  {
    Programa = 2;
  }
  else if (programa.equals(String("ESVAZIAR")))
  {
    Programa = 3;
  }
  else if (programa.equals(String("ENCHER")))
  {
    Programa = 4;
  }
  switch (Programa)
  {
  case 0: //  Muita Suja. (5342-3322-2422) =>  => CICLO_LONGO

    P0[0] = Bater(4);
    P0[1] = Molho(5);
    P0[2] = Bater(3);
    P0[3] = Molho(4);
    P0[4] = Bater(2);
    P0[5] = Molho(4);
    P0[6] = Bater(2);
    P0[7] = Drenar();
    P0[8] = Bater(3);
    P0[9] = Molho(4);
    P0[10] = Bater(2);
    P0[11] = Drenar();
    P0[12] = Bater(2);
    P0[13] = Molho(4);
    P0[14] = Bater(2);
    P0[15] = Drenar();
    break;
  case 1: // Normal.(3532)    => CICLO_MEDIO

    P2[0] = Bater(2);
    P2[1] = Molho(4);
    P2[2] = Bater(2);
    P2[3] = Drenar();
    P2[4] = Bater(2);
    P2[5] = Molho(3);
    P2[6] = Bater(1);
    P2[7] = Drenar();
    break;
  case 2: // Delicada. (222)  => "CICLO_CURTO"
    P3[0] = Bater(1);
    P3[1] = Molho(1);
    P3[2] = Bater(1);
    P3[3] = Drenar();

    break;
  case 3: // Drenar.
    if (!drenado)
      drenado = Drenar();
    break;
  case 4: // Encher
    if (!cheio)
      cheio = Encher(nivel);
    break;
  }
  Serial.println("FIM");
  buzina();
  noTone(BUZZER);
  return true;
}

////////////////////////////////  ALERTA SONORO   ////////////////////////

void buzina()
{
  long unsigned int DT = millis() / 1000 + 10;
  while (millis() / 1000 < DT)
  {
    tone(BUZZER, 200);
    delay(200);
    tone(BUZZER, 0); // colocar 10 segundos aqui => tone(BUZZER, 10)
    delay(200);
  }
}
