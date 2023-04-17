/*
 * tcp_server.h
 *
 *  Created on: Feb 12, 2021
 *      Author: Ataberk ÖKLÜ
 */

#ifndef INC_TCP_SERVER_H_
#define INC_TCP_SERVER_H_


// Prototypes
void tcp_server_init(void);

#define DATA_BUFFER_SIZE (32*1024) // 32 Kb
#define PACKAGE_BUFFER_SIZE 512

#endif /* INC_TCP_SERVER_H_ */
