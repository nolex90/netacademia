/*
 * dht.c
 *
 *  Created on: Feb 7, 2018
 *      Author: nolex
 */

#include "dht.h"

static os_timer_t DhtTimer;
static uint8_t DhtResultIdx;
static uint8_t DhtResult[42];
static uint32_t DhtTimeOffset;

static DhtHandler DhtHandle;

ICACHE_FLASH_ATTR void dhtProcess()
{
	uint8_t result[5];
	os_memset(result, 0, sizeof(result));
	uint8_t bytes;
	int8_t temp;
	uint8_t bits = 2;

	for(bytes = 0 ; bytes < 5; bytes++)
	{
		for(temp = 7; temp >= 0; temp--)
		{
			if(DhtResult[bits] > 100)
				result[bytes] |= 0x1 << temp;
			bits++;
		}
	}
	DEBUG("Result: %d,%d,%d,%d,%d\n\r",result[0],result[1],result[2],result[3],result[04]);
	if((result[0]+result[1]+result[2]+result[3]) == result[04])
	{
		DEBUG("Dht Parity ok\n\r");
	}
	else
	{
		DEBUG("Dht parity NOK, halting...");
		DhtHandle.state = ERR;
		system_os_post(0,0,(os_param_t)&DhtHandle);
	}
#if DHT_TYPE == DHT11
	DhtHandle.DhtHum = result[0];
	DhtHandle.DhtTemp = result[2];
#else
	uint16_t rawhum = result[0] << 8 | result[1];
	uint16_t rawtemp = result[2] << 8 | result[3];
	if(rawtemp & 0x8000)
	{
		DhtHandle.DhtTemp = (float) ((rawtemp & 0x7fff)/10.0) * -1.0;
	}
	else
	{
		DhtHandle.DhtTemp = (float) (rawtemp) / 10.0;
	}
	DhtHandle.DhtHum = (float)(rawhum) / 10.0;
#endif
}

void dhtGpioCbk()
{
	//DEBUG("DhtGpioCbk\n\r");
	uint32_t gpio_status;
	gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	DhtHandle.state = MEAS;
	if(gpio_status & BIT(DHT_PIN))
	{
		DhtResult[DhtResultIdx++] = WDEW_NOW() - DhtTimeOffset;
		DhtTimeOffset = WDEW_NOW();
		if(DhtResultIdx > 41)
		{
			ETS_GPIO_INTR_DISABLE();
#if DEBUG_EN == 1
			uint8_t temp5;
			for(temp5 = 0; temp5 < 42; temp5++)
			{
				DEBUG("|Dht : %d \n\r", DhtResult[temp5]);
			}
#endif
			dhtProcess();
			os_timer_disarm(&DhtTimer);
			DhtHandle.state = COMPL;
			system_os_post(0,0,(os_param_t)&DhtHandle);

		}
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(DHT_PIN));
	}
}

ICACHE_FLASH_ATTR void dhtGpioCbkSetup()
{
	ETS_GPIO_INTR_DISABLE();
	GPIO_DIS_OUTPUT(DHT_PIN);
	ETS_GPIO_INTR_ATTACH(&dhtGpioCbk, NULL);
	gpio_register_set(GPIO_PIN_ADDR(DHT_PIN), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
				| GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
				| GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));

	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(DHT_PIN));
	gpio_pin_intr_state_set(GPIO_ID_PIN(DHT_PIN), GPIO_PIN_INTR_NEGEDGE);
	ETS_GPIO_INTR_ENABLE();
	DhtResultIdx = 0;
	os_memset(&DhtResult, 0, sizeof(DhtResult));
	DhtTimeOffset = WDEW_NOW();
	DEBUG("Dht gpio cbk setup done\n\r");

}

void dhtTimerCbk()
{
	DEBUG("DhtTimerCbk %d\n\r", DhtHandle.TimeoutCnt);
	DhtHandle.TimeoutCnt++;
	GPIO_OUTPUT_SET(DHT_PIN, 1);
	GPIO_DIS_OUTPUT(DHT_PIN);
	if(DHT_TIMEOUT < DhtHandle.TimeoutCnt)
	{
		os_timer_disarm(&DhtTimer);
		DhtHandle.state = ERR;
		DEBUG("DHT Timeout!!\n\r");
		system_os_post(0,0,(os_param_t)&DhtHandle);
	}
	dhtGpioCbkSetup();
}

ICACHE_FLASH_ATTR void dhtInit()
{
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	DHT_PIN_ON();
	DhtHandle.state = PREP;
	DhtHandle.TimeoutCnt = 0;
}

ICACHE_FLASH_ATTR void dhtStart()
{
	DEBUG("Dht Measuring start\n\r");
	dhtInit();
	os_delay_us(500);
	DHT_PIN_OFF();
	//os_timer_disarm(&DhtTimer);
	os_timer_setfn(&DhtTimer, (os_timer_func_t*)dhtTimerCbk, (void*)0);
	os_timer_arm(&DhtTimer, 20, 1);

}

ICACHE_FLASH_ATTR void dhtTest()
{
	dhtStart();
	os_printf("DhtResult:hum: %d, temp: %d\n\r", DhtHandle.DhtHum, DhtHandle.DhtTemp);
}
