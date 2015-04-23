/*
    Buffered serial 1 - Setup to work with UART1 (p13,p14)
*/

#ifndef BUFFERED_SERIAL_H
#define BUFFERED_SERIAL_H

#define BUFFER_SIZE     255
#define LINE_SIZE       255
#define NEXT(x)         ((x+1)&BUFFER_SIZE)
#define IS_TX_FULL      (((tx_in + 1) & BUFFER_SIZE) == tx_out)

#include "mbed.h"
#include "rtos.h"

class BufferedSerial : public Serial
{
public:
    BufferedSerial(PinName tx, PinName rx);

    void send_line(char*);
    void read_line();
    
    char rx_data_bytes[LINE_SIZE];
    char packetLength;
    
private:
    void Tx_interrupt();
    void Rx_interrupt();
    
    IRQn device_irqn;
    
    char tx_buffer[BUFFER_SIZE];
    char rx_buffer;

    volatile int tx_in;
    volatile int tx_out;
    volatile int rx_in;
    volatile int rx_out;

    char tx_line[LINE_SIZE];

    Semaphore rx_sem;
    Semaphore tx_sem;
};

#endif