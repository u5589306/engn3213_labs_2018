/*
* 9_twi_interface_imu.c
*
* Created: 11/05/2018 9:34:58 AM
* Author : Karman Sawhney
*/

#define F_CPU 20000000UL

#include <stdio.h>
#include <util/twi.h>
#include <util/delay.h>
#include <avr/interrupt.h>

///////////////////////////////////////////////////////////////////////
//
// UART Functions
//
///////////////////////////////////////////////////////////////////////
#define BAUD_115200		115200UL
#define UBRR_VALUE		(F_CPU/16/BAUD_115200)	// 10 in the datasheet. Formula is wrong

///////////////////////////////////////////////////////////////////////
//
// UART Functions
//
///////////////////////////////////////////////////////////////////////
void uart_init()
{
	UBRR0H = (unsigned char)(UBRR_VALUE>>8);	// Baud Rate High
	UBRR0L = (unsigned char)UBRR_VALUE;			// Baud Rate Low
	UCSR0C = (0<<USBS0)|(3<<UCSZ00);			// 1 stop bit, 8 data, No parity (default)
	UCSR0B |= (1<<RXEN0)|(1<<TXEN0);			// Enable receiver and transmitter
}

void uart_putchar(unsigned char data)
{
	while ( !(UCSR0A & (1<<UDRE0)) );			// Wait for transmit buffer empty
	UDR0 = data;								// Put data into buffer, sends the data
}

void uart_putstr(char *str)
{
	while(*str)
	uart_putchar(*str++);
}

unsigned char get_char(void)
{
	while ( !( UCSR0A & (1<<RXC0)) );

	return UDR0;
}

/* TIMER INTERRUPT */
#define OCR1_VALUE_1HZ	19531/2
#define OCR1_VALUE_10HZ	1953/2

int target_OCR1_value = OCR1_VALUE_1HZ;
volatile int read_data = 0;

ISR(TIMER1_COMPA_vect)
{
	read_data = 1;
}

void timer1_init()
{
	TCCR1A = (1<<COM0A0);
	TCCR1B = (1<<WGM12)|(1<<CS12)|(0<<CS11)|(1<<CS10);	// 20Mhz divided by 1024
	OCR1AH = target_OCR1_value>>8;	// Write high byte first
	OCR1AL = target_OCR1_value;
	TIMSK1 = (1<<OCIE0A);
}


///////////////////////////////////////////////////////////////////////
//
// I2C (Inter-Integrated Communication, Philip) Read/Write
// also called TWI - Two-Wire Interface, Atmel
// The read/write protocols are explained in the Atmega328P data-sheet
//	Chapter 26.6 @ page 268 - Using the TWI
//	Chapter 26.7 @ page 271 - Mater transmitter & master receiver
//
///////////////////////////////////////////////////////////////////////
void twi_init()
{
	PORTC = (1<<DDC5)|(1<<DDC4);	// writing 1 in an input mode enables the pull-up.
	
	// Set TWI clock to 100 kHz
	//TWBR = 0x5C;	//92 = 0x5C
	TWBR = ((F_CPU / 100000) - 16) / 2;
	TWDR = 0xFF;                        // Default content = SDA released.
	TWCR = (1<<TWEN)|                   // Enable TWI-interface and release TWI pins.
	(0<<TWIE)|(0<<TWINT)|               // Disable Interupt.
	(0<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|	// No Signal requests.
	(0<<TWWC);
}

///////////////////////////////////////////////////////////////////////
//
// I2C (Inter-Integrated Communication, Philip) Write a byte (polling)
// The frame consists of
//	1. Sending slave Address (SLA) + Write-mode bit (W)
//	2. sending data #1 of sub-register within the device (IMU datasheet)
//	3. sending data #2 of byte (IMU datasheet)
//
///////////////////////////////////////////////////////////////////////
int twi_write(uint8_t addr, uint8_t sub_addr, uint8_t ch)
{
	// 1. Start
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN) | (1<<TWEA);
	while (!(TWCR & (1<<TWINT)));		// Polling the Control Register (TWCR) to check TWINT-bit
	if ((TWSR & 0xF8) != TW_START)
	return -1;
	
	// 2. Send SLA+W (Write Mode)
	TWDR = (addr << 1) | (TW_WRITE);	// Shift the SLA by one-bit and add Write-bit (0)
	TWCR = (1<<TWINT)|(1<<TWEN);		// Clear the TWINT-bit by writing '1' and start transmission
	while (!(TWCR & (1<<TWINT)));		// Polling the Control Register (TWCR) to check TWINT-bit
	if ((TWSR & 0xF8) != TW_MT_SLA_ACK)
	return -2;
	
	//	3. Send Data #1 (sub-address)
	TWDR = sub_addr;					// Data #1 (sub-address)
	TWCR = (1<<TWINT)|(1<<TWEN);		// Clear the TWINT-bit by writing '1' and start transmission
	while (!(TWCR & (1<<TWINT)));		// Polling the Control Register (TWCR) to check TWINT-bit
	if ((TWSR & 0xF8) != TW_MT_DATA_ACK)
	return -3;

	//	4. Send Data #2 (actual data)
	TWDR = ch;							// Data #2 (a byte to write at the sub-address)
	TWCR = (1<<TWINT)|(1<<TWEN);		// Clear the TWINT-bit by writing '1' and start transmission
	while (!(TWCR & (1<<TWINT)));		// Polling the Control Register (TWCR) to check TWINT-bit
	if ((TWSR & 0xF8) != TW_MT_DATA_ACK)
	return -4;

	// 5. Stop condition
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
	_delay_ms(1);						// Allow time for stop to send
	
	return 0;
}

///////////////////////////////////////////////////////////////////////
//
// I2C (Inter-Integrated Communication, Philip) Read bytes (polling)
// The frame consists of
//	1. Sending the Slave Address (SLA) + Write-mode bit (W)
//	2. Sending data #1 (sub-register within the device) (IMU datasheet)
//	3. Sending Slave Address (SLA) + Read-mode bit (R)
//	4. Reading data #2-N (IMU datasheet)
//
///////////////////////////////////////////////////////////////////////
int twi_read(uint8_t addr, uint8_t sub_addr, uint8_t *data, uint8_t size)
{
	// 1. Start
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
	while (!(TWCR & (1<<TWINT)));
	if ((TWSR & 0xF8) != TW_START)
	return -1;
	
	// 2. Send SLA+W (Write Mode)
	TWDR = (addr << 1) | TW_WRITE;
	TWCR = (1<<TWINT)|(1<<TWEN);
	while (!(TWCR & (1<<TWINT)));
	if ((TWSR & 0xF8) != TW_MT_SLA_ACK)
	return -2;
	
	// 3. Send Data #1 (sub-address)
	if (size>1)
	sub_addr |= 0x80;			// This is specific to Magnetometer

	TWDR = sub_addr;				// Sub address + Auto Increment
	TWCR = (1<<TWINT)|(1<<TWEN);	// Start transmission
	while (!(TWCR & (1<<TWINT)));
	if ((TWSR & 0xF8) != TW_MT_DATA_ACK)
	return -3;
	
	// 4. We need to change to Read mode so Re-start (repeated)
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
	while (!(TWCR & 1<<TWINT));
	if ((TWSR & 0xF8) != TW_REP_START)
	return -4;
	
	// 5. Send SLA+R (Read mode)
	TWDR = (addr << 1) | TW_READ;
	TWCR = (1<<TWINT)|(1<<TWEN);
	while (!(TWCR & (1<<TWINT)));
	if ((TWSR & 0xF8) != TW_MR_SLA_ACK)
	return -5;

	if (size > 1)	// multiple read
	{
		for (int i=0; i<size-1; i++)
		{
			// 6. Data up to #(N-1) (Read a byte). Need to ACK to the slave
			TWCR = (1<<TWINT)|(1<<TWEN)| (1<<TWEA); // ACK enabled
			while (!(TWCR & (1<<TWINT)));
			if ((TWSR & 0xF8) != TW_MR_DATA_ACK)
			return -6;
			
			*data++ = TWDR;
		}
	}
	
	// 6. Data #2 or #N (Read a byte). Last byte needs a NACK
	TWCR = (1<<TWINT)|(1<<TWEN); // No TWEA (send NACK to the slave)
	while (!(TWCR & (1<<TWINT)));
	if ((TWSR & 0xF8) != TW_MR_DATA_NACK)
	return -7;

	*data = TWDR; // last byte
	
	//  7. Stop
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
	
	_delay_ms(1);
	
	return 0;
}

///////////////////////////////////////////////////////////////////////
//
// Some IMU Registers and Bit-definitions
//		- IMU/Magnetometer LSM9DS1 datasheet
//		- Barometer LPS25HB datasheet
//
///////////////////////////////////////////////////////////////////////
#define WHO_AM_I_M		0x0F
#define MAG_ADDR		0x1E
#define IMU_ADDR		0x6B	// LSM9DS1 datasheet @ page 30
#define BARO_ADDR		0x5D	// LPS25HB datasheet @ page 25

#define CTRL_REG1_G		0x10
#define STATUS_REG		0x17
#define OUT_X_G			0x18
#define	OUT_X_XL		0x28

#define CTRL_REG1_G5	5
#define STATUS_GDA0		0
#define STATUS_XLDA1	1

#define PI				3.141592
#define MAG_RESOL		(0.14*0.001)	// gauss
#define ACCL_RESOL		(0.061*0.001)	// g
#define GYRO_RESOL		(8.75*0.001)	// deg

// For magnetometer

#define CTRL_REG1_M     0x20
#define CTRL_REG3_M     0x22
#define CTRL_REG4_M     0x23

#define CTRL_REG1_M_OM0 5
#define CTRL_REG1_M_OM1 6
#define CTRL_REG1_M_DO0 2
#define CTRL_REG1_M_DO1 3
#define CTRL_REG1_M_DO2 4

#define CTRL_REG3_M_MD0 0
#define CTRL_REG3_M_MD1 1

#define CTRL_REG4_M_OMZ0 2
#define CTRL_REG4_M_OMZ1 3

#define STATUS_REG_M 0x27
#define STATUS_REG_M_ZYXDA 3
#define OUT_X_M 0x28


// For Barometer

#define CTRL_REG1_B     0x20

#define CTRL_REG1_B_PD    7
#define CTRL_REG1_B_ODR2  6
#define CTRL_REG1_B_ODR1  5
#define CTRL_REG1_B_ODR0  4

#define  STATUS_REG_B   0x27
#define  PRESS_OUT_XL   0x28

///////////////////////////////////////////////////////////////////////
//
// Main
//
///////////////////////////////////////////////////////////////////////
int main(void) {
	
	char str_press[200];
	char str_temp[200];
	
	char str[200];
	uint8_t data[12], temp;
	int16_t xi,yi,zi;
	float x,y,z;
	
	int32_t pressure;
	float pressure_hpa;
	
	int16_t temprature;
	float temprature_c;


	DDRB |= 1<<DDB1; // Set PB1 as output

	uart_init();
	uart_putstr("\033c");		// ASCII code clear screen: 'Escape'('\033' in octal)+'c')
	uart_putstr("UART initialised - 115.2Kbps\r\n");
	
	twi_init();
	uart_putstr("TWI initialised - 100Kbps\r\n");
	
	// Read the IMU "Who_Am_I" register (0x0F) to confirm the ID (0x68)
	int i = twi_read(IMU_ADDR, 0x0F, &temp, 1);
	sprintf(str,"ID IMU: Who Am I = [0x%2X]\r\n",temp);
	uart_putstr(str);

	// Read the Magnetometer "Who_Am_I" register (0x0F) to confirm the ID (0x3D)
	i = twi_read(MAG_ADDR, 0x0F, &temp, 1);
	sprintf(str,"ID Magnetometer: Who Am I = [0x%2X]\r\n",temp);
	uart_putstr(str);

	// Read the Barometer "Who_Am_I" register (0x0F) to confirm the ID (0xBD)
	i = twi_read(BARO_ADDR, 0x0F, &temp, 1);
	sprintf(str,"ID Barometer: Who Am I = [0x%2X]\r\n",temp);
	uart_putstr(str);

	// Write the IMU control register to setup the output rate (default is zero/no data)
	temp = 1<<CTRL_REG1_G5;	// output rate 14.9Hz
	i = twi_write(IMU_ADDR, CTRL_REG1_G, temp);

	//
	// TASK 1: Initialise Magnetometer here (Device Address - 0x1E)
	//	- CTRL_REG1 (0x20): set x-y to high performance mode + set output rate (10hz)
	i = twi_read(MAG_ADDR, CTRL_REG1_M, &temp, 1);
	temp |= (1<<CTRL_REG1_M_OM1);
	temp &= ~(1<<CTRL_REG1_M_OM0);
	temp |= (1<<CTRL_REG1_M_DO2);
	temp &= ~(1<<CTRL_REG1_M_DO1);
	temp &= ~(1<<CTRL_REG1_M_DO0);
	i = twi_write(MAG_ADDR, CTRL_REG1_M, temp);
	//	- CTRL_REG3 (0x22) - set the continuous conversion
	i = twi_read(MAG_ADDR, CTRL_REG3_M, &temp, 1);
	temp &= ~(1<<CTRL_REG3_M_MD0);
	temp &= ~(1<<CTRL_REG3_M_MD1);
	i = twi_write(MAG_ADDR, CTRL_REG3_M, temp);
	//	- CTRL_REG4 (0x23) - set z to high-performance mode
	i = twi_read(MAG_ADDR, CTRL_REG4_M, &temp, 1);
	temp |= (1<<CTRL_REG4_M_OMZ1);
	temp &= ~(1<<CTRL_REG4_M_OMZ0);
	i = twi_write(MAG_ADDR, CTRL_REG4_M, temp);

	//
	// TASK 2: Initialise Barometer here (Device Address - 0x5D)
	//	- CTRL_REG1 (0x20): set PD (power down control-active) + set output rate (12.5hz)
	
	i = twi_read(BARO_ADDR, CTRL_REG1_B, &temp, 1);
	temp |= (1<<CTRL_REG1_B_PD);
	temp &= ~(1<<CTRL_REG1_B_ODR2);
	temp |= (1<<CTRL_REG1_B_ODR1);
	temp |= (1<<CTRL_REG1_B_ODR0);
	i = twi_write(BARO_ADDR, CTRL_REG1_B, temp);
	

	uart_putstr("Three I2C device IDs are read...\r\n");
	uart_putstr("Press any key to read Accelerometer data...\r\n");
	temp = get_char();
	
	timer1_init();
	sei();
	while (1)
	{
		if (read_data) {
			i = twi_read(IMU_ADDR, STATUS_REG, &temp, 1);
			if (temp == 0x7)	// data ready
			{
				i = twi_read(IMU_ADDR, OUT_X_XL, data, 6);
				
				xi = ((int16_t)data[1] << 8) | data[0];			// We first convert the byte to a word using (int16_t),
				yi = ((int16_t)data[3] << 8) | data[2];			// then shift-left to 8-bits, making it an upper byte.
				zi = ((int16_t)data[5] << 8) | data[4];			// Combine it with the lower-byte data. Note that if there are
				// mixed types of data, the compiler follows the largest type.
				x = (float) xi*ACCL_RESOL;						// The LSB (least Sig Bit) of the Accel data is defined on
				y = (float) yi*ACCL_RESOL;						// page 12 (LSM9DS1) - 0.061mg/LSB for default 2g range.
				z = (float) zi*ACCL_RESOL;

				sprintf(str,"Accel(g): %.3f, %.3f, %.3f \r\n", x, y, z);	// You will see that the %f (floating-point) does not
				uart_putstr(str);								// work in default to minimise the library size. You need to enable
				// the linker option as mentioned in the HLAB manual.
				
				//
				// TASK 3: Read 6 bytes of Gyroscope (Device Address - 0x6B)
				//	- Read 6 bytes starting from OUT_X_G (Output X-axis Gyro) at 0x18
				//	- Combine two bytes into an int16_t, and then a float with resolution
				//  - Display to UART terminal
				
				i = twi_read(IMU_ADDR, OUT_X_G, data, 6);
				
				xi = ((int16_t)data[1] << 8) | data[0];
				yi = ((int16_t)data[3] << 8) | data[2];
				zi = ((int16_t)data[5] << 8) | data[4];
				x = (float) xi*GYRO_RESOL;
				y = (float) yi*GYRO_RESOL;
				z = (float) zi*GYRO_RESOL;

				sprintf(str,"GYRO(deg/s): %.3f, %.3f, %.3f \r\n", x, y, z);
				uart_putstr(str);
			}
				//
				// TASK 4: Read 6 bytes of Magnetometer (Device Address - 0x1E)
				//	- Read a byte from the Status register (0x27) to check data ready
				//	- Read 6 bytes starting from OUT_X_M (Output X-axis Mag) at 0x28
				//	- Combine two bytes into an int16_t, and then a float with resolution
				//  - Display to UART terminal
				i = twi_read(MAG_ADDR,STATUS_REG_M, &temp, 1);
				if (temp & (1 << STATUS_REG_M_ZYXDA))
				{
					
					i = twi_read(MAG_ADDR, OUT_X_M, data, 6);
					
					xi = ((int16_t)data[1] << 8) | data[0];
					yi = ((int16_t)data[3] << 8) | data[2];
					zi = ((int16_t)data[5] << 8) | data[4];
					x = (float) xi*MAG_RESOL;
					y = (float) yi*MAG_RESOL;
					z = (float) zi*MAG_RESOL;

					sprintf(str,"MAG(gauss): %.3f, %.3f, %.3f \r\n", x, y, z);
					uart_putstr(str);
				}
				//
				// TASK 5: Read 5 bytes of Barometer (Device Address - 0x5D)
				//	- Read a byte from the Status register (0x27) to check data ready
				//	- Read 5 bytes starting from PRESS_OUT_XL (Pressure Output X-axis Low) at 0x28
				//	- Combine three bytes (pressure) into an int32_t, and convert to hPa (Page 15)
				//	- Combine two bytes (temperature) into an int16_t, and convert to deg (C) (Page 9)
				//  - Display to UART terminal
				i = twi_read(BARO_ADDR,STATUS_REG_B, &temp, 1);
				sprintf(str,"baro status reg: %d\n\r", temp);
				uart_putstr(str);
				
				i = twi_read(BARO_ADDR, PRESS_OUT_XL, data, 5);
				
				pressure = ((int32_t)data[2] < 16) | ((int32_t)data[1] < 8) | ((int32_t)data[0]);
				pressure_hpa = (float)pressure;

				temprature = ((int16_t)data[4] < 8) | ((int32_t)data[3]);
				temprature_c = (float)temprature;

				sprintf(str_press,"Pressure(hPa): %.3f\r\n", pressure_hpa);
				sprintf(str_temp, "Temprature(C): %.3f\r\n", temprature_c);
				uart_putstr(str_press);
				uart_putstr(str_temp);
			
			read_data = 0;
			PORTB ^= (1<<PORTB1);
			_delay_ms(200);
		}
	}
}

