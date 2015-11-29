/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"
#include "sr_nat.h"


#define IP_ADDR_LEN 4
#define INITIAL_TTL 64
#define BYTE_CONVERSION 4
#define EMPTY_MAC "\xff\xff\xff\xff\xff\xff"


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

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
	/* REQUIRES */
	assert(sr);

	/* Initialize cache and cache cleanup thread */
	sr_arpcache_init(&(sr->cache));
	if(sr.nat_active){
		sr_nat_init(&(sr->nat));
		sr->nat.sr = sr;
	}

	pthread_attr_init(&(sr->attr));
	pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
	pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
	pthread_t thread;

	pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

	/* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
                     uint8_t * packet/* lent */,
                     unsigned int len,
                     char* interface/* lent */)
{
	/* REQUIRES */
	assert(sr);
	assert(packet);
	assert(interface);


	/* fill in code here */
	struct sr_if* ethernet_interface = sr_get_interface(sr, interface);

	/* Fetch Interface from instance of sr for further processing. */
	if (!ethernet_interface) {
		printf("ERROR : Received Invalid Packet Ethernet Interface.");
	}

	sr_ethernet_hdr_t *ethernet_header = (sr_ethernet_hdr_t *) packet;

	/* convert network byte order to host byte order. */
	uint16_t converted_ether_type_val = ntohs(ethernet_header->ether_type);

	/*ARP Packet*/
	if (converted_ether_type_val == ethertype_arp) {
		printf("--- ARP Packet\n");
		sr_arp_hdr_t *arp_header =  (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

		unsigned short converted_arp_op_val = ntohs(arp_header->ar_op);
		switch (converted_arp_op_val) {
		case arp_op_request:
			printf("--- --- ARP Request.\n");

			/*Find out if target IP addr is one of our router's addresses. */
			struct sr_if* target_ip_interface = sr_get_ip_interface(sr, arp_header->ar_tip);
			
			if (target_ip_interface != 0) {
			

				/*Create Packet with arp and ethernet header. */
				unsigned int reply_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
				uint8_t *reply_packet = malloc(reply_len);
				create_arp_header(reply_packet, target_ip_interface->addr, arp_header->ar_sha, target_ip_interface->ip, arp_header->ar_sip, arp_op_reply);
				create_ethernet_header(reply_packet, ethernet_header->ether_shost,  target_ip_interface->addr,  ethertype_arp);

				if ((sr_send_packet(sr, reply_packet, reply_len, target_ip_interface->name)) == 0) {
				
				} else {
				
				}

				free(reply_packet);

			} else {
			
				printf("******** DROP  PACKET *******\n");
			}
			break;

		case arp_op_reply:{


			struct sr_arpreq *request =  sr_arpcache_insert(&sr->cache,
			                             arp_header->ar_sha,
			                             arp_header->ar_sip);

			if (request != NULL) {
				struct sr_packet *packet_head = request->packets;
			
				while (packet_head != NULL) {
					sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *) (packet_head->buf + sizeof(sr_ethernet_hdr_t));

					/*ip_header->ip_ttl--;*/
					int new_len = sizeof(sr_ethernet_hdr_t) + ntohs(ip_header->ip_len);
					uint8_t *packet_new = malloc(new_len);

					ip_header->ip_sum = 0;
					ip_header->ip_sum = cksum((void *) ip_header, ip_header->ip_hl * BYTE_CONVERSION);

					memcpy(packet_new + sizeof(sr_ethernet_hdr_t), ip_header, ntohs(ip_header->ip_len));
					create_ethernet_header(packet_new, ethernet_header->ether_shost, ethernet_header->ether_dhost, ethertype_ip);

					if ((sr_send_packet(sr, packet_new, new_len, interface)) == 0) {
					
					} else {
					
					}

					free(packet_new);
					if (packet_head->next) {
						packet_head = packet_head->next;
					} else {
						packet_head = NULL;
					}

				}
			
				sr_arpreq_destroy(&sr->cache, request);
			}
			break;
		}
		default:
			printf("No match of arp op code.\n");
		}
	}/*END OF ARP SECTION */
	/*IP Packet*/
	else if (converted_ether_type_val == ethertype_ip){

		struct sr_rt* gateway;
		struct sr_arpentry *arp_entry;
		uint8_t *reply_packet;
		unsigned int packet_len;

		/* Get IP header */
		sr_ip_hdr_t *ip_header =  (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
		struct sr_if* target_interface = sr_get_interface(sr, interface);

		/*checksum*/
		uint16_t packet_cksum, calculated_cksum;
		
		packet_cksum = ip_header->ip_sum;
		ip_header->ip_sum = 0;
	
		calculated_cksum =  cksum((void *) ip_header, ip_header->ip_hl * BYTE_CONVERSION);
		ip_header->ip_sum = packet_cksum;

		if (ip_header->ip_hl < 5 || packet_cksum != calculated_cksum || ip_header->ip_v != BYTE_CONVERSION) {
			return;
		}

		/*TTL Check*/
		if (ip_header->ip_ttl == 0) {
			/*get destination address*/
			gateway = sr_get_routing_entry(sr, ip_header->ip_src, ethernet_interface);
			/*Gateway can be empty*/
			/*Check gateway*/
			if(gateway == NULL){
				return;
			}
			
			arp_entry = sr_arpcache_lookup(&sr->cache, gateway->gw.s_addr);
			
			unsigned int ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t11_hdr_t);
			packet_len = sizeof(sr_ethernet_hdr_t) + ip_len;

			reply_packet = malloc(packet_len);

			/*Create ICMP packet*/
			create_icmp_11_header(reply_packet, ip_header);
			create_ip_header(reply_packet, ip_len, 100, ip_protocol_icmp, target_interface->ip, ip_header->ip_src);

			if(arp_entry != NULL){
				create_ethernet_header(reply_packet, arp_entry->mac, ethernet_header->ether_dhost,  ethertype_ip);
				sr_send_packet(sr, reply_packet, packet_len, interface);
			}else{
				create_ethernet_header(reply_packet, (uint8_t *) (EMPTY_MAC),  ethernet_header->ether_dhost,  ethertype_ip);
				struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, gateway->gw.s_addr, reply_packet, packet_len, interface);
				handle_arpreq(sr, created_req);
			}

			return;
		}


		if (sr_get_ip_interface(sr, ip_header->ip_dst) != 0){
			/*ICMP*/

			if (ip_header->ip_p == 1) {
				sr_icmp_hdr_t *icmp_header = (sr_icmp_hdr_t *)(((uint8_t *)ip_header) + ip_header->ip_hl * BYTE_CONVERSION);
				
				uint16_t icmp_packet_cksum, icmp_calculate_cksum;
				uint16_t icmp_len = ntohs(ip_header->ip_len) - ip_header->ip_hl * BYTE_CONVERSION;

				icmp_packet_cksum = icmp_header->icmp_sum;
				icmp_header->icmp_sum = 0;
				icmp_calculate_cksum =  cksum((void *) icmp_header, icmp_len);

				if(icmp_packet_cksum != icmp_calculate_cksum){
					return;
				}

				packet_len = sizeof(sr_ethernet_hdr_t) + ntohs(ip_header->ip_len);
				reply_packet = malloc(packet_len);
				
				gateway = sr_get_routing_entry(sr, ip_header->ip_src, ethernet_interface);
				if (gateway == NULL){
					return;
				}

				sr_ip_hdr_t *reply_ip_header =  (sr_ip_hdr_t *) (reply_packet + sizeof(sr_ethernet_hdr_t));
				sr_icmp_hdr_t *reply_icmp_header =  (sr_icmp_hdr_t *)(reply_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

				memcpy((void *)reply_ip_header, (void *)ip_header, ntohs(ip_header->ip_len));
				reply_ip_header->ip_dst = ip_header->ip_src;
				reply_ip_header->ip_src = ip_header->ip_dst;

				reply_ip_header->ip_sum = 0;
				reply_ip_header->ip_sum = cksum((void *) reply_ip_header, reply_ip_header->ip_hl * BYTE_CONVERSION);

				reply_icmp_header->icmp_type = 0;
				reply_icmp_header->icmp_code = 0;
				reply_icmp_header->icmp_sum = 0;
				reply_icmp_header->icmp_sum = cksum((void *) reply_icmp_header, icmp_len);

				arp_entry = sr_arpcache_lookup(&sr->cache, gateway->gw.s_addr);
				if(arp_entry != NULL){
					create_ethernet_header(reply_packet, arp_entry->mac, ethernet_header->ether_dhost,  ethertype_ip);
					sr_send_packet(sr, reply_packet, packet_len, interface);
				}else{
					create_ethernet_header(reply_packet, (uint8_t *) (EMPTY_MAC),  ethernet_header->ether_dhost,  ethertype_ip);
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, gateway->gw.s_addr, reply_packet, packet_len, interface);
					handle_arpreq(sr, created_req);
				}

			}else{
				
				gateway = sr_get_routing_entry(sr, ip_header->ip_src, ethernet_interface);
				if(gateway == NULL){
					return;
				}
				
				arp_entry = sr_arpcache_lookup(&sr->cache, gateway->gw.s_addr);

				uint16_t  ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
				packet_len = sizeof(sr_ethernet_hdr_t) + ip_len;

				reply_packet = malloc(packet_len);
				create_icmp_port_header(reply_packet, ip_header);
				create_ip_header(reply_packet, ip_len, 100, ip_protocol_icmp, ip_header->ip_dst , ip_header->ip_src);

				if(arp_entry != NULL){
					create_ethernet_header(reply_packet, arp_entry->mac, ethernet_header->ether_dhost,  ethertype_ip);
					sr_send_packet(sr, reply_packet, packet_len, interface);
				}else{
					create_ethernet_header(reply_packet, (uint8_t *) (EMPTY_MAC),  ethernet_header->ether_dhost,  ethertype_ip);
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, gateway->gw.s_addr, reply_packet, packet_len, interface);
					handle_arpreq(sr, created_req);
				}

				return;
			}
		}else{
			printf("Should be here\n");
			ip_header->ip_ttl--;

			if (ip_header->ip_ttl < 1) {
				/*get destination address*/
				gateway = sr_get_routing_entry(sr, ip_header->ip_src, ethernet_interface);
				/*Gateway can be empty*/
				/*Check gateway*/
				if(gateway == NULL){
					return;
				}
				
				arp_entry = sr_arpcache_lookup(&sr->cache, gateway->gw.s_addr);
				
				unsigned int ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t11_hdr_t);
				packet_len = sizeof(sr_ethernet_hdr_t) + ip_len;

				reply_packet = malloc(packet_len);

				/*Create ICMP packet*/
				create_icmp_11_header(reply_packet, ip_header);
				create_ip_header(reply_packet, ip_len, 100, ip_protocol_icmp, target_interface->ip, ip_header->ip_src);

				if(arp_entry != NULL){
					create_ethernet_header(reply_packet, arp_entry->mac, ethernet_header->ether_dhost,  ethertype_ip);
					sr_send_packet(sr, reply_packet, packet_len, interface);
				}else{
					create_ethernet_header(reply_packet, (uint8_t *) (EMPTY_MAC),  ethernet_header->ether_dhost,  ethertype_ip);
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, gateway->gw.s_addr, reply_packet, packet_len, interface);
					handle_arpreq(sr, created_req);
				}

				return;
			}

			struct sr_rt* target_machine_ip = sr_get_routing_entry(sr, ip_header->ip_dst, ethernet_interface);

			if (target_machine_ip == 0) {
				gateway = sr_get_routing_entry(sr, ip_header->ip_src, ethernet_interface);
				if(gateway == NULL){
					return;
				}
				arp_entry = sr_arpcache_lookup(&sr->cache, gateway->gw.s_addr);

				uint16_t  ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
				packet_len = sizeof(sr_ethernet_hdr_t) + ip_len;

				reply_packet = malloc(packet_len);

				create_icmp_net_header(reply_packet, ip_header);
				create_ip_header(reply_packet, ip_len, 100, ip_protocol_icmp, target_interface->ip , ip_header->ip_src);

				if(arp_entry != NULL){
					create_ethernet_header(reply_packet, arp_entry->mac, ethernet_header->ether_dhost,  ethertype_ip);
					sr_send_packet(sr, reply_packet, packet_len, interface);
				}else{
					create_ethernet_header(reply_packet, (uint8_t *) (EMPTY_MAC),  ethernet_header->ether_dhost,  ethertype_ip);
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, gateway->gw.s_addr, reply_packet, packet_len, interface);
					handle_arpreq(sr, created_req);
				}
				return;

			}else{
				struct sr_if* next_hop_interface = sr_get_interface(sr, target_machine_ip->interface);
				arp_entry = sr_arpcache_lookup(&sr->cache, target_machine_ip->gw.s_addr);
				/*struct sr_rt* gateway = sr_get_routing_entry(sr, ip_header->ip_src, ethernet_interface);*/

				int new_len = sizeof(sr_ethernet_hdr_t) + ntohs(ip_header->ip_len);
				uint8_t *packet_new = malloc(new_len);

				ip_header->ip_sum = 0;
				ip_header->ip_sum = cksum((void *) ip_header, ip_header->ip_hl * BYTE_CONVERSION);

				memcpy(packet_new + sizeof(sr_ethernet_hdr_t), ip_header, ntohs(ip_header->ip_len));

				if (arp_entry != NULL) {
					create_ethernet_header(packet_new, arp_entry->mac, next_hop_interface->addr, ethertype_ip);
					sr_send_packet(sr, packet_new, new_len, next_hop_interface->name);
				} else {
					create_ethernet_header(packet_new, (uint8_t *) (EMPTY_MAC),  ethernet_header->ether_dhost,  ethertype_ip);
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, target_machine_ip->gw.s_addr,
					                                packet_new, len, interface);
					handle_arpreq(sr, created_req);
					
				}
				return;

			}
		}
	}
}








void create_arp_header(uint8_t *packet, uint8_t *in_sha, uint8_t *out_sha, uint32_t in_ip, uint32_t out_ip, uint16_t op_code) {
	sr_arp_hdr_t *reply_arp_header = (sr_arp_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));
	memcpy(reply_arp_header->ar_sha, in_sha, ETHER_ADDR_LEN);
	reply_arp_header->ar_sip = in_ip;
	memcpy(reply_arp_header->ar_tha, out_sha, ETHER_ADDR_LEN);
	reply_arp_header->ar_tip = out_ip;
	reply_arp_header->ar_hrd = htons(arp_hrd_ethernet);
	reply_arp_header->ar_pro = htons(ethertype_ip);
	reply_arp_header->ar_hln = ETHER_ADDR_LEN;
	reply_arp_header->ar_pln = IP_ADDR_LEN;
	reply_arp_header->ar_op = htons(op_code);

}

void create_ethernet_header(uint8_t *packet, uint8_t *out_host, uint8_t *in_host, uint16_t eth_type) {
	sr_ethernet_hdr_t *reply_ethernet_header = (sr_ethernet_hdr_t *) packet;

	reply_ethernet_header->ether_type = htons(eth_type);
	memcpy(reply_ethernet_header->ether_dhost, out_host, sizeof(uint8_t) * ETHER_ADDR_LEN);
	memcpy(reply_ethernet_header->ether_shost, in_host, sizeof(uint8_t) * ETHER_ADDR_LEN);
}


void create_ip_header(uint8_t *packet, uint16_t ip_len, uint8_t ip_ttl, uint8_t ip_p, uint32_t ip_src, uint32_t ip_dst) {
	sr_ip_hdr_t *reply_ip_header = (sr_ip_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));
	reply_ip_header->ip_hl = sizeof(sr_ip_hdr_t) / BYTE_CONVERSION;
	reply_ip_header->ip_v = 4;
	reply_ip_header->ip_tos = 0;
	reply_ip_header->ip_len = htons(ip_len);

	reply_ip_header->ip_id  = htons(0); /* TODO */
	reply_ip_header->ip_off = htons(0);

	reply_ip_header->ip_ttl = ip_ttl;
	reply_ip_header->ip_p = ip_p;
	reply_ip_header->ip_src = ip_src;
	reply_ip_header->ip_dst = ip_dst;
	reply_ip_header->ip_sum = 0;

	/*TODO flags*/
	reply_ip_header->ip_sum = cksum((void *) reply_ip_header, reply_ip_header->ip_hl * 4);
}


void create_icmp_11_header(uint8_t *packet, sr_ip_hdr_t *old_ip) {
	sr_icmp_t11_hdr_t *reply_icmp_header = (sr_icmp_t11_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
	memcpy((void *)reply_icmp_header->data, (void *)(old_ip), sizeof(sr_ip_hdr_t) + 8);


	reply_icmp_header->icmp_type = 11;
	reply_icmp_header->icmp_code = 0;
	reply_icmp_header->icmp_sum = 0;
	reply_icmp_header->unused = 0;
	
	reply_icmp_header->icmp_sum = cksum((void *) reply_icmp_header, sizeof(sr_icmp_t11_hdr_t));
}

void create_icmp_port_header(uint8_t *packet, sr_ip_hdr_t *old_ip) {
	
	sr_icmp_t3_hdr_t *reply_icmp_header = (sr_icmp_t3_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
	memcpy((void *)reply_icmp_header->data, (void *)(old_ip), sizeof(sr_ip_hdr_t) + 8);

	reply_icmp_header->icmp_type = 3;
	reply_icmp_header->icmp_code = 3;
	reply_icmp_header->icmp_sum = 0;
	reply_icmp_header->unused = 0;
	reply_icmp_header->next_mtu = 1000; 
	reply_icmp_header->icmp_sum = cksum((void *) reply_icmp_header, sizeof(sr_icmp_t3_hdr_t));
}

void create_icmp_net_header(uint8_t *packet, sr_ip_hdr_t *old_ip) {
	
	sr_icmp_t3_hdr_t *reply_icmp_header = (sr_icmp_t3_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
	memcpy((void *)reply_icmp_header->data, (void *)(old_ip), sizeof(sr_ip_hdr_t) + 8);

	reply_icmp_header->icmp_type = 3;
	reply_icmp_header->icmp_code = 0;
	reply_icmp_header->icmp_sum = 0;
	reply_icmp_header->unused = 0;
	reply_icmp_header->next_mtu = 1000; 
	reply_icmp_header->icmp_sum = cksum((void *) reply_icmp_header, sizeof(sr_icmp_t3_hdr_t));
}


void create_icmp_host_header(uint8_t *packet, sr_ip_hdr_t *old_ip) {
	
	sr_icmp_t3_hdr_t *reply_icmp_header = (sr_icmp_t3_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
	memcpy((void *)reply_icmp_header->data, (void *)(old_ip), sizeof(sr_ip_hdr_t) + 8);

	reply_icmp_header->icmp_type = 3;
	reply_icmp_header->icmp_code = 1;
	reply_icmp_header->icmp_sum = 0;
	reply_icmp_header->unused = 0;
	reply_icmp_header->next_mtu = 1000; 
	reply_icmp_header->icmp_sum = cksum((void *) reply_icmp_header, sizeof(sr_icmp_t3_hdr_t));
}

void send_arp_request(struct sr_instance *sr, struct sr_rt *hop_entry, struct sr_if *out_interface) {

	int length = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);

	uint8_t* packet = malloc(length);

	create_arp_header(packet, out_interface->addr, (uint8_t *) (EMPTY_MAC) , out_interface->ip, hop_entry->gw.s_addr, arp_op_request);
	create_ethernet_header(packet, (uint8_t *) (EMPTY_MAC), out_interface->addr,  ethertype_arp);

	if ((sr_send_packet(sr, packet, length, out_interface->name)) == 0) {
		
	} else {
		
	}

	free(packet);

}

void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req) {
	time_t now;
	time(&now);
	assert(req);

	struct sr_if* target_interface = NULL;
	sr_ip_hdr_t *ip_header = NULL;
	unsigned int packet_len = 0;

	if (difftime(now, req->sent) > 1.0) {
		/* I'm tired of waiting for your address, screw you, delete all yours! */
		if (req->times_sent >= 5) {
		
			struct sr_packet *packet_head = req->packets;
		
			while (packet_head != NULL) {
				ip_header =  (sr_ip_hdr_t *)(packet_head->buf + sizeof(sr_ethernet_hdr_t));

				target_interface = sr_get_interface(sr, packet_head->iface);
				struct sr_rt* target_machine_ip = sr_get_routing_entry(sr, ip_header->ip_src, target_interface);


				
				uint16_t  ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
				packet_len = sizeof(sr_ethernet_hdr_t) + ip_len;

				uint8_t *reply_packet = malloc(packet_len);

				create_icmp_host_header(reply_packet, ip_header);
				create_ip_header(reply_packet, ip_len, 100, ip_protocol_icmp, target_interface->ip, ip_header->ip_src);

				struct sr_if* next_hop_interface = sr_get_interface(sr, target_machine_ip->interface);
				struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, target_machine_ip->gw.s_addr);


				if (arp_entry != NULL){
					create_ethernet_header(reply_packet, arp_entry->mac, next_hop_interface->addr,  ethertype_ip);
					if ((sr_send_packet(sr, reply_packet, packet_len, packet_head->iface)) == 0) {
					
					} else {
						printf("Failed to send ICMP Unreachable to host in handle_arpreq.\n");
					}
				}
				else {
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, target_machine_ip->gw.s_addr, reply_packet, packet_len, packet_head->iface);
					handle_arpreq(sr, created_req);
				}

				if (packet_head->next) {
					packet_head = packet_head->next;
				} else {
					packet_head = NULL;
				}
			}
			sr_arpreq_destroy(&sr->cache, req);
		
		} else {
		

			ip_header =  (sr_ip_hdr_t *)(req->packets->buf + sizeof(sr_ethernet_hdr_t));
      		struct sr_rt* target_machine_ip = sr_get_routing_entry(sr, ip_header->ip_dst, NULL);
			struct sr_if* next_hop_interface = sr_get_interface(sr, target_machine_ip->interface);

			send_arp_request(sr, target_machine_ip, next_hop_interface);
			req->sent = now;
			req->times_sent++;
		}

	}
}


