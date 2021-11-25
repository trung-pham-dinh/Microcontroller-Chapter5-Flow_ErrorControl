/*
 * MessageProcess.c
 *
 *  Created on: Nov 16, 2021
 *      Author: fhdtr
 */


#include "MessageProcess.h"
#include "Fifo.h"
#include "timer.h"
#include <stdlib.h>
#include <string.h>

static UART_HandleTypeDef* MP_uart;
static ADC_HandleTypeDef* MP_adc;
static uint8_t tempChar = 0;

typedef enum {
	MP_WaitCMD,
	MP_GetCMD,
	MP_NewStart,
	MP_NoEnd,
	MP_Done,
	MP_Timeout,
	MP_Verify
} CMDstate;

typedef enum {
	MP_IDLE,
	MP_ADC,
	MP_RESENT
} CommuState;

typedef enum {
	MP_RST_CMD,
	MP_OK_CMD,
	MP_ERROR_CMD
} CMD;

static char* MP_cmd[] = {"RST", "OK"};

static uint8_t cmdData[CMD_SIZE];
static uint8_t cmdFlag = 0;

static const uint32_t Max_delay =  1; // in practice: CMD_SIZE*8 / BAUDRATE;
//static uint32_t timer_counter = 0;
//static uint8_t timer_flag = 0;

static uint32_t adcVal = 0;

static CMD MP_CMD_translate();
static void MP_clearCMD();

void MP_init(UART_HandleTypeDef* uart, ADC_HandleTypeDef* adc, TIM_HandleTypeDef* tim) {
	MP_uart = uart;
	MP_adc = adc;
	TM_init_timer(tim);
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



void MP_command_parser() {
	static CMDstate state = MP_WaitCMD;
	static uint8_t index = 0;
	static uint8_t byte = 0;

	switch(state) {
	// wait for start character state
	case MP_WaitCMD:
		if(FF_isEmpty()) break;
		byte = FF_read();

		if(byte == '!') {
			index = 0;
			MP_clearCMD(); // clear command buffer
//			MP_setTimer(Max_delay); // set timeout on waiting command
			TM_setSecTimer(0, Max_delay);
			state = MP_GetCMD;
		}
		break;
	// Receive command state
	case MP_GetCMD:
		if(TM_getSecFlag(0)) { // check timeout
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
				//state = MP_MaxSize;
			}
		}
		else {
			state = MP_NoEnd;
		}
		break;

	// Error state
	case MP_NewStart:
		HAL_UART_Transmit(MP_uart, (uint8_t*)"NewStart\r\n", 10, HAL_MAX_DELAY);

		index = 0;
		MP_clearCMD();
//		MP_setTimer(Max_delay);
		TM_setSecTimer(0, Max_delay);
		state = MP_GetCMD;
		break;

	case MP_NoEnd:
		HAL_UART_Transmit(MP_uart, (uint8_t*)"Max CMD size\r\n", 14, HAL_MAX_DELAY);
		TM_resetSecFlag(0);
		state = MP_WaitCMD;
		break;

	case MP_Timeout:
		state = MP_WaitCMD;
		HAL_UART_Transmit(MP_uart, (uint8_t*)"timeout\r\n", 9, HAL_MAX_DELAY);
		break;


	// Successful state
	case MP_Done:
		HAL_UART_Transmit(MP_uart, (uint8_t*)"Begin to process\r\n", 18, HAL_MAX_DELAY);
		cmdFlag = 1;
		TM_resetSecFlag(0);
		state = MP_WaitCMD;
		break;


	default:
		break;
	}


}

void MP_communication() {
	if(!cmdFlag) return;
	cmdFlag = 0;

	HAL_UART_Transmit(MP_uart, cmdData, CMD_SIZE, HAL_MAX_DELAY);
	HAL_UART_Transmit(MP_uart, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
//
//	char adcString[10] = {0};
//	itoa(adcVal, adcString, 10);
//	HAL_UART_Transmit(MP_uart, (uint8_t*)adcString, 10, HAL_MAX_DELAY);
//	HAL_UART_Transmit(MP_uart, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
//	if(MP_CMD_translate() == MP_RST_CMD) {
//		char adcString[10] = {0};
//		itoa(adcVal, adcString, 10);
//		HAL_UART_Transmit(MP_uart, (uint8_t*)adcString, 10, HAL_MAX_DELAY);
//		HAL_UART_Transmit(MP_uart, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
//	}
//	CommuState state = MP_IDLE;
//	switch(state) {
//	case MP_IDLE:
//		break;
//	case MP_ADC:
//		break;
//	case MP_RESENT:
//	}
	char freqString[10] = {0};
	itoa(HAL_RCC_GetPCLK1Freq(), freqString, 10);
	HAL_UART_Transmit(MP_uart, (uint8_t*)freqString, 10, HAL_MAX_DELAY);
	HAL_UART_Transmit(MP_uart, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
}


CMD MP_CMD_translate() {
	if(strcmp(MP_cmd[MP_RST_CMD], (char*)cmdData) == 0) {
		return MP_RST_CMD;
	}
	else if(strcmp(MP_cmd[MP_OK_CMD], (char*)cmdData) == 0) {
		return MP_OK_CMD;
	}
	return MP_ERROR_CMD;
}



void MP_clearCMD() {
	for(int i = 0; i < CMD_SIZE; i++) {
		cmdData[i] = 0;
	}
}


void MP_timer_run() {
	TM_timerRun();
}

void MP_adcCalc() {
	adcVal = HAL_ADC_GetValue(MP_adc);
}

