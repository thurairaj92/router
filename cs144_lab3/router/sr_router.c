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


#define IP_ADDR_LEN 4
#define INITIAL_TTL 64


void create_arp_header(uint8_t *packet, uint8_t *in_sha, uint8_t *out_sha, uint32_t in_ip, uint32_t out_ip, uint16_t op_code);
void create_ethernet_header(uint8_t *packet, uint8_t *out_host, uint8_t *in_host, uint16_t eth_type);
void create_ip_header(uint8_t *packet, uint16_t ip_len, uint8_t ip_ttl, uint8_t ip_p, uint32_t ip_src, uint32_t ip_dst);
uint8_t *create_icmp_header(uint8_t icmp_type, uint8_t icmp_code, sr_ethernet_hdr_t *ethernet_header, sr_ip_hdr_t *ip_header, struct sr_if* target_interface, 
	struct sr_arpentry *arp_entry);
void send_arp_request(struct sr_instance *sr, struct sr_rt *hop_entry, struct sr_if *out_interface);

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

	printf("\n******** NEW  PACKET ********\n");

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
				printf("--- --- - Note : Target IP in the Router. \n");

				/*Create Packet with arp and ethernet header. */
				unsigned int reply_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
				uint8_t *reply_packet = malloc(reply_len);
				create_arp_header(reply_packet, target_ip_interface->addr, arp_header->ar_sha, target_ip_interface->ip, arp_header->ar_sip, arp_op_reply);
				create_ethernet_header(reply_packet, ethernet_header->ether_shost,  target_ip_interface->addr,  ethertype_arp);

				if ((sr_send_packet(sr, reply_packet, reply_len, target_ip_interface->name)) == 0) {
					printf("--- --- - Report : ARP Reply Sent. \n");
				} else {
					printf("--- --- - Report : ARP Reply Failed To Send. \n");
				}

				printf("******* END of Process ******\n");
				free(reply_packet);

			} else {
				printf("--- --- - Note : Target IP not for Router \n");
				printf("******** DROP  PACKET *******\n");
			}
			break;

		case arp_op_reply:
			printf("--- --- ARP Reply.\n");

			struct sr_arpreq *request =  sr_arpcache_insert(&sr->cache,
			                             arp_header->ar_sha,
			                             arp_header->ar_sip);

			if (request != NULL) {
				struct sr_packet *packet_head = request->packets;
				printf("--- --- - Note: Begin sending IP packets in queue.\n");
				while (packet_head != NULL) {
					sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *) (packet_head->buf + sizeof(sr_ethernet_hdr_t));
					print_hdrs(packet_head->buf, packet_head->len);

					ip_header->ip_ttl--;
					int new_len = sizeof(sr_ethernet_hdr_t) + ntohs(ip_header->ip_len);
					uint8_t *packet_new = malloc(new_len);

					ip_header->ip_sum = 0;
					ip_header->ip_sum = cksum((void *) ip_header, ip_header->ip_hl * 4);

					memcpy(packet_new + sizeof(sr_ethernet_hdr_t), ip_header, ntohs(ip_header->ip_len));
					create_ethernet_header(packet_new, ethernet_header->ether_shost, ethernet_header->ether_dhost, ethertype_ip);

					if ((sr_send_packet(sr, packet_new, new_len, interface)) == 0) {
						printf("--- --- - Report : IP Packet Sent based on ARP Reply\n");
					} else {
						printf("--- --- - Report : IP Packet Failed to send based on ARP Reply\n");
					}

					free(packet_new);
					if (packet_head->next) {
						packet_head = packet_head->next;
					} else {
						packet_head = NULL;
					}

				}
				printf("--- --- - Note: End sending IP packets in queue.\n");
				sr_arpreq_destroy(&sr->cache, request);
			}
			printf("******* END of Process ******\n");
			break;

		default:
			printf("No match of arp op code.\n");
		}
	}/*END OF ARP SECTION */
	/*IP Packet*/
	else if (converted_ether_type_val == ethertype_ip) {
		printf("--- IP Packet\n");

		/*Get IP header*/
		sr_ip_hdr_t *ip_header =  (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
		struct sr_if* target_ip_interface = sr_get_ip_interface(sr, ip_header->ip_dst);
		struct sr_if* target_interface = sr_get_interface(sr, interface);

		/*checksum*/
		uint16_t ip_cksum = ip_header->ip_sum;
		ip_header->ip_sum = 0;
		uint16_t in_cksum =  cksum((void *) ip_header, ip_header->ip_hl * 4);

		ip_header->ip_sum = in_cksum;

		/*Validate the header*/
		if (ip_header->ip_hl < 5 || in_cksum != ip_cksum || ip_header->ip_v != 4) {
			printf("--- --- - Error : IP Checksum\n");
			printf("******* END of Process ******\n");
			return;
		}

		/*Validate TTL*/
		if (ip_header->ip_ttl == 0) {
			printf("--- --- - Note : TTL = 0\n");
			struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, ip_header->ip_src);
			uint8_t *reply_icmp = create_icmp_header(11, 0, ethernet_header, ip_header, target_interface, arp_entry);
			unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t11_hdr_t);

			if(arp_entry != NULL)
			{
				if ((sr_send_packet(sr, reply_icmp, packet_len, interface)) == 0) {
					printf("--- --- - Report : ICMP Type 11 Sent\n");
				} else {
					printf("--- --- - Report : Failed to send ICMP Type 11\n");
				}

				free(reply_icmp);
			}
			else{
				printf("--- --- - Note : Couldn't find MAC in ARP_CACHE\n");
				printf("--- --- - Note : Try to create ARP Req for Type 11\n");
				struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_src,
		                                reply_icmp, packet_len, interface);
				handle_arpreq(sr, created_req);
			}
			printf("******* END of Process ******\n");
			return;
		}

		/*Packet for me, yay! */
		uint32_t dst_ip = ip_header->ip_dst;
		if (sr_get_ip_interface(sr, dst_ip) != 0) {
			printf("--- --- - Note : Packet for Router\n");
			/*ICMP packet*/
			if (ip_header->ip_p == 1) {

				/* ICMP checksum check */
				sr_icmp_hdr_t *icmp_header = (sr_icmp_hdr_t *)(((uint8_t *)ip_header) + ip_header->ip_hl * 4);
				uint16_t icmp_len = ntohs(ip_header->ip_len) - ip_header->ip_hl * 4;

				uint16_t icmp_cksum = icmp_header->icmp_sum;
				icmp_header->icmp_sum = 0;
				uint16_t in_icmp_cksum =  cksum((void *) icmp_header, icmp_len);

				if (icmp_cksum != in_icmp_cksum){
					return;
				}

				struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, ip_header->ip_src);

				switch (icmp_header->icmp_type) {
				case 1:
					break;
				case 3:
					break;
				case 8:
				{
					printf("--- ICMP ECHO Req Packet\n");
					uint8_t *reply_icmp = create_icmp_header(0, 0, ethernet_header, ip_header, target_interface, arp_entry);
					unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + icmp_len;
					if(arp_entry != NULL)
					{
						if ((sr_send_packet(sr, reply_icmp, packet_len, interface)) == 0) {
							printf("--- --- - Report : ICMP Type 1 Sent\n");
						} else {
							printf("Failed to send the packet.\n");
						}
					}
					else{
						printf("--- --- - Note : Try to create ARP Req for ICMP Type 0\n");
						printf("--- --- - Note : Couldn't find MAC in ARP_CACHE\n");
						struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_src,
				                                reply_icmp, packet_len, interface);
						handle_arpreq(sr, created_req);

					}
					break;
				}
				case 11:
					break;
				default:
					return;
				}
				printf("******* END of Process ******\n");
			}
			/*Not ICMP Packet*/
			else {

				ip_header->ip_ttl--;
				/*uint8_t *reply_icmp = create_icmp_header(3, 3, ethernet_header, ip_header, target_interface);*/
				/*unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);*/

				uint16_t  ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
				unsigned int reply_icmp_len = sizeof(sr_ethernet_hdr_t) + ip_len;

				uint8_t *packet = malloc(reply_icmp_len);

				sr_icmp_t3_hdr_t *reply_icmp = (sr_icmp_t3_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
				reply_icmp->icmp_type = 3;
				reply_icmp->icmp_code = 3;
				reply_icmp->icmp_sum = 0;
				reply_icmp->unused = 0;
				reply_icmp->next_mtu = 1000; /*TODO*/

				memcpy((void *)reply_icmp->data, (void *)(ip_header), sizeof(sr_ip_hdr_t) + 8);
				reply_icmp->icmp_sum = cksum((void *) reply_icmp, sizeof(sr_icmp_t3_hdr_t));


				create_ip_header(packet, ip_len, INITIAL_TTL, ip_protocol_icmp, target_interface->ip, ip_header->ip_src);
				create_ethernet_header(packet, ethernet_header->ether_shost,  ethernet_header->ether_dhost,  ethertype_ip);

				if ((sr_send_packet(sr, packet, reply_icmp_len, interface)) == 0) {
					printf("Port Unreachable ICMP successflly. \n");
				} else {
					printf("Failed to send the packet.\n");
				}

				free(packet);
				return;


				/*struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, ip_header->ip_src);
				uint8_t *reply_icmp = create_icmp_header(3, 3, ethernet_header, ip_header, target_interface, arp_entry);
				unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);

				if (arp_entry != NULL){
					if ((sr_send_packet(sr, reply_icmp, packet_len, interface)) == 0) {
						printf("--- --- - Report : ICMP Type 3 code 3 Sent\n");
					} else {
						printf("--- --- - Report : Failed to send the packet.\n");
					}
					free(reply_icmp);
				}
				else {
					printf("--- --- - Note : Try to create ARP Req for ICMP Type 3 Code 3\n");
					printf("--- --- - Note : Couldn't find MAC in ARP_CACHE\n");
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_src, reply_icmp, packet_len, interface);
					handle_arpreq(sr, created_req);
				}
				printf("******* END of Process ******\n");
				return;*/
			}

		}
		/*Not for me*/
		else {
			/* Check TTL */
			printf("--- --- - Note : Packet NOT for Router\n");
			if (ip_header->ip_ttl <= 1) {
				printf("--- --- - Note : TTL = 0\n");
				struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, ip_header->ip_src);
				uint8_t *reply_icmp = create_icmp_header(11, 0, ethernet_header, ip_header, target_interface, arp_entry);
				unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t11_hdr_t);

				if (arp_entry != NULL){
					if ((sr_send_packet(sr, reply_icmp, packet_len, interface)) == 0) {
						printf("--- --- - Report : ICMP Type 11 is sent\n");
					} else {
						printf("Failed to send the packet.\n");
					}
					free(reply_icmp);
				}
				else {
					printf("--- --- - Note : Couldn't find MAC in ARP_CACHE\n");
					printf("--- --- - Note : Try to create ARP Req for Type 11\n");
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_src, reply_icmp, packet_len, interface);
					handle_arpreq(sr, created_req);
				}
				printf("******* END of Process ******\n");
				return;
			}

			struct sr_rt* target_machine_ip = sr_get_routing_entry(sr, ip_header->ip_dst, ethernet_interface);

			/* Non-existant route for the destination ip ; No idea which interface should I use to forward this packet */
			if (target_machine_ip == 0) {
				printf("--- --- - Note : Can't find dest address in rtable\n");
				struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, ip_header->ip_src);
				uint8_t *reply_icmp = create_icmp_header(3, 0, ethernet_header, ip_header, target_interface, arp_entry);
				unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);

				if (arp_entry != NULL){
					if ((sr_send_packet(sr, reply_icmp, packet_len, interface)) == 0) {
						printf("--- --- - Report : ICMP Type 3 Code 0 is sent\n");
					} else {
						printf("Failed to send the packet.\n");
					}
					free(reply_icmp);
				}
				else {
					printf("--- --- - Note : Try to create ARP Req for ICMP Type 3\n");
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_src, reply_icmp, packet_len, interface);
					handle_arpreq(sr, created_req);
				}
				printf("******* END of Process ******\n");
				return;
			}

			/*Everything fine until now, just get me MAC address and the packet is good to go*/
			struct sr_if* next_hop_interface = sr_get_interface(sr, target_machine_ip->interface);
			struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, target_machine_ip->dest.s_addr);

			/*Found the MAC address, cache is so good!*/
			if (arp_entry != NULL) {
				ip_header->ip_ttl--;
				int new_len = sizeof(sr_ethernet_hdr_t) + ntohs(ip_header->ip_len);
				uint8_t *packet_new = malloc(new_len);

				ip_header->ip_sum = 0;

				ip_header->ip_sum = cksum((void *) ip_header, ip_header->ip_hl * 4);
				memcpy(packet_new + sizeof(sr_ethernet_hdr_t), ip_header, ntohs(ip_header->ip_len));

				create_ethernet_header(packet_new, arp_entry->mac, next_hop_interface->addr, ethertype_ip);

				if (sr_send_packet(sr, packet_new, new_len, next_hop_interface->name) == 0) {
					printf("--- --- - Report : Packet is forwarded\n");

				} else {
					printf("Failed to forward packet. \n");
				}

				free(packet_new);
				printf("******* END of Process ******\n");

			/*Didn't find the MAC address in cache :( */
			} else {
				printf("--- --- - Note : Couldn't find MAC in ARP_CACHE\n");
				printf("--- --- - Note : Try to create ARP Req for IP\n");
				struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_dst,
				                                packet, len, interface);
				handle_arpreq(sr, created_req);
				printf("******* END of Process ******\n");
			}
		}/* end sr_ForwardPacket */
	}/*END OF IP SECTION */

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
	reply_ip_header->ip_hl = sizeof(sr_ip_hdr_t) / 4;
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


uint8_t *create_icmp_header(uint8_t icmp_type, uint8_t icmp_code, sr_ethernet_hdr_t *ethernet_header, sr_ip_hdr_t *ip_header, 
	struct sr_if* target_interface, struct sr_arpentry *arp_entry){

	uint16_t ip_len = 0;
	uint8_t *packet = NULL;
	uint8_t *dest_mac = (uint8_t *) ("\xff\xff\xff\xff\xff\xff");

	if(arp_entry){
		dest_mac = arp_entry->mac;
	}
	
	if (icmp_type == 11) {
		ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t11_hdr_t);
		unsigned int reply_icmp_len = sizeof(sr_ethernet_hdr_t) + ip_len;

		packet = malloc(reply_icmp_len);

		sr_icmp_t11_hdr_t *reply_icmp = (sr_icmp_t11_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
		reply_icmp->icmp_type = icmp_type;
		reply_icmp->icmp_code = icmp_code;
		reply_icmp->icmp_sum = 0;
		reply_icmp->unused = 0;

		memcpy((void *)reply_icmp->data, (void *)(ip_header), sizeof(sr_ip_hdr_t) + 8);
		reply_icmp->icmp_sum = cksum((void *) reply_icmp, sizeof(sr_icmp_t11_hdr_t));
	}

	else if (icmp_type == 0){
		uint16_t icmp_len = ntohs(ip_header->ip_len) - ip_header->ip_hl * 4;
		ip_len = sizeof(sr_ip_hdr_t) + icmp_len;
		unsigned int reply_icmp_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + icmp_len;
		packet = malloc(reply_icmp_len);

		sr_icmp_hdr_t *echo_request_header = (sr_icmp_hdr_t *)(((uint8_t *)ip_header) + ip_header->ip_hl * 4);
		sr_icmp_hdr_t *reply_icmp = (sr_icmp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
		sr_ip_hdr_t *reply_ip = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

		memcpy(reply_icmp, echo_request_header, icmp_len);
		reply_icmp->icmp_type = icmp_type;
		reply_icmp->icmp_sum = icmp_code;
		reply_icmp->icmp_sum = cksum((void *) reply_icmp, icmp_len);

		memcpy(reply_ip, ip_header,  ip_header->ip_hl * 4);

		uint32_t ip_dst = ip_header->ip_dst;
		reply_ip->ip_src = ip_dst;
		reply_ip->ip_dst = ip_header->ip_src;
		
		create_ethernet_header(packet, dest_mac,  ethernet_header->ether_dhost,  ethertype_ip);
		return packet;
	}
	else if (icmp_type == 3 && icmp_code != 1) {

		ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
		unsigned int reply_icmp_len = sizeof(sr_ethernet_hdr_t) + ip_len;

		packet = malloc(reply_icmp_len);

		sr_icmp_t3_hdr_t *reply_icmp = (sr_icmp_t3_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
		reply_icmp->icmp_type = icmp_type;
		reply_icmp->icmp_code = icmp_code;
		reply_icmp->icmp_sum = 0;
		reply_icmp->unused = 0;
		reply_icmp->next_mtu = 1000; /*TODO*/

		memcpy((void *)reply_icmp->data, (void *)(ip_header), sizeof(sr_ip_hdr_t) + 8);
		reply_icmp->icmp_sum = cksum((void *) reply_icmp, sizeof(sr_icmp_t3_hdr_t));
	}
	else if (icmp_type == 3 && icmp_code == 1) {

		ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
		unsigned int reply_icmp_len = sizeof(sr_ethernet_hdr_t) + ip_len;

		packet = malloc(reply_icmp_len);

		sr_icmp_t3_hdr_t *reply_icmp = (sr_icmp_t3_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
		reply_icmp->icmp_type = icmp_type;
		reply_icmp->icmp_code = icmp_code;
		reply_icmp->icmp_sum = 0;
		reply_icmp->unused = 0;
		reply_icmp->next_mtu = 1000; /*TODO*/

		memcpy((void *)reply_icmp->data, (void *)(ip_header), sizeof(sr_ip_hdr_t) + 8);
		reply_icmp->icmp_sum = cksum((void *) reply_icmp, sizeof(sr_icmp_t3_hdr_t));

		/* Might be the problem too!! */
		create_ip_header(packet, ip_len, INITIAL_TTL, ip_protocol_icmp, ip_header->ip_dst, ip_header->ip_src);
		create_ethernet_header(packet, dest_mac,  ethernet_header->ether_dhost,  ethertype_ip);
		return packet;
	}

	create_ip_header(packet, ip_len, INITIAL_TTL, ip_protocol_icmp, target_interface->ip, ip_header->ip_src);
	create_ethernet_header(packet, dest_mac,  ethernet_header->ether_dhost,  ethertype_ip);
	return packet;
}

void send_arp_request(struct sr_instance *sr, struct sr_rt *hop_entry, struct sr_if *out_interface) {

	int length = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);

	uint8_t* packet = malloc(length);

	create_arp_header(packet, out_interface->addr, (uint8_t *) ("\xff\xff\xff\xff\xff\xff") , out_interface->ip, hop_entry->gw.s_addr, arp_op_request);
	create_ethernet_header(packet, (uint8_t *) ("\xff\xff\xff\xff\xff\xff"), out_interface->addr,  ethertype_arp);

	if ((sr_send_packet(sr, packet, length, out_interface->name)) == 0) {
		printf("--- --- Report : ARP REQUEST Sent. \n");
	} else {
		printf("Failed to send ARP REQUEST packet.\n");
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

	if (difftime(now, req->sent) >= 1.0) {
		/* I'm tired of waiting for your address, screw you, delete all yours! */
		if (req->times_sent >= 5) {
			printf("--- --- - Note : Same ARP Req for 5th Time \n");
			struct sr_packet *packet_head = req->packets;
			printf("--- --- - Note : Start loop \n");
			while (packet_head != NULL) {
				target_interface = sr_get_interface(sr, packet_head->iface);
				ip_header =  (sr_ip_hdr_t *)(packet_head->buf + sizeof(sr_ethernet_hdr_t));
				packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
				
				struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, ip_header->ip_src);
				uint8_t *reply_icmp = create_icmp_header(3, 1, (sr_ethernet_hdr_t *)packet_head->buf, ip_header, target_interface, arp_entry);

				if (arp_entry != NULL){
					if ((sr_send_packet(sr, reply_icmp, packet_len, packet_head->iface)) == 0) {
						printf("--- --- - Report : ICMP Type 3 Code 1 is sent\n");
					} else {
						printf("Failed to send ICMP Unreachable to host in handle_arpreq.\n");
					}
					free(reply_icmp);
				}
				else {
					printf("--- --- - Note : Try to create ARP Req for ICMP 3 Code 1\n");
					printf("--- --- - Note : Couldn't find MAC in ARP_CACHE\n");
					struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_src, reply_icmp, packet_len, packet_head->iface);
					handle_arpreq(sr, created_req);
				}

				if (packet_head->next) {
					packet_head = packet_head->next;
				} else {
					packet_head = NULL;
				}
			}
			sr_arpreq_destroy(&sr->cache, req);
			printf("--- --- - Report : Waiting packets cleared\n");
		} else {
			printf("--- --- - Note : ARP Req Preps \n");
      		struct sr_rt* target_machine_ip = sr_get_routing_entry(sr, req->ip, NULL);
			struct sr_if* next_hop_interface = sr_get_interface(sr, target_machine_ip->interface);

			send_arp_request(sr, target_machine_ip, next_hop_interface);
			req->sent = now;
			req->times_sent++;
		}

	}
}


