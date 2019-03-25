#ifndef IOCTL_SPI_H
#define IOCTL_SPI_H

#include <linux/ioctl.h>

#define PIXEL_COUNT 16
#define BITS_PER_PIXEL 24
#define RANDOM_NUMBER 2182367/*magic number for ioctl command representation*/

/*structure representing the data to be set for WS2812 LED*/
typedef struct n_led_data_strct {
    /*number of leds to lit up*/
    int n;
    /*data corresponding to n leds*/
    unsigned char data[BITS_PER_PIXEL*PIXEL_COUNT];
} n_led_data_t;

/*command given to ioctl system call when the pins need to be set*/
#define IOCTL_RESET _IO(RANDOM_NUMBER, 1)


#endif /* IOCTL_SPI_H */

