#include "buffer.h"
#include <stdio.h>

uint8_t BufferIn(Buffer *buffer, struct lsa packet, struct timer packet_timer, bool forward, bool reply_to_send_lsdb_req, linkaddr_t dst)
{
	// for debug:
	printf("BufferIn: write: %d, read: %d\r\n", buffer->write, buffer->read);

	// check if buffer is full
	if ( ( buffer->write + 1 == buffer->read ) ||
			( buffer->read == 0 && buffer->write + 1 == BUFFER_SIZE ) )
		return BUFFER_FAIL;

	// store packet and timer in the buffer
	buffer->packets[buffer->write] = packet;
	buffer->timers[buffer->write] = packet_timer;
	buffer->forward[buffer->write] = forward;
	buffer->reply_to_send_lsdb_req[buffer->write] = reply_to_send_lsdb_req;
	buffer->dst[buffer->write] = dst;

	buffer->write++;
	// if reached end of buffer set write pointer to 0
	if (buffer->write >= BUFFER_SIZE)
		buffer->write = 0;

	return BUFFER_SUCCESS;
}

uint8_t BufferOut(Buffer *buffer, struct lsa *packet, struct timer *packet_timer, bool *forward, bool *reply_to_send_lsdb_req, linkaddr_t *dst)
{
	// for debug:
	printf("BufferOut: write: %d, read: %d\r\n", buffer->write, buffer->read);

	// check if buffer is empty
	if (buffer->read == buffer->write)
		return BUFFER_FAIL;

	// get packet and timer from the buffer
	*packet = buffer->packets[buffer->read];
	*packet_timer = buffer->timers[buffer->read];
	*forward = buffer->forward[buffer->read];
	*reply_to_send_lsdb_req = buffer->reply_to_send_lsdb_req[buffer->read];
	*dst = buffer->dst[buffer->read];

	buffer->read++;
	// if reached end of buffer set read pointer to 0
	if (buffer->read >= BUFFER_SIZE)
		buffer->read = 0;

	return BUFFER_SUCCESS;
}

