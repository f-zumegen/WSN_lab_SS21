/** \file project-conf.h
 * Contains project wide variables
 *
 * Created on: May 28, 2021\n
 * Author: Nikolaos Mitsakis
 */

#include <stdint.h>

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/**MAC Layer. Options: nullmac_driver - csma_driver.*/
#define NETSTACK_CONF_MAC csma_driver

/**RDC Layer. Options: nullrdc_driver - contikimac_driver. The sink should be nullrdc_driver*/
#define NETSTACK_CONF_RDC contikimac_driver
//#define NETSTACK_CONF_RDC nullrdc_driver ///@warning IF YOU USE NULLRDC THEN MOTES FLASHED WITH CONTIKIMAC DON'T RECEIVE PACKETS (at least broadcasts)

/**The checkrate in Hz. Should be 2^x.
 * Sensor motes should have a check rate of 8.
 * Bridge motes should have a check rate of 16?.
 * The sink should have a check are of 256?.*/
///@warning DIFFERENT CHANNEL CHECK RATES ON MOTES RESULT IN LOSS OF PACKETS/PACKETS NOT ARRIVING.
//#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 8
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 16
//#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 32
//#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 128


/**
 * A global variable defining the period
 * to transmit a keep alivem message.
 * In seconds.
 */
#define KEEP_ALIVE_PERIOD 100*CLOCK_SECOND

/**
 * A global variable defining the period
 * after which a link is considered to be down
 * if no HELLO_PACKET received in DOWN_PERIOD
 * In seconds.
 */
#define DOWN_PERIOD 200*CLOCK_SECOND

/**
 * Define the frequency with which we sense data from the sensors.
 */
#define SENSOR_READ_INTERVAL 105*CLOCK_SECOND

/**
 * This defines the total number of nodes.\n
 * It is used to calculate important variables.
 * @warning Exceeding this limit leads to problems.
 */
#define TOTAL_NODES 13

/**
 * Node id of the sink.
 */
#define SINK_ID 1

/**
 * Pre backoff timer when the network first goes live\n.
 * @warning Needs to be less than the KEEP_ALIVE_PERIOD
 */
#define INIT_PRE_BACKOFF_PERIOD (10 + random_rand()%(TOTAL_NODES*2))*CLOCK_SECOND

/**
 * When this timer expires we initiate the message sequence to get the LSDB from
 * a neighbour, if one available.
 * @warning Needs to be less than the KEEP_ALIVE_PERIOD.
 * @warning Higher than the PRE_BACKOFF_PERIOD
 */
#define GET_LSDB_PERIOD (TOTAL_NODES*2+5)*CLOCK_SECOND

/**
 * Time To Live. Max number of nodes a data packet can traverse before being discarded.
 * Used to avoid infinite forwarding loops.
 * @warning Max 255 (It is a 1 byte variable).
 */
#define TTL 5

/**
 * Group Channel
 */
#define CHANNEL 14

/**
 * Transmission power
 */
#define TX_POWER 1

/**
 * In order to make it a multi-hop network in the small exam room we have to ignore
 * packet establishing links below a certain rssi.
 * Unfortunately this introduces instabilities and false positives/negatives.
 */
#define IGNORE_RSSI_BELOW -70

/**
 * Color LEDS_RED for incoming packets (broadcast/unicast/runicast).
 */
#define RX_PKT_COLOR LEDS_RED

/**
* Color LEDS_GREEN for outgoing packets (broadcast/unicast/runicast).
*/
#define TX_PKT_COLOR LEDS_GREEN

/**
 * The Rime channel used for broadcasts.
 */
#define BROADCAST_RIME_CHANNEL 129

/**
 * The Rime channel used for unicasts.
 */
#define UNICAST_RIME_CHANNEL 146

/**
 * Maximum retransmissions for runicast retransmissions.
 */
#define RUNICAST_MAX_RETRANSMISSIONS 2


/**
 * Maximum history entries for runicast receptions.
 * Used to identify duplicate packages.
 */
#define RUNICAST_RX_HISTORY_ENTRIES 2

/**
 * Used to implement lollipop-shaped sequence number spaces.
 * Hybrid of linear and circular sequence number spaces.
 * That is the value from which we start our sequnce
 * number space when first going live, or a node is reset/down.
 * When a sequence number hits 255 it circulated to 0 with a modulo oeration.
 * Then when we generate a new packet and attach the sequence number 1, the receiver
 * checks that 1 <= RESET_SQN_NO, recognizing the shift, taking over value and updating the old sequence number.
 */
#define RESET_SQN_NO 10


#define RESET   "\033[0m"
#define RED     "\033[31m"      /* Red */

#endif /* PROJECT_CONF_H_ */
