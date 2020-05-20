//Board DOIT ESP32 DEVKIT v1

#include <WiFi.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

 //Declare pin functions on RedBoard
#define stp 32
#define PinDirection 33
#define MS1 26
#define MS2 27
#define EN  25

//Declare variables for functions
// Stepper function
String user_input;
int current_menu;

float StepperMinDegree = 1.8; // pas mimimum du moteur en degree
int MicroStepping = 4; //1 2 4 ou 8
int Attente = 1000; // Attente avant photo (en ms)
int MotorGearRatio = 475;
int WormGearRatio = 10;
int DayInSec = 86160;
int CoeffCorrecteur = 1;
int Vitesse = 1;
int VitesseRapide = 100;
int TempsPose = 30; //(s)

// Interrupt for motor step
// https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/timer.html
volatile int interruptCounter;
int totalInterruptCounter;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Interrupt for taking pictures
volatile int interruptCounterPhoto;
int totalInterruptCounterPhoto;
hw_timer_t * timerPhoto = NULL;
portMUX_TYPE timerMuxPhoto = portMUX_INITIALIZER_UNLOCKED;


// Sony function
volatile int counter;
const char* ssid     = "DIRECT-CeE0:ILCE-7RM2";
const char* password = "9E8EqQDV";     // your WPA2 password
const char* host = "192.168.122.1";   // fixed IP of camera
const int httpPort = 8080;
char JSON_1[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"getVersions\",\"params\":[]}";
char JSON_2[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"startRecMode\",\"params\":[]}";
char JSON_3[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"startBulbShooting\",\"params\":[]}";
char JSON_4[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"stopBulbShooting\",\"params\":[]}";
char JSON_5[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"actTakePicture\",\"params\":[]}";
//char JSON_6[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"actHalfPressShutter\",\"params\":[]}";
//char JSON_7[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"cancelHalfPressShutter\",\"params\":[]}";
//char JSON_8[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"setSelfTimer\",\"params\":[2]}";
//char JSON_9[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"setFNumber\",\"params\":[\"5.6\"]}";
//char JSON_10[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"setShutterSpeed\",\"params\":[\"1/200\"]}";
//char JSON_11[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"setIsoSpeedRate\",\"params\":[]}";
//char JSON_6[]="{\"method\":\"getEvent\",\"params\":[true],\"id\":1,\"version\":\"1.0\"}";
//char JSON_3[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"startLiveview\",\"params\":[]}";
//char JSON_4[] = "{\"version\":\"1.0\",\"id\":1,\"method\":\"stopLiveview\",\"params\":[]}";
WiFiClient client;

unsigned long lastmillis;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  GPIO.out_w1ts = ((uint32_t)1 << stp); //Trigger a step. DigitalWrite equivalent (faster) see : https://www.reddit.com/r/esp32/comments/f529hf/results_comparing_the_speeds_of_different_gpio/
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR onTimerPhoto() {
  portENTER_CRITICAL_ISR(&timerMuxPhoto);
  interruptCounterPhoto++;
  portEXIT_CRITICAL_ISR(&timerMuxPhoto);
}

void setup() {
  pinMode(stp, OUTPUT);
  pinMode(PinDirection, OUTPUT);
  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  pinMode(EN, OUTPUT);
  resetEDPins(); //Set step, direction, microstep and enable pins to default states
  Serial.begin(115200); //Open Serial connection for debugging
  SerialBT.begin("ESP32"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");
  Serial.println("Open remote control app on your camera!");
  delay(10000);
  current_menu = 1;
  CameraConnection();
  menu_start();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////                   Menu                                  ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //Main loop menu
void loop() {
  menu();
  resetEDPins();
  
  if (interruptCounterPhoto > 0) {
 
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
 
    StopDeclenchementPhoto();
  }
}

void menu()
{
  while(SerialBT.available()){
    user_input = SerialBT.readString(); //Read user input and trigger appropriate function
    switch (current_menu) {
      case 1: 
        //Start menu
        switch (user_input.toInt()) {
          case 1:
            Avance(Vitesse);
            break;
          case 2:
            Recule(Vitesse);
            break;
          case 3:
            StopMotor();
            break;
          case 4:
            Avance(VitesseRapide);
            break;
          case 5:
            Recule(VitesseRapide);
            break;
          case 6:
            DeclenchementPhoto();
            break;
          case 7:
            Configuration();
            break;
          default:
            Error();
            break;
        }
        break;
      case 2:
        //Configuration menu
        switch (user_input.toInt()) {
          case 1:
            SerialBT.print("Vitesse actuelle : ");SerialBT.println(Vitesse);
            SerialBT.println("Entrer la nouvelle valeur de vitesse");
            current_menu = 3;
            break;
          case 2:
            SerialBT.print("Vitesse rapide actuelle : ");SerialBT.println(VitesseRapide);
            SerialBT.println("Entrer la nouvelle valeur de vitesse rapide");
            current_menu = 4;
            break;
          case 3:
            SerialBT.print("Temps de pose actuel : ");SerialBT.println(TempsPose);
            SerialBT.println("Entrer le temps de pose");
            current_menu = 5;
            break;
          case 9:
            SerialBT.println("Back selected");
            current_menu = 1;
            break;
          default:
            Error();
            break;
        }
        break;
    }
    switch (current_menu) {
      case 1:
        SerialBT.println();
        menu_start();
        break;
      case 2:
        SerialBT.println();
        menu_configuration();
        break;
      default:
        break;  
    }
  }
}

void menu_start()
{
  SerialBT.println("Enter number for control option:");
  SerialBT.println("1. Avance");
  SerialBT.println("2. Recule");
  SerialBT.println("3. Stop");
  SerialBT.println("4. Avance rapide");
  SerialBT.println("5. Recule rapide");
  SerialBT.println("6. Demarrage photo");  
  SerialBT.println("7. Configuration");
  SerialBT.println();
}

void menu_configuration() {
  SerialBT.println("Enter number for control option:");
  SerialBT.println("1. Vitesse");
  SerialBT.println("2. Vitesse Rapide");
  SerialBT.println("3. Temps de Pose");
  SerialBT.println("9. Retour menu");
  SerialBT.println();
}

void Error() {
  SerialBT.println("Aucun des menus sélectionné");
  current_menu = 1;
}

void Configuration()
{
  current_menu = 2;
}

int CalculIntervalleMoteur(void)
{
  int Intervalle = StepperMinDegree*DayInSec*Vitesse*1000*1000/(MicroStepping*MotorGearRatio*WormGearRatio*360); //intervalle en us
  return Intervalle;
}


void Avance(int vitesse)
{
  SerialBT.println("On avance");
  digitalWrite(EN, LOW); //Pull enable pin low to allow motor control
  int Direction = 1; //Avance
  int Intervalle = CalculIntervalleMoteur();
  ControleMoteur(Intervalle,Direction);
}

void Recule(int distance)
{
  SerialBT.println("On recule");
  digitalWrite(EN, LOW); //Pull enable pin low to allow motor control
  int Direction = 0; //Reule
  int Intervalle = CalculIntervalleMoteur();
  ControleMoteur(Intervalle,Direction);
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////                   Sony                                  ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CameraConnection()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {   // wait for WiFi connection
    delay(500);
    SerialBT.print(".");
  }
  SerialBT.println("");
  SerialBT.println("WiFi connected");
  SerialBT.println("IP address: ");
  SerialBT.println(WiFi.localIP());
  httpPost(JSON_1);  // initial connect to camera
  httpPost(JSON_2); // startRecMode
  //httpPost(JSON_3);  //startLiveview  - in this mode change camera settings  (skip to speedup operation)
}

void httpPost(char* jString) {
  SerialBT.print("connecting to ");
  SerialBT.println(host);
  if (!client.connect(host, httpPort)) {
    SerialBT.println("connection failed");
    return;
  }
  else {
    SerialBT.print("connected to ");
    SerialBT.print(host);
    SerialBT.print(":");
    SerialBT.println(httpPort);
  }
  // We now create a URI for the request
  String url = "/sony/camera/";
 
  SerialBT.print("Requesting URL: ");
  SerialBT.println(url);
 
  // This will send the request to the server
  client.print(String("POST " + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n"));
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(strlen(jString));
  // End of headers
  client.println();
  // Request body
  client.println(jString);
  SerialBT.println("wait for data");
  lastmillis = millis();
  while (!client.available() && millis() - lastmillis < 8000) {} // wait 8s max for answer
 
  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    SerialBT.print(line);
  }
  SerialBT.println();
  SerialBT.println("----closing connection----");
  SerialBT.println();
  client.stop();
}

void DeclenchementPhoto()
{
  timerPhoto = timerBegin(2, 80000, true); //Definition adresse timer
  timerAttachInterrupt(timerPhoto, &onTimerPhoto, true); //Def de l'action à executer
  timerAlarmWrite(timerPhoto, TempsPose*10000, true); //Def de la valeur du compteur
  timerAlarmEnable(timerPhoto); //Activation timer
  httpPost(JSON_3);  //actTakePicture
}

void StopDeclenchementPhoto()
{
  httpPost(JSON_4);  //Stop bulb
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////                   Moteur                                ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Controle moteur
void ControleMoteur(int Intervalle,int Direction)
{
  digitalWrite(PinDirection, Direction); //Choix direction
  timer = timerBegin(1, 80, true); //Definition adresse timer
  timerAttachInterrupt(timer, &onTimer, true); //Def de l'action à executer
  timerAlarmWrite(timer, Intervalle, true); //Def de la valeur du compteur
  timerAlarmEnable(timer); //Activation timer
}

void StopMotor()
{
  timerAlarmDisable(timer);
  timerAlarmDisable(timerPhoto);
  StopDeclenchementPhoto();
}


//Resolution moteur

//****Stepper driver Microstep Resolution************
// MS1  MS2 Microstep Resolution
// 0    0   Full Step (2 Phase)
// 1    0   Half Step
// 0    1   Quarter Step
// 1    1   Eigth Step
void ResolutionMoteur(int Resolution)
{
  if (Resolution = 8) {
    digitalWrite(MS1, HIGH); //Pull MS1, and MS2 high to set logic to 1/8th microstep resolution
    digitalWrite(MS2, HIGH);
    SerialBT.println("Resolution 8");
    }
  else if (Resolution = 4) {
    digitalWrite(MS1, LOW);
    digitalWrite(MS2, HIGH);
    }
  else if (Resolution = 2) {
    digitalWrite(MS1, HIGH);
    digitalWrite(MS2, LOW); 
    } 
  else if (Resolution = 1) {
    digitalWrite(MS1, LOW);
    digitalWrite(MS2, LOW);
    SerialBT.println("Resolution 1");
    }
}


//Reset Easy Driver pins to default states
void resetEDPins()
{
  digitalWrite(stp, LOW);
  // digitalWrite(dir, LOW);
  // digitalWrite(MS1, LOW);
  // digitalWrite(MS2, LOW);
  // digitalWrite(EN, HIGH);
}
