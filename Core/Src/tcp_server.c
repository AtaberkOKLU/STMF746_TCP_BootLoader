/*
 * tcp_server.c
 *
 *  Created on: Feb 12, 2021
 *      Author: Ataberk ÖKLÜ
 */

// Includes
#include "tcp_server.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "string.h" 		// For memcpy
#include "main.h"
#include "flash_if.h"

uint8_t startFlashOperation = 0;
uint8_t dataBuffer[DATA_BUFFER_SIZE] = {0};				// Stores whole data
uint32_t data_len = 0;									// Whole data len
uint32_t package_count = 0;								// # packs recieved
uint8_t isLastPackage = 0;
uint8_t dataMD5[16] = {0};
uint8_t selectedBoot;

// TCP Control Block Decleare
static struct tcp_pcb *tcp_server_pcb;

/* ECHO protocol states */
enum tcp_server_states
{
  ES_NONE = 0,
  ES_ACCEPTED,
  ES_RECEIVED,
  ES_CLOSING
};

/* structure for maintaing connection infos to be passed as argument
   to LwIP callbacks*/
struct tcp_server_struct
{
  u8_t state;             /* current connection state */
  u8_t retries;
  struct tcp_pcb *pcb;    /* pointer on the current tcp_pcb */
  struct pbuf *p;         /* pointer on the received/to be transmitted pbuf */
};

// Privates Prototypes
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void tcp_server_error(void *arg, err_t err);
static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb);
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static void tcp_server_send(struct tcp_pcb *tpcb, struct tcp_server_struct *es);
static void tcp_server_connection_close(struct tcp_pcb *tpcb, struct tcp_server_struct *es);


// TCP Server Init to start server
void tcp_server_init(void)
{
  /* create new tcp pcb */
  tcp_server_pcb = tcp_new();

  if (tcp_server_pcb != NULL)
  {
	err_t err;

	/* bind echo_pcb to SERVER_PORT (main.h) */
	err = tcp_bind(tcp_server_pcb, IP_ADDR_ANY, SERVER_PORT);

	if (err == ERR_OK)
	{
	  /* start tcp listening for echo_pcb */
	  tcp_server_pcb = tcp_listen(tcp_server_pcb);

	  /* initialize LwIP tcp_accept callback function */
	  tcp_accept(tcp_server_pcb, tcp_server_accept);
	}
	else
	{
	  /* deallocate the pcb */
	  memp_free(MEMP_TCP_PCB, tcp_server_pcb);
	}
  }
}

/**
  * @brief  This function is the implementation of tcp_accept LwIP callback
  * @param  arg: not used
  * @param  newpcb: pointer on tcp_pcb struct for the newly created tcp connection
  * @param  err: not used
  * @retval err_t: error status
  */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  err_t ret_err;
  struct tcp_server_struct *cs;

  LWIP_UNUSED_ARG(arg);
  LWIP_UNUSED_ARG(err);

  /* set priority for the newly accepted tcp connection newpcb */
  tcp_setprio(newpcb, TCP_PRIO_MIN);

  /* allocate structure cs to maintain tcp connection informations */
  cs = (struct tcp_server_struct *)mem_malloc(sizeof(struct tcp_server_struct));
  if (cs != NULL)
  {
    cs->state = ES_ACCEPTED;
    cs->pcb = newpcb;
    cs->retries = 0;
    cs->p = NULL;

    /* pass newly allocated cs structure as argument to newpcb */
    tcp_arg(newpcb, cs);

    /* initialize lwip tcp_recv callback function for newpcb  */
    tcp_recv(newpcb, tcp_server_recv);

    /* initialize lwip tcp_err callback function for newpcb  */
    tcp_err(newpcb, tcp_server_error);

    /* initialize lwip tcp_poll callback function for newpcb */
    tcp_poll(newpcb, tcp_server_poll, 0);

    ret_err = ERR_OK;
  }
  else
  {
    /*  close tcp connection */
    tcp_server_connection_close(newpcb, cs);
    /* return memory error */
    ret_err = ERR_MEM;
  }
  return ret_err;
}

/**
  * @brief  This function is the implementation for tcp_recv LwIP callback
  * @param  arg: pointer on a argument for the tcp_pcb connection
  * @param  tpcb: pointer on the tcp_pcb connection
  * @param  pbuf: pointer on the received pbuf
  * @param  err: error information regarding the reveived pbuf
  * @retval err_t: error code
  */
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  struct tcp_server_struct *cs;
  err_t ret_err;

  LWIP_ASSERT("arg != NULL",arg != NULL);

  /*
   *	Get the package informations to "cs", containing
   *		# of trials
   *		State
   *		pbuf pointer to access data
   */
  cs = (struct tcp_server_struct *)arg;

  /* if we receive an empty tcp frame from client => close connection */
  if (p == NULL)
  {
	/* p pointer is NULL, meaning no data is attached to the package */
    /* It must be FIN package: Remote host closed connection */
    cs->state = ES_CLOSING;	// Change the state to handle correctly in state machine
    if(cs->p == NULL) 		// Is there any packages that we want to send but not done yet.
    {
       /* We're done sending, close connection */
       tcp_server_connection_close(tpcb, cs);
    }
    else
    {
      /* We're not done yet */
      /* Acknowledge received packet */

      /* Set tcp_sent Callback function to handle complition */
      tcp_sent(tpcb, tcp_server_sent);

      /* Then send "cs" carring remaining pbufs, to send incompleted packages */
      tcp_server_send(tpcb, cs);
      /* When Sending process welcomed with ACK package, go to tcp_server_sent (Callback Function) */
    }
    ret_err = ERR_OK;
  }
  /* else : A non empty frame was received from client but for some reason err != ERR_OK */
  else if(err != ERR_OK)	// Which means there is an error somehow
  {
    /* Get rid of all evidences */
    if (p != NULL)
    {
      cs->p = NULL; // Data goes to trash
      pbuf_free(p); //Free the memory of received package
    }
    ret_err = err;
  }
  else if(cs->state == ES_ACCEPTED)
  {
    /* First package has arrived, data chunk in p->payload */


    /* Change the state to handle correctly in state machine */
	cs->state = ES_RECEIVED;

    /* To echo, we do not need to allocate any memory, just pass incomming data package to cs->p */
    /* Store reference to incoming pbuf (chain) */
    cs->p = p;


	// First Package
	memcpy(&selectedBoot, (uint8_t*) p->payload, p->len);

	/* Reception of package has completed */

    /* Set tcp_sent Callback function to handle complition */
    tcp_sent(tpcb, tcp_server_sent);

    /* Send "cs", containing recevied package */
    tcp_server_send(tpcb, cs);

    ret_err = ERR_OK;
  }
  else if (cs->state == ES_RECEIVED)
  {
    /* More data packages received from client*/

    /* Is it the last package we want to send */
    if(cs->p == NULL)
    {
      /* Yes, there is no other packge in queue but this one */
      cs->p = p; // Grab the package to send back

      if(!isLastPackage) {
    	  // Not Last Package
		  memcpy((dataBuffer+package_count*PACKAGE_BUFFER_SIZE), (p->payload+1), (p->len-1));
		  memcpy(&isLastPackage, p->payload, 1);
		  data_len += (p->len)-1;

		  if(data_len > DATA_BUFFER_SIZE)
			  while(1); /* Error Handling procedure is not known, not implemented */

		  /* Reception of package has completed */
		  package_count++;
      } else {
    	  // Last Data Package has already arrived, now we expect checksum
    	  memcpy(dataMD5, (p->payload), p->len);
      }

      /* tcp_sent callback function must be already set after accepting to first package */
      /* Send back received data */
      tcp_server_send(tpcb, cs);
    }
    else
    {
      /* There are another package(s) in queue */
      struct pbuf *ptr;

      /* Chain pbufs to the end of what we recv'ed previously  */
      ptr = cs->p;
      /* Attach Received Data Package (p)" pointer to the next of the last pbuf in the cs chain */
      pbuf_chain(ptr,p);
    }
    ret_err = ERR_OK;
  }
  else if(cs->state == ES_CLOSING)
  {
    /* odd case, remote side closing twice, trash data */
    tcp_recved(tpcb, p->tot_len);
    cs->p = NULL;
    pbuf_free(p);
    ret_err = ERR_OK;
  }
  else
  {
    /* unkown cs->state, trash data  */
    tcp_recved(tpcb, p->tot_len);
    cs->p = NULL;
    pbuf_free(p);
    ret_err = ERR_OK;
  }
  return ret_err;
}


/**
  * @brief  This function implements the tcp_sent LwIP callback (called when ACK
  *         is received from remote host for sent data)
  * @param  None
  * @retval None
  */
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
  /* Create "cs" to handle sending operation */
  struct tcp_server_struct *cs;

  LWIP_UNUSED_ARG(len);

  /* Get the information of sending process */
  cs = (struct tcp_server_struct *)arg;

  /* Since it is ACK'ed by receiver, reset trials */
  cs->retries = 0;

  /* Is there any package to send next */
  if(cs->p != NULL)
  {
    /* Still got pbufs to send */

	/* Set tcp_sent callback function (Reccurisive to handle all waiting packages) */
    tcp_sent(tpcb, tcp_server_sent);

    /* Sent the current package */
    tcp_server_send(tpcb, cs);
  }
  else
  {
	/* There is no data to send */

    /* Check if Client closed the connection (Handled in Recv Callback Func State Machine) */
    if(cs->state == ES_CLOSING)
      tcp_server_connection_close(tpcb, cs);
  }
  return ERR_OK;
}


/**
  * @brief  This function is used to send data for tcp connection
  * @param  tpcb: pointer on the tcp_pcb connection
  * @param  cs: pointer on conn_state structure
  * @retval None
  */
static void tcp_server_send(struct tcp_pcb *tpcb, struct tcp_server_struct *cs)
{
  struct pbuf *ptr;
  err_t wr_err = ERR_OK;

  /*
   * When:
   * 	There is no error
   *  	Not an empy package to sent
   *	The data package size do not exceed TCP_SNDBUF size
   *
   * Then:
   *	Enqueue the data to be send (Handled by LwIP RAW API)
   */
  while ((wr_err == ERR_OK) &&
         (cs->p != NULL) &&
         (cs->p->len <= tcp_sndbuf(tpcb)))
  {

    /* Get pointer on pbuf from cs structure */
    ptr = cs->p;

    /* Enqueue data for transmission */
    wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);

    if (wr_err == ERR_OK)
    {
      u16_t plen;
      u8_t freed;

      plen = ptr->len;

      /* continue with next pbuf in chain (if any) */
      cs->p = ptr->next;

      if(cs->p != NULL)
      {
        /* increment reference count for cs->p */
        pbuf_ref(cs->p);
      }

     /* chop first pbuf from chain */
      do
      {
        /* try hard to free pbuf */
        freed = pbuf_free(ptr);
      }
      while(freed == 0);
     /* we can read more data now */
     tcp_recved(tpcb, plen);
   }
   else if(wr_err == ERR_MEM)
   {
      /* we are low on memory, try later / harder, defer to poll */
     cs->p = ptr;
   }
   else
   {
     /* other problem ?? */
   }
  }
}

/**
  * @brief  This functions closes the tcp connection
  * @param  tcp_pcb: pointer on the tcp connection
  * @param  cs: pointer on conn_state structure
  * @retval None
  */
static void tcp_server_connection_close(struct tcp_pcb *tpcb, struct tcp_server_struct *cs)
{

  /* Remove all callbacks */
  tcp_arg(tpcb, NULL);
  tcp_sent(tpcb, NULL);
  tcp_recv(tpcb, NULL);
  tcp_err(tpcb, NULL);
  tcp_poll(tpcb, NULL, 0);

  /* Delete cs structure */
  if (cs != NULL)
  {
    mem_free(cs);
  }

  /* Close tcp connection */
  tcp_close(tpcb);

  /* Write packageBuffer to Flash */

  /* Is it the last package */
  if (isLastPackage)
  {
	  /* Then, trigger writing to Flash*/
	  startFlashOperation = 1;

	  /* Reset the last package flag */
	  isLastPackage = 0;
  }

}

/**
  * @brief  This function implements the tcp_err callback function (called
  *         when a fatal tcp_connection error occurs.
  * @param  arg: pointer on argument parameter
  * @param  err: not used
  * @retval None
  */
static void tcp_server_error(void *arg, err_t err)
{
  struct tcp_server_struct *cs;

  LWIP_UNUSED_ARG(err);

  cs = (struct tcp_server_struct *)arg;
  if (cs != NULL)
  {
    /*  free cs structure */
    mem_free(cs);
  }
}

/**
  * @brief  This function implements the tcp_poll LwIP callback function
  * @param  arg: pointer on argument passed to callback
  * @param  tpcb: pointer on the tcp_pcb for the current tcp connection
  * @retval err_t: error code
  */
static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb)
{
  err_t ret_err;
  struct tcp_server_struct *cs;

  cs = (struct tcp_server_struct *)arg;
  if (cs != NULL)
  {
    if (cs->p != NULL)
    {
      tcp_sent(tpcb, tcp_server_sent);
      /* there is a remaining pbuf (chain) , try to send data */
      tcp_server_send(tpcb, cs);
    }
    else
    {
      /* no remaining pbuf (chain)  */
      if(cs->state == ES_CLOSING)
      {
        /*  close tcp connection */
        tcp_server_connection_close(tpcb, cs);
      }
    }
    ret_err = ERR_OK;
  }
  else
  {
    /* nothing to be done */
    tcp_abort(tpcb);
    ret_err = ERR_ABRT;
  }
  return ret_err;
}
