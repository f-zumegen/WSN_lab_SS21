/** @file helper.c
*/

/**
 * @brief Sends a broadcast asking hearing nodes about the "age"
 * of their LSDB.
 * Nodes he hears from are added to the LSDB.
 */

/**@brief Link State Advertisment (LSA) packet TODO*/
static struct lsa{
	//uint8_t node_id;/**<Node id of sender.*/
	//uint16_t lsdb_age;/**<Age of my LSDB.*/
	//uint8_t send_lsdb;/**<If 1 send LSDB to sender.*/
	bool reply_to_send_lsdb_req;/**<If true the packet is a answer to someone asking for our LSDB.*/
	uint16_t link_cost;/**<Link cost*/
	uint8_t endpoint_addresses[2];/**<Endpoint addresses of a link (0 => Source, 1 => Destination)*/
	uint8_t seq_nr;/**<Sequence number.*/
};

/**
 * @brief Keep alive packets are used to decide wether or not
 * a node is still considered alive.\n
 * If node_id is 0, the receiver node answers with a unicast
 * message  to the sender advertising the age of its LSDB.
 */
static struct keep_alive_packet{
	bool get_lsdb_req;/**<If set to true it means we make a request to get someone LSDB.*/
	uint8_t neighbours[TOTAL_NODES];/**<List of nodes we got a keep alive packet.*/
	uint16_t battery_value;/**<My battery value, used as link cost.*/
};

/**@brief Link state database. Keeps track of links that the current has to know
 * and the respective weights, as well as other information needed for operation.*/
static struct link_state_database{
	uint16_t node_links_cost[TOTAL_NODES][TOTAL_NODES];/**<NODE/SRC DEST COST*/
	uint8_t sequence_numbers[TOTAL_NODES];/**<List of sequence numbers per node.*/
	uint16_t age;/**< With every update of the LSDB, age increases.*/
	uint8_t ka_received[TOTAL_NODES];/**<Number of Keep Alive Packets received from neighbour X in DOWN_PERIOD.*/
	uint8_t neighbours[TOTAL_NODES];/**<List of neighbours i know to be alive.*/
};

/**@brief Unicast packet used for transmitting the sensor data.
 **/
static struct unicast_packet{
	bool data_packet;/**<If true this packet contains sensor data.*/
	uint8_t data_type;/**<Depends on the value we have temperatue,moisture...*/
	uint16_t data;/**<Actual data from a sensor.*/
	uint8_t ttl;/**<Time To Live, to avoid infinite forwarding loops.*/
	uint16_t lsdb_age;/**<Age of my LSDB.*/
	bool send_lsdb;/**<If true send LSDB to sender.*/
	uint8_t path[TOTAL_NODES];/**<The path a packet took traversing our super network.*/
};

/**
 * @brief Sender history.
 * Detects duplicate callbacks at receiving nodes.
 * Duplicates appear when ACK messages are lost.
 * I noticed this actually happens quite often.
 * So we use this counter measure to not add duplicate
 * packets in the TX Buffer.
 * */
static struct history_entry{
	struct history_entry *next;
	linkaddr_t addr;
	uint8_t seq;
};

/**@brief Fill LSA packet for transimission.
 * @param tx_lsa_pkt Pointer to LSA packet.
 * @param link_cost Cost of advertising link.
 * @param src Source id of link we are advertising.
 * @param dst Destination id of link we are advertising.
 * @param seq_nr Sequence number of the node that generated the LSA.
 */
static void fill_tx_lsa_pkt(struct lsa *tx_lsa_pkt, uint16_t link_cost, uint8_t src, uint8_t dst, uint8_t seq_nr, bool reply_to_send_lsdb_req){
	printf("fill_tx_lsa_pkt() called!\n");
	tx_lsa_pkt->link_cost = link_cost;
	tx_lsa_pkt->endpoint_addresses[0] = src;
	tx_lsa_pkt->endpoint_addresses[1] = dst;
	tx_lsa_pkt->seq_nr = seq_nr;
	tx_lsa_pkt->reply_to_send_lsdb_req = reply_to_send_lsdb_req;
}

/**@brief Print my local LSDB.
 * @param lsdb Pointer to the local LSDB.*/
static void print_link_state_database(struct link_state_database *lsdb){
	uint8_t i; // Nodes
	uint8_t j; // Links
	printf("LSDB size: %d(bytes)\n", sizeof(lsdb->node_links_cost));
	for(i=0;i<TOTAL_NODES;i++){
		//printf("NODE: %d\n", i+1);
		for(j=0;j<TOTAL_NODES;j++){
			if(lsdb->node_links_cost[i][j] != 0){
				printf("%d->%d(%d) | ", i+1, j+1, lsdb->node_links_cost[i][j]);
				printf("\n");
			}
		}
		//printf("\n\r");
	}
}

/**@brief Prints the packet to which the pointer points.
 * @param tx_lsa_pkt Pointer to LSA packet.*/
static void print_tx_lsa_pkt_in_buf(struct lsa *tx_lsa_pkt){
	printf("print_tx_lsa_pkt_in_buf() called!");
	printf("\nPacket to send:\n");
	printf("Packet size: %d (bytes)\n", packetbuf_datalen());
	printf("Link cost: %d\n", tx_lsa_pkt->link_cost);
	printf("Link: %d->%d\n", tx_lsa_pkt->endpoint_addresses[0], tx_lsa_pkt->endpoint_addresses[1]);
	printf("Seq nr: %d\n", tx_lsa_pkt->seq_nr);
	printf("Reply to send LSDB req: %s\n", tx_lsa_pkt->reply_to_send_lsdb_req ? "true" : "false");
}

/**
 * @brief Prints the neighbour list and keep alives of every neighbour.
 * @param neighbours Pointer to uint8_t neighbour list.
 */
static void print_neighbour_list(uint8_t neighbours[TOTAL_NODES], uint8_t ka_received[TOTAL_NODES]){
	uint8_t i;
	printf("Neighbour (# Keep alives)\n");
	for(i=0;i<TOTAL_NODES;i++){
		if(neighbours[i] != 0){
			printf("%d (%d) | ", neighbours[i], ka_received[i]);
		}
	}
	printf("\n");
}
