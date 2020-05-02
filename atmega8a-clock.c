#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

typedef enum { false, true } bool;
#define ONE_SECOND    1000000000  // nanoseconds
#define ONE_INTERRUPT 1000040000  // one second (quarz) for timecorrection

uint8_t *output[] = {&PORTB, &PORTC, &PORTD};

volatile int hour = 11;
volatile int sleepcounter = 0;
volatile int min = 59;
volatile int sec = 50;
volatile bool showdisplay = true;
volatile bool minset = false;
volatile bool hourset = false;

int charset[11][3] = {
	{0b111, 0b101, 0b111}, // 0
	{0b110, 0b010, 0b111}, // 1
	{0b110, 0b010, 0b011}, // 2
	{0b111, 0b011, 0b111}, // 3
	{0b101, 0b111, 0b001}, // 4
	{0b011, 0b010, 0b110}, // 5
	{0b100, 0b111, 0b111}, // 6
	{0b111, 0b001, 0b001}, // 7
	{0b111, 0b111, 0b111}, // 8
	{0b111, 0b111, 0b001}, // 9
	{0b010, 0b000, 0b010}, // :
};

volatile int displaybuffer[12] = {
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000},
	{0b0000000000}
};


/**
 * Setup routine to initialize ports, enable the external crystal and allowing interrupts
 */
void setup(){
	// setup output ports for LEDs (PortC 0-3)
	DDRC |= (_BV(PC0)| _BV(PC1) | _BV(PC2) | _BV(PC3));
	// setup output ports for LEDs (PortD 4-7)
	DDRD |= (_BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7));
	// setup output ports for LEDs (PortB 0-3) + PortB 4 for D pin (shiftregister)
	DDRB |= (_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3) | _BV(PB4));

	//setup timer0 - Prescaler 64 - for display refresh at 60Hz~
	TCCR0 |= (1 << CS01);
	TCCR0 |= (1 << CS00);

	//allow overflow interrupt for timer0
	TIMSK |= (1 << TOIE0);
	
	// init counter with 0
	TCNT0 = 0;
	
	// configure interrupt for low level of int0 (button pressed)
	MCUCR &= ~((1 << ISC01) | (1 << ISC00));

	// enable pull up resistor
	PORTD |= (1 << PD2); 

	// enable external quarz oscillator
	ASSR |= (1 << AS2); // configure asynchronous timer2
	TCCR2 |= (1 << CS20) | (1 << CS21) | (1 << CS22); //set prescaler for external clock (1024 - overflow every 8 seconds)
	TCNT2 = 256-32; // preload counter for overflow every sec
	while(ASSR&0x07); // wait until timer is not busy anymore - and external crystal is working correctly
	TIFR &= ~(1<<TOV2); // just to be sure, clear the overflow flag
	TIMSK |= (1 << TOIE2); // Enable timer2 output overflow interrupt
	sei(); //allow global interrupts
}


/**
 * Send a clock signal to the shiftregisters.
 */
void clock(){
	*output[0] |= _BV(PB3);
	*output[0] &= ~_BV(PB3);
}

/**
 * Trigger the shiftregisters to move the data from shift register stage to storage
 * Used to display the rows.
 */
void str(){
	*output[0] |= _BV(PB2);
	*output[0] &= ~_BV(PB2);
}

/**
 * Push a "1" into the shiftregister stage to move the zero
 * which is representing the line currently shown on the led matrix
 */
void shiftline() {
	PORTB |= (1 << PB4);
	clock();
}

/**
 * Reset all LED ports at once, to clear the display
 */
void resetIO() {
	PORTB &= ~((1 << PB0) | (1 << PB1));
	PORTC &= ~((1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3));
	PORTD &= ~((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7));
}


/**
 * Function that fills the displaybuffer array which is displayed afterwards (by multiplex function).
 * Input: int[] filled with the digits of the numbers
 * firstline hours
 * secondline: minutes
 * thirdline: seconds
 * example: firstline[0] = 0, firstline[1] = 9, 09: is displayed for hours
 */
void prepareFrame(int firstline[2], int secondline[2], int thirdline[2]) {
	int x = 0;
	int y = 0;
	int line = 8;
	
	// shifts the corresponding numbers to the hour lines
	for (x = 0; x < 3; x++) {
		displaybuffer[x] |= charset[firstline[0]][x] << 7; // shift the first digit to position 10-8
		displaybuffer[x] |= charset[firstline[1]][x] << 3; // shift the second digit to position 6-4
		if (sec % 2 == 0) {
		 	displaybuffer[x] |= charset[10][x]; // for displaying the : every other second
		}
	}

	// shifts the corresponding numbers to the minutes lines
	for (x = 4; x < 7; x++) {
		displaybuffer[x] |= charset[secondline[0]][y] << 7; // shift the first digit to position 10-8
		displaybuffer[x] |= charset[secondline[1]][y] << 3; // shift the second digit to position 6-4
		if (sec % 2 == 0) {
		 	displaybuffer[x] |= charset[10][y]; // for displaying the : every other second
		}
		y++;
	}
	
	y = 0;
	
	// shifts the corresponding numbers to the seconds line
	for (x = 8; x < 11; x++) {
		displaybuffer[x] |= charset[thirdline[0]][y] << 7; // shift the first digit to position 10-8
		displaybuffer[x] |= charset[thirdline[1]][y] << 3; // shift the second digit to position 6-4
		y++;
	}
	
	// to display a slowly filling sleepcounter at the bottom of the screen
	for (int ct = 0; ct <= sleepcounter % 10; ct++) {
		displaybuffer[11] |= (1 << ct);
	}

	// to display a filling column (if 3 leds are lid, the clock goes to sleep)
	for (int ct = 0; ct < sleepcounter / 10; ct++) {
		displaybuffer[line] |= (1 << 1);
		line++;
	}

	// special things for set mode
	if(hourset) {	 
		displaybuffer[0] |= (1 << 0);
	} else if (minset) {
		displaybuffer[1] |= (1 << 0);
	}
}

/**
 * Resets all rows of the displaybuffer to 0x000 (Empty!)
 */
void resetDisplayBuffer() {
	for (int x = 0; x < 12; x++) {
		displaybuffer[x] = 0x000;
	}
}

/**
 * Initialize the shiftregisters with ten 1s and one 0 (1 represents line not shown on display, 0 line is displayed)
 * Needed for multiplexing
 */
void initregister() {
	int i;
	
	for (i = 0; i < 11; i++) {
		PORTB |= (1 << PB4); // Set 1
		clock();
	}

	PORTB &= ~(1 << PB4); // Set 0
	clock();
}

/**
 * Function to get each digit of a number
 * Input: number = 34, position = 0
 * output: 3
 */
int getDigit(int number, int position) {
	int mydigits[2];
	
	mydigits[1] = number % 10;
	mydigits[0] = (number - mydigits[1]) / 10;

	return mydigits[position];
}


/**
 * Sending microcontroller to sleep (Timer2 is enabled)
 */
void powersave_sleep() {
	showdisplay = false; // disable the display
	set_sleep_mode(SLEEP_MODE_PWR_SAVE); // set the sleepmode to pwr save
	GICR |= (1 << INT0); // Enable button interrupt to wake the clock from sleep with
	sleep_mode(); // go to sleep
	GICR &= ~(1 << INT0); // disable button interrupt, to prevent bad things happen after wakeup
}

/**
 * Sending microcontroller into powerdown_mode (Timer2 is disabled)
 * For demonstration purposes only
 */
void powerdown_sleep() {
	showdisplay = false; // disable the display
	set_sleep_mode(SLEEP_MODE_PWR_DOWN); // set the sleepmode to pwr save
	GICR |= (1 << INT0); // Enable button interrupt to wake the clock from sleep with
	sleep_mode(); // go to sleep
	GICR &= ~(1 << INT0); // disable button interrupt, to prevent bad things happen after wakeup
}

/**
 * Main Function, clockset routine implemented
 */
int main() {
	setup(); // setup all the things
	initregister(); // init the shiftregisters
	int btntimer = 0;

	while(1) {
		// increase an int as long the button is pressed
		while(bit_is_clear(PIND, PD2) && btntimer <= 125) {
			btntimer++;
		}
		// if btntimer is 125 go to hourset mode
		if (btntimer >= 125 && (!hourset && !minset)) {
				hourset = true;
		} else if (btntimer >= 125 && hourset) { // press again and it switches to minset mode
				hourset = false;
				minset = true;
		} else if (btntimer >= 125 && (minset && !hourset)) { // go to normal mode
				minset = false;
		}

		if (hourset && (btntimer > 1 && btntimer < 30)) {
			incrementHour();
			sleepcounter = 0;
		} else if (minset && (btntimer > 1 && btntimer < 30)) {
			incrementMinute();
			sleepcounter = 0;
		}

		btntimer = 0;
		
		if (sleepcounter == 30) {
			powersave_sleep();
			//powerdown_sleep(); // for demonstration purposes
		}
	}
}

/**
 * Multiplex the displaybuffer to the ledmatrix (Show the current data in the displaybuffer on screen)
 * inserts a 0 after shifting thrue all lines
 */
void multiplex() {
	for (int i = 0; i < 11; i++) {
		PORTB = (PORTB & 0xFC) | ((displaybuffer[i] & 0x300) >> 8); // Read the first 2 bits (from left to right)
		PORTC = (PORTC & 0xF0) | (displaybuffer[i] & 0x0F);  // Read the last 4 bits (from left to right)
		PORTD = (displaybuffer[i] & 0xF0) | (PORTD & 0x0F); // Read the bit 3-6
		str(); // display the line
		shiftline(); // shift the line once
		resetIO(); // clear the PORTB/PORTC/PORTD pins for the next line
		str(); // display it (that is useful to get a clean screen if we go into sleep)
	}

	PORTB &= ~(1 << PB4);
	clock();
}

/**
 * Refreshes the frame with new clock values
 */
void refreshFrame() {
	int firstline[2] = {getDigit(hour,0), getDigit(hour,1)};
	int secondline[2] = {getDigit(min,0), getDigit(min,1)};
	int thirdline[2] = {getDigit(sec,0), getDigit(sec,1)};
	resetDisplayBuffer();

	prepareFrame(firstline, secondline, thirdline);
}

/**
 * Increments the minute counter
 */
void incrementMinute() {
	min++;
	if (min == 60) {
		min = 0;
	}
	sec = 0;
}

/**
 * Increments the hour counter
 */
void incrementHour() {
	hour++;
	if (hour == 24) {
		hour = 0;
	}
	sec = 0;
}

/**
 * Interrupt service routine for timer0 (shows the display)
 */
ISR(TIMER0_OVF_vect) {
	if (showdisplay) {
		multiplex();
		refreshFrame();
	}
}

/**
 * Interrupt service routine for timer2 (counts seconds and does some timecorrection)
 */
ISR(TIMER2_OVF_vect) {
	TCNT2 = 256-32;

	if (!hourset && !minset) {

		//Time Correction 20ppm
		static uint32_t unaccounted_time;

    	unaccounted_time += ONE_INTERRUPT;
    	while (unaccounted_time >= ONE_SECOND) {
	        sec++;
        	unaccounted_time -= ONE_SECOND;
    	}
	
		if (sec == 60) {
			sec = 0;
			min++;
		}
	
		if (min == 60) {
			min = 0;
			hour++;
		}

		if (hour == 24) {
			hour = 0;
		}
	}

	if (showdisplay && !hourset && !minset) {
		sleepcounter++;
	}
}

/**
 * Interrupt service routine for INT0 (button wakeup)
 */
ISR(INT0_vect) {
	showdisplay = true;
	sleepcounter = 0;
}
