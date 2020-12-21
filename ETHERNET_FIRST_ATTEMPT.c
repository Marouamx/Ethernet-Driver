#include "main.h"
#include "wizchip_conf.h"
#include "socket.h"
#include <string.h>
#include <stdio.h>

#define _WIZCHIP_ W5500



#define SOCK_TCPS       0
#define SOCK_UDPS       1
#define PORT_TCPS		5000
#define PORT_UDPS       3000

#define DATA_BUF_SIZE   2048


#define SEPARATOR            "=============================================\r\n"
#define WELCOME_MSG  		 "Welcome to STM32Nucleo Ethernet configuration\r\n"
#define NETWORK_MSG  		 "Network configuration:\r\n"
#define IP_MSG 		 		 "  IP ADDRESS:  %d.%d.%d.%d\r\n"
#define NETMASK_MSG	         "  NETMASK:     %d.%d.%d.%d\r\n"
#define GW_MSG 		 		 "  GATEWAY:     %d.%d.%d.%d\r\n"
#define MAC_MSG		 		 "  MAC ADDRESS: %x:%x:%x:%x:%x:%x\r\n"
#define GREETING_MSG 		 "Well done guys! Welcome to the IoT world. Bye!\r\n"
#define CONN_ESTABLISHED_MSG "Connection established with remote IP: %d.%d.%d.%d:%d\r\n"
#define SENT_MESSAGE_MSG	 "Sent a message. Let's close the socket!\r\n"
#define WRONG_RETVAL_MSG	 "Something went wrong; return value: %d\r\n"
#define WRONG_STATUS_MSG	 "Something went wrong; STATUS: %d\r\n"
#define LISTEN_ERR_MSG		 "LISTEN Error!\r\n"


#define PRINT_STR(msg) do  {\
  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);\
} while(0)


#define PRINT_NETINFO(netInfo) do { 																					\
  HAL_UART_Transmit(&huart2, (uint8_t*)NETWORK_MSG, strlen(NETWORK_MSG), 100);											\
  sprintf(msg, MAC_MSG, netInfo.mac[0], netInfo.mac[1], netInfo.mac[2], netInfo.mac[3], netInfo.mac[4], netInfo.mac[5]);\
  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);															\
  sprintf(msg, IP_MSG, netInfo.ip[0], netInfo.ip[1], netInfo.ip[2], netInfo.ip[3]);										\
  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);															\
  sprintf(msg, NETMASK_MSG, netInfo.sn[0], netInfo.sn[1], netInfo.sn[2], netInfo.sn[3]);								\
  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);															\
  sprintf(msg, GW_MSG, netInfo.gw[0], netInfo.gw[1], netInfo.gw[2], netInfo.gw[3]);										\
  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);															\
} while(0)


uint8_t gDATABUF[DATA_BUF_SIZE];



wiz_NetInfo gWIZNETINFO = { .mac = {0x00, 0x08, 0xdc,0x00, 0xab, 0xcd},
                            .ip = {192, 168, 1, 40},
                            .sn = {255,255,255,0},
                            .gw = {192, 168, 1, 1},
                            .dns = {8,8,8,8},
                            .dhcp = NETINFO_STATIC };


uint8_t destip[4] = {192, 168, 1, 120};
uint16_t destport = 5000;



SPI_HandleTypeDef 	hspi2;
UART_HandleTypeDef  huart2;


char msg[60];

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART2_UART_Init(void);



void  wizchip_select(void);
void  wizchip_deselect(void);
void  wizchip_write(uint8_t wb);
uint8_t wizchip_read();

void network_init(void);


int main(void)
{
	   uint8_t tmp = 0;
	   uint8_t memsize[2][8] = { {2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};

	   uint8_t retVal, sockStatus;
	    int16_t rcvLen;
	     uint8_t rcvBuf[20], bufSize[] = {2, 2, 2, 2};


  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_USART2_UART_Init();



  PRINT_STR("Here we START\r\n");

  wizchip_deselect();
  reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);

  /* SPI read & write callback function */
  reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);

  /* WIZCHIP SOCKET Buffer initialize */
  if(ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize) == -1)
  {
	  PRINT_STR("WIZCHIP Initialized fail.\r\n");
     while(1);
  }
  else PRINT_STR("WIZCHIP Initialized success.\r\n");
  /* PHY link status check */

  do
  {
     if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp) == -1)
    	 PRINT_STR("Unknown PHY Link stauts.\r\n");

  } while(tmp == PHY_LINK_OFF);

  /* Network Initialization */
  network_init(); // and print network info

  reconnect:
  PRINT_STR("Enters loop \r\n");
    /* Open socket 0 as TCP_SOCKET with port 5000 */
    if((retVal = socket(0, Sn_MR_TCP, 3000, 0)) == 0) {
  	  /* Put socket in LISTEN mode. This means we are creating a TCP server */
    	PRINT_STR("TCP server created,LISTEN MODE \r\n");
    	if((retVal = listen(0)) == SOCK_OK) {
    		/* While socket is in LISTEN mode we wait for a remote connection */
    	PRINT_STR("WAITING FOR REMOTE CNX\r\n");
  		  while(sockStatus = getSn_SR(0) == SOCK_LISTEN)
  			  HAL_Delay(100);
  		  /* OK. Got a remote peer. Let's send a message to it */
  		  while(1) {
  			  /* If connection is ESTABLISHED with remote peer */
  			  if(sockStatus = getSn_SR(0) == SOCK_ESTABLISHED) {


  				  PRINT_STR("Cnx established with remote peer\r\n");

  				  uint8_t remoteIP[4];
  				  uint16_t remotePort;
  				  /* Retrieving remote peer IP and port number */
  				  getsockopt(0, SO_DESTIP, remoteIP);
  				  getsockopt(0, SO_DESTPORT, (uint8_t*)&remotePort);
  				  sprintf(msg, CONN_ESTABLISHED_MSG, remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3], remotePort);
  				  PRINT_STR(msg);
  				  /* Let's send a welcome message and closing socket */
  				  if(retVal = send(0, GREETING_MSG, strlen(GREETING_MSG)) == (int16_t)strlen(GREETING_MSG))
  					  PRINT_STR(SENT_MESSAGE_MSG);
  				  else { /* Ops: something went wrong during data transfer */
  					  sprintf(msg, WRONG_RETVAL_MSG, retVal);
  					  PRINT_STR(msg);
  				  }
  				  break;
  			  }
  			  else { /* Something went wrong with remote peer, maybe the connection was closed unexpectedly */
  				  sprintf(msg, WRONG_STATUS_MSG, sockStatus);
  				  PRINT_STR("cnx was closed\r\n");
  				  break;
  			  }
  		  }

  	  } else /* Ops: socket not in LISTEN mode. Something went wrong */
  		  PRINT_STR("socket not in LISTEN mode\r\n");
    } else { /* Can't open the socket. This means something is wrong with W5100 configuration: maybe SPI issue? */
  	  sprintf(msg, WRONG_RETVAL_MSG, retVal);
  	  PRINT_STR("Can't open the socket\r\n");
    }

    /* We close the socket and start a connection again */
    disconnect(0);
    close(0);
    goto reconnect;

    /* Infinite loop */
    while (1)
    {
    }

/*
   /* Main Loop
   while(1)
   {
	//loopback_tcps(SOCK_TCPS, gDATABUF, PORT_TCPS);
	loopback_udps(SOCK_UDPS, gDATABUF, PORT_UDPS);


	PRINT_STR("here I dont know what is goinf on! \r\n");

	HAL_Delay(5000);
	//loopback_ret = loopback_tcpc(SOCK_TCPS, gDATABUF, destip, destport);
	//if(loopback_ret < 0) trace_printf("loopback ret: %ld\r\n", loopback_ret);
	// TCP Socket
   }

   */

}


void  wizchip_select(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); //CS LOW
	}

void  wizchip_deselect(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET); //CS HIGH
}

void  wizchip_write(uint8_t wb)
{
	HAL_SPI_Transmit(&hspi2, &wb, 1, 0xFFFFFFFF);
}

uint8_t wizchip_read()
{   uint8_t rbuf;
	HAL_SPI_Receive(&hspi2, &rbuf, 1, 0xFFFFFFFF);
	return rbuf;
}

void network_init(void)
{



	ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);



    ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);

	// Display Network Information

    PRINT_NETINFO(gWIZNETINFO);
}


void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}
void MX_GPIO_Init(void) {

	GPIO_InitTypeDef GPIO_InitStruct;

	/* GPIO Ports Clock Enable */
	__GPIOC_CLK_ENABLE()
	;

	__GPIOA_CLK_ENABLE()
	;
	__GPIOB_CLK_ENABLE()
	;

	/* Configure GPIO pin : PC13 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/* Configure GPIO pin : PA5 */
	GPIO_InitStruct.Pin = GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* SPI2 init function */
void MX_SPI2_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLED;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLED;

    __SPI2_CLK_ENABLE();
    /**SPI2 GPIO Configuration
    PB12     ------> SPI2_NSS
    PB13     ------> SPI2_SCK
    PB14     ------> SPI2_MISO
    PB15     ------> SPI2_MOSI
    */
    GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pin : PA5 */
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	HAL_SPI_Init(&hspi2);
}

/* USART2 init function */
void MX_USART2_UART_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    __USART2_CLK_ENABLE();
      /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	HAL_UART_Init(&huart2);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

/*
static void usart_transmit_string(char* data) {
	for (int i=0 ; data[i] != 0 ; i++) {
		 HAL_UART_Transmit(&huart1, data[i], sizeof(data[i]), HAL_MAX_DELAY);
	}
}
*/

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
