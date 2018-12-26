/*
 * Author: Joel Hutton
 * Date Completed:2016-12-15
 * This program sends sensor values to a server as well as sniffing
 * wifi packets to determine what devices are nearby.
 */

#include "ESP8266WiFi.h"
//#include "ESP8266Ping.h"
#include <WiFiUdp.h>
#include <EEPROM.h>
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
#define TRANSMIT_INTERVAL 60000
#define HOP_INTERVAL 2000

extern "C" {  //required for read Vdd Voltage
#include "user_interface.h"
}
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
unsigned long startedSniffing=0;
uint8_t startedChannel=0;
unsigned long lastHop=0;
const char* wifiKeyword="wifi:(";
const char* macKeyword="macs:(";
const char* clearKeyword="clear";
byte macAddressHex[6];
char macAddress[13];
unsigned short localPort = 1234;// local port to listen for UDP packets
unsigned short destPort = 1234; 
const char* serverIP = "109.228.52.225";      // local port to listen for UDP packets
//const char* serverIP = "192.168.0.18";      // local port to listen for UDP packets
const char* hex ="0123456789ABCDEF";
char ssidBuf[64];//=NULL;
char passwdBuf[64];//=NULL;
//char* ssidBuf="khyber";
//char* passwdBuf="khyberkey";
#define SNIFF   0
#define CONNECT 1
#define CONFIGURATION   2

uint8_t deviceMode=CONNECT;

byte packetBuffer[512]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

unsigned long* lastSeen;
byte* macTable;
uint8_t numMacs=0;

//compare to byte arrays for equality
bool arrayCompare(byte *arr1, byte *arr2, int len){
    for(int i=0; i<len; i++){
        if(arr1[i]!=arr2[i]){
            return false;
        }
    } 
    return true;
}

void printHex(uint8_t *data, uint8_t start, uint8_t length){
    char tmp[length*2+1];
    int j=0;
    for (uint8_t i=0; i<length; i++) 
    {
        tmp[j]=hex[data[i+start]>>4];
        j++;
        tmp[j]=hex[(data[i+start]&0x0F)];
        j++;
    }
    tmp[length*2] = 0;
    Serial.print(tmp);
}

char* formatMac(char* data){
    char* macStr=(char*) os_malloc(sizeof(char)*(6*3));
    byte first;
    int j=0;
    for (uint8_t i=0; i<6; i++) 
    {
        macStr[j]=hex[data[i]>>4];
        j++;
        macStr[j]=hex[(data[i]&0x0F)];
        j++;
        macStr[j]=':';
        j++;
    }
    macStr[(6*3)-1] = '\0';
    return macStr;
}

void printMac(char* data){
    char* macStr=formatMac(data);
    int i=0;
    Serial.print(macStr);
    os_free(macStr);
}

void channelHop()
{
    // 1 - 13 channel hopping
    uint8 new_channel = wifi_get_channel() % 12 + 1;
    wifi_set_channel(new_channel);
    Serial.print("jumping to channel: ");
    Serial.println(new_channel);
}

void printMacTable(){
    Serial.println("macs:");
    for(int i=0; i<numMacs;i++){
        Serial.print("\t");
        printMac((char*) (macTable+(sizeof(byte)*6*i)));
        Serial.print("\t");
        Serial.print("last seen: ");
        if(lastSeen[i]==0){
            Serial.print("\tnever\n");
        }
        else{
            Serial.print((millis()-lastSeen[i])/1000);
            Serial.print("\t seconds ago");
            Serial.print("\tat ");
            Serial.println(lastSeen[i]/1000);
            Serial.print("\n");
        }
    }
}

//look for 802.11 frames with 
static void ICACHE_FLASH_ATTR promiscCb(uint8 *buf, uint16 len)
{
#define ADDR1 16
#define ADDR2 22
#define ADDR3 28
#define ADDR4 36
    uint8_t addresses[3]={ADDR1,ADDR2,ADDR3};
    for(int i=0; i<3;i++){ 
        for(int j=0; j<numMacs;j++){
            //macTable[j]==[current mac]
            if( arrayCompare((buf+addresses[i]), macTable+(j*6*sizeof(byte)), 6) ){
                //update the last seen time if the mac is already in the table
                lastSeen[j]=millis();
            }
        }
    }
}

//get mac address of chip and convert mac address from hex representation to string
void getMac(){
    WiFi.macAddress(macAddressHex);
    for(int i=0; i<12; i++){
        if(i%2==0){
            macAddress[i]=hex[macAddressHex[i/2]>>4];  
        }
        else{
            macAddress[i]=hex[macAddressHex[i/2]&0xF];
        }
    }
    macAddress[13]='\0';
}

void printWifiStatus() {
    String wifiStatus;
    switch(WiFi.status()){
        case (255):
            wifiStatus="no shield";
            break;
        case (0):
            wifiStatus="no ssid available";
            break;
        case (2):
            wifiStatus="scan completed";
            break;
        case (3):
            wifiStatus="connected";
            break;
        case (4):
            wifiStatus="connect failed";
            break;
        case (5):
            wifiStatus="connection lost";
            break;
        case (6):
            wifiStatus="disconnected";
            break;
        default:
            wifiStatus="unknown";
            break;
    }
    // print the SSID of the network you're attached to:
    Serial.print("status:");
    Serial.println(wifiStatus);
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("BSSID: ");
    printMac((char*) WiFi.BSSID());
    Serial.print("\n");
    //Serial.print("Ping: ");
    //bool ret = Ping.ping("www.google.com");
    //Serial.println(ret);

    IPAddress ip = WiFi.localIP();
    Serial.print("Station IP Address: ");
    Serial.println(ip);
    ip = WiFi.softAPIP();
    Serial.print("AP IP Address: ");
    Serial.println(ip);
    Serial.print("MAC Address: ");
    Serial.println(macAddress);
}

void writeCredentials(String ssid, String pass){
    String writeString=wifiKeyword + ssid + "," + pass +')' + '\0';
    for(int i=0;i<writeString.length();i++){
        EEPROM.write(i,writeString.charAt(i));
    }
    EEPROM.commit();
}

void writeMac(String mac){
    Serial.print("writing mac: ");
    Serial.println(mac);
    int start=0;
    //look for the end of the wifi:(passwd,ssid) block
    for(int i=0; i<512;i++){
        if(EEPROM.read(i)==')'){
            numMacs=EEPROM.read(i+1);
            start=i+2 + (6*numMacs);
            numMacs++;
            EEPROM.write(i+1,numMacs);
            break; 
        }
    }
    int i=start;
    int j=0;
    while(i<512 && j<mac.length()){
        uint8_t toWrite=0;
        uint8_t partA=0;
        uint8_t partB=0;
        //if char is 0-9
        if(mac.charAt(j) >=48 and mac.charAt(j) <=57){
            partA += (mac.charAt(j)-48) << 4;
        }
        else if(mac.charAt(j) >=65 and mac.charAt(j) <=70){
            partA += (mac.charAt(j)-55) << 4;
        }
        Serial.print("toWrite = ");
        Serial.print(partA);
        Serial.print("+");
        if(mac.charAt(j+1) >=48 and mac.charAt(j+1) <=57){
            partB += (mac.charAt(j+1)-48);
        }
        else if(mac.charAt(j+1) >=65 and mac.charAt(j+1) <=70){
            partB += (mac.charAt(j+1)-55);
        }
        toWrite=partA+partB;
        Serial.print(toWrite);
        Serial.print(" ");
        EEPROM.write(i,toWrite);
        i++;
        //these are hex characters so 2 to a byte
        j+=2;
    }
    while(i<512){
        EEPROM.write(i,0);
        i++;
    }
    Serial.println("");
    EEPROM.commit();
    dumpEEPROM(512);
}


void connectToRouter(){
    deviceMode=CONNECT;
    if(ssidBuf != NULL && passwdBuf != NULL){
        // Wait for connection to AP
        Serial.print("[Connecting]");
        Serial.print(ssidBuf);
        Serial.print(",");
        Serial.print(passwdBuf);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssidBuf, passwdBuf);
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
        //printWifiStatus();
        //Serial.print("UDP server started at port ");
        //Serial.println(localPort);
        //Udp.begin(localPort);
    }
    else{
        Serial.println("null pointers for ssid and/or passwd");
    }
}

void dumpEEPROM(int limit){
    Serial.println("EEPROM:");
    for(int i=0; i<limit; i++){
        Serial.print("\t");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(EEPROM.read(i));
        Serial.print("\t");
        Serial.print((char) EEPROM.read(i));
        Serial.print("\t");
        Serial.print( hex[EEPROM.read(i)>>4]);
        Serial.println( hex[EEPROM.read(i)&0x0F]);
    }

}

void clearEEPROM(){
    Serial.println("clearing EEPROM");
    for(int i=0;i<512;i++){
        EEPROM.write(i,0);
    }
    EEPROM.commit();
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

        // display the packet contents in HEX
        for (int i=0;i<numBytes;i++){
            Serial.print(packetBuffer[i-1],HEX);
            receivedCommand = receivedCommand + char(packetBuffer[i]);
            if (i % 32 == 0)
            {
                Serial.println();
            }
            else Serial.print(' ');
        } // end for
        Serial.println();

        //respond to packet
        //Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        //Udp.write("Answer from ESP8266 ChipID#");
        //Udp.print(system_get_chip_id());
        //Udp.write("#IP of ESP8266#");
        //Udp.println(WiFi.localIP());
        //Udp.endPacket();

        Serial.println(receivedCommand);
        bool wifi=false;
        if(numBytes>5){
            wifi=arrayCompare((byte*) packetBuffer,(byte*) wifiKeyword, 6); 
            /*
               bool wifi=true;
               for(int i=0;i<6;i++){
               if( receivedCommand[i]!=wifiKeyword[i]){
               wifi=false; 
               break;
               } 
               }
             */
            if(wifi){
                String ssid="";
                String passwd="";
                bool onPasswd=false;
                for(int i=6; i<numBytes; i++){
                    if(receivedCommand[i]==','){
                        onPasswd=true;
                    }
                    else if(receivedCommand[i]==')'){
                        break;
                    }
                    else if(onPasswd){
                        passwd+=receivedCommand[i];
                    }
                    else{
                        ssid+=receivedCommand[i];
                    }
                }
                Serial.print("#");
                Serial.print(ssid);
                Serial.println("#");
                Serial.print("#");
                Serial.print(passwd);
                Serial.println("#");
                //ssidBuf=(char*) malloc((ssid.length()+1)*sizeof(char));
                //passwdBuf=(char*) malloc((passwd.length()+1)*sizeof(char));
                ssid.toCharArray(ssidBuf, ssid.length()+1);
                passwd.toCharArray(passwdBuf, passwd.length()+1);
                writeCredentials(ssid,passwd);
            }
            else{
                bool macs=true;
                for(int i=0;i<6;i++){
                    if( receivedCommand[i]!=macKeyword[i]){
                        macs=false; 
                        break;
                    } 
                }
                if(macs){
                    Serial.println("macs received");
                    String mac="";
                    for(int i=6; i<numBytes; i++){
                        if(receivedCommand[i]!=')'){
                            mac+=receivedCommand[i];
                            if((i-5)%12==0 && i!=6){
                                writeMac(mac);
                                mac="";
                            }
                        }
                        else{
                            break;
                        }
                    }
                }
            }
        }
        if(!wifi && numBytes>=5){
            bool clear=arrayCompare((byte*) packetBuffer,(byte*) clearKeyword, 5); 
            if(clear){
                clearEEPROM();
            } 
        }
    }
}

void promiscMode(){
    deviceMode=SNIFF;
    //start sniffing
    Serial.println(" -> Promisc mode setup ... ");
    wifi_set_opmode_current(STATION_MODE);
    wifi_station_disconnect();
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(promiscCb);
    wifi_promiscuous_enable(1);
    Serial.println("done.");
    Serial.print(" -> Set opmode ... ");
    wifi_set_opmode_current( 0x1 );
    Serial.println("done.");
    Serial.println(" -> Init finished!");
    startedSniffing=millis(); 
    startedChannel=wifi_get_channel();
}

//circular buffer to allow for handling double press, long press etcetera
unsigned long int buttonStates[4]={0,0,0,0};
uint8_t buttonStateIndex=0;
#define BUTTON_DEBOUNCE_TIME 100
//interrupt routine called when button is pressed
void buttonPressed(){
    if(millis()-buttonStates[(buttonStateIndex-1)%4]>BUTTON_DEBOUNCE_TIME){
        Serial.println("button press registered");
        buttonStateIndex=(buttonStateIndex+1)%4;
        buttonStates[buttonStateIndex]=millis(); 
        if(digitalRead(BUTTON)==0){
            if(deviceMode==CONFIGURATION){
                Serial.println("exitting configuration mode to SNIFF mode");
                deviceMode=SNIFF;
                digitalWrite(LED,LOW);
            }
            else{
                connectToRouter();
                deviceMode=CONFIGURATION;
                Serial.println("starting hotspot");
                digitalWrite(LED,HIGH);
                WiFi.mode(WIFI_AP_STA);
                WiFi.softAP(macAddress,"crocodile");
                Serial.print("UDP server started at port ");
                Serial.println(localPort);
                Udp.begin(localPort);
            }
        }
    }
}

void setup(){
    // Open serial communications and wait for port to open:
    Serial.begin(115200);
    ////set interrupt routine for button
    pinMode(BUTTON,INPUT_PULLUP);
    //attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, CHANGE);
    sensors.begin();
    getMac();
    EEPROM.begin(512);
    pinMode(A0, INPUT);
    pinMode(MOTION_PIN, INPUT);
    pinMode(TEMPERATURE_PIN, INPUT_PULLUP);
    pinMode(LED, OUTPUT);
    int index=0;
    ////check for stored wifi credentials
    bool wifi=true;
    for(int i=0;i<6;i++){
        if( EEPROM.read(i)!=wifiKeyword[i]){
            wifi=false; 
        }
    }
    //get stored wifi credentials/stored mac addresses
    if(wifi){
        Serial.println("stored credentials found");
        String ssid=""; 
        String passwd="";
        bool onPasswd=false;
        for(index=6;index<512;index++){
            char c=EEPROM.read(index);
            if(c==','){
                onPasswd=true;
            }
            else if (c==')'){
                break;
            }
            else if (onPasswd){
                passwd+=c;
            }
            else{
                ssid+=c;
            }
        }
        ssid.toCharArray(ssidBuf, ssid.length()+1);
        passwd.toCharArray(passwdBuf, passwd.length()+1);
        Serial.print("ssid:");
        Serial.println(ssidBuf);
        Serial.print("password:");
        Serial.println(passwdBuf);
        index+=1;
        numMacs=EEPROM.read(index);
        index+=1;
        Serial.print(numMacs);
        Serial.print(" stored macs:");
        macTable=(byte*) os_malloc(6*numMacs*sizeof(char));
        lastSeen=(unsigned long int*) os_malloc(numMacs*sizeof(unsigned long int));
        if(numMacs > 0){
            int finalIndex=index + (numMacs*6);
            int i=0;
            for(;index<=finalIndex;index++){
                macTable[i]=EEPROM.read(index);
                Serial.print(EEPROM.read(index));
                Serial.print(" ");
                i++;
            }
            Serial.print(" ");
            printHex(macTable, 0, 6*numMacs);
        }
        for(int i=0; i<numMacs; i++){
            lastSeen[i]=0;
        }
        Serial.print("\n"); 
    }
    //get wifi credentials through hotspot if there are none stored 
    else{
        Serial.println("no stored credentials found");
        deviceMode=CONFIGURATION;
    }
    if(digitalRead(BUTTON)==0){
        Serial.println("button pressed, going into configuration made");
        deviceMode=CONFIGURATION;
    }
    if(deviceMode==CONFIGURATION){
        Serial.println("starting hotspot");
        digitalWrite(LED,HIGH);
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(macAddress,"crocodile");
        Serial.print("UDP server started at port ");
        Serial.println(localPort);
        Udp.begin(localPort);
        while(deviceMode==CONFIGURATION){
            handleUDP();
        }
        digitalWrite(LED,LOW);
        Serial.println("exitting configuration mode");
        WiFi.mode(WIFI_STA);
        deviceMode=CONNECT;
    }
    deviceMode=(EEPROM.read(511)==1); 
    if(deviceMode==SNIFF){
        Serial.println("going into sniff mode");
        promiscMode();
    }
    //deviceMode==CONNECT
    else{
        Serial.println("going into connect mode");
        connectToRouter();
    }
    Serial.println();
}

unsigned long lastPrint=0;
void loop(){
    delay(10);
    if(deviceMode==CONFIGURATION){
        handleUDP();
    }
    if(deviceMode==SNIFF && millis()-lastPrint>=5000){
        printMacTable();
        lastPrint=millis();
        if(millis()-lastHop>HOP_INTERVAL){
            channelHop();
        }
        if(millis()-startedSniffing>TRANSMIT_INTERVAL){
            for(int i=0;i<numMacs;i++){
                if(lastSeen[i]!=0){
                    EEPROM.write(510-i,1);
                }
                else{
                    EEPROM.write(510-i,0);
                }
            }
            EEPROM.write(511,1);
            EEPROM.commit();
            wifi_promiscuous_enable(0);
            Serial.println("restarting");
            ESP.restart();
            while(true){

            }
        }
    }

    if(deviceMode==CONNECT){
        //send data to the server 
        lastTransmit=millis();

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
        Udp.beginPacket(serverIP, destPort);
        Udp.print(macAddress);
        Udp.print("-");
        Udp.print("motion-");
        Udp.print(motionString);
        Udp.endPacket();

        delay(100); 
        //send temperature
        Udp.beginPacket(serverIP, destPort);
        Udp.print(macAddress);
        Udp.print("-");
        Udp.print("temperature-");
        Udp.print(tempString);
        Udp.endPacket();

        delay(100); 
        //send light level
        Udp.beginPacket(serverIP, destPort);
        Udp.print(macAddress);
        Udp.print("-");
        Udp.print("light-");
        Udp.print(lightString);
        Udp.endPacket();


        for(int i=0; i<numMacs;i++){ 
            if(EEPROM.read(510-i)){
                char mac[13];
                for(int j=0;j<12;j++){
                    if(j%2==0){
                        mac[j]=hex[macTable[(6*i)+(j/2)]>>4];
                    }
                    else{
                        mac[j]=hex[macTable[(6*i)+(j/2)]&0x0F];
                    }
                }
                mac[12]='\0';
                Serial.println(mac);
                delay(100); 
                Udp.beginPacket(serverIP, destPort);
                Udp.print(macAddress);
                Udp.print("-");
                Udp.print("network-");
                Udp.print(mac);
                Udp.endPacket();
            }
        }

        EEPROM.write(511,0);
        EEPROM.commit();
        Serial.println("waiting for response from server");
        long unsigned int startedWaiting=millis();
        while(millis()-startedWaiting < 5000){
            handleUDP();
            //so wifi stack doesn't fail
            delay(10);
        }
        Serial.println("restarting");
        ESP.restart();
    }
}
