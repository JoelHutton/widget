#include <stdio.h>
#include <string.h>
#include <EEPROM.h>

#include "ESP8266WiFi.h"
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

extern "C" {
#include "c_types.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "smartconfig.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/dns.h"
#include "user_interface.h"
}

#define TEMPERATURE_PIN 2
#define MOTION_PIN 12
#define LED 5
#define BUTTON 4

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2
// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);


int status = WL_IDLE_STATUS;
//time last readings were sent (in milliseconds since startup)
unsigned long lastTransmit=0;
unsigned short localPort = 1234;// local port to listen for UDP packets
unsigned short destPort = 1234;
char serverAddr[512];
byte macAddressHex[6];
char macAddress[13];
char ssid[500];
char passwd[500];
byte packetBuffer[512]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

int writeCredentials(int argc, char** argv){
	int i,j,index = 0;
	EEPROM.begin(512);
	for(i=0;i<argc;i++){
		for(j=0;j<strlen(argv[i]) && index < 512;j++){
			EEPROM.write(index,argv[i][j]);
			index++;
		}
		if(i != argc-1){
			EEPROM.write(index,'-');
			index++;
		}
	}
	EEPROM.write(index,'\0');
	index++;
	EEPROM.commit();
	EEPROM.end();
	digitalWrite(LED,0);
	Serial.print("credentials written, rebooting...");
	ESP.restart();
	return 0;
}

int clearEEPROM(int argc, char** argv){
	int i;
	Serial.println("clearing EEPROM");
	EEPROM.begin(512);
	for(i=0; i<512 ; i++){
		EEPROM.write(i,'\0');
	}
	EEPROM.commit();
	EEPROM.end();
	return 0;
}

struct command{
	const char* keyword;
	int (*runCommand) (int argc, char** argv);
};
const struct command commands[]={
	(struct command) { "wifi", &writeCredentials},
	(struct command) { "clear", &clearEEPROM}
};

void getMac(){
	WiFi.macAddress(macAddressHex);
	sprintf(macAddress, "%x", macAddressHex);
	macAddress[13]='\0';
}

void connectToRouter(){
	// Wait for connection to AP
	Serial.print("[Connecting]");
	Serial.print(ssid);
	Serial.print(",");
	Serial.print(passwd);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, passwd);
	int tries=0;
	while(WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		tries++;
		if (tries > 30){
			break;
		}
	}
	Serial.println();
	Serial.print("my ip:");
	Serial.println(WiFi.localIP());
}

void handleUDP(){
	int numBytes = Udp.parsePacket();
	String receivedCommand = "";
	//receive UDP packets to give wifi configuration
	if (numBytes){
		//debug 
		Serial.print(millis() / 1000);
		Serial.print(":Packet of ");
		Serial.print(numBytes);
		Serial.print(" received from ");
		Serial.print(Udp.remoteIP());
		Serial.print(":");
		Serial.println(Udp.remotePort());
		// We've received a packet, read the data from it
		Udp.read(packetBuffer,numBytes); // read the packet into the buffer
		packetBuffer[numBytes]='\0';
		Serial.println((char*) packetBuffer);
	}
}

int checkForCredentials(){
	int i,argc;
	char* buff = (char*) malloc(512);
	char* argv[4];
	const char* keyword="wifi";
	EEPROM.begin(512);
	Serial.println("checking for stored credentials");
	for(i = 0; i<512; i++){
		buff[i]=EEPROM.read(i);
		Serial.print(buff[i]);
	}
	Serial.println();
	buff[i] = '\0';
	argc = tokeniseStr(buff, argv, sizeof(argv)/sizeof(char*), "-");
	if(strcmp(buff, keyword)==0){
		if(argc == 4){
			strcpy(ssid,argv[1]);
			strcpy(passwd,argv[2]);
			strcpy(serverAddr,argv[3]);
			free(buff);
			return 1;
		}
		else{
			Serial.println("malformed expression in EEPROM");
			free(buff);
			return 0;
		}
	
	}
	EEPROM.end();
	free(buff);
	return 0;
}


void serialRead(char* buff, int bufflen){
	int i;
	Serial.println("READY");
	for(i=0; i < bufflen; i++){
		while(Serial.available() == 0 )
			;
		buff[i]=Serial.read();
		if(buff[i]=='\r' || buff[i]=='\n' || buff[i] == '\0')
			break;
	}
	buff[i] = '\0';
}

int tokeniseStr(char* buff, char** argv, int argc, char* sep){
	int i;
	char* strptr = buff;
	for(i=0; i<argc;){
		argv[i] = strsep(&strptr,sep);
		if( argv[i] != NULL){
			i++;
		}
		else{
			break;
		}
	}
	return i;
}

// Returns a function pointer to the runCommand function for that command
// Returns type is int(*) (int, char**)
// Takes int argc, char** argv as arguments
int(*parseCommand(int argc, char** argv))(int,char**) {
	int i,numCommands;
	numCommands = sizeof(commands)/sizeof(command);
	for(i = 0; i< numCommands;i++){
		if(strcmp(argv[0],commands[i].keyword)==0){
			return commands[i].runCommand;
		}
	}
	Serial.println("command not found");
	return NULL;
}

//wait on commands from serial
void serialSlave(){
	char* buff = (char*) malloc(512);
	const char* keyword="wifi";
	Serial.println();
	Serial.println("serialSlave started");
	digitalWrite(LED,1);
	while(true){
		int i,argc;
		char* argv[5];
		int (*runCommand) (int argc, char** argv);
		serialRead(buff, 512);
		argc = tokeniseStr(buff, argv, sizeof(argv)/sizeof(char*), "-");
		runCommand = parseCommand(argc, argv);
		if(runCommand != NULL){
			if(runCommand(argc, argv)==0){
				Serial.println("command executed successfully");
			}
			else{
				Serial.println("command failed");
			}
		}
		else{
			Serial.println("command not found");
		}
		delay(100);
	}
	free(buff);
}

void setup(){
	// Open serial communications and wait for port to open:
	Serial.begin(115200);
	pinMode(BUTTON,INPUT_PULLUP);
	sensors.begin();
	getMac();
	pinMode(A0, INPUT);
	pinMode(MOTION_PIN, INPUT);
	pinMode(TEMPERATURE_PIN, INPUT_PULLUP);
	pinMode(LED, OUTPUT);
	ssid[0]='\0';
	passwd[0]='\0';
	serverAddr[0]='\0';
	if(digitalRead(BUTTON)==0){
		serialSlave();
	}
	else if(checkForCredentials()){
		Serial.println("credentials found");
		connectToRouter();
		Udp.begin(localPort);
	}
	else{
		Serial.print("no stored credentials found, going into serialSlave mode");
		serialSlave();
	}
}

unsigned long lastPrint=0;
void loop(){
	delay(10);

	sensors.requestTemperatures(); // Send the command to get temperatures
	float temp=sensors.getTempCByIndex(0);
	int motion=digitalRead(MOTION_PIN);
	int light=analogRead(A0);
	String tempString= String(temp);
	String motionString= String(motion);
	String lightString= String(light);
	Serial.print(" motion:");
	Serial.print(motionString);
	Serial.print(" light:");
	Serial.print(lightString);
	Serial.print(" button:");
	Serial.print(digitalRead(BUTTON));
	Serial.print(" temperature:");
	Serial.println(tempString);

	delay(100);
	//send motion
	Udp.beginPacket(serverAddr, destPort);
	Udp.print(macAddress);
	Udp.print("-");
	Udp.print("motion-");
	Udp.print(motionString);
	Udp.endPacket();

	delay(100);
	//send temperature
	Udp.beginPacket(serverAddr, destPort);
	Udp.print(macAddress);
	Udp.print("-");
	Udp.print("temperature-");
	Udp.print(tempString);
	Udp.endPacket();

	delay(100);
	//send light level
	Udp.beginPacket(serverAddr, destPort);
	Udp.print(macAddress);
	Udp.print("-");
	Udp.print("light-");
	Udp.print(lightString);
	Udp.endPacket();

	Serial.println("waiting for response from server");
	long unsigned int startedWaiting=millis();
	while(millis()-startedWaiting < 5000){
		handleUDP();
		//so wifi stack doesn't fail
		delay(10);
	}
}
