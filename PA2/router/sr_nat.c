
#include <signal.h>
#include <assert.h>
#include "sr_nat.h"
#include <unistd.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

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
  nat->unsolicited_tcp = NULL;
  /* Initialize any variables here */

  return success;
}

/*HELPERS*/

interface_map determine_ip_interface(struct sr_instance *sr, uint32_t ip_val){

  struct sr_if* nat_if = sr_get_ip_interface(sr,ip_val);
  if(nat_if){
    return nat_if;
  }

  struct sr_rt* nat_rt  = sr_get_routing_entry(sr, ip_val, NULL);
  if(nat_rt){
    if((strcmp(nat_rt->interface,"eth2",4) == 0) || (strcmp(nat_rt->interface,"eth3",4) == 0)){
      return external_if;
    } else{
      return internal_if;
    }

  }
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
  packet_direction = determine_packet_direction(src_if,dst_if);
  return packet_direction;

}

uint16_t calclate_tcp_cksum(uint8_t *tcp_pack, sr_ip_hdr_t *ip_pack, unsigned int tcp_len){

  uint8_t temp = (uint8_t *)malloc(sizeof(sr_tcp_cksum_hdr_t) + tcp_en);

  sr_tcp_cksum_hdr_t *tcp_hdr_cksum = (sr_tcp_cksum_hdr_t *)temp;
  tcp_hdr_cksum->ip_p = ip_pack->ip_p;
  tcp_hdr_cksum->ip_dst = ip_pack->ip_dst;
  tcp_hdr_cksum->ip_src = ip_pack->ip_src;
  tcp_hdr_cksum->len = htons(len);
  memcpy(temp + sizeof(sr_tcp_cksum_hdr_t), tcp_pack, tcp_len);

  return cksum(temp,sizeof(sr_tcp_cksum_hdr_t) + tcp_en);
}


int handle_packet_protocol(uint8_t *packet, sr_nat_mapping *mapping, uint16_t *port_val){

 sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;

 if(ip_header->ip_p == ip_protocol_icmp){




 }



}


int sr_nat_destroy(struct sr_nat *nat) {  /* Destroys the nat (free memory) */

  pthread_mutex_lock(&(nat->lock));

  /* free nat memory here */

  pthread_kill(nat->thread, SIGKILL);
  return pthread_mutex_destroy(&(nat->lock)) &&
    pthread_mutexattr_destroy(&(nat->attr));

}


int transform_inbound_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len){


  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;


  sr_nat_mapping_type packet_mapping_type;
  sr_nat_mapping packet_mapping  = NULL;
  uint16_t port_val;
  int valid_protocol = 1;

  if(ip_header->ip_p == ip_protocol_icmp){
    packet_mapping_type = nat_mapping_icmp;
    sr_icmp_nat_hdr_t *icmp_hdr = (sr_icmp_nat_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
    port_val = icmp_hdr->icmp_id;

  } else if(ip_header->ip_p = ip_protocol_tcp){
    packet_mapping_type = nat_mapping_tcp;
    sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
    port_val = tcp_hdr->dst_port;

  } else{
    valid_protocol = 0;
  }

  if(valid_protocol){
    packet_mapping = sr_nat_lookup_external(&(sr->nat), port_val,packet_mapping_type);
  } else{
    return DROP_PACKET;
  }

  if(!packet_mapping){
    if(ip_header->ip_p == ip_protocol_icmp){
      //DROP IT.
    } else if(ip_header->ip_p == ip_protocol_tcp){
        sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
        //Check its SYN
        if(tcp_hdr->falgs & TCP_SYN_FLAG){
          struct sr_unsolicited_tcp *tcps = sr->nat->unsolicited_tcp;
          //Check if already in queue.
          while(tcps != NULL){
            if(tcps->src_ip == ip_header->ip_src && tcps->port_val_ext == port_val){
              break;
            }
            tcps = tcps->next;
          }

          if(tcps == NULL){
            //Insert the unsolicited SYN.
            struct sr_unsolicited_tcp *new_tcp = (struct sr_unsolicited_tcp *)malloc(sizeof(struct sr_unsolicited_tcp));
            new_tcp->port_val_ext = port_val;
            memcpy(new_tcp->ip_data,ip_header,28);
            new_tcp->src_ip = ip_header->ip_src;
            new_tcp->arrival_time = time(NULL);

            if(sr->nat->unsolicited_tcp){
              sr->nat->unsolicited_tcp->prev = new_tcp;
            }
            new_tcp->prev = NULL;
            new_tcp->next = sr->nat->unsolicited_tcp;
            sr->nat->unsolicited_tcp = new_tcp;


          }

        }
    }

    return DROP_PACKET; 
  }



  if(ip_hdr->ip_p == ip_protocol_tcp){
    /*Update TCP connections HERE. */
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
    tcp_hdr->cksum = calclate_tcp_cksum(tcp_hdr, ip_header, len-sizeof(sr_ip_hdr_t));


  }

  return PACKET_FINE;

  
}

int transform_outbound_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len){


  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;


  sr_nat_mapping_type packet_mapping_type;
  sr_nat_mapping packet_mapping  = NULL;
  uint16_t port_val;
  int valid_protocol = 1;

  if(ip_header->ip_p == ip_protocol_icmp){
    packet_mapping_type = nat_mapping_icmp;
    sr_icmp_nat_hdr_t *icmp_hdr = (sr_icmp_nat_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
    port_val = icmp_hdr->icmp_id;

  } else if(ip_header->ip_p = ip_protocol_tcp){
    packet_mapping_type = nat_mapping_tcp;
    sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
    port_val = tcp_hdr->src_port;

  } else{
    valid_protocol = 0;
  }

  if(valid_protocol){
    packet_mapping = sr_nat_lookup_internal(&(sr->nat), port_val,packet_mapping_type);
  } else{
    return DROP_PACKET;
  }

  if(!packet_mapping){
    /*create mapping. */
    if(ip_header->ip_p = ip_protocol_tcp){
      sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(packet + sizeof(sr_ip_hdr_t));
      if(!(tcp_hdr->flags & TCP_SYN_FLAG)){
        return DROP_PACKET:
      }

      uint32_t ip_int = ip_hdr->ip_src;
      uint16_t aux_int = port_val;

      

      packet_mapping = sr_nat_insert_mapping(&(sr->nat),ip_int, aux_int, packet_mapping_type);

    }
    
    /*if still null return DROP_PACKET;*/

  }

  if(ip_hdr->ip_p == ip_protocol_tcp){
    /*Update TCP connections HERE. */
  }

  /*REWRITE THE PACKET using packet mapping.*/
  struct sr_if* src_if = sr_get_interface(sr, "eth1");

  ip_header->ip_dst = packet_mapping->ip_ext;
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
    tcp_hdr->cksum = calclate_tcp_cksum(tcp_hdr, ip_header, len-sizeof(sr_ip_hdr_t));


  }


  return PACKET_FINE;


  
}



int transform_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len){


  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t*)packet;

  
  packet_direction packet_dir =  calclate_packet_direction(sr,packet);

  sr_nat_mapping_type packet_mapping_type;
  sr_nat_mapping packet_mapping  = NULL;
  uint16_t port_val;
  int valid_protocol = 1;

  if(packet_direction == inbound_packet){
    return transform_inbound_packet(sr,packet,len);
  } else if(packet_direction == outbound_packet){
    return transform_outbound_packet(sr,packet,len);
  } else{
    /*We let the confused packet figure out its way. :) */
    return PACKET_FINE;
  }



  return PACKET_FINE;







}






void *sr_nat_timeout(void *nat_ptr) {  /* Periodic Timout handling */
  struct sr_nat *nat = (struct sr_nat *)nat_ptr;
  while (1) {
    sleep(1.0);
    pthread_mutex_lock(&(nat->lock));

    time_t curtime = time(NULL);

    /* handle periodic tasks here */

    pthread_mutex_unlock(&(nat->lock));
  }
  return NULL;
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

/* Insert a new mapping into the nat's mapping table.
   Actually returns a copy to the new mapping, for thread safety.
 */
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  pthread_mutex_lock(&(nat->lock));

   struct sr_if* ext_if = sr_get_interface(sr, "eth2");
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
  if(nat->available_port < 1024){
    nat->available_port = 1024;
  }

  if(nat->mappings){
    nat->mappings->prev = mapping;
  }
  mapping->next = nat->mappings;
  mapping->prev = NULL:

  copy = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping));
  memcpy(copy,mapping,sizeof(struct sr_nat_mapping));


  pthread_mutex_unlock(&(nat->lock));
  return copy;
}
