/*
 * 4. uart_rx.c
 *
 *	Objective:  Understand the UART (universal asynchronous receiver and transmitter)
 *				We will use the UART to receive a byte from the user terminal.
 *
 *  Function:  Read character in, write character back out and change LED state.
 *
 * Created: 2/9/2018 9:32:26 PM
 * Author : Jon Kim
 */ 

#define F_CPU	20000000

#define BAUD_RATE	9600
#define BAUD_VALUE	(F_CPU/16/BAUD_RATE - 1)		// 129 @9600 (Table 24.7)

#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>


void put_char(unsigned char ch)
{
	while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0 = ch;
}

void put_str(unsigned char* str)
{
	while (*str != 0)
		put_char(*str++);
}

unsigned char get_char(void)
{
	while ( !( UCSR0A & (1<<RXC0)) );
	return UDR0;				
}


int main(void)
{
	int i = 0;
	unsigned char ch;
	unsigned char str[128], data[128];

	DDRB = (1<<DDB1);
	
	UBRR0H = (unsigned char)(BAUD_VALUE>>8);	
	UBRR0L = (unsigned char)(BAUD_VALUE);
	UCSR0C = (3<<UCSZ00);
	UCSR0B = (1<<TXEN0)|(1<<RXEN0);

    while (1) 
    {
		put_str("Type any keys: ");

		sprintf(str,"[%c]\r\n", get_char());
		put_str(str);

		PORTB = ~PORTB;
		//_delay_ms(500);
    }
}