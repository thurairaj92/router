
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
  nat->available_port = 1024;
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
		if((strcmp(nat_rt->interface,"eth2") == 0) || (strcmp(nat_rt->interface,"eth3") == 0)){
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

    struct sr_nat_mapping *to_free = NULL:
    while(mapping_list != NULL){
      double time_diff = difftime(present,mapping_list->last_updated);

      if(mapping_list->type == nat_mapping_icmp){
        if(time_diff >= ICMP_TIMEOUT){
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
          if(conns->server_fin && conns->client_fin){
             connection_type = 1;
          }

          double time_diff = difftime(present,conns->last_updated);
          int connection_remove = 0;

          if(connection_type == 0){
            if(time_diff > TCP_TRANSITION_TIMEOUT){
              connection_remove = 1;
            }
          } else {
            if(time_diff > TCP_DEFAULT_TIMEOUT){
              connection_remove = 1;
            }
          }

          struct sr_nat_connection *to_free = NULL:
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
		    return DROP_PACKET; 
		} 

		else if(ip_header->ip_p == ip_protocol_tcp){
		    sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
		    /*Check its SYN*/
		    if(tcp_hdr->flags & TCP_SYN_FLAG){
				struct sr_unsolicited_tcp *tcps = sr->nat.unsolicited_tcp;
				/*Check if already in queue.*/
				while(tcps != NULL){

					if(tcps->src_ip == ip_header->ip_src && tcps->port_val_ext == port_val){
						break;
					}
					tcps = tcps->next;
				}

				if(tcps == NULL){
					/*Insert the unsolicited SYN.*/
					struct sr_unsolicited_tcp *new_tcp = (struct sr_unsolicited_tcp *)malloc(sizeof(struct sr_unsolicited_tcp));
					new_tcp->port_val_ext = port_val;
					new_tcp->src_ip = ip_header->ip_src;
					new_tcp->arrival_time = time(NULL);

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
	struct sr_if* src_if = sr_get_interface(sr, "eth1");

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

	fprintf(stderr, "sr_nat_tcp_lookup_internal called\n" );

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

	struct sr_unsolicited_tcp *tcps = sr->nat.unsolicited_tcp;
	/*Check if already in queue.*/
	fprintf(stderr, "going through unsolicited tcps\n");


	/*while(tcps){
		fprintf(stderr, "%s\n", "---------START------------");		
		if(tcps){
			fprintf(stderr, "%s\n", "not null");
		}else{
			fprintf(stderr, "%s\n", "this is dumb");
		}
		fprintf(stderr, "%ld\n", tcps->arrival_time);
		fprintf(stderr, "time printed\n");
		print_addr_ip_normal(tcps->src_ip);
		fprintf(stderr, "ip printed\n");
		fprintf(stderr, "%d\n" , tcps->port_val_ext);
		fprintf(stderr, "port printed\n");
		
		if(tcps->src_ip == server_ip && tcps->port_val_ext == server_port){
			fprintf(stderr, "Match found\n");
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
	*/

	fprintf(stderr, "sr_nat_lookup_connection called\n");
	conns_mapping = sr_nat_lookup_connection(packet_mapping,  server_ip, server_port);
	
	if(conns_mapping){
		fprintf(stderr, "%s\n", "really? how does that make sense");
	}else{
		fprintf(stderr, "%s\n", "Ok then problem is somewhere elese");
	}

	if(!conns_mapping){
		fprintf(stderr, "%s\n", "New connections");
		struct sr_nat_connection *new_conns = (struct sr_nat_connection *)malloc(sizeof(struct sr_nat_connection));
		new_conns->server_syn = 0;
		new_conns->client_syn = 0;

		new_conns->server_fin = 0;
		new_conns->client_fin = 0;

		new_conns->syn_ack = 0;
		new_conns->fin_ack = 0;

		new_conns->fin_ack_seq = -1;
		new_conns->fin_last_ack = -1;

		new_conns->prev = NULL;
	  	new_conns->next = NULL;

	  	new_conns->server_ip = server_ip;
	  	new_conns->server_port = server_port;

	  	if(packet_mapping->conns){
	  		fprintf(stderr, "%s\n", "Conns is not emplty");
	  		packet_mapping->conns->prev = new_conns;
	  		new_conns->next = packet_mapping->conns;
	  		packet_mapping->conns = new_conns;
	  	}else{
	  		fprintf(stderr, "%s\n", "Conns are emplty");
	  		packet_mapping->conns = new_conns;
	  		fprintf(stderr, "%s\n", "Conns no longer emplty");
	  	}
	  	conns_mapping = new_conns;
	}

	fprintf(stderr, "%s\n", "TCP flags");
	if(tcp_hdr->flags & TCP_SYN_FLAG){
		if(packet_dir == inbound_packet){
			conns_mapping->server_syn = 1;
		}else if(packet_dir == outbound_packet){
			conns_mapping->client_syn = 1;
		}
	}
	else if(tcp_hdr->flags & TCP_FIN_FLAG){
		if(packet_dir == inbound_packet){
			conns_mapping->server_fin = 1;
		}else if(packet_dir == outbound_packet){
			conns_mapping->client_fin = 1;
		}

		if(tcp_hdr->flags & TCP_ACK_FLAG){
			new_conns->fin_ack_seq = tcp_hdr->seq;
		}
	}

	print_connection(conns_mapping); 

	fprintf(stderr, "%s\n", "check fin acknowledgement");
	if(conns_mapping->server_fin + conns_mapping->client_fin > 1){
		fprintf(stderr, "%s\n", "fin acknowledged?? how?");
		if(conns_mapping->prev){
			conns_mapping->prev->next = conns_mapping->next;	
		}else{
			packet_mapping->conns = conns_mapping->next;
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

    //*Add Unsolicited Timeout.
    struct sr_unsolicited_tcp *unsolicited_tcp = nat->unsolicited_tcp;
    while(unsolicited_tcp != NULL){
      double time_diff = difftime(present,unsolicited_tcp->arrival_time)
      struct sr_unsolicited_tcp *to_free = NULL;

      if(time_diff > 6){
        //Send ICMP error.





        
         if(unsolicited_tcp->next){
            unsolicited_tcp->next->prev = unsolicited_tcp->prev;
          }
          if(unsolicited_tcp->prev){
            unsolicited_tcp->prev->next = unsolicited_tcp->next;
          } else{
            unsolicited_tcp->conns = unsolicited_tcp->next;
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

	fprintf(stderr, "%s %d\n" ,"SYN from server",conns_mapping->server_syn);
	fprintf(stderr, "%s %d\n" ,"SYN from client",conns_mapping->client_syn);

	fprintf(stderr, "%s %d\n" ,"FIN from server",conns_mapping->server_fin);
	fprintf(stderr, "%s %d\n" ,"FIN from client",conns_mapping->client_fin);


	fprintf(stderr, "%s\n", " -- End Connection Overview --");
}

/* Get the mapping associated with given external port.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
   uint16_t aux_ext, sr_nat_mapping_type type ) {

	fprintf(stderr, "Port from Server : %u\n", (unsigned int)aux_ext);


	pthread_mutex_lock(&(nat->lock));

	/* handle lookup here, malloc and assign to copy */
	struct sr_nat_mapping *copy = NULL;
	struct sr_nat_mapping *mappings = nat->mappings;
	while(mappings != NULL){
	if(mappings->aux_ext == aux_ext){
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
		if(mappings->aux_ext == aux_ext){
		  //mappings->last_updated = time(NULL);
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
    if(mappings->ip_int == ip_int && mappings->aux_int == aux_int){
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
		fprintf(stderr, "Mapping\n");
		if(mappings->ip_int == ip_int && mappings->aux_int == aux_int && mappings->type){
			fprintf(stderr, "Mapping found\n");
			//mappings->last_updated = time(NULL);
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
	fprintf(stderr, "Insert Mapping\n");
	pthread_mutex_lock(&(nat->lock));

	struct sr_if* ext_if = nat->ext_if;
	/* handle insert here, create a mapping, and then return a copy of it */
	struct sr_nat_mapping *mapping = NULL;
	struct sr_nat_mapping *copy = NULL;
	mapping = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping));
	mapping->aux_int = aux_int;
	mapping->aux_ext = htons(nat->available_port);
	mapping->ip_int = ip_int;
	mapping->ip_ext = ext_if->ip;
	/*mapping->ip_ext = ip_ext;*/
	mapping->last_updated = time(NULL);
	mapping->type = type;
	mapping->conns = NULL;

	nat->available_port = (nat->available_port + 1)%65535;
	if(nat->available_port < 1024){
	nat->available_port = 1024;
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


