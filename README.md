# atmega8a-clock
Just a simple atmega8a clock with a 32khz crystal resonator

The board is from a "franzis retro tennis game" (ISBN:  40 19631 67007-6). It got a atmega8a, two shift register and a 10x12 LED matrix on it. 
Programming pins for the atmega8a are at the back of the board. So you can solder a programming interface to it.

The crystal resonator is also not included, I choosed a 32khz crystal resonator. Like these: EuroQuartz Quarzkristall QUARZ TC26 Zylinder 32.768 kHz 12.5 pF from volkner or conrad. 

There are also 2 pins to the external interrupts of the atmega8a, so you can attach a button for wakeup or something like that.

Features of the clock:
- its a clock, yay?
- some sort of time error correction (20ppm~)
- goes to deep sleep after 30sec (so you basically have VERY low power consumption of this device)
- wakes up on button click
- set mode -> long press the button to get to hour set mode, press long again for minute set mode

Maybe someone can benefit from my example code.. so have fun building it yourself. :)

![clock gif](/images/clock.gif)
