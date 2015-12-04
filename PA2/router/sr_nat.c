
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

void tcp_inbound(struct sr_instance *sr, uint8_t *packet);
void tcp_outbound(struct sr_instance *sr, uint8_t *packet);
struct sr_nat_mapping *sr_nat_tcp_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type );

struct sr_nat_connection *sr_nat_lookup_connection(struct sr_nat_mapping *mapping,  uint32_t ip_int, uint16_t aux_int);

struct sr_nat_mapping *sr_nat_tcp_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );

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


int transform_inbound_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len){
	printf("--------------Before---------------\n");
	sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;

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
		printf("Valid Protocol\n");
		packet_mapping = sr_nat_lookup_external(&(sr->nat), port_val,packet_mapping_type);
	} else{
		return DROP_PACKET;
	}

	if(!packet_mapping){
		if(ip_header->ip_p == ip_protocol_icmp){
			printf("Drop Packet\n");
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
					fprintf(stderr, "New tcps inserted\n");
					struct sr_unsolicited_tcp *new_tcp = (struct sr_unsolicited_tcp *)malloc(sizeof(struct sr_unsolicited_tcp));
					new_tcp->port_val_ext = port_val;
					memcpy(new_tcp->ip_data,ip_header,28);
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

		return DROP_PACKET; 
	}



  if(ip_header->ip_p == ip_protocol_tcp){
    	tcp_inbound(sr, packet);
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

  return PACKET_FINE;
}

void tcp_inbound(struct sr_instance *sr, uint8_t *packet){
	struct sr_nat *nat  = &(sr->nat);
	sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
	sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;
	struct sr_nat_mapping *packet_mapping  = NULL;
	struct sr_nat_connection *conns_mapping = NULL;

	pthread_mutex_lock(&(nat->lock));

	packet_mapping = sr_nat_tcp_lookup_external(&(sr->nat), tcp_hdr->dst_port, nat_mapping_tcp);
	if(packet_mapping == NULL){
		pthread_mutex_unlock(&(nat->lock));
		return;
	}
	fprintf(stderr, "Shouldn''t affect anything!!!\n");
	struct sr_unsolicited_tcp *tcps = sr->nat.unsolicited_tcp;
	/*Check if already in queue.*/
	while(tcps != NULL){

		if(tcps->src_ip == ip_header->ip_src && tcps->port_val_ext == tcp_hdr->src_port){
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
	}

	conns_mapping = sr_nat_lookup_connection(packet_mapping,  ip_header->ip_src, tcp_hdr->src_port);
	
	if(!conns_mapping){
		struct sr_nat_connection *new_conns = (struct sr_nat_connection *)malloc(sizeof(struct sr_nat_connection));
		new_conns->server_syn = 0;
		new_conns->client_syn = 0;

		new_conns->server_fin = 0;
		new_conns->client_fin = 0;

		new_conns->syn_ack = 0;
		new_conns->fin_ack = 0;

		new_conns->prev = NULL;
	  	new_conns->next = NULL;

	  	new_conns->server_ip = ip_header->ip_src;
	  	new_conns->server_port = tcp_hdr->src_port;		

	  	if(packet_mapping->conns){
	  		packet_mapping->conns->prev = new_conns;
	  		new_conns->next = packet_mapping->conns;
	  		packet_mapping->conns = new_conns;
	  	}else{
	  		packet_mapping->conns = new_conns;
	  	}
	}

	if(tcp_hdr->flags & TCP_SYN_FLAG){
		conns_mapping->server_syn = 1;
	}
	else if(tcp_hdr->flags & TCP_FIN_FLAG){
		conns_mapping->server_fin = 1;
	}

	if(conns_mapping->server_fin & conns_mapping->client_fin){
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

int transform_outbound_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len){


  	sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;
  	printf("--------------Before---------------\n");

	sr_nat_mapping_type packet_mapping_type;
	struct sr_nat_mapping *packet_mapping  = NULL;
	uint16_t port_val;
	int valid_protocol = 1;

	if(ip_header->ip_p == ip_protocol_icmp){
		printf("It is an ICMP Protocol\n");
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
		printf("Mapping is not found\n");
		/*create mapping. */
		if(ip_header->ip_p == ip_protocol_tcp){
			printf("Initial TCP record\n");
			sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
			if(!(tcp_hdr->flags & TCP_SYN_FLAG)){
				return DROP_PACKET;
			}
		}

		uint32_t ip_int = ip_header->ip_src;
		uint32_t ip_ext = ip_header->ip_dst;
		uint16_t aux_int = port_val;

		packet_mapping = sr_nat_insert_mapping(&(sr->nat),ip_int, ip_ext, aux_int, packet_mapping_type);
		printf("Mapping created\n");

	/*if still null return DROP_PACKET;*/
	}

	if(ip_header->ip_p == ip_protocol_tcp){
		/*Update TCP connections HERE. */
		printf("tcp outbound funnction called\n");
		tcp_outbound(sr, packet);
		printf("tcp outbound funnction ended\n");
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
		tcp_hdr->dst_port = packet_mapping->aux_ext;
		tcp_hdr->cksum = 0;
		tcp_hdr->cksum = calclate_tcp_cksum((uint8_t *)tcp_hdr, ip_header, len-sizeof(sr_ip_hdr_t));
	}

	print_hdr_ip(packet);
	printf("----Done Trasform-------\n");

  	return PACKET_FINE;
}


void tcp_outbound(struct sr_instance *sr, uint8_t *packet){
	struct sr_nat *nat  = &(sr->nat);

	sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
	sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;
	struct sr_nat_mapping *packet_mapping  = NULL;
	struct sr_nat_connection *conns_mapping = NULL;

	pthread_mutex_lock(&(nat->lock));
	fprintf(stderr, "sr_nat_tcp_lookup_internal called\n" );
	packet_mapping = sr_nat_tcp_lookup_internal(&(sr->nat), ip_header->ip_src, tcp_hdr->src_port, nat_mapping_tcp);
	
	if(packet_mapping == NULL){
		pthread_mutex_unlock(&(nat->lock));
		return;
	}


	struct sr_unsolicited_tcp *tcps = sr->nat.unsolicited_tcp;
	/*Check if already in queue.*/
	fprintf(stderr, "going through unsolicited tcps\n");
	while(tcps != NULL){
		fprintf(stderr, "TCPS IP is :");
		print_addr_ip_int(tcps->src_ip);
		fprintf(stderr, "Packet destination ip :");
		print_addr_ip_int(ip_header->ip_dst);
		
		/*if(tcps->src_ip == ip_header->ip_dst && tcps->port_val_ext == tcp_hdr->dst_port){
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
		}*/
		
		tcps = tcps->next;

		if(tcps == NULL){
			fprintf(stderr, "This is insane");
		}
	}

	fprintf(stderr, "sr_nat_lookup_connection called\n");
	conns_mapping = sr_nat_lookup_connection(packet_mapping,  ip_header->ip_dst, tcp_hdr->dst_port);
	
	if(!conns_mapping){
		struct sr_nat_connection *new_conns = (struct sr_nat_connection *)malloc(sizeof(struct sr_nat_connection));
		new_conns->server_syn = 0;
		new_conns->client_syn = 0;

		new_conns->server_fin = 0;
		new_conns->client_fin = 0;

		new_conns->syn_ack = 0;
		new_conns->fin_ack = 0;

		new_conns->prev = NULL;
	  	new_conns->next = NULL;

	  	new_conns->server_ip = ip_header->ip_dst;
	  	new_conns->server_port = tcp_hdr->dst_port;

	  	if(packet_mapping->conns){
	  		packet_mapping->conns->prev = new_conns;
	  		new_conns->next = packet_mapping->conns;
	  		packet_mapping->conns = new_conns;
	  	}else{
	  		packet_mapping->conns = new_conns;
	  	}
	}

	if(tcp_hdr->flags & TCP_SYN_FLAG){
		conns_mapping->client_syn = 1;
	}
	else if(tcp_hdr->flags & TCP_FIN_FLAG){
		conns_mapping->client_fin = 1;
	}

	if(conns_mapping->server_fin & conns_mapping->client_fin){
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

	printf("Transforming packet\n");

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

/* Get the mapping associated with given external port.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type ) {

  pthread_mutex_lock(&(nat->lock));

  /* handle lookup here, malloc and assign to copy */
  struct sr_nat_mapping *copy = NULL;
  struct sr_nat_mapping *mappings = nat->mappings;
  while(mappings != NULL){
  	printf("--Mapping---\n");
  	print_mapping(mappings);
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
			printf("--Mapping---\n");
			print_mapping(mappings);
		if(mappings->aux_ext == aux_ext){
		  mappings->last_updated = time(NULL);
		  break;
		}
		mappings = mappings->next;
	}

	return mappings;
}

struct sr_nat_connection *sr_nat_lookup_connection(struct sr_nat_mapping *mapping,  uint32_t ip_int, uint16_t aux_int){
	struct sr_nat_connection *cur_connection = mapping->conns;

	while(cur_connection != NULL){
		if(cur_connection->server_ip == ip_int && cur_connection->server_port == aux_int){
			break;
		}

		cur_connection = cur_connection->next;
	}

	return cur_connection;
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
		if(mappings->ip_int == ip_int && mappings->aux_int == aux_int && mappings->type){
			mappings->last_updated = time(NULL);
		    break;
		}
		mappings = mappings->next;
	}

	return mappings;
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

	print_mapping(mapping);

	copy = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping));
	memcpy(copy,mapping,sizeof(struct sr_nat_mapping));


	pthread_mutex_unlock(&(nat->lock));
	return copy;
}
