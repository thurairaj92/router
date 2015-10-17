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


 void create_arp_header(uint8_t *packet, uint8_t *in_sha, uint8_t *out_sha, uint32_t in_ip, uint32_t out_ip, uint16_t op_code);
 void create_ethernet_header(uint8_t *packet, uint8_t *out_host, uint8_t *in_host, uint16_t eth_type);

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

  printf("*** -> Received packet of length %d \n",len);

  /* fill in code here */
  struct sr_if* ethernet_interface = sr_get_interface(sr, interface);

  /* Fetch Interface from instance of sr for further processing. */
  if(!ethernet_interface){
    printf("Received Invalid Packet Ethernet Interface. Error");
  }

  sr_ethernet_hdr_t *ethernet_header = (sr_ethernet_hdr_t *) packet;

  /* convert network byte order to host byte order. */
  uint16_t converted_ether_type_val = ntohs(ethernet_header->ether_type);

  if(converted_ether_type_val == ethertype_arp){
    printf("Received ARP packet. \n");
    sr_arp_hdr_t *arp_header =  (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
    
    print_hdrs(packet, len);

    unsigned short converted_arp_op_val = ntohs(arp_header->ar_op);
    switch(converted_arp_op_val){
      case arp_op_request:
        printf("Received ARP Request.\n");

        /*Find out if target Ip addr is one of our router's addresses. */
        struct sr_if* target_ip_interface = sr_get_ip_interface(sr, arp_header->ar_tip);
        if(target_ip_interface != 0){
          printf("Target IP of ARP request in the router. \n");
        
          /*Create Packet with arp and ethernet header. */
          unsigned int reply_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
          uint8_t *reply_packet = malloc(reply_len);
          create_arp_header(reply_packet, target_ip_interface->addr, arp_header->ar_sha, target_ip_interface->ip, arp_header->ar_sip, arp_op_reply);
          create_ethernet_header(reply_packet, ethernet_header->ether_shost,  target_ip_interface->addr,  ethertype_arp);

          /* DEBUG FUNCTIONS remove Later. */
          printf("ARP REPLY Created: \n");
          print_hdrs(reply_packet, reply_len);

          if(sr_send_packet(sr, reply_packet, len, target_ip_interface->name) == 0){
            printf("Packet Sent successflly. \n");
          } else {
            printf("Failed to send the packet.\n");
          }

        } else{
          printf("Target IP not for Router.\n");
        }


        break;

      case arp_op_reply:
        printf("Received ARP Reply.\n");
        break;

      default:
        printf("No match of arp op code.\n");
    }



  } else if (converted_ether_type_val == ethertype_ip){
    printf("Received IP Packet. \n");
  }

}/* end sr_ForwardPacket */

 void create_arp_header(uint8_t *packet, uint8_t *in_sha, uint8_t *out_sha, uint32_t in_ip, uint32_t out_ip, uint16_t op_code){
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

 void create_ethernet_header(uint8_t *packet, uint8_t *out_host, uint8_t *in_host, uint16_t eth_type){
    sr_ethernet_hdr_t *reply_ethernet_header = (sr_ethernet_hdr_t *) packet;

    reply_ethernet_header->ether_type = htons(eth_type);
    memcpy(reply_ethernet_header->ether_dhost, out_host, sizeof(uint8_t) * ETHER_ADDR_LEN);
    memcpy(reply_ethernet_header->ether_shost, in_host, sizeof(uint8_t) * ETHER_ADDR_LEN);
       
    
 }

