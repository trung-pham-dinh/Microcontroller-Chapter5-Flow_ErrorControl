/*
 * MessageProcess.c
 *
 *  Created on: Nov 16, 2021
 *      Author: fhdtr
 */


#include "MessageProcess.h"
#include "Fifo.h"
#include <stdlib.h>

static UART_HandleTypeDef* MP_uart;
static ADC_HandleTypeDef* MP_adc;
static uint8_t tempChar = 0;

typedef enum {
	MP_WaitCMD,
	MP_GetCMD,
	MP_NewStart,
	MP_MaxSize,
	MP_NoEnd,
	MP_Done,
	MP_Timeout,
	MP_Verify
} CMDstate;

static uint8_t cmdData[CMD_SIZE];
static uint8_t cmdFlag = 0;

static const uint32_t Max_delay =  1000; // in practice: CMD_SIZE*8 / BAUDRATE;
static uint32_t timer_counter = 0;
static uint8_t timer_flag = 0;

static uint32_t adcVal = 0;

void MP_init(UART_HandleTypeDef* uart, ADC_HandleTypeDef* adc) {
	MP_uart = uart;
	MP_adc = adc;
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

//typedef enum {
//	MP_OkayByte,
//	MP_StartByte,
//	MP_NoTerm,
//	MP_Done
//}ByteStatus;

void MP_processByte() {
	static CMDstate state = MP_WaitCMD;
	static uint8_t index = 0;
	static uint8_t byte = 0;
//	static ByteStatus status = MP_OkayByte;

//	while(!FF_isEmpty()) {
//		byte = FF_read();
//		//HAL_UART_Transmit(MP_uart, &byte, 1, HAL_MAX_DELAY);
//
//		switch(state) {
//		case MP_WaitCMD:
//			if(byte == '!') {
//				state = MP_GetCMD;
//				index = 0;
//				MP_setTimer(Max_delay); // set timeout
//			}
//			break;
//		case MP_GetCMD:
//			if(index < CMD_SIZE) {
//				cmdData[index++] = byte;
//				if(index == CMD_SIZE) {
//					state = MP_Done;
//				}
//			}
//			break;
//		case MP_Done:
//			if(byte == '#') {
//				cmdFlag = 1;
//			}
//			else {
//				MP_clearCMD();
//				HAL_UART_Transmit(MP_uart, (uint8_t*)"error\r\n", 7, HAL_MAX_DELAY);
//			}
//			MP_resetTimer();
//			state = MP_WaitCMD;
//			break;
//		}
//	}
//	if(MP_getTimerFlag()) {
//		state = MP_WaitCMD; // timeout
//		MP_clearCMD();
//		HAL_UART_Transmit(MP_uart, (uint8_t*)"timeout\r\n", 9, HAL_MAX_DELAY);
//	}

//	while(!FF_isEmpty()) {
//		byte = FF_read();
//		if(byte == '!') {
//			index = 0;
//			MP_clearCMD();
//		}
//		else if(index >= 0 && index <= CMD_SIZE) {
//			if(byte == '#') {
//				index = -1;
//				cmdFlag = 1;
//			}
//			if(index == CMD_SIZE) {
//
//			}
//			else
//		}
//		else {
//
//		}
//	}

	switch(state) {
	case MP_WaitCMD:
		if(FF_isEmpty()) break;
		byte = FF_read();

		if(byte == '!') {
			index = 0;
			MP_clearCMD();
			MP_setTimer(Max_delay);
			state = MP_GetCMD;
		}
		break;

	case MP_GetCMD:
		if(MP_getTimerFlag()) {
			state = MP_Timeout;
		}
		if(FF_isEmpty()) break;
		byte = FF_read();

		if(byte == '!') {
			state = MP_NewStart;
		}
		else if(byte == '#') {
			state = MP_Done;
		}
		else if(index < CMD_SIZE){
			cmdData[index++] = byte;
			if(index == CMD_SIZE) {
				state = MP_MaxSize;
			}
		}
		break;
	case MP_MaxSize:
		if(MP_getTimerFlag()) {
			state = MP_Timeout;
		}
		if(FF_isEmpty()) break;
		byte = FF_read();

		if(byte == '#') {
			state = MP_Done;
		}
		else {
			state = MP_NoEnd;
		}
		break;



	case MP_NewStart:
		HAL_UART_Transmit(MP_uart, (uint8_t*)"NewStart\r\n", 10, HAL_MAX_DELAY);

		index = 0;
		MP_clearCMD();
		MP_setTimer(Max_delay);
		state = MP_GetCMD;
		break;

	case MP_NoEnd:
		HAL_UART_Transmit(MP_uart, (uint8_t*)"No terminate\r\n", 14, HAL_MAX_DELAY);
		MP_resetTimer();
		state = MP_WaitCMD;
		break;

	case MP_Timeout:
		state = MP_WaitCMD;
		HAL_UART_Transmit(MP_uart, (uint8_t*)"timeout\r\n", 9, HAL_MAX_DELAY);
		break;



	case MP_Done:
		HAL_UART_Transmit(MP_uart, (uint8_t*)"Begin to process\r\n", 18, HAL_MAX_DELAY);
		cmdFlag = 1;
		MP_resetTimer();
		state = MP_WaitCMD;
		break;


	default:
		break;
	}

//	if(MP_getTimerFlag()) {
//		state = MP_WaitCMD; // timeout
//		MP_clearCMD();
//		HAL_UART_Transmit(MP_uart, (uint8_t*)"timeout\r\n", 9, HAL_MAX_DELAY);
//	}

}

void MP_processCMD() {
	if(!cmdFlag) return;
	cmdFlag = 0;

	HAL_UART_Transmit(MP_uart, cmdData, CMD_SIZE, HAL_MAX_DELAY);
	HAL_UART_Transmit(MP_uart, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);

	char adcString[10] = {0};
	itoa(adcVal, adcString, 10);
	HAL_UART_Transmit(MP_uart, (uint8_t*)adcString, 10, HAL_MAX_DELAY);
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

void MP_resetTimer() {
	timer_counter = 0;
	timer_flag = 0;
}

void MP_adcCalc() {
	adcVal = HAL_ADC_GetValue(MP_adc);
}

uint8_t MP_getTimerFlag() {
	if(timer_flag) {
		timer_flag = 0;
		return 1;
	}
	return 0;
}

