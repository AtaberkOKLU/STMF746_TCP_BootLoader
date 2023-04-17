/*
 * partition_table_handler.h
 *
 *  Created on: 23 Şub 2021
 *      Author: atabe
 */

#ifndef INC_PARTITION_TABLE_HANDLER_H_
#define INC_PARTITION_TABLE_HANDLER_H_

/* Defines */
#define PARTITION_TABLE_LOC 0x08000000
#define TABLE_BOOT0_LOC (PARTITION_TABLE_LOC + 0x40)
#define TABLE_BOOT1_LOC (TABLE_BOOT0_LOC + 0x40)


/* Includes */
#include "main.h"
#include "flash_if.h"
#include "string.h"
#include "tcp_server.h"

/* TypeDefs */

typedef struct OTA {
	uint8_t BootID;
	uint32_t Address;
	uint32_t Size;
	uint8_t MD5[16];
} OTA ;

typedef struct PartitionTable{
	uint8_t NumOfBoots;
	int8_t 	selectedOTA;
	OTA* boot0;
	OTA* boot1;
} PartitionTable;

/* Function Prototypes */

/* Local Configuration Update Helpers */
void updateBootConfiguration(OTA* bootMask, uint8_t bootID, uint32_t bootAddress, uint32_t size, uint8_t* checksum);
void updatePartitionTableConfigurations(PartitionTable* partitionTable, uint8_t numOfBoots, int8_t defaultBootID, uint8_t updatedBootID, OTA* boot);

/* Partition Table Helpers */
void readPartitionTableFromFlash(uint8_t* partitionTableLocation, PartitionTable* partitionTable);
void writePartitionTable2Flash(uint32_t partitionTableLocation, PartitionTable* partitionTable);

/* Boot Configurations in Partition Table Helpers */
void readBootFromTable(uint8_t* bootLocation, OTA* boot);
void writeBoot2Table(uint32_t bootLocation, OTA* boot);


/* Boot Programming Helpers */
void writeBoot2Flash(uint8_t bootID, uint32_t bootAddress, uint8_t* data, uint32_t data_size);
void resetBootingProccess(uint8_t* FlashOperationFlag, uint8_t* data, uint32_t* data_size, uint32_t* package_counter);
uint8_t erasePartitionTable(void); /* Fixed: Location Sector 0 */
void updatePartitionTableInFlash(__IO uint8_t* partitionTableLocation, uint32_t size, PartitionTable* partitionTable);

/* MD5 Checksum Control */
uint8_t checkBootMD5(OTA* boot, uint8_t* calculatedMD5);


#endif /* INC_PARTITION_TABLE_HANDLER_H_ */

