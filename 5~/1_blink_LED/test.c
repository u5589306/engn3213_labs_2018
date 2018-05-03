/*
 * GccApplication4.c
 *
 * Created: 10/04/2018 5:07:38 PM
 * Author : u4259952
 */ 

#define F_CPU 10000000

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	DDRB = 0x02;
	
    /* Replace with your application code */
    while (1) 
    {
		PORTB = ~PORTB;
		_delay_ms(2000);
	}
}

