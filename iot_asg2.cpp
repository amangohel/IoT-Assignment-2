//Author
//Aman Gohel

//IOT Assignment 2 - Sensor Data, Network and Reliability
//This is my submission for the 2018 IOT Assessment 2 which taught me alot about
//UDP and how to periodically send data as well as validate that data and ensure
//there is a consistent connection between my client and the server.

//I would like to acknowledge the usage a library for my implementation of CRC-8-CCITT.
//The library has come from the following source: https://3dbrew.org/wiki/CRC-8-CCITT
//Both the crc8.cpp and crc8.h files are required from this page in order to make use of
//the ccitt function below in my transmit_packet function.

//All rights and ownership of the CRC8 library go towards the developers. 

//Minimal imports for assessment 2
#include "mbed.h"
#include "LM75B.h"

//Recommended libraries to simplify the UDP connection
#include "easy-connect.h"

//Additional libraries you use
#include "EthernetInterface.h"
#include "C12832.h"
#include "UDPSocket.h"
#include "crc8.h"
#include <math.h>

C12832 lcd(D11, D13, D12, D7, D10); //for LCD
LM75B tmp(D14,D15); //for temperature readings
Ticker ticker; //ticker for transmitting packet
Ticker ack_ticker; //ticker for requesting acknowledgement

UDPSocket socket; //udp socket

InterruptIn sw2(SW2); //sw2 interrupt
InterruptIn sw3(SW3);//sw3 interrupt

//joystick interrupts
InterruptIn up(A2);
InterruptIn down(A3);
InterruptIn left(A4);
InterruptIn rightt(A5);
InterruptIn fire(D4);


volatile int ec = 0; //flag that assumes connection's been established
volatile int ack_f=0; //acknowledgement flag
int retryAck = 0;

uint16_t temperature = tmp.read(); //unsigned 16bit int for reading temp.
uint8_t first = temperature; //takes the first 8 bits of the temp reading
uint8_t second = (temperature >> 8); //takes second 8 bits and shifts right 8 times.

uint8_t firstSID = 0x8e; //first 8 bit value of 16bit sender ID
uint8_t secondSID = 0x95; //second 8 bit value of 16bit sender ID

const char* host = "lora.kent.ac.uk"; //external host...

const int port = 1789; //port for the given host above
uint8_t packet[8]; //unsigned 8 bit array for packet

int sequence_number=0; //sequence number, incremented with each packet sent

//button bit masks - each button is assigned a mask value
const unsigned char sw2_m = 0x01; //sw2 button flag = 0000 0001
const unsigned char sw3_m = 0x02; //sw3 button flag = 0000 0010
const unsigned char up_m = 0x04; //upm button flag = 0000 0100
const unsigned char down_m = 0x08; //down_m button flag = 0000 1000
const unsigned char left_m = 0x10; //left_m button flag = 0001 0000
const unsigned char right_m = 0x20; //right_m button flag = 0010 0000
const unsigned char fire_m = 0x40; //fire_m button flag = 0100 0000

const unsigned char ack_req_f = 0x01; //ack_req_f flag = 0000 0001
const unsigned char ccitt_f = 0x02; //ccitt_f flag = 0000 0010
const unsigned char retry_f = 0x04; //retry_f flag = 0000 0001 - unused

uint8_t button_flag; //unsigned 8 bit value that contains the various bits set for each flag.
uint8_t packet_options; //unsigned 8 bit value that contains various bits set for packet options (ack/ccitt/retry)

int base_value = 2; //base value assigned for wait time and exponential backoff.

//method to set ec flag
void set_est_conn(void)
{
    ec = 1;
}

//method to set acknowledgement flag
void set_ack_flag(void)
{
    
    ack_f = 1;
}

//receive_packet
//The purpose of this function is to return something from the server after a
//receive from request is made. This method assumes that the acknowledgement flag
//has been set before continuing execution. If the size of the value returned from 
//the server is less than 0, then we can assume that we havent got an ack.
//The function will then perform some exponential back off until an acknowledgment is
//received. If an acknowledgement is received, the size will be > 0 and we return
//the size of the data returned which is 3, since it is the sender ID + seq number
//of the packet that the call returned. We then reset the ack flag ready for the 
//next call.
void receive_packet()
{
    lcd.cls();
    lcd.locate(0, 5);
    lcd.printf("Inside of receive packet...");
    
    if(ack_f){
        
    int JitterRand = 0;
    //receive packet from lora
    char a[3];
    socket.set_timeout(2.0);
    int size = socket.recvfrom(NULL, a, sizeof(a));
    if(size < 0)
    {
        wait(base_value);
    
        base_value = base_value * 2;
        JitterRand = (rand() % 45) + 5;
        
        if(base_value > 16){
            JitterRand = JitterRand - base_value;
        }
        
        lcd.cls();
        lcd.locate(0, 5);
        retryAck++;
        lcd.printf("No Acknowledgement received, retrying...");
    }
    
    if(size > 0){
        lcd.cls();
        lcd.locate(0, 5);
        lcd.printf("Successful connection");
        lcd.locate(1, 5);
        lcd.printf("Acknowledgement received: %i\n", size);
    }
        ack_f=0;
    }
    
}

//transmit_packet
//The transmit packet function is called every 10 seconds by a ticker and places
//the relevant values into each index of the packet array. Once the packet's 
//values have been assigned, a call is made with the packet to the host, the
//button_flag and packet_options are both reset, ready for the next call to the transmit packet
//method and the sequence number is incremented to show the ascending order of the
//packets.

//Please note the use of the external CRC8 implementation on line 169.
void transmit_packet()
{ 
    packet_options |= ccitt_f;

    packet[0] = secondSID;
    packet[1] = firstSID;
    packet[2] = sequence_number;
    packet[3] = packet_options;
    packet[4] = first;
    packet[5] = second;
    packet[6] = button_flag;

//Note - both library files are required for the functionality of this function.
//<-------------CRC-8-CCITT library code begins------------->
    packet[7] = crc8ccitt(packet, 7);
//<-------------CRC-8-CCITT library code ends------------->
    
    socket.sendto(host, port, packet, sizeof(packet));

    packet_options = 0;
    button_flag = 0;
    sequence_number++;
}


//inclusive OR's the button flag and bit mask value of fire_m, defined above.
void fire_pressed(void)
{
    button_flag |= fire_m;
}

//inclusive OR's the button flag and bit mask value of up_m, defined above.
void up_pressed(void)
{
    button_flag |= up_m;
}

//inclusive OR's the button flag and bit mask value of down_m, defined above.
void down_pressed(void)
{
    button_flag |= down_m;
}

//inclusive OR's the button flag and bit mask value of left_m, defined above.
void left_pressed(void)
{
    button_flag |= left_m ;
}

//inclusive OR's the button flag and bit mask value of right_m, defined above.
void right_pressed(void)
{
    button_flag |= right_m;
}

//inclusive OR's the button flag and bit mask value of sw2_m, defined above.
void sw2_pressed(void)
{
    button_flag |= sw2_m;
}

//inclusive OR's the button flag and bit mask value of sw3_m, defined above.
void sw3_pressed(void)
{
    button_flag |= sw3_m;
}

//Method that instantiates each button and when each are pressed, assign the
//various flags.
void setup_tickers_buttons(void)
{
    fire.fall(fire_pressed);
    up.fall(up_pressed);
    down.fall(down_pressed);
    left.fall(left_pressed);
    rightt.fall(right_pressed);
    sw2.fall(sw2_pressed);
    sw3.fall(sw3_pressed);
}

//Establishes the connection by setting up the ethernet interface and the UDP
//socket connections.
void establish_connection(void)
{
    EthernetInterface eth;
    eth.connect();
    lcd.cls();
    lcd.locate(0, 3);
    lcd.printf("The current local IP address is: %s", eth.get_ip_address());
    
    socket.open(&eth);
}

//Main
//The main method firstly calls establish connection and setup_tickers_buttons
//to establish a connection to the server and instantiate each button such that
//if one is set, then the relevant button flag can be set.

//Entering into the loop, if the ec flag is set (via ticker), then make a call to
//the transmit packet function which will take all the relevant data, append and send it.

//the ack_f flag is set via the ack_ticker and will be called every 60 seocnds.
//This will send one request to the server for an acknowledgement at most ever
//60 seconds. Here the bit is being set in the packet options flag that we are
//requesting an ack, and only if the flag has been set will the call to receive
//packet be made.
int main() {
    
    establish_connection();
    setup_tickers_buttons();
    
    ticker.attach(set_est_conn, 10.0);
    ack_ticker.attach(set_ack_flag, 60.0);
    
    while (1) {
             
        if(ec)
        {
            transmit_packet();
            ec=0;
            if(ack_f) 
            {
                packet_options |= ack_req_f;
                if(packet_options & ack_req_f)
                {
                    receive_packet();
                }
            }
        }
    } 
}
