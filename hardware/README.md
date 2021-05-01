# VFD Interface Board for [Connected Status Monitor][6]

The Futaba 162SD03 [VFD][3] is a relatively power hungry device (especially compared to a low-power microcontroller). VFDs can also suffer from fading with use over time.

I therefore did not want the VFD on all the time -- I wanted it to be powered down when I was not using [Connected Status Monitor][6], just like I turn off the LCD backlight when I am not in the room.

## Power Control Circuit

Since the VFD pulls a relatively large amount of current -- about 320 to 360 mA when powered depending on the number of elements activate -- I need to use a beefier FET to switch the current. I chose an IRL540 N-channel MOSFET. I could not fully turn on the MOSFET at these current levels with 3.3 V logic, so I use 2N3904 to control the MOSFET.

## Control Signal Buffer

I buffer the VFD logic signals with a CD40109 tri-state level shifter. I am mainly using this as a tri-state buffer to disconnect the logic signals from the VFD when it is powered down, but it also level shifts the signals to 5V logic levels. Although the VFD requires Vcc of 5 V, 3.3 V logic levels are within spec for the control signals, so the level shifting is not strictly necessary.  

## Ambient Light Sensor

The board contains a simple [light detection circuit][5] using a 10K resistor and a cheap photo resistor. The 10K and photo resistors are wired in series, with the photo resistor pulled to Vcc and the 10K resistor connected to ground. Analog Pin A19 reads the voltage at the 10K/photo resistor connection (a simple voltage divider).

## Signal and Power Connections

The interface board has a center-positive 2.1mm barrel jack for 5 V power which is supplied to the LaunchPad through a BoosterPack pin, and the LaunchPad then supplies regulated 3.3 V back to the interface board.

The I/O pins used to interface with the interface board are as follows:

```text
LaunchPad             
   Pin     Direction  Interface Board
---------  ---------  ----------------------------------------------
   41         -->     3.3 V Power
   61         <--     5 V Power
   62         <->     GND
   72         -->     VFD Reset signal (via CD40109)
   73         -->     VFD Data signal (via CD40109)
   74         -->     VFD Clock signal (via CS40109)
   76         -->     CD40109 Enable (all enables are tied together)
   77         -->     VFD Power Control (active low)
   68         <--     Ambient Light Sensor (Analog A19)
```

VFD module signal definitions (connector J1):

```text
Pin  Description
---  -----------
 1    Vcc (5 V)
 2    Clock
 3    GND
 4    DATA
 5    Reset
```

## References

- [Connected Status Monitor][6]
- Interface Board [schematic][1]
- Interface Board [picture][2]
- Futaba [162SD03][3] Vacuum Fluorescent Display
- [CD40109][4] Buffer/Level-Shifter
- Arduino Playground photo resistor [tutorial][5]

[1]: ./Interface-Board-Schematic.JPG
[2]: ./Interface-Board-Pic.JPG
[3]: https://www.allelectronics.com/mas_assets/media/allelectronics2018/spec/VFD-162.pdf
[4]: https://www.ti.com/lit/ds/symlink/cd40109b.pdf
[5]: https://playground.arduino.cc/Learning/PhotoResistor
[6]: https://github.com/Andy4495/ConnectedStatusMonitor
[100]: https://choosealicense.com/licenses/mit/
[101]: ../LICENSE
[200]: https://github.com/Andy4495/ConnectedStatusMonitor
