/*
 * MessageProcess.c
 *
 *  Created on: Nov 16, 2021
 *      Author: fhdtr
 */


#include "MessageProcess.h"
#include "Fifo.h"

static UART_HandleTypeDef* MP_uart;
static uint8_t tempChar = 0;

typedef enum {
	MP_WaitCMD,
	MP_GetCMD,
	MP_Done
} CMDstate;

static uint8_t cmdData[CMD_SIZE];
static uint8_t cmdFlag = 0;

static const uint32_t Max_delay =  1000; // in practice: CMD_SIZE*8 / BAUDRATE;
static uint32_t timer_counter = 0;
static uint8_t timer_flag = 0;

void MP_init(UART_HandleTypeDef* uart) {
	MP_uart = uart;
	HAL_Delay(10);


	HAL_UART_Transmit(MP_uart, (uint8_t*)"Message Process is ready\r\n", 26, HAL_MAX_DELAY);
	HAL_UART_Receive_IT(MP_uart, &tempChar, 1);
}

void MP_readByte() {
	if(FF_isFull()) return;

	FF_write(tempChar);

//	uint8_t front = FF_read();
//	HAL_UART_Transmit(MP_uart, &front, 1, HAL_MAX_DELAY);

	HAL_UART_Receive_IT(MP_uart, &tempChar, 1);
}



void MP_processByte() {
	static CMDstate state = MP_WaitCMD;
	static uint8_t index = 0;
	static uint8_t byte = 0;

	while(!FF_isEmpty()) {
		byte = FF_read();
		//HAL_UART_Transmit(MP_uart, &byte, 1, HAL_MAX_DELAY);

		switch(state) {
		case MP_WaitCMD:
			if(byte == '!') {
				state = MP_GetCMD;
				index = 0;
				MP_setTimer(Max_delay); // set timeout
			}
			break;
		case MP_GetCMD:
			if(index < CMD_SIZE) {
				cmdData[index++] = byte;
				if(index == CMD_SIZE) {
					state = MP_Done;
				}
			}
			break;
		case MP_Done:
			if(byte == '#') {
				cmdFlag = 1;
			}
			else {
				MP_clearCMD();
				HAL_UART_Transmit(MP_uart, (uint8_t*)"error\r\n", 7, HAL_MAX_DELAY);
			}
			timer_flag = 0;
			state = MP_WaitCMD;
			break;
		}
	}
	if(MP_getTimerFlag()) {
		state = MP_WaitCMD; // timeout
		MP_clearCMD();
		HAL_UART_Transmit(MP_uart, (uint8_t*)"timeout\r\n", 9, HAL_MAX_DELAY);
	}

}

void MP_processCMD() {
	if(!cmdFlag) return;
	cmdFlag = 0;

	HAL_UART_Transmit(MP_uart, cmdData, CMD_SIZE, HAL_MAX_DELAY);
	HAL_UART_Transmit(MP_uart, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
}

void MP_clearCMD() {
	for(int i = 0; i < CMD_SIZE; i++) {
		cmdData[i] = 0;
	}
}

void MP_setTimer(uint32_t ms) {
	timer_counter = ms / INTERRUPT_PERIOD;
}

void MP_decreaseTimer() {
	if(timer_counter) {
		timer_counter--;
		if(timer_counter == 0) {
			timer_flag = 1;
		}
	}
}

uint8_t MP_getTimerFlag() {
	if(timer_flag) {
		timer_flag = 0;
		return 1;
	}
	return 0;
}

