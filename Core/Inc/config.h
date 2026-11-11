/*
 * config.h
 *
 *  Created on: 30.09.2023
 *      Author: stancecoke
 */

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#define CAL_I 38

#define P_FACTOR 200
#define I_FACTOR 10

//avoid redefined warning führt imDEBUG-Modus zu falscher Baudrate die C500-Kommunikation
//#ifndef DEBUG
//#define DEBUG
//#endif

/* UART1 wird für die C500-Kommunikation verwendet */
#ifdef DEBUG
#undef DEBUG
#endif

#define PERIOD 3000 //@48MHz for 16kHz PWM frequency
#define THROTTLE_OFFSET 700
#define THROTTLE_MAX 2820
#define BATTERY_CURRENT_MAX 15000
#define WHEEL_CIRCUMFERENCE 2200
#define GEAR_RATIO 76
#define PULSES_PER_REVOLUTION 1
#define SPEEDLIMIT 25
/* PAS assist level speed limits in 0.01 km/h (level 0 = no assist).
 * Level 5 is additionally capped by SPEEDLIMIT. */
#define PAS_SPEED_LIMIT_L1_X100 600
#define PAS_SPEED_LIMIT_L2_X100 1000
#define PAS_SPEED_LIMIT_L3_X100 1500
#define PAS_SPEED_LIMIT_L4_X100 2000
#define PAS_SPEED_LIMIT_L5_X100 2500
#define THROTTLE_SPEED_LIMIT_X100 600 //xxxx 6.00 km/h max speed for throttle only (0 = disabled)
#define THROTTLE_SPEED_RAMP_X100 50   //xxxx 0.5 km/h ramp before the throttle speed limit
#define THROTTLE_POWER_BOOST 125      //xxxx percent throttle response (100 = linear)
#define PAS_TIMEOUT 8000 // half a second
#define RAMP_END 1000
#define TS_COEF 6000
#define NCTE_OFFSET 1960
#define ERIDER_OFFSET 1500
#endif /* INC_CONFIG_H_ */
