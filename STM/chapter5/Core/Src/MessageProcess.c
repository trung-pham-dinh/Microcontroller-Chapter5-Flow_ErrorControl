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

#define OK_TIMEOUT 3
#define MESS_TIMEOUT 1

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
	MP_SENT
} CommuState;

typedef enum {
	MP_RST_CMD,
	MP_OK_CMD,
	MP_ERROR_CMD,
	MP_NO_CMD
} CMD;

static char* MP_cmd[] = {"RST", "OK"};

static uint8_t cmdData[CMD_SIZE+1]; // slot for '\0', standardize cmd buffer to C-string
static uint8_t cmdFlag = 0;
static uint32_t adcVal = 0;

static CMD MP_CMD_translate();
static void MP_send(char *str);

void MP_init(UART_HandleTypeDef* uart, ADC_HandleTypeDef* adc, TIM_HandleTypeDef* tim) {
	MP_uart = uart;
	MP_adc = adc;
	TM_init_timer(tim);
	HAL_Delay(10);


//	HAL_UART_Transmit(MP_uart, (uint8_t*)"Message Process is ready\r\n", 26, HAL_MAX_DELAY);
	MP_send("Message Process is ready");
	HAL_UART_Receive_IT(MP_uart, &tempChar, 1);
}

void MP_readByte() { // call this in Uart receive callback
	if(FF_isFull()) return;

	FF_write(tempChar);
	HAL_UART_Receive_IT(MP_uart, &tempChar, 1);
}



void MP_command_parser() {
	static CMDstate state = MP_WaitCMD;
	static uint8_t index = 0;
	static uint8_t byte = 0;

	switch(state) {
	// Wait for start character state
	case MP_WaitCMD:
		if(FF_isEmpty()) break;
		byte = FF_read();

		if(byte == '!') {
			index = 0;
			memset(cmdData,0,CMD_SIZE+1); // clear command buffer
			TM_setSecTimer(0, MESS_TIMEOUT);
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
		}
		else {
			state = MP_NoEnd;
		}
		break;

	// Error state
	case MP_NewStart:
		MP_send("Error: start without terminate");

		index = 0;
		memset(cmdData,0,CMD_SIZE+1);
		TM_setSecTimer(0, MESS_TIMEOUT);
		state = MP_GetCMD;
		break;

	case MP_NoEnd:
		MP_send("Error: command too long");

		TM_resetSecFlag(0);
		state = MP_WaitCMD;
		break;

	case MP_Timeout:
		state = MP_WaitCMD;
		MP_send("Error: timeout in receiving message");
		break;


	// Successful state
	case MP_Done:
		cmdFlag = 1;
		cmdData[CMD_SIZE] = '\0';
		TM_resetSecFlag(0);
		state = MP_WaitCMD;
		break;


	default:
		break;
	}


}

void MP_communication() {
	static CommuState state = MP_IDLE;
	static uint8_t send = 1;
	static char adcString[20] = {0};
	static char temp[10] = {0};
	static CMD cmd = MP_NO_CMD;

	switch(state) {
	case MP_IDLE:
		cmd = MP_CMD_translate();
		if(cmd == MP_RST_CMD) {
			state = MP_ADC;
		}
		else if(cmd == MP_ERROR_CMD) {
			MP_send("Error: unrecognized command");
		}
		break;
	case MP_ADC:
		itoa(adcVal, temp, 10);
		memset(adcString,0,20);
		strcat(adcString, "!ADC=");
		strcat(adcString, temp);
		strcat(adcString, "#");
		send = 1;
		state = MP_SENT;

		HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
	case MP_SENT:
		if(send) {
			send = 0;
			MP_send(adcString);

			TM_setSecTimer(1, OK_TIMEOUT);
		}
		if(TM_getSecFlag(1)) {
			send = 1;
		}

		cmd = MP_CMD_translate();
		if(cmd == MP_OK_CMD) {
			TM_resetSecFlag(1);
			state = MP_IDLE;
		}
		else if(cmd == MP_RST_CMD) {
			TM_resetSecFlag(1);
			state = MP_ADC;
		}
		else if(cmd == MP_ERROR_CMD) {
			MP_send("Error: unrecognized command");
		}
		break;
	default:
		break;
	}
}


CMD MP_CMD_translate() {
	if(!cmdFlag) return MP_NO_CMD;
	cmdFlag = 0;

	MP_send((char*)cmdData);

	if(strcmp(MP_cmd[MP_RST_CMD], (char*)cmdData) == 0) {
		return MP_RST_CMD;
	}
	else if(strcmp(MP_cmd[MP_OK_CMD], (char*)cmdData) == 0) {
		return MP_OK_CMD;
	}
	return MP_ERROR_CMD;
}

void MP_send(char *str) {
	HAL_UART_Transmit(MP_uart, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
	HAL_UART_Transmit(MP_uart, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
}

void MP_timer_run() {
	TM_timerRun();
}

void MP_adcCalc() {
	adcVal = HAL_ADC_GetValue(MP_adc);
}

