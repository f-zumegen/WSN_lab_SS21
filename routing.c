/** @file routing.c*/

// Contiki specific includes
#include "contiki.h"
#include "net/rime/rime.h"    // Establish connections.
#include "net/netstack.h"     // Wireless-stack definitions.
#include "lib/random.h"
#include "dev/leds.h"
#include "dev/adc-zoul.h"     // ADC
#include "dev/zoul-sensors.h" // Sensor functions.
#include "lib/list.h"
#include "lib/memb.h"
#include "dev/serial-line.h" // Access data received from serial port

// Standard C includes
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <project-conf.h>
#include <buffer.c>
#include <sensor_conversion_functions.h>

//***** TIMERS *****
/**
 * @brief Timer to send keep alive packets.\n
 * Used to indicate that a node is still alive.*/
static struct etimer keep_alive_timer;
/**
 * @brief If down_timer expires and no keep_alive packets
 * received in that period a node is considered down.*/
static struct etimer down_timer;
/**
 * @brief Timer for a intial pre-backoff to avoid congestion and
 * collisions when nodes first go live.*/
static struct etimer initial_pre_backoff_timer;

/**
 * @brief Timer to indicate when to ask for the age of the
 * LSDB of my neighbors. Should be smaller than the keep_aliver_timer.
 */
static struct etimer get_lsdb_timer;

/**@brief When expired we read an adc3 value and send a data packet containing sensor data.*/
static struct etimer sensor_reading_timer;

//***** CONNECTION STUFF *****
/** @brief Instance of a broadcast connection.*/
static struct broadcast_conn broadcast;

/** @brief Instance of a unicast connection.*/
static struct unicast_conn unicast;

/**@brief Instance of a (r)unicast connection.*/
static struct runicast_conn runicast;

//***** PACKET INSTANCES *****
/** @brief Keep alive packet for reception.*/
static struct keep_alive_packet rx_ka_pkt;

/** @brief Keep alive packet for transimission.*/
static struct keep_alive_packet tx_ka_pkt;

/** @brief Link State Adverisment packet for reception.*/
static struct lsa rx_lsa_pkt;

/** @brief Link State Advertisment packet for transimission.*/
static struct lsa tx_lsa_pkt;

/**@brief Unicast packet for transmissions.*/
static struct unicast_packet tx_uni_pkt;

/**@brief Unicast packet for reception.*/
static struct unicast_packet rx_uni_pkt;

//***** MISC VARIABLES*****
/**@brief If forward True we have received an LCA and do reliable forwarding to neighbours.
 * If forward False we generated the packet and reliably flood it to our neighbours.*/
static bool forward;

/**@brief Buffer to store packets until TX.*/
static Buffer buffer;

/**@brief Node ID.*/
static uint8_t node_id;

/**@brief ID of sender.*/
static uint8_t sender_id;

/**@brief Global destination variable used by various functions
 * to construct a link address and send a (r)/unicast.*/
static linkaddr_t dst_t;

/**@brief Destination of sensor data.*/
static linkaddr_t sensor_dest;

/**@brief List of received ages when first going live.*/
static uint8_t rx_ages[TOTAL_NODES];

/**@brief Used when calculating the next hop for data packets.*/
static uint8_t next_hop[TOTAL_NODES];

/**@brief My sequence number, that i attach to every packet every
 * time i advertise a link update (up/down)*/
static uint8_t sequence_number;

/**@brief Our local link state database.*/
static struct link_state_database lsdb;

static int tx_power;

/**@brief Definition in "lib/list.h"*/
LIST(history_table);

/**@brief Definition in "lib/memb.h"*/
MEMB(history_mem, struct history_entry, RUNICAST_RX_HISTORY_ENTRIES);

PROCESS(routing_process, "Routing process");
PROCESS(send_process, "Send process");

/**@brief Put a LSA packet in the buffer with a random timer (pre-backoff).
 * @param tx_pkt Packet to enqueue
 * @forward Used later with send_runicast_to_neighbours(). Definition above.
 */
static void enqueue_packet(struct lsa tx_pkt, bool forward, bool reply_to_send_lsdb_req, linkaddr_t dst){
	uint8_t return_code;
	struct timer pre_backoff_timer;
	printf("enqueue_packet() called!\n");

	timer_set(&pre_backoff_timer, CLOCK_SECOND*(node_id+random_rand()%(TOTAL_NODES*2)));
	// Put packet and timer in queue
	return_code = BufferIn(&buffer, tx_pkt, pre_backoff_timer, forward, reply_to_send_lsdb_req, dst);
	if(return_code == BUFFER_FAIL){
		printf("Buffer is full!");
	}else{
		//Inform send process a new packet was enqueued.
		process_post(&send_process, PROCESS_EVENT_MSG, 0);
	}
}

/**@brief Send my LSDB age to dest.
 * Only if age non zero.
 * @param dst Destination to send unicast.
 */
static void send_lsdb_age(uint8_t dst){
	printf("send_lsdb_age() called!\n");
	if(lsdb.age > 0){///@warning Only reply if we have an age bigger than 0.
		printf("SEND LSDB AGE TO: %d\n", dst);
		tx_uni_pkt.data_packet = false;
		tx_uni_pkt.lsdb_age = lsdb.age;
		tx_uni_pkt.send_lsdb = false;
		packetbuf_copyfrom(&tx_uni_pkt, sizeof(tx_uni_pkt));
		dst_t.u8[0] = 0;
		dst_t.u8[1] = dst;
		leds_on(TX_PKT_COLOR);
		unicast_send(&unicast, &dst_t);
		leds_off(TX_PKT_COLOR);
	}else{
		printf("NOT SENDING AGE %d TO: %d\n", lsdb.age, dst);
	}
}

/**
 * @brief Send LSDB to destination
 * Our LSDB is (should be) symmetric so we only need to send
 * the upper half without the diagonal and only for the links
 * where the weight is non-zero.
 * @param dest Destinaiton to send LSDB.
 */
static void send_lsdb_to(uint8_t dst){
	int i,j;
	printf("send_lsdb_to() called!\n");

	for(i=0;i<TOTAL_NODES;i++){
		for(j=0;j<TOTAL_NODES;j++){
			if(lsdb.node_links_cost[i][j] > 0){ ///@warning Only if non-zero.
				if((i+1) % 2 != 0){
				///@warning If src is not a Sensor mote.
					fill_tx_lsa_pkt(&tx_lsa_pkt, lsdb.node_links_cost[i][j], i+1, j+1, sequence_number, true);
					printf("SEND LSDB LINK TO: %d\n", dst);
					dst_t.u8[0] = 0;
					dst_t.u8[1] = dst;
					print_tx_lsa_pkt_in_buf(&tx_lsa_pkt);
					enqueue_packet(tx_lsa_pkt, false, true, dst_t);
				}
			}
		}
	}
}

/**
 * @brief Sends a unicast packets.
 * When called this function assumes that the packet to be sent is
 * already in the buffer.
 * @param forward If true we are forwarding a packet generated from someone
 * else, if false we are runicasting our own generated packet.
 * */
static void send_runicast_to_neighbours(struct lsa tx_lsa_pkt, bool forward){
	int i;
	printf("send_runicast_to_neighbours(forward=%s) called!\n", forward ? "true":"false");
	for(i=0;i<TOTAL_NODES;i++){
		if(forward == false){
			// Send the packet we generated to:
			if(lsdb.node_links_cost[node_id-1][i]>0){
				//To your outgoing links.
				//You only have outgoing links to a bridge or the sink.
				if(tx_lsa_pkt.endpoint_addresses[0]%2==0){
					if(tx_lsa_pkt.endpoint_addresses[1] == i+1){
						dst_t.u8[0] = 0;
						dst_t.u8[1] = i+1;//lsdb.neighbours[i];
						printf(RED"SENDING LSA TO: %d\n"RESET, i+1);//lsdb.neighbours[i]);
						packetbuf_copyfrom(&tx_lsa_pkt, sizeof(tx_lsa_pkt));
						print_tx_lsa_pkt_in_buf(&tx_lsa_pkt);
						leds_on(TX_PKT_COLOR);
						runicast_send(&runicast, &dst_t, RUNICAST_MAX_RETRANSMISSIONS);
						leds_off(TX_PKT_COLOR);
					}
				}else{
					dst_t.u8[0] = 0;
					dst_t.u8[1] = i+1;//lsdb.neighbours[i];
					printf(RED"SENDING LSA TO: %d\n"RESET, i+1);//lsdb.neighbours[i]);
					packetbuf_copyfrom(&tx_lsa_pkt, sizeof(tx_lsa_pkt));
					print_tx_lsa_pkt_in_buf(&tx_lsa_pkt);
					leds_on(TX_PKT_COLOR);
					runicast_send(&runicast, &dst_t, RUNICAST_MAX_RETRANSMISSIONS);
					leds_off(TX_PKT_COLOR);
				}
			}
		}else{
			// Controlled flooding
			// Forward to all our neighbours execpt:
			if(i+1 != tx_lsa_pkt.endpoint_addresses[0]){///@warning Link src.
				if(i+1 != tx_lsa_pkt.endpoint_addresses[1]){///@warning Link dst.
					if(i+1 != sender_id){///@warning Node id of the sender who send as the runicast packet.
						//TODO sender_id might be overwritten
						if(lsdb.node_links_cost[node_id-1][i] > 0){///@warning Only to neighbours to which there is an outgoing link.
							dst_t.u8[0] = 0;
							dst_t.u8[1] = i+1;//lsdb.neighbours[i];
							printf(RED"FORWARDING LSA TO: %d\n"RESET, i+1);//lsdb.neighbours[i]);
							packetbuf_copyfrom(&tx_lsa_pkt, sizeof(tx_lsa_pkt));
							print_tx_lsa_pkt_in_buf(&tx_lsa_pkt);
							leds_on(TX_PKT_COLOR);
							runicast_send(&runicast, &dst_t, RUNICAST_MAX_RETRANSMISSIONS);
							leds_off(TX_PKT_COLOR);
						}
					}
				}
			}
		}
	}
}

/**
 * @brief Removes link bidirectionally from the local link state database
 * by setting the weight to 0.
 * Calls the fill_tx_lsa_pkt() function to fill the packet buffer.
 * Calls the send_runicast_to_neighbours() function to forward the link down
 * packet to our neighbours.
 * @param src Source of the link.
 * @param dst Destination of the link (the node that is considered down).
 * @param seq_nr Sequence number generated by src.
 * */
static void remove_link_from_lsdb(uint8_t src, uint8_t dst, uint8_t seq_nr){
	printf("remove_link_from_lsdb() with seq_nr %d called!\n", seq_nr);
	if(seq_nr > lsdb.sequence_numbers[src-1] || seq_nr <= RESET_SQN_NO){///@warning RX SEQ NR higher than that of our record. Take over value.
		printf(RED"SEQ NR higher, %d >= %d OR SEQ _NR %d <= 10\n"RESET, seq_nr, lsdb.sequence_numbers[src-1], seq_nr);
		if(lsdb.node_links_cost[src-1][dst-1]>0){
			lsdb.node_links_cost[src-1][dst-1] = 0;
			lsdb.age += 1;
			printf("\nLostLink: %d -> %d\n", src, dst);//For the GUI.
			if(src == node_id){
				// We generated the packet
				forward = false;
			}else{
				forward = true;
			}
			fill_tx_lsa_pkt(&tx_lsa_pkt, 0, src, dst, seq_nr, false);
			enqueue_packet(tx_lsa_pkt, forward, false, dst_t);
		}

		if(lsdb.node_links_cost[dst-1][src-1]>0){
			lsdb.node_links_cost[dst-1][src-1] = 0;
			lsdb.age += 1;
			printf("\nLostLink: %d -> %d\n", dst, src);//For the GUI.
			if(src == node_id){
				// We generated the packet
				forward = false;
			}else{
				forward = true;
			}
			fill_tx_lsa_pkt(&tx_lsa_pkt, 0, dst, src, seq_nr, false);
			enqueue_packet(tx_lsa_pkt, forward, false, dst_t);
		}

		// Update old sequence number
		lsdb.sequence_numbers[src-1] = seq_nr;
		lsdb.sequence_numbers[dst-1] = RESET_SQN_NO;
	}else if(seq_nr < lsdb.sequence_numbers[src-1]){///@warning RX SEQ NR lower than our record. Update what will the forwarded.
		// We don't change our LSDB as we have the newest update.
		forward = false;
		fill_tx_lsa_pkt(&tx_lsa_pkt, lsdb.node_links_cost[src-1][dst-1], src, dst, lsdb.sequence_numbers[src-1], false);
		enqueue_packet(tx_lsa_pkt, forward, false, dst_t);
	}else{
		printf("IGNORING LSA with the sequence number %d from source %d, we already got that!\n", seq_nr, src);
	}
	print_link_state_database(&lsdb);
}

/**
 * @brief Add a directional link to the local link state database.\n
 * Calls the fill_tx_lsa_pkt() function passing a packet depending
 * on the sequence number.\n
 * Calls the send_runicast_to_neighbours() function to forward the link up
 * packet to our neighbours.
 * @param src Source of the link.
 * @param dst Destination of the link.
 * @param cost Cost of the link.
 * @param seq_nr Sequence number generated by src.
 * */
static void add_link_to_lsdb(uint8_t src, uint8_t dst, uint16_t cost, uint8_t seq_nr){
	printf("add_link_to_lsdb()\n");
	if(lsdb.node_links_cost[src-1][dst-1] >0){///@warning Link is in DB. Chech sequence numbers.
		printf(RED"Link %d->%d is in DB, checking seq numbers!\n"RESET, src, dst);
		if(seq_nr > lsdb.sequence_numbers[src-1] || seq_nr <= RESET_SQN_NO){///@warning RX SEQ NR higher than that of our record. Take over value.
			printf(RED"SEQ NR higher, %d >= %d OR SEQ _NR %d <= RESET_SQN_NO\n"RESET, seq_nr, lsdb.sequence_numbers[src-1], seq_nr);
			lsdb.node_links_cost[src-1][dst-1] = cost;
			printf("\nNewLink: %d -> %d\n", src, dst);//For the GUI
			lsdb.age += 1;
			lsdb.sequence_numbers[src-1] = seq_nr;
			/*
			if(src == node_id){
				// We generated the packet
			lsdb.node_links_cost[dst-1][src-1] = cost + 1;
			lsdb.age += 1;
				forward = false;
			}else{
				forward = true;
			}*/
			fill_tx_lsa_pkt(&tx_lsa_pkt, cost, src, dst, seq_nr, false);
			enqueue_packet(tx_lsa_pkt, forward, false, dst_t);
		}else if(seq_nr < lsdb.sequence_numbers[src-1]){///@warning RX SEQ NR lower than that of our record. Update what will be forwarded.
			printf(RED"SEQ NR lower, %d < %d\n"RESET, seq_nr, lsdb.sequence_numbers[src-1]);
			forward = false;
			fill_tx_lsa_pkt(&tx_lsa_pkt, lsdb.node_links_cost[src-1][dst-1], src, dst, lsdb.sequence_numbers[src-1], false);
			enqueue_packet(tx_lsa_pkt, forward, false, dst_t);
		}else{///@warning RX SEQ NR is the same. Don't do anything.
			printf("IGNORING LSA with the sequence number %d from source %d, we already got that!\n", seq_nr, src);
		}

	}else if(lsdb.node_links_cost[src-1][dst-1] == 0){///@warnign Link not in DB, add it.
		if(src == node_id){
			// We generated the packet
			forward = false;
			if(src == SINK_ID){///@warning If src node 1 we dont do anything
				printf("\n");

			}else if(dst == SINK_ID){
				printf(RED"Link %d->%d (%d) not in DB, adding\n"RESET, src, dst, cost);
				printf("\nNewLink: %d -> %d\n", src, dst);//For the GUI
				sequence_number = (sequence_number + 1)%255;///@warning Circular sequence number.
				lsdb.node_links_cost[src-1][dst-1] =  cost;//vdd3_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
				lsdb.age += 1;
				fill_tx_lsa_pkt(&tx_lsa_pkt, cost, src, dst, seq_nr, false);
				enqueue_packet(tx_lsa_pkt, forward, false, dst_t);

			}else{///@warning If not src/dst 1.
				if(src % 2 != 0 && dst % 2 != 0){///@warning SRC and DST are bridges => DUPLEX Link.
					printf(RED"Link %d->%d (%d) not in DB, adding\n"RESET, src, dst, cost);
					printf("\nNewLink: %d -> %d\n", src, dst);//For the GUI
					sequence_number = (sequence_number + 1)%255;///@warning Circular sequence number.
					lsdb.node_links_cost[src-1][dst-1] = cost;
					lsdb.age += 1;
					fill_tx_lsa_pkt(&tx_lsa_pkt, cost, src, dst, seq_nr, false);
					enqueue_packet(tx_lsa_pkt, forward, false, dst_t);

				}else if(src % 2 != 0 && dst % 2 == 0){///@warning SRC Bridge and DST Sensor => Directed link from B->S.
					printf("\n");
				}else if(src % 2 == 0 && dst % 2 != 0){///@warning SRC Sensor and DST Bridge => Directed link from S->B.
					printf(RED"Link %d->%d (%d) not in DB, adding\n"RESET, src, dst, cost);
					printf("\nNewLink: %d -> %d\n", src, dst);//For the GUI
					sequence_number = (sequence_number + 1)%255;///@warning Circular sequence number.
					lsdb.node_links_cost[src-1][dst-1] = cost;
					lsdb.age += 1;
					fill_tx_lsa_pkt(&tx_lsa_pkt, cost, src, dst, seq_nr, false);
					enqueue_packet(tx_lsa_pkt, forward, false, dst_t);
				}
			}
		}else{
			// Someone forwarded the packet to us.
			printf(RED"Link %d->%d (%d) not in DB, adding\n"RESET, src, dst, cost);
			printf("\nNewLink: %d -> %d\n", src, dst);//For the GUI
			lsdb.node_links_cost[src-1][dst-1] = cost;
			lsdb.age += 1;
			lsdb.sequence_numbers[src-1] = seq_nr;
			fill_tx_lsa_pkt(&tx_lsa_pkt, cost, src, dst, seq_nr, false);
			forward = true;
			enqueue_packet(tx_lsa_pkt, forward, false, dst_t);
		}
	}
	print_link_state_database(&lsdb);
}

/**@brief Callback function when we receive a broadcast.
 * We receive a broadcast in two cases:
 * 1) Someone is asking the age of our LSDB.
 * 2) Keep Alive (Hello) packet.*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){
	int i;
	int16_t rssi;
	leds_on(RX_PKT_COLOR);
	rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
	printf("Broadcast message received from %d | ", from->u8[1]);
	printf("RSSI: %d\n", rssi);
	if(rssi >= IGNORE_RSSI_BELOW){
		packetbuf_copyto(&rx_ka_pkt);
		printf("Packet size %d(bytes):\n", packetbuf_datalen());
		printf("Node ID: %d\n", from->u8[1]);
		printf("Battery value: %d\n", rx_ka_pkt.battery_value);
		printf("Neighbours: ");
		for(i=0;i<TOTAL_NODES;i++){
			if(rx_ka_pkt.neighbours[i] != 0){
				printf("%d | ", rx_ka_pkt.neighbours[i]);
			}
		}
		printf("\n");
	}else{
		printf("Ignoring broadcast packet with RSSI:%d\n", rssi);
		leds_off(RX_PKT_COLOR);
		return;
	}

	if(rx_ka_pkt.get_lsdb_req == true){///@warning Sender asking for LSDB age.
		lsdb.neighbours[from->u8[1]-1] = from->u8[1];///@warning Add LSDB Age asker to neighbours.
		lsdb.ka_received[from->u8[1]-1] += 1;

		//TODO MAYBE IF WE ALREADY SEE OUR NODE ID IN THE RX NEIGBOUR LIST ADD A LINK.
		//if(node_id != 1 && node_id % 2 != 0){///@warning The sink (node 1) or Sensor nodes don't respond to this request.
		if(node_id % 2 != 0){
			dst_t.u8[0] = 0;
			dst_t.u8[1] = from->u8[1];
			send_lsdb_age(from->u8[1]);
		}else{
			printf("Not responding to LSDB age request since we have a node id: %d\n", node_id);
		}
	}else if(rx_ka_pkt.get_lsdb_req == false){///@warning Normal keep alive message.
		if(lsdb.neighbours[from->u8[1]-1] != from->u8[1]){///@warning Neighbour not in list.
			lsdb.neighbours[from->u8[1]-1] = from->u8[1];

		}
		if(node_id == rx_ka_pkt.neighbours[node_id-1]){///@warning My node id is in the received neighbours list.
			if(lsdb.ka_received[from->u8[1]-1] >= 0 && (lsdb.node_links_cost[node_id-1][from->u8[1]-1] == 0)){
				///@warning If we go from 0 keep alive packets received to 1 and the link was previously down, then the link is completely new. Since in the case of a link between sensor and bridge we only add one directed link.

				if( (lsdb.node_links_cost[node_id-1][SINK_ID-1]>0||lsdb.neighbours[SINK_ID-1]>0) && rx_ka_pkt.neighbours[SINK_ID-1] == SINK_ID){///@warning If SRC and DST both have node 1 as neighbour, no need for link between us.
					printf("No need for link between: %d->%d, both can reach 1 with one hop!\n", node_id, from->u8[1]);
				}else{
					add_link_to_lsdb(node_id, from->u8[1], rx_ka_pkt.battery_value, sequence_number);
				}
			}else if(lsdb.ka_received[from->u8[1]-1] > 0 && lsdb.node_links_cost[node_id-1][from->u8[1]-1] > 0){
				///@warning We already have that link. Update to latest cost received from him.
				lsdb.node_links_cost[node_id-1][from->u8[1]-1] = rx_ka_pkt.battery_value;
			}else if(lsdb.ka_received[from->u8[1]-1] > 0 && lsdb.node_links_cost[from->u8[1]-1][node_id-1] > 0){
				///@warning Update link cost of them to send to me. Just for looks.
				lsdb.node_links_cost[from->u8[1]-1][node_id-1] = vdd3_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
			}
		}
		lsdb.ka_received[from->u8[1]-1] += 1;
	}
	leds_off(RX_PKT_COLOR);
}

/**@brief Callback function when we receive a runicast transmission.
 * Runicast transmissions are used for LSA (Link State Advertisment) packets,
 * aka if a link is up/down.
 * */
static void runicast_recv(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	leds_on(RX_PKT_COLOR);
	packetbuf_copyto(&rx_lsa_pkt);

	// Since we heard from the sender
	lsdb.ka_received[from->u8[1]-1] += 1;

		/*Sender History.*/
	struct history_entry *e = NULL;
	for(e = list_head(history_table); e != NULL; e = e->next){
		if(linkaddr_cmp(&e->addr, from)){
			break;
		}
	}
	if(e == NULL){
		/*Create new history entry.*/
		e = memb_alloc(&history_mem);
		printf("Creating new history entry.\n");
		if(e == NULL){
			e = list_chop(history_table); /*Remove oldest at full history.*/
		}
		linkaddr_copy(&e->addr, from);
		e->seq = seqno;
		list_push(history_table, e);
	}else{
		/*Detect duplicate callbacks.*/
		if(e->seq == seqno){
			printf("(DUPLICATE) Runicast message received from %d, seqno %d\n", from->u8[1], seqno);
			return;
		}
		/*Update existing history entry.*/
		e->seq = seqno;
		printf("Updating existing history entry.\n");
	}

	sender_id = from->u8[1];
	printf("Runicast message received from %d | ", sender_id);
	printf("Packet size: %d(bytes)\n", packetbuf_datalen());
	printf("Node id: %d\n", from->u8[1]);//rx_lsa_pkt.node_id);
	printf("Link cost: %d\n", rx_lsa_pkt.link_cost);
	printf("Link: %d->%d\n", rx_lsa_pkt.endpoint_addresses[0], rx_lsa_pkt.endpoint_addresses[1]);
	printf("Seq nr: %d\n", rx_lsa_pkt.seq_nr);
	printf("Reply to send LSDB req: %s\n", tx_lsa_pkt.reply_to_send_lsdb_req ? "true":"false");

	if(rx_lsa_pkt.reply_to_send_lsdb_req == true){///@warning We got a reply to our send LSDB request.
		lsdb.node_links_cost[rx_lsa_pkt.endpoint_addresses[0]-1][rx_lsa_pkt.endpoint_addresses[1]-1] = rx_lsa_pkt.link_cost;
		print_link_state_database(&lsdb);
	}else if(rx_lsa_pkt.reply_to_send_lsdb_req == false){///@warning Normal LSA.
		if(rx_lsa_pkt.link_cost > 0){
			add_link_to_lsdb(
					rx_lsa_pkt.endpoint_addresses[0],
					rx_lsa_pkt.endpoint_addresses[1],
					rx_lsa_pkt.link_cost,
					rx_lsa_pkt.seq_nr);
		}else if(rx_lsa_pkt.link_cost == 0){
			remove_link_from_lsdb(
					rx_lsa_pkt.endpoint_addresses[0],
					rx_lsa_pkt.endpoint_addresses[1],
					rx_lsa_pkt.seq_nr);
		}
	}
	leds_off(RX_PKT_COLOR);
}

/**Callback function for unicast trasnmissions.
 * We have unicast transmissions when we:
 * 1) Get a reply to our LSDB age reqeust.\n
 * 2) Ask from someone for their LSDB.\n
 * 3) Get someones LSDB.
 * 4) Normal sensor data transmission.
 */
static void unicast_recv(struct unicast_conn *c, const linkaddr_t *from){

	uint8_t i;
	uint16_t max;
	leds_on(RX_PKT_COLOR);
	// Since we heard from the sender
	lsdb.ka_received[from->u8[1]-1] += 1;

	packetbuf_copyto(&rx_uni_pkt);
	printf("Unicast message received from %d | ", from->u8[1]);
	printf("Packet size: %d(bytes)\n", packetbuf_datalen());
	printf("Node id: %d\n", from->u8[1]);
	printf("LSDB Age: %d\n", rx_uni_pkt.lsdb_age);
	printf("Send LSDB: %d\n", rx_uni_pkt.send_lsdb);
	printf("TTL (only for data packets:): %d\n", rx_uni_pkt.ttl);

	if(rx_uni_pkt.data_packet == false){///@warning Not a data packet.
		if(rx_uni_pkt.lsdb_age > 0 && rx_uni_pkt.send_lsdb == false){///@warning Received age to our get age request.
			printf("Received age %d from %d\n", rx_uni_pkt.lsdb_age, from->u8[1]);
			rx_ages[from->u8[1]-1] = rx_uni_pkt.lsdb_age;
			lsdb.neighbours[from->u8[1]-1] = from->u8[1];///@warning Add LSDB Age sender to neighbour list.

		}else if(rx_uni_pkt.send_lsdb == true){///@warning Got LSDB send request.
	  send_lsdb_to(from->u8[1]);
		}
	}else if(rx_uni_pkt.data_packet == true){///@warning Data packet.
		printf("Got data packet from: %d!\n", from->u8[1]);
		if(node_id == SINK_ID){///@warning Package arrived at sink!
			printf(RED"Package arrived at destination: %d!\n"RESET, node_id);
			printf("\nDataType: %d Data: %d\n", rx_uni_pkt.data_type, rx_uni_pkt.data);
			printf("PacketPath:");
			for(i=0;i<TOTAL_NODES;i++){
				if(rx_uni_pkt.path[i] != 0){
					printf(" %d ->", rx_uni_pkt.path[i]);
				}else{
					printf(" %d\n", node_id);
					break;
				}
			}
		}else{
			rx_uni_pkt.ttl -= 1;
			if(rx_uni_pkt.ttl <= 0 && node_id != SINK_ID){
				//@warning TTL expired and we are not node 1.
				//Discard packet and do not do anything.
				printf("Expired TTL, discarding data packet:\n");
				printf("DataType: %d Data: %d\n", rx_uni_pkt.data_type, rx_uni_pkt.data);
				printf("TTL: %d\n", rx_uni_pkt.ttl);
				leds_off(RX_PKT_COLOR);
				return;
			}
			for(i=0;i<TOTAL_NODES;i++){
				if(rx_uni_pkt.path[i] != 0){
					printf("Path taken so far: %d -> ", rx_uni_pkt.path[i]);
				}else{
					printf("%d\n", node_id);
					rx_uni_pkt.path[i] = node_id;
					break;
				}
			}
			printf("DEBUG: node_links_cost[node_id-1][SINK_ID-1]: %d\n", lsdb.node_links_cost[node_id-1][SINK_ID-1]);
			if(lsdb.node_links_cost[node_id-1][SINK_ID-1]>0){
				///@warning We have a direct link to the sink!
				printf("We have a direct link to the sink!\n");
				dst_t.u8[0] = 0;
				dst_t.u8[1] = SINK_ID;
				printf("Data packet send to: %d\n", dst_t.u8[1]);
				packetbuf_copyfrom(&rx_uni_pkt, sizeof(rx_uni_pkt));
				leds_on(TX_PKT_COLOR);
				unicast_send(&unicast, &dst_t);
				leds_off(TX_PKT_COLOR);
			}else{
				///@warning We don't have a direct link to the sink!
				printf("We don't have a direct link to the sink!\n");
				dst_t.u8[0] = 0;
				dst_t.u8[1] = 0;
				//for(i=2;i<TOTAL_NODES;i+=2){ KEEP IT SIMPLE AT BEGINNING
				for(i=0;i<TOTAL_NODES;i++){
					//We dont have a direct link to 1 so we start at 2.
					//We are not gonna send to a sensor mote.
					next_hop[i] = 0;
					if(lsdb.node_links_cost[node_id-1][i]  > 0 && lsdb.node_links_cost[i][SINK_ID-1]>0 && i+1!=from->u8[1]){
						//If i have a link to someone and that someone to the sink.
						//Dont send to the one you received from
						next_hop[i] = lsdb.node_links_cost[node_id-1][i];
					}
				}
				max = 0;
				for(i=0;i<TOTAL_NODES;i++){
					//Find best next hop of the ones adjacent to 1.
					if(max < next_hop[i]){
						max = next_hop[i];
						dst_t.u8[1] = i+1;
					}
				}
				if(dst_t.u8[1] == 0){
					printf("We don't have a direct link and none of our neighbours has a direct link to the sink!\n");
					//I know this is not very efficient and does not really prevent infinite routing loops, BUT
					//it is only supposed to work for a limited number of hops.
					///Dont send from where you received.
					///We don't have a direct link to the sink. Send to bridge with highest battery left.
					max = 0;
					for(i=0;i<TOTAL_NODES;i++){
						if(max < lsdb.node_links_cost[node_id-1][i] && i+1!=from->u8[1]){
							max = lsdb.node_links_cost[node_id-1][i];
							dst_t.u8[1] = i+1;
						}
					}
				}
				printf("Data packet send to: %d\n", dst_t.u8[1]);
				packetbuf_copyfrom(&rx_uni_pkt, sizeof(rx_uni_pkt));
				leds_on(TX_PKT_COLOR);
				unicast_send(&unicast, &dst_t);
				leds_off(TX_PKT_COLOR);
			}
		}
	}
	leds_off(RX_PKT_COLOR);
}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
	printf("Runicast message sent to %d, (RE)-TRANSMISSIONS: %d\n", to->u8[1], retransmissions);
}


// Callback functions
static struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct unicast_callbacks unicast_call = {unicast_recv};
static struct runicast_callbacks runicast_call = {runicast_recv, sent_runicast};


AUTOSTART_PROCESSES(&routing_process, &send_process);

PROCESS_THREAD(send_process, ev, data){
	PROCESS_EXITHANDLER(runicast_close(&runicast);)
	PROCESS_BEGIN();
	printf("send_process started!\n");

	static struct etimer t;
	static struct timer packet_timer;
	static struct lsa tx_packet;
	static bool reply_to_send_lsdb_req;
	static linkaddr_t dst;
	static bool forward;
	static uint8_t return_code;
	static uint8_t is_processing_packet = 0;
	static clock_time_t remaining_time;

	while(1) {
		PROCESS_WAIT_EVENT();

		// a new packet has been added to the buffer
		if (ev == PROCESS_EVENT_MSG) {
			// is a packet already processed?
			if ( ! is_processing_packet ) {
				is_processing_packet = 1;
				// get a packet from the buffer
				return_code = BufferOut(&buffer, &tx_packet, &packet_timer, &forward, &reply_to_send_lsdb_req, &dst);
				// there was nothing in the buffer
				if (return_code == BUFFER_FAIL){
					is_processing_packet = 0;
				}
				// there was something in the buffer
				else {
					remaining_time = timer_remaining(&packet_timer);
					// check if the timer has already expired
					if(timer_expired(&packet_timer)){
						printf("pre backoff expired, in send_process!\n");
						if(runicast_is_transmitting(&runicast)){
							printf("Runicast is transmitting other packet, put back in buffer!\n");
							enqueue_packet(tx_packet, forward, reply_to_send_lsdb_req, dst);
						}else if(!runicast_is_transmitting(&runicast)){
							if(reply_to_send_lsdb_req == true){
								packetbuf_copyfrom(&tx_packet, sizeof(tx_packet));
								leds_on(TX_PKT_COLOR);
								runicast_send(&runicast, &dst, RUNICAST_MAX_RETRANSMISSIONS);
								leds_off(TX_PKT_COLOR);
								printf("Replying with LSDB link to get LSDB request to: %d%d!\n", dst.u8[0],dst.u8[1]);
							}else if(reply_to_send_lsdb_req == false){
								send_runicast_to_neighbours(tx_packet, forward);
							}
						}
						is_processing_packet = 0;
						process_post(&send_process, PROCESS_EVENT_MSG, 0);
					}
					else {
						// wait for the remaining time
						etimer_set(&t, remaining_time);
					}
				}
			}
		} else if(ev == PROCESS_EVENT_TIMER){
			// timer indicating time to send expired
			if(etimer_expired(&t)){
				printf("pre backoff expired, in send_process!\n");
				if(runicast_is_transmitting(&runicast)){
					printf("Runicast is transmitting other packet, put back in buffer!\n");
					enqueue_packet(tx_packet, forward, reply_to_send_lsdb_req, dst);
				}else if(!runicast_is_transmitting(&runicast)){
					if(reply_to_send_lsdb_req == true){
						packetbuf_copyfrom(&tx_packet, sizeof(tx_packet));
						leds_on(TX_PKT_COLOR);
						runicast_send(&runicast, &dst, RUNICAST_MAX_RETRANSMISSIONS);
						leds_off(TX_PKT_COLOR);
						printf("Replying with LSDB link to get LSDB request to: %d%d!\n", dst.u8[0],dst.u8[1]);
					}else if(reply_to_send_lsdb_req == false){
						send_runicast_to_neighbours(tx_packet, forward);
					}
				}
				is_processing_packet = 0;
				// tell the process to check if there is another packet in the
				// buffer
				process_post(&send_process, PROCESS_EVENT_MSG, 0);
			}
		}
	}
	PROCESS_END();
}


PROCESS_THREAD(routing_process, ev, data){
	PROCESS_EXITHANDLER(unicast_close(&unicast);)
	PROCESS_BEGIN();
	printf("routing_process started!\n");
	node_id = linkaddr_node_addr.u8[1];


	// Set timers.
	if(node_id == SINK_ID){
		etimer_set(&initial_pre_backoff_timer, CLOCK_SECOND);
	}else{
		etimer_set(&initial_pre_backoff_timer, INIT_PRE_BACKOFF_PERIOD);
	}
	etimer_set(&keep_alive_timer, KEEP_ALIVE_PERIOD);
	etimer_set(&down_timer, DOWN_PERIOD);
	etimer_set(&get_lsdb_timer, GET_LSDB_PERIOD);
	etimer_set(&sensor_reading_timer, SENSOR_READ_INTERVAL);

	/*Set radio parameters.*/
	NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, CHANNEL);
	NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER);

	/*Configure ADC port.*/
	adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC1 | ZOUL_SENSORS_ADC3);

	/*Open connections.*/
	broadcast_open(&broadcast, BROADCAST_RIME_CHANNEL, &broadcast_call);
	unicast_open(&unicast, UNICAST_RIME_CHANNEL, &unicast_call);
	runicast_open(&runicast, 144, &runicast_call);

	uint16_t max;
	uint8_t i;
	uint8_t get_lsdb;
	static uint16_t adc3_value;
	static int sensor_value;

	/*RX runi sender history.*/
	list_init(history_table);
	memb_init(&history_mem);

	while(1){
		PROCESS_WAIT_EVENT();
		if(ev == serial_line_event_message){
			if(strcmp(data, "print.lsdb") == 0){
				print_link_state_database(&lsdb);
			}else if(strcmp(data, "print.n") == 0){
				print_neighbour_list(lsdb.neighbours, lsdb.ka_received);
			}else if(strcmp(data, "whoami") == 0){//hahaha
				printf("I am: %d\n", node_id);
			}
		}else if(etimer_expired(&keep_alive_timer) && etimer_expired(&initial_pre_backoff_timer)){
			printf("keep_alive_timer EXPIRED! | I am node: %d | ", node_id);
			tx_ka_pkt.battery_value = vdd3_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
			printf("My battery value: %d\n", tx_ka_pkt.battery_value);
			tx_ka_pkt.get_lsdb_req = false;
			memcpy(tx_ka_pkt.neighbours, lsdb.neighbours, sizeof(lsdb.neighbours));
			packetbuf_copyfrom(&tx_ka_pkt, sizeof(tx_ka_pkt));
			printf("BROADCAST PACKET SIZE: %d (bytes)\n", sizeof(tx_ka_pkt));
			broadcast_send(&broadcast);
			etimer_set(&keep_alive_timer, KEEP_ALIVE_PERIOD);
			NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_TXPOWER, &tx_power);
			printf("Broadcast message sent with power: %d\r\n", tx_power);


		}else if(etimer_expired(&down_timer) && etimer_expired(&initial_pre_backoff_timer)){
			printf("down_timer EXPIRED!\n");
			for(i=0;i<TOTAL_NODES;i++){
				if(lsdb.ka_received[i] == 0){
					//No keep alives in DOWN_PERIOD.
					if(lsdb.node_links_cost[node_id-1][i] > 0 || lsdb.node_links_cost[i][node_id-1]>0){
						//Link was previously up -> Link is now considered down.
						printf(RED"I have a link down!\n"RESET);
						sequence_number = (sequence_number + 1)%255;///@warning Circular sequence number.
						lsdb.neighbours[i] = 0;
						remove_link_from_lsdb(node_id, i+1, sequence_number);
					}
				}else{
					// Needs to be reset to 0 after every down timer expiration.
					lsdb.ka_received[i] = 0;
					lsdb.neighbours[i] = 0;///@warning Check if this screws everything up :)
				}
			}
			etimer_set(&down_timer, DOWN_PERIOD);

		}else if(etimer_expired(&sensor_reading_timer) && etimer_expired(&initial_pre_backoff_timer)){
			/*Read ADC values. Data is in the 12 MSBs.*/
			if(node_id%2==0){
			adc3_value = adc_zoul.value(ZOUL_SENSORS_ADC3) >> 4;
			printf("ADC3 value [Raw] = %d\n", adc3_value);
			}

			switch(node_id){
				case 2: sensor_value = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED); break;
				case 4: sensor_value = (int)getSoilMoisture1(adc3_value); break;
				case 6: sensor_value = (int)getSoilMoisture2(adc3_value); break;
				case 8: sensor_value = (int)getLightSensorValue(adc3_value); break;
				case 10: sensor_value = (int)getpHlevel(adc3_value); break;
				case 12: sensor_value = (int)getHumidityValue(adc3_value); break;
			}
			if(node_id%2==0){
				//Only write to buffer if we have to.
				printf("Sensor value converted: %d\n", sensor_value);
				tx_uni_pkt.data_packet = true;
				tx_uni_pkt.data_type = node_id;
				tx_uni_pkt.data = sensor_value;
				tx_uni_pkt.path[0] = node_id;
				tx_uni_pkt.ttl = TTL;
				printf("Data packet size: (%d) bytes\n", sizeof(tx_uni_pkt));
				packetbuf_copyfrom(&tx_uni_pkt, sizeof(tx_uni_pkt));
				sensor_dest.u8[1] = SINK_ID;
				if(lsdb.node_links_cost[node_id-1][sensor_dest.u8[1]-1]>0){
					///We have a direct link to the sink.
					printf("We have a direct link to the sink!\n");
					printf("Data packet send to: %d\n", sensor_dest.u8[1]);
					leds_on(TX_PKT_COLOR);
					unicast_send(&unicast, &sensor_dest);
					leds_off(TX_PKT_COLOR);
				}else if(lsdb.node_links_cost[node_id-1][sensor_dest.u8[1]-1] == 0){
					///We don't have a direct link to the sink. Send to bridge with highest battery left.
					printf("We don't have a direct link to the sink!\n");
					max = 0;
					for(i=0;i<TOTAL_NODES;i++){
						if(max < lsdb.node_links_cost[node_id-1][i]){
							max = lsdb.node_links_cost[node_id-1][i];
							sensor_dest.u8[1] = i+1;
						}
					}
					if(sensor_dest.u8[1] != SINK_ID){///Since we don't have a link to one.
						printf("Data packet send to: %d\n", sensor_dest.u8[1]);
						leds_on(TX_PKT_COLOR);
						unicast_send(&unicast, &sensor_dest);
						leds_off(TX_PKT_COLOR);
					}
				}
			}
			etimer_set(&sensor_reading_timer, SENSOR_READ_INTERVAL);

		}else if(etimer_expired(&get_lsdb_timer) && etimer_expired(&initial_pre_backoff_timer)){
			printf("get_lsdb_timer EXPIRED!\n");
			etimer_restart(&keep_alive_timer);
			etimer_restart(&sensor_reading_timer);
			etimer_restart(&down_timer);
			max = 0;
			get_lsdb = 0;

			if(lsdb.neighbours[0] != SINK_ID){///@warning No need to get the LSDB of neighbours, if we are adjacent to the sink.
				for(i=0;i<TOTAL_NODES;i++){
					if(max < rx_ages[i]){
						max = rx_ages[i];
						get_lsdb = i+1;
					}
				}
				if(get_lsdb > 0){///@warning Only send unicast if node id not 0.
					dst_t.u8[0] = 0;
					dst_t.u8[1] = get_lsdb;
					printf("GET LSDB FROM: %d\n", dst_t.u8[1]);
					tx_uni_pkt.data_packet = false;
					tx_uni_pkt.send_lsdb = true;
					tx_uni_pkt.lsdb_age = 0;
					packetbuf_copyfrom(&tx_uni_pkt, sizeof(tx_uni_pkt));
					leds_on(TX_PKT_COLOR);
					unicast_send(&unicast, &dst_t);
					leds_off(TX_PKT_COLOR);
				}else if(get_lsdb == 0){
					printf("GOT NO AGE REPLIES!\n");
				}
			}else{
				printf("Not getting LSDB from neighbours, since we are adjacent to node 1!\n");
			}
		}else if(etimer_expired(&initial_pre_backoff_timer)){
			printf("initial_pre_backoff_timer EXPIRED!\n");
			sequence_number = RESET_SQN_NO;
			lsdb.age = 0;
			if(node_id % 2 != 0){
				printf("Asking for LSDB Ages!\n");
				tx_ka_pkt.get_lsdb_req = true;
				memcpy(tx_ka_pkt.neighbours, lsdb.neighbours, sizeof(lsdb.neighbours));
				packetbuf_copyfrom(&tx_ka_pkt, sizeof(tx_ka_pkt));
				broadcast_send(&broadcast);
			}else{
				printf("Not asking for LSDB Ages, since we are a sensor mote!\n");
			}
			etimer_restart(&keep_alive_timer);
			etimer_restart(&sensor_reading_timer);
			etimer_restart(&down_timer);
			//etimer_restart(&get_lsdb_timer);
		}
	}

	PROCESS_END();
}
