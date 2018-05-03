/*
 * 7_uart_interrupt_tx_buffered.c
 *
 *	Objective:  Understand the UART (universal asynchronous receiver and transmitter)
 *				We will use the UART to echo back a received byte using UDRE interrupt.
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
#include <avr/interrupt.h>
#include <string.h>

#define BUF_SIZE	128

unsigned char tx_buf[BUF_SIZE];
int tx_buf_head = 0;
volatile int tx_buf_tail = 0;
volatile int tx_data_size = 0;

unsigned char in_buffer[BUF_SIZE];
int in_buf_head;
volatile int in_buf_tail = 0;
volatile int in_data_size = 0;

ISR(USART_UDRE_vect)
{
	/*
		[TASK 2] Implement the ISR code here

		1. Check first whether or not there are data in the buffer.
		2. If yes, write a byte to the UDR register. Make sure the tail index
		   increments and rolls over to zero when it reaches the max buf size.
		3. If not, ISR was invoked from the last buf data. So disable the UDRE interrupt.
	*/
	if (tx_data_size > 0){
		UDR0 = tx_buf[tx_buf_tail];
		tx_buf_tail = tx_buf_tail + 1;
		if (tx_buf_tail >= BUF_SIZE){
			tx_buf_tail = 0;
		}
		tx_data_size--;
	}
	else{
		unsigned char ch = UDR0;
		UDR0 = ch;
	}
}
;

void put_char_buf(char ch)
{
	/*
		[TASK 1] Implement the code here

		1. Write a byte to the buffer incrementing the head index
		2. If the head index reaches the end of the buf, then reset to zero
		3. Check if the buf is overrun. We can utilise the data size in the buf
		   to detect this and maybe discard the last byte.
	*/

	tx_buf[tx_buf_head] = ch;

	tx_buf_head++;
	tx_data_size++;

	if (tx_buf_head >= BUF_SIZE){
		tx_buf_head = 0;
	}

	if (tx_data_size >= BUF_SIZE){
		tx_buf_tail++;
	}

}

void put_str_buf(unsigned char* str)
{
	for (int i=0; i< strlen(str); i++)
	{
		put_char_buf(str[i]);
	}

	// We've put data in the buffer so turn on the TX-UDRE interrupt
	UCSR0B |= (1<<UDRIE0);
}

void put_char(unsigned char ch)
{
	while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0 = ch;
}

void get_char(void){
	
}

int main(void)
{
	int i = 0;
	unsigned char str[128];

	DDRB = (1<<DDB1);

	UBRR0H = (unsigned char)(BAUD_VALUE>>8);
	UBRR0L = (unsigned char)(BAUD_VALUE);
	UCSR0C = (3<<UCSZ00);
	UCSR0B = (1<<TXEN0) | (1<<RXEN0);

	sei();


    while (1)
    {

		// sprintf(str, "[%d] Hello World!\r\n", ++i);

		/*
			[TASK 3] Comment out the polling function put)str()
			and then demonstrate the working function - put_str_buf()
		*/
		put_str_buf(str);
		// put_str(str);

		PORTB = ~PORTB;
		_delay_ms(100);
    }
}