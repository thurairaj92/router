
#ifndef SR_NAT_TABLE_H
#define SR_NAT_TABLE_H

#include <inttypes.h>
#include <time.h>
#include <pthread.h>

#include <netinet/in.h>
#include <sys/time.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "string.h"

#include "sr_protocol.h"
#include "sr_arpcache.h"

#define DROP_PACKET 1
#define PACKET_FINE 0


typedef enum {
  nat_mapping_icmp,
  nat_mapping_tcp
  /* nat_mapping_udp, */
} sr_nat_mapping_type;


typedef enum {

  inbound_packet,
  outbound_packet,
  confused_packet
} packet_direction;


typedef enum {

  nat_if,
  internal_if,
  external_if,
  unknown_if
} interface_map;




struct sr_nat_connection {
  	/* add TCP connection state data members here */	
	int server_syn;
	int client_syn;

	int server_fin;
	int client_fin;

	long fin_ack_seq;
	long fin_last_ack;

	int server_ip;
	int server_port;

	struct sr_nat_connection *prev;
  	struct sr_nat_connection *next;
};

struct sr_nat_mapping {
  sr_nat_mapping_type type;
  uint32_t ip_int; /* internal ip addr */
  uint32_t ip_ext; /* external ip addr */
  uint16_t aux_int; /* internal port or icmp id */
  uint16_t aux_ext; /* external port or icmp id */
  time_t last_updated; /* use to timeout mappings */
  struct sr_nat_connection *conns; /* list of connections. null for ICMP */
  struct sr_nat_mapping *next;
  struct sr_nat_mapping *prev;
}	;


struct sr_unsolicited_tcp {
  time_t arrival_time;
  uint32_t src_ip;
  uint8_t ip_data[28];
  uint16_t port_val_ext;

  struct sr_unsolicited_tcp *next;
  struct sr_unsolicited_tcp *prev;
};

struct sr_nat {
  /* add any fields here */
  struct sr_nat_mapping *mappings;
  struct sr_unsolicited_tcp *unsolicited_tcp;
  uint16_t available_port;

  struct sr_if* ext_if;
  struct sr_if* int_if;
  /* threading */
  pthread_mutex_t lock;
  pthread_mutexattr_t attr;
  pthread_attr_t thread_attr;
  pthread_t thread;
};


int   sr_nat_init(struct sr_nat *nat);     /* Initializes the nat */
int   sr_nat_destroy(struct sr_nat *nat);  /* Destroys the nat (free memory) */
void *sr_nat_timeout(void *nat_ptr);  /* Periodic Timout */

/* Get the mapping associated with given external port.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type );

/* Get the mapping associated with given internal (ip, port) pair.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );

/* Insert a new mapping into the nat's mapping table.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat,
uint32_t ip_int,uint32_t ip_ext, uint16_t aux_int, sr_nat_mapping_type type );
int transform_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len);


struct sr_instance
{
    int  sockfd;   /* socket to server */
    char user[32]; /* user name */
    char host[32]; /* host name */ 
    char template[30]; /* template name if any */
    unsigned short topo_id;
    struct sockaddr_in sr_addr; /* address to server */
    struct sr_if* if_list; /* list of interfaces */
    struct sr_rt* routing_table; /* routing table */
    struct sr_arpcache cache;   /* ARP cache */
    pthread_attr_t attr;
    struct sr_nat nat;
    int nat_active;
    int icmp_timeout;
    int tcp_default_timeout;
    int tcp_transition_timeout;
    FILE* logfile;
};



#endif