//tx
/*
 * main.c
 *
 * Created: 4/20/2018 1:54:58 PM
 * Author : Damian Cisneros and Jiajian Chen
 * Description : This program monitors temperature using an LM34 sensor. It
 *				 sends the temperature through RF every 1s and 
 *				 displays it on a serial terminal. Using 8Mhz clock
 */ 

#define BAUD 9600
#define F_CPU 8000000UL

#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <string.h>
#include <util/delay.h>
#include "nrf24l01.h"

void init_USART();

void setup_timer(void);
nRF24L01 *setup_rf(void);

volatile bool rf_interrupt = false;
volatile bool send_message = false;

static int put_char(char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM(put_char, NULL, _FDEV_SETUP_WRITE);

int ADCvalue;
char c[10];
ISR(ADC_vect)
{
	ADCvalue = ADCH;
	// only need to read the high value for 8 bit
}

int main(void) {
	
	stdout = &mystdout; //set the output stream
	
	ADMUX = 0; // use ADC0
	ADMUX |= (1 << REFS0); // use AVcc as the reference
	ADMUX |= (1 << ADLAR); // left adjust for 8 bit resolution
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0); // 128 prescale for 16Mhz
	ADCSRA |= (1 << ADATE); // Set ADC Auto Trigger Enable
	ADCSRB = 0; // 0 for free running mode
	ADCSRA |= (1 << ADEN); // Enable the ADC
	ADCSRA |= (1 << ADIE); // Enable Interrupts
	ADCSRA |= (1 << ADSC); // Start the ADC conversion
    uint8_t to_address[5] = { 0x01, 0x01, 0x01, 0x01, 0x01 };
    bool on = false;
	
	init_USART();
    sei();
    
	nRF24L01 *rf = setup_rf();
    setup_timer();

    while (true) {
		ADCvalue=((ADCvalue*5)/256)*100;
		
        if (rf_interrupt) {
            rf_interrupt = false;
            int success = nRF24L01_transmit_success(rf);
            if (success != 0)
                nRF24L01_flush_transmit_message(rf);
        }

        if (send_message) {
            send_message = false;
            on = !on;
            nRF24L01Message msg;
         if (on){ 
			 dtostrf(ADCvalue,3,0,c);
			 memcpy(msg.data,c , 6);
			 printf("Sending: ");
			 printf(c);
			 printf("F");
			 printf("\n");
			_delay_ms(1000);
		 }
		 }
            msg.length = strlen((char *)msg.data) + 1;
            nRF24L01_transmit(rf, to_address, &msg);
        }
    }
    return 0;
}

nRF24L01 *setup_rf(void) {
    nRF24L01 *rf = nRF24L01_init();
    rf->ss.port = &PORTB;
    rf->ss.pin = PB2;
    rf->ce.port = &PORTB;
    rf->ce.pin = PB1;
    rf->sck.port = &PORTB;
    rf->sck.pin = PB5;
    rf->mosi.port = &PORTB;
    rf->mosi.pin = PB3;
    rf->miso.port = &PORTB;
    rf->miso.pin = PB4;
    // interrupt on falling edge of INT0 (PD2)
    EICRA |= _BV(ISC01);
    EIMSK |= _BV(INT0);
    nRF24L01_begin(rf);
    return rf;
}

// setup timer to trigger interrupt every second when at 1MHz
void setup_timer(void) {
    TCCR1B |= _BV(WGM12);
    TIMSK1 |= _BV(OCIE1A);
    OCR1A = 15624;
    TCCR1B |= _BV(CS10) | _BV(CS11);
}

// each one second interrupt
ISR(TIMER1_COMPA_vect) {
   send_message = true;
}

// nRF24L01 interrupt
ISR(INT0_vect) {
    rf_interrupt = true;
}

void init_USART(){
	unsigned int BAUDrate;

	//set BAUD rate: UBRR = [F_CPU/(16*BAUD)]-1
	BAUDrate = ((F_CPU/16)/BAUD) - 1;
	UBRR0H = (unsigned char) (BAUDrate >> 8); //shift top 8 bits into UBRR0H
	UBRR0L = (unsigned char) BAUDrate; //shift rest of 8 bits into UBRR0L
	UCSR0B |= (1 << RXEN0) | (1 << TXEN0); //enable receiver and trasmitter
	// UCSR0B |= (1 << RXCIE0); //enable receiver interrupt
	UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00); //set data frame: 8 bit, 1 stop
}

static int put_char(char c, FILE *stream)
{
	while(!(UCSR0A &(1<<UDRE0))); // wait for UDR to be clear
	UDR0 = c;    //send the character
	return 0;
}