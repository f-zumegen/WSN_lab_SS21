/**@file buffer.h*/

#ifndef BUFFER_H
#define BUFFER_H

#include "contiki.h"

#include <stdint.h>
#include <helper.c>

/**Maximum size of the buffer.*/
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 15
#endif

/**Return code for buffer failure.*/
#ifndef BUFFER_FAIL
#define BUFFER_FAIL 0
#endif

/**Return code for buffer success.*/
#ifndef BUFFER_SUCCESS
#define BUFFER_SUCCESS  1
#endif

/**@brief Buffer structure used for outgoing LSA packets.*/
typedef struct
{
	struct timer timers[BUFFER_SIZE];
	struct lsa packets[BUFFER_SIZE];
	bool forward[BUFFER_SIZE];
	bool reply_to_send_lsdb_req[BUFFER_SIZE];
	linkaddr_t dst[BUFFER_SIZE];
	uint8_t read;
	uint8_t write;
}Buffer;

// puts a packet and a timer in the buffer
// returns BUFFER_FAIL if buffer is full
uint8_t BufferIn(Buffer *buffer, struct lsa packet, struct timer packet_timer, bool forward, bool reply_to_send_lsdb_req, linkaddr_t dst);

// removes a packet from a buffer
// returns BUFFER_FAIL if buffer is empty
uint8_t BufferOut(Buffer *buffer, struct lsa *packet, struct timer *packet_timer, bool *forward, bool *reply_to_send_lsdb_req, linkaddr_t *dst);

#endif /* BUFFER_H */

