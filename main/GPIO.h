/*
 * GPIO.h
 *
 *  Created on: 6 May 2025
 *      Author: BrianMcDonald
 */
#include "esp_system.h"

#ifndef MAIN_GPIO_H_
#define MAIN_GPIO_H_

//GPIOs for devkitC-1
//#define INHA    6
//#define INHB    7
//#define INHC	15
//#define INHD    16

//GPIOs for TinyS3

#define INH1    3
#define INH2    2
#define INH3	0
#define INH4    21
#define INH5    8
#define INH6    9
#define INH7	6
#define INH8    7
#define LED_ENABLE 17
#define ANTENNA_ENABLE 38

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<INH1) | (1ULL<<INH2)|(1ULL<<INH3)|(1ULL<<INH4)|(1ULL<<INH5) | (1ULL<<INH6)|(1ULL<<INH7)|(1ULL<<INH8)|(1ULL<<LED_ENABLE)|(1ULL<<ANTENNA_ENABLE))
#define GPIO_CH1_4  ((1ULL<<INH1) | (1ULL<<INH2)|(1ULL<<INH3)|(1ULL<<INH4))
#define GPIO_CH5_8  ((1ULL<<INH1) | (1ULL<<INH2)|(1ULL<<INH3)|(1ULL<<INH4)|(1ULL<<INH5) | (1ULL<<INH6)|(1ULL<<INH7)|(1ULL<<INH8))
#define GPIO_ALL  (GPIO_CH1_4 | GPIO_CH5_8)
#endif /* MAIN_GPIO_H_ */


