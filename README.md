# APA102module
## Introduction
This Python module was created to drive APA102 pixels on the Raspberry Pi. It may work on other devices, but is only guarenteed to work on a Raspberry Pi.
I based this off of a similar library by tinue located [here](https://github.com/tinue/APA102_Pi)
It functions almost exactly the same as tinue's library only it is more effecient due to being written in C instead of Python.
## Setup
tinue wrote a very in depth setup on his project, so I will just point to his
## Migrating from tinue's code
If you are wanting to replace tinue's library with mine, you will only need to make a few changes to your code.
The first is in the creation of the APA102 object. His code creates the SPI device by receiving the MOSI and MISO pins. To create an object in his code, you declare it like this: APA102(num_led, global_brightness, mosi, sclk, order)
This library does note create the SPI device and instead requires you to pass in the write function upon creation like this: APA102(num_led, write_callback, global_brightness, order)
Here is the code to create a APA102 object using the default SPI pins on a Raspberry Pi
      import spidev
      device = spidev.SpiDev()
			device.open(0, 0)
			device.max_speed_hz= 400000
			device.mode = 0
			device.cshigh = False
      APA102(num_led=10, write_callback=device.writebytes, global_brightness=31, order='rgb')
I originally did this because I did not know how to create a SPI object from C. But I decided to keep it designed this way so that it would be easier to debug on another computer before running it on the Raspberry Pi. You can set write_callback to any function that accepts a list of numbers as its only argument. Or you could just set write_callback to None, causing the write function to do nothing.

Since the SPI device is not stored in the APA102 object anymore, the cleanup function has been removed, so you will have to clean up. You can simply replace calls to the cleanup function with device.close() to acomodate for this change.

Since the data held for the pixels is no longer accessable as a python list, I added a few helper functions that allow you to get a pixel's color from the object:
  get_pixel_color_str returns the pixel's color as a hex string in the format "#RRGGBB"
  get_pixel_color_rgb returns the pixel's color as a long in the format 0xRRGGBB
  get_pixel_color returns the pixel's color as a tuple ofr 3 numbers in the format (R, G, B)
  
 A minor change was made to the set_pixel function too.
 The bright_percent argument was changed to brightness and instead accepts a number from 0 to 31. This was made to correspond to the range that a pixel takes for it's brighness byte
 
 Additionally, the clear_strip function no longer calls show after clearing the strip
 
 I also added set_all, set_all_rgb, set_range, and set_range_rgb functions to set multple pixels to the same color in C thus making it more effecient than using a loop in Python.

