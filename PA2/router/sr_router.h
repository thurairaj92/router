/*-----------------------------------------------------------------------------
 * File: sr_router.h
 * Date: ?
 * Authors: Guido Apenzeller, Martin Casado, Virkam V.
 * Contact: casado@stanford.edu
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_ROUTER_H
#define SR_ROUTER_H

#include "sr_nat.h"

/* we dont like this debug , but what to do for varargs ? */
#ifdef _DEBUG_
#define Debug(x, args...) printf(x, ## args)
#define DebugMAC(x) \
  do { int ivyl; for(ivyl=0; ivyl<5; ivyl++) printf("%02x:", \
  (unsigned char)(x[ivyl])); printf("%02x",(unsigned char)(x[5])); } while (0)
#else
#define Debug(x, args...) do{}while(0)
#define DebugMAC(x) do{}while(0)
#endif

#define INIT_TTL 255
#define PACKET_DUMP_SIZE 1024

#define IP_ADDR_LEN 4
#define INITIAL_TTL 64
#define EMPTY_MAC "\xff\xff\xff\xff\xff\xff"

/* forward declare */
struct sr_if;
struct sr_rt;

/* ----------------------------------------------------------------------------
 * struct sr_instance
 *
 * Encapsulation of the state for a single virtual router.
 *
 * -------------------------------------------------------------------------- */


/* -- sr_main.c -- */
int sr_verify_routing_table(struct sr_instance* sr);

/* -- sr_vns_comm.c -- */
int sr_send_packet(struct sr_instance* , uint8_t* , unsigned int , const char*);
int sr_connect_to_server(struct sr_instance* ,unsigned short , char* );
int sr_read_from_server(struct sr_instance* );
void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req);

/* -- sr_router.c -- */
void sr_init(struct sr_instance* );
void sr_handlepacket(struct sr_instance* , uint8_t * , unsigned int , char* );

/* -- sr_if.c -- */
void sr_add_interface(struct sr_instance* , const char* );
void sr_set_ether_ip(struct sr_instance* , uint32_t );
void sr_set_ether_addr(struct sr_instance* , const unsigned char* );
void sr_print_if_list(struct sr_instance* );


void create_arp_header(uint8_t *packet, uint8_t *in_sha, uint8_t *out_sha, uint32_t in_ip, uint32_t out_ip, uint16_t op_code);
void create_ethernet_header(uint8_t *packet, uint8_t *out_host, uint8_t *in_host, uint16_t eth_type);
void create_ip_header(uint8_t *packet, uint16_t ip_len, uint8_t ip_ttl, uint8_t ip_p, uint32_t ip_src, uint32_t ip_dst);
uint8_t *create_icmp_header(uint8_t icmp_type, uint8_t icmp_code, sr_ethernet_hdr_t *ethernet_header, sr_ip_hdr_t *ip_header, struct sr_if* target_interface, 
	struct sr_arpentry *arp_entry);
void send_arp_request(struct sr_instance *sr, struct sr_rt *hop_entry, struct sr_if *out_interface);
void create_icmp_11_header(uint8_t *packet, sr_ip_hdr_t *old_ip);
void create_icmp_port_header(uint8_t *packet, sr_ip_hdr_t *old_ip);
void create_icmp_net_header(uint8_t *packet, sr_ip_hdr_t *old_ip);
void create_icmp_host_header(uint8_t *packet, sr_ip_hdr_t *old_ip);

#endif /* SR_ROUTER_H */
