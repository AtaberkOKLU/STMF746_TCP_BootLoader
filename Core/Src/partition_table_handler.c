/*
 * partition_table_handler.c
 *
 *  Created on: 23 Şub 2021
 *      Author: Ataberk ÖKLÜ
 */


/* Includes */
#include "partition_table_handler.h"

/* Private Variables */
uint8_t calculatedMD5[16] = {0};

/* Masks */
OTA boot = {.BootID = 0xFF, .Address = 0xFFFFFFFF, .Size = 0xFFFFFFFF, .MD5 = {[0 ... 15] = 0xFF}};

/*
OTA Boot0 = {.BootID = 0xFF, .Address = 0xFFFFFFFF, .Size = 0xFFFFFFFF, .MD5 = {[0 ... 15] = 0xFF}};
OTA Boot1 = {.BootID = 0xFF, .Address = 0xFFFFFFFF, .Size = 0xFFFFFFFF, .MD5 = {[0 ... 15] = 0xFF}};
*/

PartitionTable PartitionTable2Boots = {.NumOfBoots = 0x02, .selectedOTA = 0xFF, .boot0 = (OTA*) TABLE_BOOT0_LOC, .boot1 = (OTA*) TABLE_BOOT1_LOC};

/* Externals */
extern uint8_t startFlashOperation;
extern uint8_t dataBuffer[DATA_BUFFER_SIZE];
extern uint32_t data_len;
extern uint32_t package_count;


/* Functions */

void readPartitionTableFromFlash(uint8_t* partitionTableLocation, PartitionTable* partitionTable)
{
	uint8_t* ptr = (uint8_t* ) partitionTable;
	for (int i = 0; i < sizeof(PartitionTable); i++, ptr++, partitionTableLocation++ )
	 *ptr = (uint8_t) (*partitionTableLocation);
}

void writePartitionTable2Flash(uint32_t partitionTableLocation, PartitionTable* partitionTable)
{
	FLASH_If_Init();
	uint8_t* data_p = (uint8_t*) partitionTable;
	for ( uint32_t i = 0; i < sizeof(PartitionTable); i++, data_p++, partitionTableLocation++ )
	  HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, partitionTableLocation, *data_p);
}

void readBootFromTable(uint8_t* bootLocation, OTA* boot)
{
	uint8_t* ptr = (uint8_t* ) boot;
	for (int i = 0; i < sizeof(OTA); i++, ptr++, bootLocation++ )
	 *ptr = (uint8_t) (*bootLocation);
}

void writeBoot2Table(uint32_t bootLocation, OTA* boot)
{
	FLASH_If_Init();
	uint8_t* data_p = (uint8_t*) boot;
	for ( uint32_t i = 0; i < sizeof(OTA); i++, data_p++, bootLocation++ )
	  HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, bootLocation, *data_p);
}

void updateBootConfiguration(OTA* bootMask, uint8_t bootID, uint32_t bootAddress, uint32_t size, uint8_t* checksum)
{
	bootMask->BootID = bootID;
	bootMask->Address = bootAddress;
	bootMask->Size = size;
	memcpy(bootMask->MD5, checksum, 16);
}

void updatePartitionTableConfigurations(PartitionTable* partitionTable, uint8_t numOfBoots, int8_t defaultBootID, uint8_t updatedBootID, OTA* boot)
{
	UNUSED(numOfBoots);		/* Fixed Number = 2*/
	UNUSED(updatedBootID);	/* The Location is fixed, refer to partition_table_handler.h */
	UNUSED(boot);			/* The Location is fixed, refer to partition_table_handler.h */

	partitionTable->selectedOTA = defaultBootID; /* -1, 0, 1 */

}

void writeBoot2Flash(uint8_t bootID, uint32_t bootAddress, uint8_t* data, uint32_t data_size)
{
	static __IO uint32_t FlashWriteAddress;

	/* Init flash */
	FLASH_If_Init();

	/* Erase user flash area */
	FLASH_If_Erase(bootAddress, bootID);
	FlashWriteAddress = bootAddress;
	FLASH_If_Write(&(FlashWriteAddress), (uint8_t*)(data), data_size);
}

void resetBootingProccess(uint8_t* FlashOperationFlag, uint8_t* data, uint32_t* data_size, uint32_t* package_counter)
{
	*FlashOperationFlag = 0;
	memset(data, 0, *data_size);
	*package_counter = 0;
	*data_size = 0;
}

uint8_t erasePartitionTable(void)
{
	FLASH_If_Init();

	FLASH_EraseInitTypeDef FLASH_EraseInitStruct;
	uint32_t sectornb = 0;

	FLASH_EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
	FLASH_EraseInitStruct.Sector = FLASH_SECTOR_0;
	FLASH_EraseInitStruct.NbSectors = 1;
	FLASH_EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

	if (HAL_FLASHEx_Erase(&FLASH_EraseInitStruct, &sectornb) != HAL_OK)
	  return (1);

	return (0);
}

/*
 *	This function is prototype for MD5 checksum control of boot
 */

uint8_t checkBootMD5(OTA* boot, uint8_t* calculatedMD5)
{
	UNUSED(boot);
	UNUSED(calculatedMD5);
	/*
	 *	return !(memcmp ( boot->MD5, calculatedMD5, 16 ));
	 *
	 *	memcmp returns 0, when there is no difference
	 *
	 */

	return 1;
}


/*
 * 	This function takes updated local partition table configurations,
 * 		then erases old values in flash to reprogram,
 * 		when it lays in a sector that hold program codes.
 */
void updatePartitionTableFlash(__IO uint8_t* partitionTableLocation, uint32_t size, PartitionTable* partitionTable);

