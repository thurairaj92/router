
#include <signal.h>
#include <assert.h>
#include <unistd.h>


#include "sr_nat.h"
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"




void tcp_handling(struct sr_instance *sr, uint8_t *packet, packet_direction packet_dir);
void print_connection(struct sr_nat_connection *conns_mapping);
struct sr_nat_mapping *sr_nat_tcp_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type );

struct sr_nat_connection *sr_nat_lookup_connection(struct sr_nat_mapping *mapping,  uint32_t ip_int, uint16_t aux_int);

struct sr_nat_mapping *sr_nat_tcp_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );


void print_addr_ip_normal(uint32_t ip) {
  uint32_t curOctet = (ip << 24) >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = (ip << 16) >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = (ip << 8) >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = ip >> 24;
  fprintf(stderr, "%d.\n", curOctet);
}



int sr_nat_init(struct sr_nat *nat) { /* Initializes the nat */

  assert(nat);

  /* Acquire mutex lock */
  pthread_mutexattr_init(&(nat->attr));
  pthread_mutexattr_settype(&(nat->attr), PTHREAD_MUTEX_RECURSIVE);
  int success = pthread_mutex_init(&(nat->lock), &(nat->attr));

  /* Initialize timeout thread */

  pthread_attr_init(&(nat->thread_attr));
  pthread_attr_setdetachstate(&(nat->thread_attr), PTHREAD_CREATE_JOINABLE);
  pthread_attr_setscope(&(nat->thread_attr), PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setscope(&(nat->thread_attr), PTHREAD_SCOPE_SYSTEM);
  pthread_create(&(nat->thread), &(nat->thread_attr), sr_nat_timeout, nat);

  /* CAREFUL MODIFYING CODE ABOVE THIS LINE! */

  nat->mappings = NULL;
  nat->unsolicited_tcp = NULL;
  fprintf(stderr, "%s\n", "Initializes unsolicited_tcp");
  nat->available_port = 1050;
  /* Initialize any variables here */

  return success;
}

/*HELPERS*/

interface_map determine_ip_interface(struct sr_instance *sr, uint32_t ip_val){

	struct sr_if* nat_packet_if = sr_get_ip_interface(sr,ip_val);
	
	if(nat_packet_if){
		return nat_if;
	}

	struct sr_rt* nat_rt  = sr_get_routing_entry(sr, ip_val, NULL);
	if(nat_rt){
		if((strcmp(nat_rt->interface,"eth2") == 0)){
			return external_if;
		} else{
			return internal_if;
		}

	}

	return unknown_if;
}


packet_direction determine_packet_direction(interface_map src_val, interface_map dst_val){

	if(src_val == internal_if && dst_val == external_if){
		return outbound_packet;
	} else if(src_val == external_if && dst_val == internal_if){
		return inbound_packet;
	} else if(src_val == external_if && dst_val == nat_if){
		return inbound_packet;
	} else{
		return confused_packet;
	}

}

packet_direction calclate_packet_direction(struct sr_instance *sr, uint8_t *packet){
  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;

  interface_map src_if = determine_ip_interface(sr,ip_header->ip_src);
  interface_map dst_if = determine_ip_interface(sr,ip_header->ip_dst);
  packet_direction this_packet_direction = determine_packet_direction(src_if,dst_if);
  return this_packet_direction;

}

uint16_t calclate_tcp_cksum(uint8_t *tcp_pack, sr_ip_hdr_t *ip_pack, unsigned int tcp_len){

  uint8_t *temp = (uint8_t *)malloc(sizeof(sr_tcp_cksum_hdr_t) + tcp_len);

  sr_tcp_cksum_hdr_t *tcp_hdr_cksum = (sr_tcp_cksum_hdr_t *)temp;
  tcp_hdr_cksum->ip_p = ip_pack->ip_p;
  tcp_hdr_cksum->ip_dst = ip_pack->ip_dst;
  tcp_hdr_cksum->ip_src = ip_pack->ip_src;
  tcp_hdr_cksum->len = htons(tcp_len);
  memcpy(temp + sizeof(sr_tcp_cksum_hdr_t), tcp_pack, tcp_len);

  return cksum(temp,sizeof(sr_tcp_cksum_hdr_t) + tcp_len);
}


int sr_nat_destroy(struct sr_nat *nat) {  /* Destroys the nat (free memory) */

	pthread_mutex_lock(&(nat->lock));

	/* free nat memory here */
	pthread_kill(nat->thread, SIGKILL);
	return pthread_mutex_destroy(&(nat->lock)) &&
		pthread_mutexattr_destroy(&(nat->attr));
}

void nat_mapping_timeout(void *nat_ptr){
   struct sr_nat *nat_instance = (struct sr_nat *)nat_ptr;
    time_t present = time(NULL);

    struct sr_nat_mapping *mapping_list = nat_instance->mappings;

    struct sr_nat_mapping *to_free = NULL;
    while(mapping_list != NULL){
      double time_diff = difftime(present,mapping_list->last_updated);

      if(mapping_list->type == nat_mapping_icmp){
        if(time_diff >= nat_instance->icmp_timeout){
            fprintf(stderr, "%s\n", "##############################################");
        	fprintf(stderr, "%s : %d\n ", "ICMP Timeout", nat_instance->icmp_timeout);
           	fprintf(stderr, "%s\n", "##############################################");
          if(mapping_list->next){
            mapping_list->next->prev = mapping_list->prev;
          }
          if(mapping_list->prev){
            mapping_list->prev->next = mapping_list->next;
          } else{
            nat_instance->mappings = mapping_list->next;
          }
          to_free = mapping_list;
        }
        

      } else if(mapping_list->type == nat_mapping_tcp){
        struct sr_nat_connection *conns = mapping_list->conns;
        while(conns != NULL){
          int connection_type = 0;
          /*Should it be fin_ack?*/
          if(conns->server_fin && conns->client_fin){
             connection_type = 1;
          }

          double time_diff = difftime(present,conns->last_updated);
          int connection_remove = 0;

          fprintf(stderr, "Time diff %.f\n", time_diff);

          if(time_diff > 1){
          	fprintf(stderr, "%s\n", "fprintf is fucked up\n" );
          }

          if(connection_type == 0){
            if(time_diff > TCP_TRANSITION_TIMEOUT){
            	fprintf(stderr, "%s\n", "##############################################");
            	fprintf(stderr, "%s\n", "TCP Transition");
            	fprintf(stderr, "%s\n", "##############################################");
              connection_remove = 1;
            }
          } else {
            if(time_diff > TCP_DEFAULT_TIMEOUT){
            	fprintf(stderr, "%s\n", "##############################################");
            	fprintf(stderr, "%s\n", "TCP Timeout");
            	fprintf(stderr, "%s\n", "##############################################");
              connection_remove = 1;
            }
          }

          struct sr_nat_connection *to_free = NULL;
          if(connection_remove){
              if(conns->next){
                conns->next->prev = conns->prev;
              }
              if(conns->prev){
                conns->prev->next = conns->next;
              } else{
                mapping_list->conns = conns->next;
              }
              to_free = conns;
          }
          conns = conns->next;
            if(to_free){
              free(to_free);
            }
        }

        if(mapping_list->conns == NULL){
          if(mapping_list->next){
            mapping_list->next->prev = mapping_list->prev;
          }
          if(mapping_list->prev){
            mapping_list->prev->next = mapping_list->next;
          } else{
            nat_instance->mappings = mapping_list->next;
          }
          to_free = mapping_list;
        }  
      }

      mapping_list = mapping_list->next;
      if(to_free){
          free(to_free);
      }
    }

}


int transform_inbound_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len){
	printf("--------------Before---------------\n");
	sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;
	fprintf(stderr, "%s", "Source IP : ");
	print_addr_ip_normal(ip_header->ip_src);
	fprintf(stderr, "%s", "Destination IP : ");
	print_addr_ip_normal(ip_header->ip_dst);

	printf("%s\n", "     ----    ");


	sr_nat_mapping_type packet_mapping_type;
	struct sr_nat_mapping *packet_mapping  = NULL;
	uint16_t port_val;
	int valid_protocol = 1;

	if(ip_header->ip_p == ip_protocol_icmp){
		packet_mapping_type = nat_mapping_icmp;
		sr_icmp_nat_hdr_t *icmp_hdr = (sr_icmp_nat_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		port_val = icmp_hdr->icmp_id;
	}
	else if(ip_header->ip_p == ip_protocol_tcp){
		packet_mapping_type = nat_mapping_tcp;
		sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		port_val = tcp_hdr->dst_port;
	} 
	else{
		valid_protocol = 0;
	}

	if(valid_protocol){
		packet_mapping = sr_nat_lookup_external(&(sr->nat), port_val,packet_mapping_type);
	} else{
		fprintf(stderr, "%s\n", "Drop Packet 1");
		return DROP_PACKET;
	}

	if(!packet_mapping){
		if(ip_header->ip_p == ip_protocol_icmp){
			fprintf(stderr, "%s\n", "Drop Packet 2");
		    return PACKET_FINE; 
		} 

		else if(ip_header->ip_p == ip_protocol_tcp){
		    sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		    /*Check its SYN*/
		    if(tcp_hdr->flags & TCP_SYN_FLAG){
		    	fprintf(stderr, "Port number : %d\n", tcp_hdr->dst_port);
		    	/*if(tcp_hdr->dst_port < 1024){
		    		struct sr_if* ethernet_interface = sr->nat.ext_if;
			        struct sr_rt* gateway;
					struct sr_arpentry *arp_entry;
					uint8_t *reply_packet;
					unsigned int packet_len;

			      	gateway = sr_get_routing_entry(sr, ip_header->ip_src, NULL);
					if(gateway == NULL){
						return DROP_PACKET;
					}
					
					arp_entry = sr_arpcache_lookup(&(sr->cache), gateway->gw.s_addr);

					uint16_t  ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
					packet_len = sizeof(sr_ethernet_hdr_t) + ip_len;

					reply_packet = malloc(packet_len);
					create_icmp_port_header(reply_packet, ip_header);
					create_ip_header(reply_packet, ip_len, 100, ip_protocol_icmp, ethernet_interface->ip , ip_header->ip_src);

					if(arp_entry != NULL){
						create_ethernet_header(reply_packet, arp_entry->mac, ethernet_interface->addr,  ethertype_ip);
						sr_send_packet(sr, reply_packet, packet_len, "eth2");
						return DROP_PACKET;
					}else{
						create_ethernet_header(reply_packet, (uint8_t *) (EMPTY_MAC),  ethernet_interface->addr,  ethertype_ip);
						struct sr_arpreq *created_req = sr_arpcache_queuereq(&(sr->cache), gateway->gw.s_addr, reply_packet, packet_len, "eth2");
						handle_arpreq(sr, created_req);
						return DROP_PACKET;
					}
		    	}*/

				struct sr_unsolicited_tcp *tcps = sr->nat.unsolicited_tcp;
				/*Check if already in queue.*/
				fprintf(stderr, "1 Port number : %d\n", tcp_hdr->dst_port);
				while(tcps != NULL){

					if(tcps->src_ip == ip_header->ip_src && tcps->port_val_ext == port_val){
						break;
					}
					tcps = tcps->next;
				}

				fprintf(stderr, "2 Port number : %d\n", tcp_hdr->dst_port);

				if(tcps == NULL){
					/*Insert the unsolicited SYN.*/
					struct sr_unsolicited_tcp *new_tcp = (struct sr_unsolicited_tcp *)malloc(sizeof(struct sr_unsolicited_tcp));
					new_tcp->port_val_ext = port_val;
					new_tcp->src_ip = ip_header->ip_src;
					new_tcp->arrival_time = time(NULL);
					fprintf(stderr, "4 Port number : %d\n", tcp_hdr->dst_port);
					new_tcp->ip_header = (void *)malloc(28);
					memcpy(new_tcp->ip_header, (void *)ip_header, 28);


					if(sr->nat.unsolicited_tcp){
					  sr->nat.unsolicited_tcp->prev = new_tcp;
					}
					new_tcp->prev = NULL;
					new_tcp->next = sr->nat.unsolicited_tcp;
					sr->nat.unsolicited_tcp = new_tcp;
		        }
		    }
		}
		fprintf(stderr, "%s\n", "Drop Packet 3");
		return DROP_PACKET; 
	}

	if(ip_header->ip_p == ip_protocol_tcp){
		/*tcp_inbound(sr, packet);*/
		tcp_handling(sr, packet, inbound_packet);
	}

	/*REWRITE THE PACKET.*/
	ip_header->ip_dst = packet_mapping->ip_int;
	port_val = packet_mapping->aux_int;
	ip_header->ip_sum = 0;
	ip_header->ip_sum = cksum(ip_header,sizeof(sr_ip_hdr_t));

	if(ip_header->ip_p == ip_protocol_icmp){
		sr_icmp_nat_hdr_t *icmp_nat_hdr = (sr_icmp_nat_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		icmp_nat_hdr->icmp_id = port_val;
		icmp_nat_hdr->icmp_sum = 0;
		icmp_nat_hdr->icmp_sum = cksum(icmp_nat_hdr,sizeof(sr_icmp_nat_hdr_t));

	} else if(ip_header->ip_p == ip_protocol_tcp){

		sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		tcp_hdr->dst_port = packet_mapping->aux_int;
		tcp_hdr->cksum = 0;
		tcp_hdr->cksum = calclate_tcp_cksum((uint8_t *)tcp_hdr, ip_header, len-sizeof(sr_ip_hdr_t));


	}
	fprintf(stderr, "%s", "Source IP : ");
	print_addr_ip_normal(ip_header->ip_src);
	fprintf(stderr, "%s", "Destination IP : ");
	print_addr_ip_normal(ip_header->ip_dst);
	printf("----Done Trasform-------\n");
  return PACKET_FINE;
}


int transform_outbound_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len){



  	sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;
  	printf("--------------Before---------------\n");

  	fprintf(stderr, "%s", "Source IP : ");
	print_addr_ip_normal(ip_header->ip_src);
	fprintf(stderr, "%s", "Destination IP : ");
	print_addr_ip_normal(ip_header->ip_dst);

	printf("%s\n", "     ----    ");

	sr_nat_mapping_type packet_mapping_type;
	struct sr_nat_mapping *packet_mapping  = NULL;
	uint16_t port_val;
	int valid_protocol = 1;

	if(ip_header->ip_p == ip_protocol_icmp){
		return PACKET_FINE;
		if (sr_get_ip_interface(sr, ip_header->ip_dst) != 0){
			return PACKET_FINE;
		}
		packet_mapping_type = nat_mapping_icmp;
		sr_icmp_nat_hdr_t *icmp_hdr = (sr_icmp_nat_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		port_val = icmp_hdr->icmp_id;
	} else if(ip_header->ip_p == ip_protocol_tcp){
		packet_mapping_type = nat_mapping_tcp;
		sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		port_val = tcp_hdr->src_port;
	} else{
		valid_protocol = 0;
	}

	if(valid_protocol){
		packet_mapping = sr_nat_lookup_internal(&(sr->nat), ip_header->ip_src ,port_val,packet_mapping_type);
	} else{
		return DROP_PACKET;
	}

	if(!packet_mapping){
		/*create mapping. */
		if(ip_header->ip_p == ip_protocol_tcp){
			sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
			if(!(tcp_hdr->flags & TCP_SYN_FLAG)){
				return DROP_PACKET;
			}
		}

		uint32_t ip_int = ip_header->ip_src;
		uint32_t ip_ext = ip_header->ip_dst;
		uint16_t aux_int = port_val;

		packet_mapping = sr_nat_insert_mapping(&(sr->nat),ip_int, ip_ext, aux_int, packet_mapping_type);

	/*if still null return DROP_PACKET;*/
	}
	if(ip_header->ip_p == ip_protocol_tcp){
		/*Update TCP connections HERE. */
		/*tcp_outbound(sr, packet);*/
		 tcp_handling(sr, packet, outbound_packet);
	}

  	/*REWRITE THE PACKET using packet mapping.*/
	struct sr_if* src_if = sr_get_interface(sr, "eth2");

	/*ip_header->ip_dst = packet_mapping->ip_ext;*/
	ip_header->ip_src = src_if->ip;
	port_val = packet_mapping->aux_ext;
	ip_header->ip_sum = 0;
	ip_header->ip_sum = cksum(ip_header,sizeof(sr_ip_hdr_t));

	if(ip_header->ip_p == ip_protocol_icmp){
		sr_icmp_nat_hdr_t *icmp_nat_hdr = (sr_icmp_nat_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		icmp_nat_hdr->icmp_id = port_val;
		icmp_nat_hdr->icmp_sum = 0;
		icmp_nat_hdr->icmp_sum = cksum(icmp_nat_hdr,sizeof(sr_icmp_nat_hdr_t));

	} else if(ip_header->ip_p == ip_protocol_tcp){

		sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		tcp_hdr->src_port = packet_mapping->aux_ext;
		tcp_hdr->cksum = 0;
		tcp_hdr->cksum = calclate_tcp_cksum((uint8_t *)tcp_hdr, ip_header, len-sizeof(sr_ip_hdr_t));
	}


	fprintf(stderr, "%s", "Source IP : ");
	print_addr_ip_normal(ip_header->ip_src);
	fprintf(stderr, "%s", "Destination IP : ");
	print_addr_ip_normal(ip_header->ip_dst);
	fprintf(stderr, "Port to Server : %u\n", (unsigned int)port_val);
	printf("----Done Trasform-------\n");

  	return PACKET_FINE;
}


void tcp_handling(struct sr_instance *sr, uint8_t *packet, packet_direction packet_dir){


	struct sr_nat *nat  = &(sr->nat);
	
	pthread_mutex_lock(&(nat->lock));
	uint16_t server_port;
	uint16_t client_port;

	uint32_t server_ip;
	uint32_t client_ip;

	sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
	sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;
	struct sr_nat_mapping *packet_mapping  = NULL;
	struct sr_nat_connection *conns_mapping = NULL;


	if(packet_dir == inbound_packet){
		server_port = tcp_hdr->src_port;
		client_port = tcp_hdr->dst_port;

		server_ip =  ip_header->ip_src;
		client_ip =  ip_header->ip_dst;

		packet_mapping = sr_nat_tcp_lookup_external(&(sr->nat), client_port, nat_mapping_tcp );
	}else if(packet_dir == outbound_packet){
		server_port = tcp_hdr->dst_port;
		client_port = tcp_hdr->src_port;

		server_ip =  ip_header->ip_dst;
		client_ip =  ip_header->ip_src;
		packet_mapping = sr_nat_tcp_lookup_internal(&(sr->nat), client_ip, client_port, nat_mapping_tcp);
	}else{
    	pthread_mutex_unlock(&(nat->lock));
		return;
	}

	
	if(packet_mapping){
		conns_mapping = packet_mapping->conns;
	}else{
		pthread_mutex_unlock(&(nat->lock));
		return;
	}

	struct sr_unsolicited_tcp *tcps = nat->unsolicited_tcp;
	/*Check if already in queue.*/


	while(tcps){
		
		if(tcps->src_ip == server_ip && tcps->port_val_ext == server_port){
			if(tcps->prev){
				tcps->prev->next = tcps->next;
			}else{
				sr->nat.unsolicited_tcp = tcps->next;
			}

			if(tcps->next){
				tcps->next->prev = tcps->prev;
			}
			break;
		}
		
		tcps = tcps->next;
		fprintf(stderr, "%s\n", "----------------------");
	}

	conns_mapping = sr_nat_lookup_connection(packet_mapping,  server_ip, server_port);
	

	if(!conns_mapping){
		struct sr_nat_connection *new_conns = (struct sr_nat_connection *)malloc(sizeof(struct sr_nat_connection));
		
		new_conns->server_syn = 0;
		new_conns->client_syn = 0;

		new_conns->server_fin = 0;
		new_conns->client_fin = 0;

		new_conns->server_fins_ack = 0;
		new_conns->client_fins_ack = 0;

		new_conns->server_fin_ack = 0;
		new_conns->client_fin_ack = 0;

		new_conns->client_fin_set = 0;
		new_conns->server_fin_set = 0;

		new_conns->prev = NULL;
	  	new_conns->next = NULL;


	  	new_conns->server_ip = server_ip;
	  	new_conns->server_port = server_port;

	  	if(packet_mapping->conns){
	  		packet_mapping->conns->prev = new_conns;
	  		new_conns->next = packet_mapping->conns;
	  		packet_mapping->conns = new_conns;
	  	}else{
	  		packet_mapping->conns = new_conns;
	  	}
	  	conns_mapping = new_conns;
	}

	 conns_mapping->last_updated = time(NULL);
	 fprintf(stderr, "---SEQ------%lu\n", (unsigned long)tcp_hdr->seq);
	 fprintf(stderr, "----ACK-----%lu\n", (unsigned long)tcp_hdr->ack);
	
	if(packet_dir == inbound_packet){
		fprintf(stderr, "%s\n", "INBOUND");
		if(tcp_hdr->flags &  TCP_SYN_FLAG){
			fprintf(stderr, "%s\n", "SYN");
			conns_mapping->server_syn = tcp_hdr->seq;
		}

		if(tcp_hdr->flags &  TCP_FIN_FLAG){
			fprintf(stderr, "%s\n", "FIN");
			conns_mapping->server_fin = tcp_hdr->seq;
			conns_mapping->server_fins_ack = tcp_hdr->ack;
			conns_mapping->server_fin_set = 1;
		}

		if(conns_mapping->client_fin_set > 0){
			if(tcp_hdr->seq >= conns_mapping->client_fins_ack){
				conns_mapping->server_fin_ack = 1;
			}
		}
	}else{
		if(tcp_hdr->flags &  TCP_SYN_FLAG){
			conns_mapping->client_syn = tcp_hdr->seq;
		}

		if(tcp_hdr->flags &  TCP_FIN_FLAG){
			conns_mapping->client_fin = tcp_hdr->seq;
			conns_mapping->client_fins_ack = tcp_hdr->ack;
			conns_mapping->client_fin_set = 1;
		}

		if(conns_mapping->server_fin_set > 0 ){
			if(tcp_hdr->seq >= conns_mapping->server_fins_ack){
				conns_mapping->client_fin_ack = 1;
			}
		}
	}
	print_connection(conns_mapping); 

	if((conns_mapping->client_fin_ack > 0 && conns_mapping->server_fin_ack > 0 )|| tcp_hdr->flags == TCP_RESET_FLAG){
		
		if(conns_mapping->prev){
			conns_mapping->prev->next = conns_mapping->next;	
		}else{
			packet_mapping->conns = conns_mapping->next;
			if(!packet_mapping->conns){
				fprintf(stderr, "Mapping is removed !!!!!!!\n");
				if(packet_mapping->next){
	        	    packet_mapping->next->prev = packet_mapping->prev;
				}

				if(packet_mapping->prev){
					packet_mapping->prev->next = packet_mapping->next;
				} else{
					nat->mappings = packet_mapping->next;
				}	
			}
		}

		if(conns_mapping->next){
			conns_mapping->next->prev = conns_mapping->prev;
		}
		
		free(conns_mapping);
	}
	
	pthread_mutex_unlock(&(nat->lock));
	return;
}


int transform_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len){
	packet_direction packet_dir =  calclate_packet_direction(sr,packet);

	sr_ip_hdr_t* ip_header = (sr_ip_hdr_t *)packet;
	uint16_t packet_cksum, calculated_cksum;
		
	packet_cksum = ip_header->ip_sum;
	ip_header->ip_sum = 0;

	calculated_cksum =  cksum((void *) ip_header, ip_header->ip_hl * BYTE_CONVERSION);
	ip_header->ip_sum = packet_cksum;

	if (ip_header->ip_hl < 5 || packet_cksum != calculated_cksum || ip_header->ip_v != BYTE_CONVERSION) {
		return DROP_PACKET;
	}



	if(ip_header->ip_p == 1){
		sr_icmp_nat_hdr_t *icmp_nat_hdr = (sr_icmp_nat_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
				
		uint16_t icmp_packet_cksum, icmp_calculate_cksum;
		uint16_t icmp_len = ntohs(ip_header->ip_len) - ip_header->ip_hl * BYTE_CONVERSION;

		icmp_packet_cksum = icmp_nat_hdr->icmp_sum;
		icmp_nat_hdr->icmp_sum = 0;
		icmp_calculate_cksum =  cksum((void *) icmp_nat_hdr, icmp_len);

		if(icmp_packet_cksum != icmp_calculate_cksum){
			return DROP_PACKET;
		}
	}


	if(packet_dir == inbound_packet){
		printf("IN\n");
		return transform_inbound_packet(sr,packet,len);
	} else if(packet_dir == outbound_packet){
		printf("OUT\n");
		return transform_outbound_packet(sr,packet,len);
	} else{
		/*We let the confused packet figure out its way. :) */
		/*It is a quest for that packet, Good luck, packet, May The Force Be With You */
		printf("confused\n");
		return PACKET_FINE;
	}
  	return PACKET_FINE;
}

void *sr_nat_timeout(void *nat_ptr) {  /* Periodic Timout handling */
  struct sr_nat *nat = (struct sr_nat *)nat_ptr;
  while (1) {
    sleep(1.0);
    pthread_mutex_lock(&(nat->lock));

    /* handle periodic tasks here */
    nat_mapping_timeout(nat_ptr);
    time_t present = time(NULL);

    /*Add Unsolicited Timeout.*/
    struct sr_unsolicited_tcp *unsolicited_tcp = nat->unsolicited_tcp;
    while(unsolicited_tcp != NULL){
      double time_diff = difftime(present,unsolicited_tcp->arrival_time);
      struct sr_unsolicited_tcp *to_free = NULL;

      if(time_diff > 6){
      	fprintf(stderr, "ICMP 3\n");
        /*Send ICMP error. (how to get interface?)*/
       /* if(unsolicited_tcp->port_val_ext > 1024){*/
	        struct sr_if* ethernet_interface = nat->ext_if;
	        struct sr_rt* gateway;
			struct sr_arpentry *arp_entry;
			uint8_t *reply_packet;
			unsigned int packet_len;

	      	gateway = sr_get_routing_entry(nat->sr, unsolicited_tcp->src_ip, NULL);
			if(gateway == NULL){
				return NULL;
			}
			
			arp_entry = sr_arpcache_lookup(&(nat->sr->cache), gateway->gw.s_addr);

			uint16_t  ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
			packet_len = sizeof(sr_ethernet_hdr_t) + ip_len;

			reply_packet = malloc(packet_len);
			create_icmp_port_header(reply_packet, (sr_ip_hdr_t *)unsolicited_tcp->ip_header);
			create_ip_header(reply_packet, ip_len, 100, ip_protocol_icmp, ethernet_interface->ip , unsolicited_tcp->src_ip);

			if(arp_entry != NULL){
				create_ethernet_header(reply_packet, arp_entry->mac, ethernet_interface->addr,  ethertype_ip);
				sr_send_packet(nat->sr, reply_packet, packet_len, "eth2");
			}else{
				create_ethernet_header(reply_packet, (uint8_t *) (EMPTY_MAC),  ethernet_interface->addr,  ethertype_ip);
				struct sr_arpreq *created_req = sr_arpcache_queuereq(&(nat->sr->cache), gateway->gw.s_addr, reply_packet, packet_len, "eth2");
				handle_arpreq(nat->sr, created_req);
			}
		/*}*/
        
        


         if(unsolicited_tcp->next){
            unsolicited_tcp->next->prev = unsolicited_tcp->prev;
          }
          if(unsolicited_tcp->prev){
            unsolicited_tcp->prev->next = unsolicited_tcp->next;
          } else{
            /*corr : unsolicited_tcp->conns = unsolicited_tcp->next;*/
          	nat->unsolicited_tcp = unsolicited_tcp->next;
          }
          to_free = unsolicited_tcp;



      }
      unsolicited_tcp = unsolicited_tcp->next;
      if(to_free){
        free(to_free);
      }


    }


    pthread_mutex_unlock(&(nat->lock));
  }
  return NULL;
}

void print_mapping(struct sr_nat_mapping *map){

	fprintf(stderr, "Internal IP : ");
	print_addr_ip_int(map->ip_int);
	fprintf(stderr, "External IP : ");
	print_addr_ip_int(map->ip_ext);

	printf("Internal Port : %" PRId16 "\n", map->aux_int);
	printf("Internal IP : %" PRId16 "\n", map->aux_ext);
}

void print_connection(struct sr_nat_connection *conns_mapping){
	fprintf(stderr, "%s\n", " -- Connection Overview --");

	fprintf(stderr, "%s %lu\n" ,"SYN from server",(unsigned long)conns_mapping->server_syn);
	fprintf(stderr, "%s %lu\n" ,"SYN from client",(unsigned long)conns_mapping->client_syn);

	fprintf(stderr, "%s %lu\n" ,"FIN from server",(unsigned long)conns_mapping->server_fin);
	fprintf(stderr, "%s %lu\n" ,"FIN from client",(unsigned long)conns_mapping->client_fin);

	fprintf(stderr, "%s %lu\n" ,"FIN ACK from server",(unsigned long)conns_mapping->server_fin_ack);
	fprintf(stderr, "%s %lu\n" ,"FIN ACK from client",(unsigned long)conns_mapping->client_fin_ack);



	fprintf(stderr, "%s\n", " -- End Connection Overview --");
}

/* Get the mapping associated with given external port.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
   uint16_t aux_ext, sr_nat_mapping_type type ) {



	pthread_mutex_lock(&(nat->lock));

	/* handle lookup here, malloc and assign to copy */
	struct sr_nat_mapping *copy = NULL;
	struct sr_nat_mapping *mappings = nat->mappings;
	while(mappings != NULL){
	if(mappings->aux_ext == aux_ext && mappings->type == type){
	  mappings->last_updated = time(NULL);
	  copy = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping));
	  memcpy(copy,mappings,sizeof(struct sr_nat_mapping));
	  break;
	}
	mappings = mappings->next;
	}

	pthread_mutex_unlock(&(nat->lock));
	return copy;
}


struct sr_nat_mapping *sr_nat_tcp_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type ) {

  /* handle lookup here, malloc and assign to copy */
	struct sr_nat_mapping *mappings = nat->mappings;
	while(mappings != NULL){
		if(mappings->aux_ext == aux_ext && mappings->type == type){
		  break;
		}
		mappings = mappings->next;
	}

	return mappings;
}

struct sr_nat_connection *sr_nat_lookup_connection(struct sr_nat_mapping *mapping,  uint32_t ip_int, uint16_t aux_int){
	struct sr_nat_connection *cur_connection;

	cur_connection = mapping->conns;
	while(cur_connection){
		fprintf(stderr, "in loop\n");

		if(cur_connection->server_ip == ip_int && cur_connection->server_port == aux_int){
			return cur_connection;
		}

		cur_connection = cur_connection->next;
	}
	fprintf(stderr, "%s\n", "NULL");
	return NULL;
}


/* Get the mapping associated with given internal (ip, port) pair.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  pthread_mutex_lock(&(nat->lock));

  /* handle lookup here, malloc and assign to copy. */
  struct sr_nat_mapping *copy = NULL;
  struct sr_nat_mapping *mappings = nat->mappings;
  while(mappings != NULL){
    if(mappings->ip_int == ip_int && mappings->aux_int == aux_int && mappings->type == type){
      mappings->last_updated = time(NULL);
      copy = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping));
      memcpy(copy,mappings,sizeof(struct sr_nat_mapping));
      break;
    }
    mappings = mappings->next;
  }

  pthread_mutex_unlock(&(nat->lock));
  return copy;
}


struct sr_nat_mapping *sr_nat_tcp_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  /* handle lookup here, malloc and assign to copy. */
  	struct sr_nat_mapping *mappings = nat->mappings;
	while(mappings != NULL){
		if(mappings->ip_int == ip_int && mappings->aux_int == aux_int && mappings->type == type){
			return mappings;
		}
		mappings = mappings->next;
	}

	return NULL;
}


/* Insert a new mapping into the nat's mapping table.
   Actually returns a copy to the new mapping, for thread safety.
 */
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat, uint32_t ip_int, uint32_t ip_ext, uint16_t aux_int, sr_nat_mapping_type type ) {
	pthread_mutex_lock(&(nat->lock));


	fprintf(stderr, "%s %d\n", "Inserting mapping : " , type);

	struct sr_if* ext_if = nat->ext_if;
	/* handle insert here, create a mapping, and then return a copy of it */
	struct sr_nat_mapping *mapping = NULL;
	struct sr_nat_mapping *copy = NULL;
	mapping = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping));
	mapping->aux_int = aux_int;
	mapping->aux_ext = htons(nat->available_port);
	mapping->ip_int = ip_int;

	mapping->ip_ext = ext_if->ip;
	mapping->last_updated = time(NULL);
	mapping->type = type;
	mapping->conns = NULL;


	nat->available_port = (nat->available_port + 1)%65535;
	if(nat->available_port < 1050){
	nat->available_port = 1050;
	}

	if(nat->mappings){
	nat->mappings->prev = mapping;
	}
	mapping->next = nat->mappings;
	mapping->prev = NULL;

	nat->mappings = mapping;


	copy = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping));
	memcpy(copy,mapping,sizeof(struct sr_nat_mapping));


	pthread_mutex_unlock(&(nat->lock));
	return copy;
}


