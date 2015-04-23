//**************************************************************************
// PROJECT: relay05
// Version: V01.00
// Author: Mehmet Bora YILDIZ
// Update: 16.06.2014
//**************************************************************************
//**************************************************************************
 

//**************************************************************************
//INCLUDE LIBRARIES
//**************************************************************************
#include "mbed.h"
#include "rtos.h"
#include "EthernetInterface.h"
#include "buffered_serial.h"
#include <string>
#include <iostream>
#include <stdlib.h>
using namespace std;


//**************************************************************************
//DEFINITIONS
//**************************************************************************
//UDP
#define UDP_PORT   51983
#define RS485_Read   0
#define RS485_Write   1
#define feedback_IP   "192.168.1.51"




//**************************************************************************
//GLOBAL VARIABLES
//**************************************************************************
//GLOBAL
char feedbackString[255];

//SYSTEM CONFIG
int deviceID = 1;
string IPAddress = "192.168.1.101";
string SubnetMask = "255.255.255.0";
string Gateway = "192.168.1.1";

//STATUS
bool statusRelay1 = false;
bool statusRelay2 = false;
bool statusRelay3 = false;
bool statusRelay4 = false;
bool statusRelay5 = false;

//RS485
char tx_line[255];
char rx_line[255];

//ETHERNET
EthernetInterface ethernet;

//UDP
char UDP_buffer[255];
UDPSocket UDP_server;
Endpoint UDP_endpoint;

//Packet Parser
char Packet_DeviceID;
char Packet_DataType;
char Packet_Channel;
char Packet_Data;

//Mutexs
Mutex PacketParser_Mutex;
Mutex PacketHandler_Mutex;
Mutex WriteRelay_Mutex;

//GPIO
int inputLowcounter[5];
int inputHighcounter[5];
bool inputFlag[5];

//LOCAL FILE SYSTEM
FILE *file;
LocalFileSystem local("local");               
char line[255];

//**************************************************************************
//HARDWARE CONFIGURATION
//**************************************************************************

//USB
Serial USB(USBTX, USBRX);                       // tx, rx : USB

//RS485
BufferedSerial RS485(p13,p14);                  // UART1 - p13, p14
DigitalOut RS485_Mode(p20);

//RELAY
DigitalOut Relay1(p15);                         // Relay1
DigitalOut Relay2(p16);                         // Relay2
DigitalOut Relay3(p17);                         // Relay3
DigitalOut Relay4(p18);                         // Relay4
DigitalOut Relay5(p19);                         // Relay5

//GPIO
DigitalInOut GPIO1(p26);                        // GPIO2
DigitalInOut GPIO2(p25);                        // GPIO2
DigitalInOut GPIO3(p24);                        // GPIO3
DigitalInOut GPIO4(p23);                        // GPIO4
DigitalInOut GPIO5(p22);                        // GPIO5

//LED
DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);
DigitalOut led4(LED4);


//**************************************************************************
//FUNCTION DECLERATIONS
//**************************************************************************
//MAIN START
void mainStart();

//GLOBAL FUNCTIONS
void append(char* s, char c);
void packetParser(char* packet);
void packetHandler();
void commandHandler();
void writeRelay(char channel, char value);
void relayStatusFeedback(char channel, char value);

//LOCAL FILE SYSTEM 
void read_ConfigFile();
string parse_Line(const char* ptrLine);

//**************************************************************************
//THREADS
//**************************************************************************

//RS485_thread
void RS485_thread(const void *args)
{

    while (true) {
        led2 = !led2;
                     
        //RS485 read
        RS485_Mode = RS485_Read; 
        
        //Wait for packet receive       
        printf("Waiting for RS485 data...\n"); 
          
        //Read a line from the large rx buffer from rx interrupt routine
        RS485.read_line();
        
        //Print Data & CheckSum
        char Checksum = 0;
        printf("RS485 Data: "); 
        for(int i = 0 ; i < RS485.packetLength; i++)
        {
            printf("%c", RS485.rx_data_bytes[i]); 
            if (i != RS485.packetLength - 1 ) Checksum = Checksum + RS485.rx_data_bytes[i];
        }
        
        //CheckSum
        if(Checksum == RS485.rx_data_bytes[RS485.packetLength - 1])
        {
            //Packet Parser 
            packetParser(RS485.rx_data_bytes); 
                    
             //Packet Handler
            packetHandler();     
        }
        
        //Clear RS485 Buffer
        memset(RS485.rx_data_bytes, 0x00, 255);
                
    }    
}

//UDP_thread
void UDP_thread(void const *args) {
    while (true) {
        //Thread Debug Led
        led3= !led3;
        
        //Wait for packet receive       
        printf("Waiting for UDP packet...\n"); Thread::wait(100); 
        int n = UDP_server.receiveFrom(UDP_endpoint, UDP_buffer, sizeof(UDP_buffer));
                        
        //Packet Parser 
        packetParser(UDP_buffer); 
        
        //Packet Handler
        packetHandler();       
        
        //Clear UDP_buffer
        memset(UDP_buffer, 0x00, 255);
        
        //Thread wait in ms   
        Thread::wait(250);         
    }
}

//GPIO_thread
void GPIO_thread(void const *args)
{
    while (true) {
        //Thread Debug Led
        led4= !led4;  
        
        //Check GPIO
        if( GPIO1 == 0) inputLowcounter[0]++; if( GPIO1 == 1) inputHighcounter[0]++;       
        if( GPIO2 == 0) inputLowcounter[1]++; if( GPIO2 == 1) inputHighcounter[1]++;
        if( GPIO3 == 0) inputLowcounter[2]++; if( GPIO3 == 1) inputHighcounter[2]++;
        if( GPIO4 == 0) inputLowcounter[3]++; if( GPIO4 == 1) inputHighcounter[3]++;
        if( GPIO5 == 0) inputLowcounter[4]++; if( GPIO5 == 1) inputHighcounter[4]++;

        //GPIO State Result
        for(int i = 0; i < 5; i++)
        {
            if (inputLowcounter[i] > 45) 
            {                
                //Print status
                printf("GPIO%d LOW \n", (i+1));
                
                //Toggle Relay
                if(inputFlag[i] == false)
                {
                    switch (i)
                    {
                        case 0:
                            writeRelay(1, !Relay1);
                            break;
                        case 1:
                            writeRelay(2, !Relay2);
                            break;
                        case 2:
                            writeRelay(3, !Relay3);
                            writeRelay(4, Relay3);
                            writeRelay(5, Relay3);
                            break;
                        case 3:
                            writeRelay(4, !Relay4);
                            break;
                        case 4:
                            writeRelay(5, !Relay5);
                            break;
                    }
                }
                
                //Set input flag
                inputFlag[i] = true;
                
                //Clear counters
                inputHighcounter[i] = 0;
                inputLowcounter[i] = 0;
                               
            }
    
            if (inputHighcounter[i] > 5) 
            {                
                //Reset input flag
                inputFlag[i] = false;
                
                //Clear counters
                inputHighcounter[i] = 0;
                inputLowcounter[i] = 0;
            }
         }
        
        //Thread wait in ms              
        Thread::wait(5);        
    }
}


//**************************************************************************
//MAIN
//**************************************************************************
int main()
{
    //Initialize the System
    mainStart();
    
    //Start Threads
    Thread threadUDP(UDP_thread);
    Thread threadGPIO(GPIO_thread);
    Thread threadRS485(RS485_thread);

    //Infinite Loop 
    while (1) 
    {
    led1 = !led1;        
    Thread::wait(1000);
    }
}

//**************************************************************************
//MAIN START
//**************************************************************************
void mainStart()
{

    //SYSTEM CONFIGURATION
    read_ConfigFile();
    
    //ETHERNET Use Static
    ethernet.init(IPAddress.c_str(), SubnetMask.c_str(), Gateway.c_str());     
    ethernet.connect();
    printf("IP Address is %s\n", ethernet.getIPAddress());
    
    //UDP Init    
    UDP_server.bind(UDP_PORT);
    
    //RS485 Init
    RS485.baud(9600);
    RS485_Mode = RS485_Read;

    //Send "USB Initialized...\n" to USB
    USB.baud(9600);
    USB.printf("USB Initialized...\n");
    
    //GPIO
    GPIO1.mode(PullUp);
    GPIO2.mode(PullUp);
    GPIO3.mode(PullUp);
    GPIO4.mode(PullUp);
    GPIO5.mode(PullUp);
    
    //Write System Initialize OK... to USB
    printf("System Initialize OK...\n");
}


//**************************************************************************
//ROOT PROTOCOL HANDLER
//**************************************************************************
//Packet Parser
void packetParser(char* packet)
{
    //Lock Mutex   
    PacketParser_Mutex.lock();   
  
    //Device ID
    Packet_DeviceID = packet[1];

    //Data Type
    Packet_DataType = packet[2];
   
    //Channel
    Packet_Channel = packet[3];
    
    //Data
    Packet_Data = packet[5];
    
    //Unlock Mutex
    PacketParser_Mutex.unlock();
    
}

//Packet Handler
void packetHandler()
{
   //Lock Mutex 
   PacketHandler_Mutex.lock();

   //Check the packet is for this Device
   if (Packet_DeviceID == deviceID)
   {
        commandHandler();      
   }
   
   //Unlock Mutex
   PacketHandler_Mutex.unlock();
}

//Command Handler
void commandHandler()
{
    //Write Command
    if (Packet_DataType == 'W')
    {
        writeRelay(Packet_Channel, Packet_Data);
    }     
}

//Write Relay
void writeRelay(char channel, char value)
{
    //Lock Mutex
    WriteRelay_Mutex.lock();
    
    switch(channel){
        case 1:
            Relay1 = value;  
            statusRelay1 = value;  
            relayStatusFeedback(channel, value); 
            break;   
        case 2:
            Relay2 = value; 
            statusRelay2 = value; 
            relayStatusFeedback(channel, value); 
            break; 
        case 3:
            Relay3 = value; 
            statusRelay3 = value; 
            relayStatusFeedback(channel, value); 
            break; 
        case 4:
            Relay4 = value; 
            statusRelay4 = value; 
            relayStatusFeedback(channel, value); 
            break; 
        case 5:
            Relay5 = value; 
            statusRelay5 = value; 
            relayStatusFeedback(channel, value); 
            break; 
            
        //All Channels
        case 255:
            Relay1 = Relay2 = Relay3 = Relay4 = Relay5 = value;  
            statusRelay1 = statusRelay2 = statusRelay3 = statusRelay4 = statusRelay5 = value;  
            relayStatusFeedback(channel, value); 
            break;  
        default:
    }  
    
    //Unlock Mutex
    WriteRelay_Mutex.unlock();
}

//Relay Status Feedback
void relayStatusFeedback(char channel, char value)
{
    
    //Clear feedbackstring
    memset(feedbackString, 0x00, 255);   
    
    //Feedback received packet
    feedbackString[0] = 62;                                                                     //Start of Data                                         
    feedbackString[1] = deviceID;                                                               //Device ID
    feedbackString[2] = 'S';                                                                    //Data Type : Status
    feedbackString[3] = channel;                                                                //Channel
    feedbackString[4] = 1;                                                                      //DataLength
    feedbackString[5] = value;                                                                  //Data
    feedbackString[6] = feedbackString[0] + feedbackString[1] + feedbackString[2] +             //Checksum                                                     
                        feedbackString[3] + feedbackString[4] + feedbackString[5];    
                        
                        
    //Send feedback data Broadcast to UDP    
    UDP_endpoint.set_address(feedback_IP, 51983); 
    UDP_server.sendTo(UDP_endpoint, feedbackString, 7);
      
    //Send feedback data to RS485 
    wait_ms(8);
    RS485_Mode = RS485_Write;                                            //Thread::wait(100); //9600bps => 48bits(6Bytes) = 5ms
    RS485.send_line(feedbackString); wait_ms(8);
    RS485_Mode = RS485_Read;
    
     //Send feedback data to USB
    printf("Status Message: "); 
    for(int i = 0 ; i < 7; i++){
        printf("%c", feedbackString[i]); 
    }
    
}


//**************************************************************************
//LOCAL FILE
//**************************************************************************


//Read Local File System Config File and set the deviceID and IP
void read_ConfigFile()
{
    printf("Reading Config File...\n");
     
    file = fopen("/local/Config.txt", "r");              
    char line[64];
       
    if (file != NULL) 
    {
        
        //read the line #1,#2,#3,#4
        for (int i = 0; i < 6; i++)
        { 
            fgets(line, 64, file);
            
            switch(i)
            {
                case 0: 
                    deviceID = atoi(parse_Line(line).c_str());
                    printf("deviceID: %d\n", deviceID);
                    break;
                case 1: 
                    IPAddress = parse_Line(line);
                    printf("IPAddress: %s\n", IPAddress);
                    break;
                case 2: 
                    SubnetMask = parse_Line(line);
                    printf("SubnetMask: %s\n", SubnetMask);
                    break;
                case 3: 
                    Gateway = parse_Line(line); 
                    printf("Gateway: %s\n", Gateway);
                    break;
            }
            
        }

        //Close the file
        fclose(file);
              
    }
    else
    {
        printf("Error: There is no Config file\n");   
    }
}


//Parse Line
string parse_Line(const char* ptrLine)
{
    char field[64];
    int n;
    int index = 0;
    string returnStr;

    //Parse Line with ":" delimiter
    while ( sscanf(ptrLine, "%63[^:]%n", field, &n) == 1 ) 
    {
        index++;
        ptrLine += n;                       /* advance the pointer by the number of characters read */
        
        if ( *ptrLine != ':' ) 
        {
            break;                          /* didn't find an expected delimiter, done? */
        }
        
        ++ptrLine;                          /* skip the delimiter */

        //Get desired string
        if (index == 2) 
        {
            returnStr = field;
        }
    }
    
    return returnStr;
}



//**************************************************************************
// GLOBAL FUNCTIONS
//**************************************************************************

//Append one char to string
void append(char* s, char c)
{
    int len = strlen(s);
    s[len] = c;
    s[len+1] = '\0';
}








