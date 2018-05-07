/*
 * 8_timer_interrupt.c
 *
 *	Objective:  Understand the timer's CTC (Clear Timer on Compare match) mode operation 
 *				to generate a desired frequency of the timer interrupt event. 
 *				The timer period value computed at http://eleccelerator.com/avr-timer-calculator/
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

ISR(TIMER1_COMPA_vect)
{
 	OCR1AH = OCR1_VALUE_1HZ>>8;		// Write high byte first
 	OCR1AL = OCR1_VALUE_1HZ;
	//PORTB = ~PORTB;	// this is equivalent to set the COM0A0 bit in TCCR1A
}

int main(void)
{
	DDRB = (1<<DDB1); 

	TCCR1A = (1<<COM0A0);
	TCCR1B = (1<<WGM12)|(1<<CS12)|(0<<CS11)|(1<<CS10);	// 20Mhz divided by 1024
	OCR1AH = OCR1_VALUE_10HZ>>8;	// Write high byte first
	OCR1AL = OCR1_VALUE_10HZ;			
	TIMSK1 = (1<<OCIE0A);							
			
	sei();										
	
    while (1) 
    {
		// do some stuff
		
		_delay_ms(1000);
    }
}