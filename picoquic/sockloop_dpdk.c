#ifdef _WINDOWS
#define WIN32_LEAN_AND_MEAN

#include <WinSock2.h>
#include <Windows.h>
#include <assert.h>
#include <iphlpapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ws2tcpip.h>

#ifndef SOCKET_TYPE
#define SOCKET_TYPE SOCKET
#endif
#ifndef SOCKET_CLOSE
#define SOCKET_CLOSE(x) closesocket(x)
#endif
#ifndef WSA_LAST_ERROR
#define WSA_LAST_ERROR(x) WSAGetLastError()
#endif
#ifndef socklen_t
#define socklen_t int
#endif

#else /* Linux */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef __USE_XOPEN2K
#define __USE_XOPEN2K
#endif
#ifndef __USE_POSIX
#define __USE_POSIX
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>

#ifndef SOCKET_TYPE
#define SOCKET_TYPE int
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_CLOSE
#define SOCKET_CLOSE(x) close(x)
#endif
#ifndef WSA_LAST_ERROR
#define WSA_LAST_ERROR(x) ((long)(x))
#endif
#endif

#include "picosocks.h"
#include "picoquic.h"
#include "picoquic_internal.h"
#include "picoquic_packet_loop.h"
#include "picoquic_unified_log.h"
#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_errno.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_timer.h>
#include <rte_power.h>
#include <rte_eal.h>
#include <rte_spinlock.h>
#include <rte_version.h>

// DPDK
#define _DPDK
#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
#define MEMPOOL_CACHE_SIZE 256
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024
#define IP_DEFTTL 64
#define IP_MAC_ARRAYS_LENGTH 20

struct lcore_queue_conf
{
    unsigned n_rx_port;
    unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

#if defined(_WINDOWS)
static int udp_gso_available = 0;
#else
#if defined(UDP_SEGMENT)
static int udp_gso_available = 1;
#else
static int udp_gso_available = 0;
#endif
#endif

void setup_pkt_udp_ip_headers_test(struct rte_ipv4_hdr *ip_hdr,
                                   struct rte_udp_hdr *udp_hdr,
                                   uint16_t pkt_data_len)
{

    uint32_t tx_ip_src_addr = (198U << 24) | (18 << 16) | (0 << 8) | 1;
    uint32_t tx_ip_dst_addr = (198U << 24) | (18 << 16) | (0 << 8) | 2;

    uint16_t tx_udp_src_port = 55;
    uint16_t tx_udp_dst_port = 55;

    uint16_t *ptr16;
    uint32_t ip_cksum;
    uint16_t pkt_len;

    // printf("====================clean===============\n");
    // printf("src_adr %u\n",rte_cpu_to_be_32(tx_ip_src_addr));
    // printf("dst_adr %u\n",rte_cpu_to_be_32(tx_ip_dst_addr));
    // printf("src_port %zu\n",rte_cpu_to_be_16(tx_udp_src_port));
    // printf("dst_port %zu\n",rte_cpu_to_be_16(tx_udp_dst_port));
    /*
     * Initialize UDP header.
     */
    pkt_len = (uint16_t)(pkt_data_len + sizeof(struct rte_udp_hdr));
    udp_hdr->src_port = rte_cpu_to_be_16(tx_udp_src_port);
    udp_hdr->dst_port = rte_cpu_to_be_16(tx_udp_dst_port);
    udp_hdr->dgram_len = rte_cpu_to_be_16(pkt_len);
    udp_hdr->dgram_cksum = 0; /* No UDP checksum. */

    /*
     * Initialize IP header.
     */
    pkt_len = (uint16_t)(pkt_len + sizeof(struct rte_ipv4_hdr));
    ip_hdr->version_ihl = RTE_IPV4_VHL_DEF;
    ip_hdr->type_of_service = 0;
    ip_hdr->fragment_offset = 0;
    ip_hdr->time_to_live = IP_DEFTTL;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->packet_id = 0;
    ip_hdr->total_length = rte_cpu_to_be_16(pkt_len);
    ip_hdr->src_addr = rte_cpu_to_be_32(tx_ip_src_addr);
    ip_hdr->dst_addr = rte_cpu_to_be_32(tx_ip_dst_addr);

    /*
     * Compute IP header checksum.
     */
    ptr16 = (unaligned_uint16_t *)ip_hdr;
    ip_cksum = 0;
    ip_cksum += ptr16[0];
    ip_cksum += ptr16[1];
    ip_cksum += ptr16[2];
    ip_cksum += ptr16[3];
    ip_cksum += ptr16[4];
    ip_cksum += ptr16[6];
    ip_cksum += ptr16[7];
    ip_cksum += ptr16[8];
    ip_cksum += ptr16[9];

    /*
     * Reduce 32 bit checksum to 16 bits and complement it.
     */
    ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
               (ip_cksum & 0x0000FFFF);
    if (ip_cksum > 65535)
        ip_cksum -= 65535;
    ip_cksum = (~ip_cksum) & 0x0000FFFF;
    if (ip_cksum == 0)
        ip_cksum = 0xFFFF;
    ip_hdr->hdr_checksum = (uint16_t)ip_cksum;
}

struct rte_ether_addr find_mac_from_ip(uint32_t ip_addr, uint32_t *ip_addresses, struct rte_ether_addr *mac_addresses, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (ip_addresses[i] == ip_addr)
        {
            return mac_addresses[i];
        }
    }
}

int add_mac_ip_pair(uint32_t ip_addr, struct rte_ether_addr mac_addr, uint32_t *ip_addresses, struct rte_ether_addr *mac_addresses, int length)
{
    // printf("ip_addr : %u\n",ip_addr);
    for (int i = 0; i < length; i++)
    {
        if (ip_addresses[i] == ip_addr)
        {
            // printf("i : %d\n",i);
            // printf("already present\n");
            return 0;
        }
        if (ip_addresses[i] == 0)
        {
            // printf("ip : %u\n",ip_addr);
            // printf("mac : %x:%x:%x:%x:%x:%x\n", mac_addr.addr_bytes[0], mac_addr.addr_bytes[1], mac_addr.addr_bytes[2], mac_addr.addr_bytes[3], mac_addr.addr_bytes[4], mac_addr.addr_bytes[5]);
            ip_addresses[i] = ip_addr;
            mac_addresses[i] = mac_addr;
            return 0;
        }
    }
    return -1;
}

int picoquic_packet_loop_dpdk(picoquic_quic_t *quic,
                              int local_port,
                              int local_af,
                              int dest_if,
                              int socket_buffer_size,
                              int do_not_use_gso,
                              picoquic_packet_loop_cb_fn loop_callback,
                              void *loop_callback_ctx,
                              int *is_running,
                              unsigned portid,
                              unsigned queueid,
                              int batching_size,
                              struct sockaddr_storage my_addr,
                              struct rte_ether_addr *my_mac,
                              struct rte_ether_addr *peer_mac,
                              struct rte_mempool *mb_pool,
                              struct rte_eth_dev_tx_buffer *tx_buffer)
{
    //===================DPDK==========================//
    // printf("queueid loop: %u\n",queueid);
    uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
    uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;
    struct rte_ether_addr eth_addr;
    int MAX_PKT_BURST = batching_size;
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    struct lcore_queue_conf *qconf;
    int ret;
    struct rte_eth_rxconf rxq_conf;
    struct rte_eth_txconf txq_conf;
    struct rte_eth_dev_info dev_info;
    void *tmp;
    ret = rte_eth_macaddr_get(portid, &eth_addr);
    struct sockaddr_in *sin = (struct sockaddr_in *)&my_addr;
    unsigned char *ip = (unsigned char *)&sin->sin_addr.s_addr; 

   //===================DPDK==========================//
    uint64_t current_time = picoquic_get_quic_time(quic);
    int64_t delay_max = 10000000;
    struct sockaddr_storage addr_from;
    struct sockaddr_storage addr_to;

    // handling packets
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
    struct rte_mbuf *m;
    int udp_payload_offset = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);
    // addresses
    rte_be32_t src_addr;
    rte_be32_t dst_addr;
    rte_be16_t src_port;
    rte_be16_t dst_port;

    int if_index_to;
    uint8_t buffer[1536];
    uint8_t *send_buffer = NULL;
    size_t send_length = 0;
    size_t send_msg_size = 0;
    size_t send_buffer_size = 1536;
    size_t *send_msg_ptr = NULL;
    int bytes_recv;
    picoquic_connection_id_t log_cid;
    SOCKET_TYPE s_socket[PICOQUIC_PACKET_LOOP_SOCKETS_MAX];
    int sock_af[PICOQUIC_PACKET_LOOP_SOCKETS_MAX];
    uint16_t sock_ports[PICOQUIC_PACKET_LOOP_SOCKETS_MAX];
    int nb_sockets = 0;
    int testing_migration = 0; /* Hook for the migration test */
    uint16_t next_port = 0;    /* Data for the migration test */
    picoquic_cnx_t *last_cnx = NULL;
    int loop_immediate = 0;
    picoquic_packet_loop_options_t options = { 0 };

    int pkts_recv;
    // debugging
    FILE *fptr_send;
    FILE *fptr_rcv;
    int receiv_counter = 0;
    int send_counter = 0;
    
    uint32_t ip_addresses[IP_MAC_ARRAYS_LENGTH];
    struct rte_ether_addr mac_addresses[IP_MAC_ARRAYS_LENGTH];
    memset(ip_addresses,0,sizeof(ip_addresses));
    memset(mac_addresses,0,sizeof(mac_addresses));
    
#ifdef _WINDOWS
    WSADATA wsaData = {0};
    (void)WSA_START(MAKEWORD(2, 2), &wsaData);
#endif


    if (my_mac == NULL) {
        printf("Unknown port MAC address. Using default device MAC address...\n");
        my_mac = rte_malloc(NULL, sizeof(struct rte_ether_addr), 16);
        if (!my_mac) {
            printf("Could not allocate memory !");
            return -ENOMEM;
        }
        if (rte_eth_macaddr_get(portid, my_mac) != 0) {
            printf("Could not find MAC address for port %d\n", portid);
            free(my_mac);
            my_mac = 0;
        }

    }

    send_msg_ptr = &send_msg_size;
    send_buffer = malloc(send_buffer_size);
    if (send_buffer == NULL)
    {
        ret = -1;
        return -1;
    }
    bool need_to_alloc = true;
    bool should_i_print = true;
    bool should_i_print2 = true;
    while (ret == 0 && *is_running)
    {
        int64_t delta_t = 0;
        unsigned char received_ecn;

        if_index_to = 0;

        if (!loop_immediate) {
            delta_t = picoquic_get_next_wake_delay(quic, current_time, delay_max);
            if (options.do_time_check) {
                packet_loop_time_check_arg_t time_check_arg;
                time_check_arg.current_time = current_time;
                time_check_arg.delta_t = delta_t;
                ret = loop_callback(quic, picoquic_packet_loop_time_check, loop_callback_ctx, &time_check_arg);
                if (time_check_arg.delta_t < delta_t) {
                    delta_t = time_check_arg.delta_t;
                }
            }
        }
        loop_immediate = 0;

        pkts_recv = rte_eth_rx_burst(portid, queueid, pkts_burst, MAX_PKT_BURST);

        current_time = picoquic_current_time();

        uint64_t loop_time = current_time;
        uint16_t len;

        for (int i = 0; i < pkts_recv; i++)
        {
            struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(pkts_burst[i], struct rte_ether_hdr *);
            // receiv_counter++;
            // printf("received packets ethernet : %u\n",portid);

            /* access ethernet header of rcv'd pkt */
            if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
            {
                ip_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkts_burst[i], char *) + sizeof(struct rte_ether_hdr));

                if (ip_hdr->next_proto_id == IPPROTO_UDP)
                {
                    udp_hdr = (struct rte_udp_hdr *)((unsigned char *)ip_hdr + sizeof(struct rte_ipv4_hdr));

                    src_addr = ip_hdr->src_addr;
                    dst_addr = ip_hdr->dst_addr;
                    src_port = udp_hdr->src_port;
                    dst_port = udp_hdr->dst_port;

#if RTE_VERSION < RTE_VERSION_NUM(21, 11, 0, 0)
                    add_mac_ip_pair(src_addr, (*eth_hdr).s_addr, ip_addresses, mac_addresses, IP_MAC_ARRAYS_LENGTH);
#else
                    add_mac_ip_pair(src_addr, (*eth_hdr).src_addr, ip_addresses, mac_addresses, IP_MAC_ARRAYS_LENGTH);
#endif

                    (*(struct sockaddr_in *)(&addr_from)).sin_family = AF_INET;
                    (*(struct sockaddr_in *)(&addr_from)).sin_port = src_port;
                    (*(struct sockaddr_in *)(&addr_from)).sin_addr.s_addr = src_addr;

                    (*(struct sockaddr_in *)(&addr_to)).sin_family = AF_INET;
                    (*(struct sockaddr_in *)(&addr_to)).sin_port = dst_port;
                    (*(struct sockaddr_in *)(&addr_to)).sin_addr.s_addr = dst_addr;

                    unsigned char *payload = (unsigned char *)(udp_hdr + 1);
                    rte_be16_t length = udp_hdr->dgram_len;
                    size_t payload_length = htons(length) - sizeof(struct rte_udp_hdr);
                    (void)picoquic_incoming_packet_ex(quic, payload,
                                                      payload_length, (struct sockaddr *)&addr_from,
                                                      (struct sockaddr *)&addr_to, if_index_to, received_ecn,
                                                      &last_cnx, current_time);

                    if (loop_callback != NULL)
                    {
                        size_t b_recvd = (size_t)payload_length;
                        ret = loop_callback(quic, picoquic_packet_loop_after_receive, loop_callback_ctx, &b_recvd);
                    }
                    rte_pktmbuf_free(pkts_burst[i]);
                    if (ret == 0)
                    {
                        continue;
                    }
                }
                if (ip_hdr->next_proto_id == IPPROTO_ICMP)
                {
                    printf("ICMP packet received : ignored\n");
                    rte_pktmbuf_free(pkts_burst[i]);
                    continue;
                }
                else
                {
                    printf("Unknown IP protocol : %x\n", ip_hdr->next_proto_id);
                    rte_pktmbuf_free(pkts_burst[i]);
                    continue;
                }
            }
            else if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
            {
                struct rte_arp_hdr *arp_hdr;
                printf("ARP packet received\n");
                arp_hdr = (struct rte_arp_hdr *)((char *)(eth_hdr + 1) + 0);
                uint32_t bond_ip = (*(struct sockaddr_in *)(&my_addr)).sin_addr.s_addr;
                printf("ARP IP %x %x", bond_ip, arp_hdr->arp_data.arp_tip);
                if (arp_hdr->arp_data.arp_tip == bond_ip)
                {
                    if (arp_hdr->arp_opcode == rte_cpu_to_be_16(RTE_ARP_OP_REQUEST))
                    {
                        printf("ARP request received, sending reply\n");

                        // The packet is created in place : we rewrite the packet received and send it back to avoid memory allocation
                        arp_hdr->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);
                        /* Switch src and dst data and set bonding MAC */
#if RTE_VERSION < RTE_VERSION_NUM(21, 11, 0, 0)
                        rte_ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
                        rte_ether_addr_copy(my_mac, &eth_hdr->s_addr);
#else
                        rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
                        rte_ether_addr_copy(my_mac, &eth_hdr->src_addr);
#endif
                        rte_ether_addr_copy(&arp_hdr->arp_data.arp_sha,
                                            &arp_hdr->arp_data.arp_tha);
                        arp_hdr->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;
                        rte_ether_addr_copy(my_mac, &arp_hdr->arp_data.arp_sha);
                        arp_hdr->arp_data.arp_sip = bond_ip;

                        rte_eth_tx_burst(portid, queueid, &pkts_burst[i], 1);
                    }
                }
                continue;
            }
            else
            {
                printf("Unknown ethernet protocol %x\n", eth_hdr->ether_type);
                rte_pktmbuf_free(pkts_burst[i]);
            }
        } // For all packets received

        if (ret != PICOQUIC_NO_ERROR_SIMULATE_NAT && ret != PICOQUIC_NO_ERROR_SIMULATE_MIGRATION)
        {
            size_t bytes_sent = 0;

            while (ret == 0)
            {
                int if_index = dest_if;
                send_length = 0;
                struct sockaddr_storage peer_addr;
                struct sockaddr_storage local_addr;
                if (need_to_alloc)
                {
                    m = rte_pktmbuf_alloc(mb_pool);
                    if (m == NULL)
                    {
                        printf("fail to init pktmbuf\n");
                        rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));
                        return 0;
                    }
                    need_to_alloc = false;
                }

                uint8_t *payload_ptr = rte_pktmbuf_mtod_offset(m, char *, (size_t)udp_payload_offset);

                ret = picoquic_prepare_next_packet_ex(quic, loop_time,
                                                      payload_ptr, send_buffer_size, &send_length,
                                                      &peer_addr, &local_addr, &if_index, &log_cid, &last_cnx,
                                                      send_msg_ptr);
                if (ret == 0 && send_length > 0)
                {
                    bytes_sent += send_length;
                    int offset = 0;
                    struct rte_ipv4_hdr ip_hdr_struct;
                    struct rte_udp_hdr udp_hdr_struct;
                    struct rte_ether_hdr eth_hdr_struct;
                    struct rte_ether_hdr *eth_ptr = &eth_hdr_struct;

#if RTE_VERSION < RTE_VERSION_NUM(21, 11, 0, 0)
                    rte_ether_addr_copy(&eth_addr, &eth_ptr->s_addr);
#else
                    rte_ether_addr_copy(&eth_addr, &eth_ptr->src_addr);
#endif

                    if (peer_mac != NULL)
                    {
#if RTE_VERSION < RTE_VERSION_NUM(21, 11, 0, 0)
                        rte_ether_addr_copy(peer_mac, &eth_ptr->d_addr);
#else
                        rte_ether_addr_copy(peer_mac, &eth_ptr->dst_addr);
#endif
                    }
                    else
                    {
                        struct rte_ether_addr peer_mac_addr = find_mac_from_ip((*(struct sockaddr_in *)(&peer_addr)).sin_addr.s_addr, ip_addresses, mac_addresses, IP_MAC_ARRAYS_LENGTH);
#if RTE_VERSION < RTE_VERSION_NUM(21, 11, 0, 0)
                        rte_ether_addr_copy(&peer_mac_addr, &eth_ptr->d_addr);
#else
                        rte_ether_addr_copy(&peer_mac_addr, &eth_ptr->dst_addr);
#endif
                    }

                    setup_pkt_udp_ip_headers(&ip_hdr_struct, &udp_hdr_struct, send_length, my_addr, peer_addr);
                    (&eth_hdr_struct)->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
                    copy_buf_to_pkt(&eth_hdr_struct, sizeof(struct rte_ether_hdr), m, offset);
                    offset += sizeof(struct rte_ether_hdr);
                    copy_buf_to_pkt(&ip_hdr_struct, sizeof(struct rte_ipv4_hdr), m, offset);
                    offset += sizeof(struct rte_ipv4_hdr);
                    copy_buf_to_pkt(&udp_hdr_struct, sizeof(struct rte_udp_hdr), m, offset);
                    offset += sizeof(struct rte_udp_hdr);

                    offset += send_length;

                    m->data_len = offset;
                    m->pkt_len = offset;
                    int sent = rte_eth_tx_buffer(portid, queueid, tx_buffer, m);

                    send_counter += sent;
                    need_to_alloc = true;
                }

                else
                {
                    int sent = rte_eth_tx_buffer_flush(portid, queueid, tx_buffer);
                    send_counter += sent;
                    break;
                }
            }

            if (ret == 0 && loop_callback != NULL)
            {
                ret = loop_callback(quic, picoquic_packet_loop_after_send, loop_callback_ctx, &bytes_sent);
            }
        }
    }
    if (ret == PICOQUIC_NO_ERROR_TERMINATE_PACKET_LOOP)
    {
        /* Normal termination requested by the application, returns no error */
        ret = 0;
    }

    if (send_buffer != NULL)
    {
        free(send_buffer);
    }
    return ret;
}
