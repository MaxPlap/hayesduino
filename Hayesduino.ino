/***********************************************
HAYESDUINO PROJECT - COPYRIGHT 2013, PAYTON BYRD

Project homepage: http://hayesduino.codeplex.com
License: http://hayesduino.codeplex.com/license
***********************************************/
#include "Arduino.h"

#ifdef UBRR1H
#define __MEGA__
#define POWER_LED 29
#define DEBUG 1
#else
#define __UNO__
#endif

#include "DEBUG.h"

#include "libraries/Ethernet/EthernetClient.h"
#include "ModemBase.h"
#include "libraries/Ethernet/EthernetServer.h"
#include "Ethernet.h"
#include "Dns.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SPI.h"
#include "Global.h"
#include "EEPROM.h"
#include "HardwareSerial.h"

#include <SD.h>


#if DEBUG == 1 && !defined(__UNO__)
#include "Logger.h"
#endif

int IEC_ATN_IN = A5;
int IEC_RESET_IN = A4;
int IEC_CLK_IN = A3;
int IEC_CLK_OUT = A2;
int IEC_DATA_IN = A1;
int IEC_DATA_OUT = A0;

int SPI_MOSI = 11;
int SPI_MISO = 12;
int SPI_CLK = 13;
int CS_WIZ = 10;
int CS_FLASH = 9;

boolean statuses[4];


byte mac[6];

ModemBase modem; 
EthernetClient client;
EthernetServer EthServer(23);

File myLogFile;

int currentClient = MAX_SOCK_NUM;


void disconnectClient(EthernetClient *client)
{
	currentClient = MAX_SOCK_NUM;
	if(client) client->stop();
	myLogFile.flush();
	myLogFile.close();
}

void dialout(char * host, ModemBase *modm)
{
	char* index;
	uint16_t port = 23;
	String hostname = String(host);
	char hostnamebuffer[81];

	for(int i=0; i<81; ++i) hostnamebuffer[i] = '\0';

	if((index = strstr(host, ":")) != NULL)
	{
		index[0] = '\0';
		hostname = String(host);
		port = atoi(index + 1);
	}

	client = EthernetClient();

	if(hostname == "5551212")
	{
		hostname = "qlink.lyonlabs.org";
		port = 5190;
	}

	hostname.toCharArray(hostnamebuffer, sizeof(hostnamebuffer), 0U);

	int ret = 0;
	DNSClient dns;
	IPAddress remote_addr;
	//modem.println(hostnamebuffer);
	dns.begin(Ethernet.dnsServerIP());
	ret = dns.getHostByName(hostnamebuffer, remote_addr);

	if(ret == 1)
	{
		//modm->print(F("connecting to: "));
		//remote_addr.printTo(*modm);
		//modm->println();

		if(client.connect(remote_addr, port))
		{
			currentClient = client.getSock();
			modm->connectOut();
		}
		else
		{
			modm->println(F("NO ANSWER"));
		}
	}
	else
	{
		modm->println(F("COULD NOT RESOLVE HOSTNAME:"));
		modm->println(hostnamebuffer);
	}

	delay(1000);
}

void setup()
{

	pinMode(IEC_ATN_IN, INPUT);
	pinMode(IEC_RESET_IN, INPUT);
	pinMode(IEC_DATA_IN, INPUT);
	pinMode(IEC_CLK_IN, INPUT);
	pinMode(IEC_DATA_OUT, OUTPUT);
	pinMode(IEC_CLK_OUT, OUTPUT);

	pinMode(SPI_MOSI, OUTPUT);
	pinMode(SPI_MISO, INPUT);
	pinMode(SPI_CLK, OUTPUT);
	pinMode(CS_FLASH, OUTPUT);
	pinMode(CS_WIZ, OUTPUT);

	pinMode(53, OUTPUT);
	digitalWrite(10, HIGH);
	pinMode(4, OUTPUT);
	digitalWrite(4, HIGH);

#ifndef __UNO__
	if(Serial) Serial.begin(115200);
	Serial.println("Initialized serial.");
#endif

#ifdef __UNO__
	modem.begin(&Serial, &disconnectClient, &dialout);
#else
	Serial1.begin(2400);
	modem.begin(&Serial1, &disconnectClient, &dialout);
	//modem.factoryReset();

#endif

	//Serial.println("Before SD.begin");

	//if(!SD.begin(4))
	//{
	//	Serial.println("Could not initialize SD card.  FREEZING.");
	//	//while(true);
	//}


	byte usedDHCP = 0;

	mac[0] = EEPROM.read(MAC_1);
	mac[1] = EEPROM.read(MAC_2);
	mac[2] = EEPROM.read(MAC_3);
	mac[3] = EEPROM.read(MAC_4);
	mac[4] = EEPROM.read(MAC_5);
	mac[5] = EEPROM.read(MAC_6);

	Serial.println("Before Ethernet.");
	if(EEPROM.read(USE_DHCP) != 0)
	{
		usedDHCP = 1;
		if(Ethernet.begin(mac) == 0)
		{
			modem.println(F("COULD NOT OBTAIN DHCP INFORMATION."));
		}
	}
	else
	{
		Ethernet.begin(mac, 
			IPAddress(
			EEPROM.read(LOCAL_IP_1), EEPROM.read(LOCAL_IP_2),
			EEPROM.read(LOCAL_IP_3), EEPROM.read(LOCAL_IP_4)),
			IPAddress(
			EEPROM.read(DNS_IP_1), EEPROM.read(DNS_IP_2),
			EEPROM.read(DNS_IP_3), EEPROM.read(DNS_IP_4)),
			IPAddress(
			EEPROM.read(GATEWAY_IP_1), EEPROM.read(GATEWAY_IP_2),
			EEPROM.read(GATEWAY_IP_3), EEPROM.read(GATEWAY_IP_4)));
	}

	Serial.println("Before EthServer.begin");
	EthServer.begin();
	Serial.println("After EthServer.begin");

	pinMode(STATUS_LED, OUTPUT);

	modem.println(usedDHCP == 1 ? F("USING DHCP") : F("STATIC IP"));

	modem.print(F("DNS SERVER: "));
	Ethernet.dnsServerIP().printTo(modem);
	modem.println();

	modem.print(F("GATEWAY   : "));
	Ethernet.gatewayIP().printTo(modem);
	modem.println();

	modem.print(F("LOCAL IP  : "));
	Ethernet.localIP().printTo(modem);
	Ethernet.localIP().printTo(Serial);
	modem.println();

#ifdef __MEGA__
	pinMode(POWER_LED, OUTPUT);
	digitalWrite(POWER_LED, HIGH);
#endif

	//modem.println(digitalRead(STATUS_LED));
}

#if DEBUG == 1
char buffer[41];
#endif

void loop()
{
	if(currentClient != MAX_SOCK_NUM)
	{
		client = EthernetClient(currentClient);

		if(modem.getIsConnected() && !client.connected())
		{
			modem.disconnect(&client);
			return;
		}

		//if(!client.connected()) return;
		//else if(!modem.getIsConnected())
		//{
		//	client.println(F("modem in limbo. disconnecting."));
		//	client.stop();
		//	currentClient == MAX_SOCK_NUM;
		//}
	}

	uint8_t remoteIP[4];
	for(int i=0; i<4; ++i)
	{
		if(currentClient < MAX_SOCK_NUM
			&& i != currentClient 
			&& EthServer.connected(i))
		{
			EthernetClient *tempClient = new EthernetClient(i);


			tempClient->println(F("SYSTEM IS BUSY.  TRY AGAIN LATER."));
			tempClient->stop();
			delete tempClient;
		}
	}

	EthernetClient tempClient;
	if(
		(tempClient = EthServer.connected()) &&
		!modem.getIsConnected()
		)
	{
		delay(1000);
		if(EthServer.connected(tempClient.getSock()))
		{
			client = tempClient;
			currentClient = client.getSock();

			client.getRemoteIP(remoteIP);

			myLogFile = SD.open("bbs.log", FILE_WRITE);
			myLogFile.print(remoteIP[0]); myLogFile.print('.');
			myLogFile.print(remoteIP[1]); myLogFile.print('.');
			myLogFile.print(remoteIP[2]); myLogFile.print('.');
			myLogFile.print(remoteIP[3]); myLogFile.print('\n');

			modem.connect(&client);

			for(int i = 0; i < 25; ++i)
			{
				client.println(".");
			}
			client.println(F("CONNECTING TO SYSTEM."));
			myLogFile.println(F("CONNECTING TO SYSTEM."));
		}
	} 
	

	if(modem.getIsRinging())
	{
		modem.setIsRinging(false);
	}

	char inbound;

	if(client && client.connected())
	{
		if(!modem.getIsCommandMode() && client.available() > 0)
		{
			//digitalWrite(DCE_RTS, HIGH);
			inbound = client.read();

			modem.write(inbound);
			myLogFile.write(inbound);
			Serial.write(inbound);
		}  
		else if(!modem.getIsCommandMode() && client.available() == 0)
		{
			//digitalWrite(DCE_RTS, LOW);
		}
		else if(modem.getIsCommandMode() && client.available() > 0)
		{
			client.println(F("modem is in command mode."));
			modem.println(F("modem is in command mode."));
		}
	}
	else if(modem.getIsConnected() || 
		!modem.getIsCommandMode())
	{
		delay(5000);
		modem.disconnect(&client);
	}
	else if(!modem.getIsConnected() &&
		modem.getIsCommandMode())
	{
		//digitalWrite(DCE_RTS, LOW);
	}
	//else if(digitalRead(DTE_CTS) == HIGH)
	//{
	//	digitalWrite(DTE_RTS, LOW);
	//}

	modem.processData(&client, &myLogFile);

	//digitalWrite(DCE_RTS, HIGH);

}

void resetToModemDefaults()
{
	modem.resetToDefaults();
}
