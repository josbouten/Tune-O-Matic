# Tune-O-Matic
Tuner for analog synths, inspired by Amanda Ghassaei's Instructables and Sam's LookMumNoComputer Videos

This code is an adapted version of the code published by Amanda and Sam. 
I came across the tuner while looking at Sam's video on the 1112 performance 
oscillators and responded to his challenge for anyone to look at the code and maybe send him a
new version. I streamlined the code a bit and added some comments. 

Have a look at LOOKMUMNOCOMPUTER's video's on building the 
1112 performance oscillators here https://www.lookmumnocomputer.com/projects#/1222-performance-vco 

Note, the code contains one define LED_TEST which you can use to test your hardware.
When uncommented and uploaded it allows you to check all the leds and the patterns used to 
show the tuning in the display. Once you are sure the leds perform as they should, comment the line,
recompile and upload the code and you should be set.

The code is usable for a common anode or common cathode led display. 
Follow the instructions in the code and set LED_DISPLAY_TYPE to the one you use.
