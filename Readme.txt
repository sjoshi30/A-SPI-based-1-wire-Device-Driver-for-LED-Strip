*****************************************************************
Name			       ASU-ID
*****************************************************************
Aditya Sharma		       1210903678
Dheemanth Bykere Mallikarjun   1213381271
-------------------------------------------------------------------------------------------------------------------
Assignment-3 :PART1= Device driver to control the LED strip WS2812 using SPI protocol.
	      PART2= Module developmentfor bit bang transmission using HR Timer and ndelay function.

Hardware Connections to be made for Part1:
******************************************************************************************************************
1.Connect D1 pin of LED strip to IO11 of galileo gen2.
2.Connect Vcc pin of LED strip to 5V.
3.Connect GND pin of LED strip to ground.

Hardware Connections to be made for Part2(for testing both hrtimer and ndelay):
******************************************************************************************************************
1.Connect D1 pin of LED strip to IO1 of galileo gen2.
2.Connect Vcc pin of LED strip to 5V.
3.Connect GND pin of LED strip to ground.

Steps to follow for Part1:
*******************************************************************************************************************
1. There are two folders A3P1 & A3P2 representing part1 & part2 of the assignment respectively; In 
   A3P2 again there are folders named hrtimer & ndelay representing modules developed to achieve bit
   bang transmission using hrtimer and ndelay functionality. Each Modules have their separate Makefiles
   inside their respective folders.
2. For executing the Part1,traverse inside A3P1 folder and run the "make" utility,which generates the
   spi_device.ko & spi_driver.ko module files representing the device & driver respectively and also 
   the tester program executable spi_led_test.
3. Using scp utility transfer both the .ko files along with tester program executable.
   Ex: scp spi_device root@ip_addr_of_board:/path/to/desired/directory
       scp spi_driver root@ip_addr_of_board:/path/to/desired/directory
       scp spi_led_test root@ip_addr_of_board:/path/to/desired/directory
4. Now Traverse to the directory where you placed the files & using the insmod utility load both the modules.
   Ex: insmod spi_device.ko
       insmod spi_driver.ko
5. Now run the executable tester program from the directory where you have placed it.
   ./spi_led_test
Output: All pixels will blink in circular manner and color will change after every cycle completion. This process will
go on continuously. Please press control-Z to give SIGTSTP signal to program, which will stop this infinite loop and will
turn off all the LEDs.

This test program also contains the code of sending IOCTL_RESET signal which will perform spi-reset.

Steps to follow for Part2-hrtimer:
*******************************************************************************************************************
1.Traverse inside the hrtimer folder which is located inside A3P2 folder.
2.Run the "make" utility which produces the module file bit_bang_hrtimer.ko
3.Using scp utility transfer  .ko file
   Ex: scp bit_bang_hrtimer.ko root@ip_addr_of_board:/path/to/desired/directory
4.Now Traverse to the directory where you placed the file & using the insmod utility load the module.
  Ex : insmod bit_bang_hrtimer.ko

Output: Since we are starting bit-banging in init function itself, therefore as soon as you insmod the kernel module you can
see the LED blinking in circular manner.

Steps to follow for Part2-ndelay:
*******************************************************************************************************************
1.Traverse inside the ndelay folder which is located inside A3P2 folder.
2.Run the "make" utility which produces the module file bit_bang_ndelayr.ko
3.Using scp utility transfer  .ko file
   Ex: scp bit_bang_ndelay.ko root@ip_addr_of_board:/path/to/desired/directory
4.Now Traverse to the directory where you placed the file & using the insmod utility load the module.
  Ex : insmod bit_bang_ndelay.ko

Output: Since we are starting bit-banging in init function itself, therefore as soon as you insmod the kernel module you can
see the LED blinking in circular manner.
