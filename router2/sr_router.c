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
uint8_t *create_icmp_header(uint8_t icmp_type, uint8_t icmp_code, sr_ethernet_hdr_t *ethernet_header, sr_ip_hdr_t *ip_header);
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

	printf("*** -> Received packet of length %d \n", len);



	/* fill in code here */
	struct sr_if* ethernet_interface = sr_get_interface(sr, interface);

	/* Fetch Interface from instance of sr for further processing. */
	if (!ethernet_interface) {
		printf("Received Invalid Packet Ethernet Interface. Error");
	}

	sr_ethernet_hdr_t *ethernet_header = (sr_ethernet_hdr_t *) packet;

	/* convert network byte order to host byte order. */
	uint16_t converted_ether_type_val = ntohs(ethernet_header->ether_type);

	if (converted_ether_type_val == ethertype_arp) {
		printf("Received ARP packet. \n");
		sr_arp_hdr_t *arp_header =  (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

		unsigned short converted_arp_op_val = ntohs(arp_header->ar_op);
		switch (converted_arp_op_val) {
		case arp_op_request:
			printf("Received ARP Request.\n");

			/*Find out if target Ip addr is one of our router's addresses. */
			struct sr_if* target_ip_interface = sr_get_ip_interface(sr, arp_header->ar_tip);
			
			if (target_ip_interface != 0) {
				printf("Target IP of ARP request in the router. \n");

				/*Create Packet with arp and ethernet header. */
				unsigned int reply_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
				uint8_t *reply_packet = malloc(reply_len);
				create_arp_header(reply_packet, target_ip_interface->addr, arp_header->ar_sha, target_ip_interface->ip, arp_header->ar_sip, arp_op_reply);
				create_ethernet_header(reply_packet, ethernet_header->ether_shost,  target_ip_interface->addr,  ethertype_arp);

				/* DEBUG FUNCTIONS remove Later.
				printf("ARP REPLY Created: \n");
				print_hdrs(reply_packet, reply_len);*/

				if ((sr_send_packet(sr, reply_packet, reply_len, target_ip_interface->name)) == 0) {
					printf("ARP Reply Sent successflly. \n");
				} else {
					printf("Failed to send the packet.\n");
				}

				free(reply_packet);

			} else {
				printf("Target IP not for Router.\n");
			}


			break;

		case arp_op_reply:
			printf("Received ARP Reply.\n");

			struct sr_arpreq *request =  sr_arpcache_insert(&sr->cache,
			                             arp_header->ar_sha,
			                             arp_header->ar_sip);

			if (request != NULL) {
				struct sr_packet *packet_head = request->packets;
				while (packet_head != NULL) {
					sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *) (packet_head->buf + sizeof(sr_ethernet_hdr_t));

					ip_header->ip_ttl--;
					int new_len = sizeof(sr_ethernet_hdr_t) + ntohs(ip_header->ip_len);
					uint8_t *packet_new = malloc(new_len);

					ip_header->ip_sum = 0;
					ip_header->ip_sum = cksum((void *) ip_header, ip_header->ip_hl * 4);

					memcpy(packet_new + sizeof(sr_ethernet_hdr_t), ip_header, ntohs(ip_header->ip_len));
					create_ethernet_header(packet_new, arp_header->ar_sha, arp_header->ar_tha, ethertype_ip);


					if ((sr_send_packet(sr, packet_new, packet_head->len, interface)) == 0) {
						printf("Broadcasted packet from arp Req upon receivng arp reply. \n");
					} else {
						printf("Failed to broadcast ARP Req packet upon receiving arp reply. \n");
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

		default:
			printf("No match of arp op code.\n");
		}



	} else if (converted_ether_type_val == ethertype_ip) {
		printf("Received IP Packet. \n");


		/*Get IP header*/
		sr_ip_hdr_t *ip_header =  (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
		struct sr_if* target_ip_interface = sr_get_ip_interface(sr, ip_header->ip_dst);




		/*checksum*/
		uint16_t ip_cksum = ip_header->ip_sum;
		ip_header->ip_sum = 0;
		uint16_t in_cksum =  cksum((void *) ip_header, ip_header->ip_hl * 4);

		ip_header->ip_sum = in_cksum;

		/*Validate the header*/
		if (ip_header->ip_hl < 5 || in_cksum != ip_cksum) {
			printf("Checksum or Length Error in packet\n");
			return;
		}

		/*TTL*/
		if (ip_header->ip_ttl <= 1) {
			uint8_t *reply_icmp = create_icmp_header(11, 0, ethernet_header, ip_header);
			unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t11_hdr_t);

			if ((sr_send_packet(sr, reply_icmp, packet_len, interface)) == 0) {
				printf("ICMP TTL Sent successflly. \n");
			} else {
				printf("Failed to send the packet.\n");
			}

			free(reply_icmp);
			return;
		}

		/*Packet for my IP*/
		uint32_t dst_ip = ip_header->ip_dst;
		if (sr_get_ip_interface(sr, dst_ip) != 0) {
			printf("Target IP Packet for Router.\n");
			/*If ICMP*/
			if (ip_header->ip_p == 1) {

				sr_icmp_hdr_t *icmp_header = (sr_icmp_hdr_t *)(((uint8_t *)ip_header) + ip_header->ip_hl * 4);
				uint16_t icmp_len = ntohs(ip_header->ip_len) - ip_header->ip_hl * 4;


				/*	uint8_t *icmp =  (uint8_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));*/
				switch (icmp_header->icmp_type) {
				case 1:
					break;
				case 3:
					break;
				case 8:
				{
					/*echo request*/
					uint16_t ip_len = sizeof(sr_ip_hdr_t) + icmp_len;
					unsigned int reply_icmp_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + icmp_len;
					uint8_t *reply_icmp = malloc(reply_icmp_len);

					/*create_icmp_header(reply_icmp, 0, 0);*/
					sr_icmp_hdr_t *out_icmp =
					    (sr_icmp_hdr_t *)(reply_icmp + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

					memcpy(out_icmp, icmp_header, icmp_len);

					out_icmp->icmp_type = 0;

					out_icmp->icmp_sum = 0;

					out_icmp->icmp_sum = cksum((void *) reply_icmp, icmp_len);


					create_ip_header(reply_icmp, ip_len, INITIAL_TTL, ip_protocol_icmp, ip_header->ip_dst, ip_header->ip_src);

					create_ethernet_header(reply_icmp, ethernet_header->ether_shost,  ethernet_header->ether_dhost,  ethertype_ip);



					if ((sr_send_packet(sr, reply_icmp, reply_icmp_len, interface)) == 0) {
						printf("Echo Reply successflly. \n");
					} else {
						printf("Failed to send the packet.\n");
					}
					break;
				}
				case 11:
					break;
				default:
					return;
				}
			}
			/*If TCP/UDP we don't care about specific protocol */
			else {
				ip_header->ip_ttl--;
				uint8_t *reply_icmp = create_icmp_header(3, 3, ethernet_header, ip_header);
				unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
				if ((sr_send_packet(sr, reply_icmp, packet_len, interface)) == 0) {
					printf("Port Unreachable ICMP successflly. \n");
				} else {
					printf("Failed to send the packet.\n");
				}

				free(reply_icmp);
				return;
			}

		}
		/*Not for me*/
		else {
			printf("Target IP not for Router.\n Destination :");
			print_addr_ip_int(ntohl(ip_header->ip_dst));
			struct sr_rt* target_machine_ip = sr_get_routing_entry(sr, ip_header->ip_dst);

			if (target_machine_ip == 0) {
				uint8_t *reply_icmp = create_icmp_header(3, 1, ethernet_header, ip_header);
				unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
				if ((sr_send_packet(sr, reply_icmp, packet_len, interface)) == 0) {
					printf("Destination net ICMP successflly. \n");
				} else {
					printf("Failed to send the packet.\n");
				}

				free(reply_icmp);
				printf("IP not handled by router as it does not exist in router. \n");
				return;
			}

			struct sr_if* next_hop_interface = sr_get_interface(sr, target_machine_ip->interface);

			struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, ip_header->ip_dst);
			if (arp_entry != NULL) {

				ip_header->ip_ttl--;
				int new_len = sizeof(sr_ethernet_hdr_t) + ntohs(ip_header->ip_len);
				uint8_t *packet_new = malloc(new_len);

				ip_header->ip_sum = 0;

				ip_header->ip_sum = cksum((void *) ip_header, ip_header->ip_hl * 4);
				memcpy(packet_new + sizeof(sr_ethernet_hdr_t), ip_header, ntohs(ip_header->ip_len));

				create_ethernet_header(packet_new, arp_entry->mac, next_hop_interface->addr, ethertype_ip);

				if (sr_send_packet(sr, packet_new, new_len, next_hop_interface->name) == 0) {
					printf("Packet Forwarded successflly. \n");

				} else {
					printf("Failed to forward packet. \n");
				}

				free(packet_new);


			} else {
				struct sr_arpreq *created_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_dst,
				                                packet, len, interface);

				handle_arpreq(sr, created_req);

			}
		}
	}

}/* end sr_ForwardPacket */

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


uint8_t *create_icmp_header(uint8_t icmp_type,
                            uint8_t icmp_code,
                            sr_ethernet_hdr_t *ethernet_header,
                            sr_ip_hdr_t *ip_header) {
	uint16_t ip_len;
	uint8_t *packet;
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

	else if (icmp_type == 3) {

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

	create_ip_header(packet, ip_len, INITIAL_TTL, ip_protocol_icmp, ip_header->ip_dst, ip_header->ip_src);
	create_ethernet_header(packet, ethernet_header->ether_shost,  ethernet_header->ether_dhost,  ethertype_ip);
	return packet;
}






void send_arp_request(struct sr_instance *sr, struct sr_rt *hop_entry, struct sr_if *out_interface) {

	int length = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);

	uint8_t* packet = malloc(length);

	create_arp_header(packet, out_interface->addr, (uint8_t *) ("\xff\xff\xff\xff\xff\xff") , out_interface->ip, hop_entry->gw.s_addr, arp_op_request);
	create_ethernet_header(packet, (uint8_t *) ("\xff\xff\xff\xff\xff\xff"), out_interface->addr,  ethertype_arp);

	if ((sr_send_packet(sr, packet, length, out_interface->name)) == 0) {
		printf("ARP REQUEST Sent successflly. \n");
	} else {
		printf("Failed to send ARP REQUEST packet.\n");
	}

	free(packet);

}

void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req) {
	time_t now;
	time(&now);
	assert(req);

	if (difftime(now, req->sent) > 1.0) {
		if (req->times_sent >= 5) {
			struct sr_packet *packet_head = req->packets;
			while (packet_head != NULL) {
				/*create icmp error and send to source of all packets. */


				if ((sr_send_packet(sr, packet_head->buf, packet_head->len, packet_head->iface)) == 0) {
					printf("ICMP Host Unreachable Sent to host in handle_arpreq. \n");
				} else {
					printf("Failed to send ICMP Unreachable to host in handle_arpreq.\n");
				}

				if (packet_head->next) {
					packet_head = packet_head->next;
				} else {
					packet_head = NULL;
				}

			}

			sr_arpreq_destroy(&sr->cache, req);
		} else {
			printf("Preparing To send ARP Req. \n");
			struct sr_rt* target_machine_ip = sr_get_routing_entry(sr, req->ip);
			struct sr_if* next_hop_interface = sr_get_interface(sr, target_machine_ip->interface);

			send_arp_request(sr, target_machine_ip, next_hop_interface);
			req->sent = now;
			req->times_sent++;
		}

	}

}


