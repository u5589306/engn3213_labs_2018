/*
 * 7_uart_interrupt_tx_buffered.c
 *
 *	Objective:  Understand the UART (universal asynchronous receiver and transmitter)
 *				We will use the UART to echo back a received byte using UDRE interrupt. 
 *
 * Created: 2/9/2018 9:32:26 PM
 * Author : Jon Kim
 */ 

#define F_CPU 20000000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define OCR1_VALUE_1HZ	19531/2
#define OCR1_VALUE_10HZ	1953/2

#define BAUD_RATE	9600
#define BAUD_VALUE	(F_CPU/16/BAUD_RATE - 1)		// 129 @9600 (Table 24.7)

volatile int rate = 19531;

ISR(TIMER1_COMPA_vect)
{
	OCR1AH = (rate)>>8;		// Write high byte first
	OCR1AL = rate;
	//PORTB = ~PORTB;	// this is equivalent to set the COM0A0 bit in TCCR1A
}

void put_char(unsigned char ch)
{
	while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0 = ch;
}

void put_str(char* str)
{
	while (*str != 0)
		put_char(*str++);
}

unsigned char get_char(void)
{
	while ( !( UCSR0A & (1<<RXC0)) );
	return UDR0;				
}

void get_str(char* str)
{
	unsigned char ch;
	while ((ch = get_char()) != '\r'){
		*str++ = ch;
	}
	*str = '\0';

}

int main(void)
{
	DDRB = (1<<DDB1);

	TCCR1A = (1<<COM0A0);
	TCCR1B = (1<<WGM12)|(1<<CS12)|(0<<CS11)|(1<<CS10);	// 20Mhz divided by 1024
	OCR1AH = OCR1_VALUE_10HZ>>8;	// Write high byte first
	OCR1AL = OCR1_VALUE_10HZ;
	TIMSK1 = (1<<OCIE0A);
	
	UBRR0H = (unsigned char)(BAUD_VALUE>>8);
	UBRR0L = (unsigned char)(BAUD_VALUE);
	UCSR0C = (3<<UCSZ00);
	UCSR0B = (1<<TXEN0)|(1<<RXEN0);
	char str[128], data[128], myrate[15];
	int i = 0;
	sei();
	int freq;
	while (1)
	{
		// do some stuff
		put_str("Please enter the frequency: ");
		get_str(data);
		sprintf(str,"[%d][%s]\r\n", ++i, data);
		put_str(str);
		freq = atoi(str);
		rate = (OCR1_VALUE_1HZ / freq);
		itoa(rate,myrate,10);
		put_str(myrate);
		_delay_ms(1000);
	}
}