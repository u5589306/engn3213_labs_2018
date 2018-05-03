/*
 * 2. uart_tx.c
 *
 *	Objective:  Understand the UART (universal asynchronous receiver and transmitter)
 *				We will use the UART to transmit a byte data
 *
 * Created: 2/9/2018 9:32:26 PM
 * Author : Jon Kim
 */ 

// define clock rate as the external oscillator is being used.
#define F_CPU	20000000

#define BAUD_RATE	9600
#define BAUD_VALUE	(F_CPU/16/BAUD_RATE - 1)		// 129 @9600 (Table 24.7)

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	DDRB = (1<<DDB1);
	
	UBRR0H = (unsigned char)(BAUD_VALUE>>8);	
	UBRR0L = (unsigned char)(BAUD_VALUE);
	UCSR0C = (3<<UCSZ00);
	UCSR0B = (1<<TXEN0);
	
	PORTB &= ~(1<<DDB1);
	
    while (1) 
    {
		while ( !( UCSR0A & (1<<UDRE0)) );
		UDR0 = 'A';							
	
		PORTB = ~PORTB;
		_delay_ms(500);
    }
}