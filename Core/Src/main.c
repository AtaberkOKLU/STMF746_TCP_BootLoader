/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lwip.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lwip/init.h"
#include "app_ethernet.h"
#include "tcp_server.h"
#include "flash_if.h"
#include "string.h"		// For memset
#include "partition_table_handler.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef  void (*pFunction)(void);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* HASH handler declaration */

struct netif gnetif;
pFunction Jump_To_Application;
uint32_t JumpAddress;

/* Partition Table Variables */
uint32_t bootAddress, backupBootLoc, bootLocation;
OTA backupBoot;
PartitionTable oldPartitionTable;

extern uint8_t startFlashOperation;
static __IO uint32_t FlashWriteAddress;
extern uint8_t dataBuffer[DATA_BUFFER_SIZE];
extern uint32_t data_len;
extern uint32_t package_count;
extern uint8_t dataMD5[16];
extern uint8_t selectedBoot;
extern PartitionTable PartitionTable2Boots;
extern OTA boot;
extern uint8_t calculatedMD5[16];



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Netif_Config(void);
static void BSP_Config(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */


  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */
  /* Configure Key Button */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);

  /* Test if Key push-button is not pressed */
  if (BSP_PB_GetState(BUTTON_KEY) == 0x00)
  { /* Key push-button not pressed: jump to user application */
	readPartitionTableFromFlash((uint8_t*) PARTITION_TABLE_LOC, &oldPartitionTable);
	if(oldPartitionTable.selectedOTA == 0)
	{
		bootLocation 	= TABLE_BOOT0_LOC;
	}
	else if(oldPartitionTable.selectedOTA == 1)
	{
		bootLocation 	= TABLE_BOOT1_LOC;
	}

	readBootFromTable((uint8_t*) bootLocation, &boot);
	uint32_t SelectedBootAddress = boot.Address;
	/* Check if valid stack address (RAM address) then jump to user application */

	if (((*(__IO uint32_t*)SelectedBootAddress) & 0x2FF20000 ) == 0x20000000)
	// if (((*(__IO uint32_t*)SelectedBootAddress)) == 0x20050000)
	{
		if(checkBootMD5(&boot, calculatedMD5)) {
			/* Jump to user application */
			JumpAddress = *(__IO uint32_t*) (SelectedBootAddress + 4);
			Jump_To_Application = (pFunction) JumpAddress;
			/* Initialize user application's Stack Pointer */

			__disable_irq();
			__set_MSP(*(__IO uint32_t*) SelectedBootAddress);
			SCB->VTOR = SelectedBootAddress;
			__enable_irq();

			Jump_To_Application();
			/* do nothing */
			while(1);
		}
		else
		{
			/* LED3 (RED) ON to indicate bad software (when not mached MD5 Checksum) */
			BSP_LED_Init(LED3);
			BSP_LED_On(LED3);
			/* do nothing */
			while(1);
		}
	}
	else
	{/* Otherwise, do nothing */
	  /* LED3 (RED) ON to indicate bad software (when not valid stack address) */
	  BSP_LED_Init(LED3);
	  BSP_LED_On(LED3);
	  /* do nothing */
	  while(1);
	}
  } else {
	  // Enter IAP MODE
  Netif_Config();
  BSP_Config();
  ethernetif_set_link(&gnetif);
  tcp_server_init();
  User_notification(&gnetif);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	MX_LWIP_Process();

	#ifdef USE_DHCP
		/* handle periodic timers for DHCP */
		DHCP_Periodic_Handle(&gnetif);
	#endif

	if(startFlashOperation)
	{
		/* Select boot address */
		if(selectedBoot == 0)
		{
			bootAddress 	= USER_FLASH_BOOT0_ADDRESS;
			bootLocation 	= TABLE_BOOT0_LOC;
			backupBootLoc	= TABLE_BOOT1_LOC;
		}
		else if(selectedBoot == 1)
		{
			bootAddress 	= USER_FLASH_BOOT1_ADDRESS;
			bootLocation 	= TABLE_BOOT1_LOC;
			backupBootLoc	= TABLE_BOOT0_LOC;
		}

		/* Perform Write Operation */
		writeBoot2Flash(selectedBoot, bootAddress, dataBuffer, data_len);

		/* Update local boot object */
		updateBootConfiguration(&boot, selectedBoot, bootAddress, data_len, dataMD5);

		/* Update local partition table */
		updatePartitionTableConfigurations(&PartitionTable2Boots, 2, selectedBoot, selectedBoot, &boot);

		/* Retrive Other Boot Configurations Before Erase */
		readPartitionTableFromFlash((uint8_t*) PARTITION_TABLE_LOC, &oldPartitionTable);

		if(selectedBoot == 0)
		{
			readBootFromTable((uint8_t*) TABLE_BOOT1_LOC, &backupBoot);
		}
		else if(selectedBoot == 1)
		{
			readBootFromTable((uint8_t*) TABLE_BOOT0_LOC, &backupBoot);
		}

		/* Erase Partition Table (Sector 0) */
		erasePartitionTable();

		/* Write Updated Boot Object to Flash */
		writeBoot2Table(bootLocation, &boot);

		/* Write Updated Boot Object to Flash */
		writeBoot2Table(backupBootLoc, &backupBoot);

		/* Write Updated Partition Table Object to Flash */
		writePartitionTable2Flash(PARTITION_TABLE_LOC, &PartitionTable2Boots);

		/* Reset Booting Process to be able to handle many times */
		resetBootingProccess(&startFlashOperation, dataBuffer, &data_len, &package_count);


	}
  }
		  }		// END IAP
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3|RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static void BSP_Config(void)
{

  /* Configure LED1, LED2, LED3 */
  BSP_LED_Init(LED1);
  BSP_LED_Init(LED2);
  BSP_LED_Init(LED3);

  /* Set Systick Interrupt to the highest priority */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0x0, 0x0);
}

/**
  * @brief  Setup the network interface
  * @param  None
  * @retval None
  */
static void Netif_Config(void)
{
  /* Initilialize the LwIP stack without RTOS */
  lwip_init();

  /* IP addresses initialization with DHCP (IPv4) */
  ip4_addr_t ipaddr;
  ip4_addr_t netmask;
  ip4_addr_t gw;

#ifdef USE_DHCP
  ip_addr_set_zero_ip4(&ipaddr);
  ip_addr_set_zero_ip4(&netmask);
  ip_addr_set_zero_ip4(&gw);
#else
  IP_ADDR4(&ipaddr,IP_ADDR0,IP_ADDR1,IP_ADDR2,IP_ADDR3);
  IP_ADDR4(&netmask,NETMASK_ADDR0,NETMASK_ADDR1,NETMASK_ADDR2,NETMASK_ADDR3);
  IP_ADDR4(&gw,GW_ADDR0,GW_ADDR1,GW_ADDR2,GW_ADDR3);
#endif /* USE_DHCP */

  /* add the network interface */
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);

  /*  Registers the default network interface */
  netif_set_default(&gnetif);

  if (netif_is_link_up(&gnetif))
  {
    /* When the netif is fully configured this function must be called */
    netif_set_up(&gnetif);
  }
  else
  {
    /* When the netif link is down this function must be called */
    netif_set_down(&gnetif);
  }

  /* Set the link callback function, this function is called on change of link status*/
  netif_set_link_callback(&gnetif, ethernetif_update_config);

	#ifdef USE_DHCP
  	  /* Start DHCP negotiation for a network interface (IPv4) */
    dhcp_start(&gnetif);
	#endif
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
