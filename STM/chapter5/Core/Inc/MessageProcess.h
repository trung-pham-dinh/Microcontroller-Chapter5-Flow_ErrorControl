/*
 * MessageProcess.h
 *
 *  Created on: Nov 16, 2021
 *      Author: fhdtr
 */

#ifndef INC_MESSAGEPROCESS_H_
#define INC_MESSAGEPROCESS_H_

#include "main.h"

#define CMD_SIZE 			3
#define INTERRUPT_PERIOD    10
#define BAUDRATE			9600

void MP_init(UART_HandleTypeDef* uart, ADC_HandleTypeDef* adc, TIM_HandleTypeDef* tim);
void MP_command_parser();
void MP_communication();
void MP_readByte();
void MP_setTimer(uint32_t ms);
void MP_timer_run();
void MP_adcCalc();

#endif /* INC_MESSAGEPROCESS_H_ */
