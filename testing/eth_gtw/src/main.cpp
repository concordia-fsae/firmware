#include <Arduino.h>
#include <SPI.h>


SPIClass SPI_MCP = SPIClass(VSPI);

#include "mcp_can.h"


MCP_CAN CAN = MCP_CAN(5);

bool led_state = false;

void IRAM_ATTR CANRX_ISR(){
	digitalWrite(2, led_state);
	led_state = !led_state;
}

void setup(){
    Serial.begin(9600);
    /* while (Serial.read() <= 0) {} */

	CAN.begin(MCP_ANY, CAN_1000KBPS, MCP_16MHZ);
	CAN.setMode(MCP_NORMAL);

	pinMode(2, OUTPUT);
	pinMode(22, INPUT);
	/* attachInterrupt(22, CANRX_ISR, FALLING); */
}

void loop(){
	if(CAN_MSGAVAIL == CAN.checkReceive()){
		long unsigned int recv_id;
		uint8_t recv_len;
		uint8_t recv_msg[8];

		CAN.readMsgBuf(&recv_id, &recv_len, recv_msg);
		digitalWrite(2, led_state);
		led_state = !led_state;
	}
}

