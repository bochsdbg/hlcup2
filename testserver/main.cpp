#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>   //For standard things
#include <stdlib.h>  //malloc
#include <string.h>  //strlen

#include <arpa/inet.h>
#include <net/ethernet.h>      //For ether_header
#include <netinet/if_ether.h>  //For ETH_P_ALL
#include <netinet/ip.h>        //Provides declarations for ip header
#include <netinet/ip_icmp.h>   //Provides declarations for icmp header
#include <netinet/tcp.h>       //Provides declarations for tcp header
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/if_packet.h>
#include <linux/net_tstamp.h>
//#include <netpacket/packet.h>

#include "virtio_net.hpp"

#include <net/if.h>

#include <atomic>
#include <bitset>
#include <thread>

#include <io.hpp>
#include <socket.hpp>
#include <syscall.hpp>

#include <poll.h>
#include <sys/mman.h>

#include "../fmt/format.hpp"

#include "utils.hpp"

#define BUSY_WAIT 1
#define USE_TX_RING 0
#define USE_SEND_THREAD 0
#define USE_CUSTOM_HANDSHAKE 1
#define USE_TCP_OPTIONS 0
#define USE_VNET_HDR 0

const unsigned int kSendFlags = MSG_DONTWAIT;

template <typename T>
static T check_error_impl(T result, const char *msg) {
    if (result < 0) { fmt::print(stderr, "{}: {} ({})\n", msg, strerror(static_cast<int>(result)), result); }
    return result;
}

#define CHECK_ERROR(x) check_error_impl((x), #x)

static const uint16_t kPort = 80;

using namespace ef;

uint16_t cksum_generic(unsigned char *p, size_t len, uint16_t initial) {
    uint32_t        sum = htons(initial);
    const uint16_t *u16 = reinterpret_cast<const uint16_t *>(p);

    while (len >= (sizeof(*u16) * 4)) {
        sum += u16[0];
        sum += u16[1];
        sum += u16[2];
        sum += u16[3];
        len -= sizeof(*u16) * 4;
        u16 += 4;
    }
    while (len >= sizeof(*u16)) {
        sum += *u16;
        len -= sizeof(*u16);
        u16 += 1;
    }

    /* if length is in odd bytes */
    if (len == 1) sum += *reinterpret_cast<const uint8_t *>(u16);

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ntohs(static_cast<uint16_t>(~sum));
}

#if 0
uint32_t pseudo_header_initial(const int8_t *buf, size_t len) {
    const uint16_t *hwbuf      = (const uint16_t *)buf;
    int8_t          ipv        = (buf[0] & 0xF0) >> 4;
    int8_t          proto      = 0;
    int             headersize = 0;

    if (ipv == 4) {  // IPv4
        proto      = buf[9];
        headersize = (buf[0] & 0x0F) * 4;
    } else if (ipv == 6) {  // IPv6
        proto      = buf[6];
        headersize = 40;
    } else {
        return 0xFFFF0001;
    }

    if (proto == 6 || proto == 17) {  // TCP || UDP
        uint32_t sum = 0;
        len -= headersize;
        if (ipv == 4) {  // IPv4
            if (cksum_generic((unsigned char *)buf, headersize, 0) != 0) { return 0xFFFF0002; }
            sum = htons(len & 0x0000FFFF) + (proto << 8) + hwbuf[6] + hwbuf[7] + hwbuf[8] + hwbuf[9];

        } else {  // IPv6
            sum = hwbuf[2] + (proto << 8);
            int i;
            for (i = 4; i < 20; i += 4) { sum += hwbuf[i] + hwbuf[i + 1] + hwbuf[i + 2] + hwbuf[i + 3]; }
        }
        sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
        sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
        return ntohs(sum);
    }
    return 0xFFFF0001;
}
#endif

static const std::string response(
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 3\r\n"
    "Connection: keep-alive\r\n"
    "Content-Type: application/json\r\n"
    "\r\n"
    "{}\n");

void dump_addr(sockaddr_ll *addr) {
    if (addr->sll_halen == 0) return;

    fmt::print("family: {}, proto: {}, ifindex: {}, hatype: {}, pkttype: {}, halen: {}, addr: '{:02x}", addr->sll_family, addr->sll_protocol, addr->sll_ifindex,
               ntohs(addr->sll_hatype), addr->sll_pkttype, addr->sll_halen, addr->sll_addr[0]);
    for (unsigned char i = 1; i < addr->sll_halen; i++) { fmt::print(":{:02x}", addr->sll_addr[i]); }
    fmt::print("'\n", addr->sll_family);
}

void dump_tpacket_hdr(tpacket_hdr *hdr) {
    fmt::print("TPacket status {}, len {}, snaplen {}, mac {}, net {}, sec {}, usec {}\n", hdr->tp_status, hdr->tp_len, hdr->tp_snaplen, hdr->tp_mac,
               hdr->tp_net, hdr->tp_sec, hdr->tp_usec);
}

void dump_eth(ethhdr *hdr) {
    fmt::print("proto: {}, src: '{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}', dest: '{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}'\n", hdr->h_proto,
               hdr->h_source[0], hdr->h_source[1], hdr->h_source[2], hdr->h_source[3], hdr->h_source[4], hdr->h_source[5], hdr->h_dest[0], hdr->h_dest[1],
               hdr->h_dest[2], hdr->h_dest[3], hdr->h_dest[4], hdr->h_dest[5]);
}

std::string ip_to_string(uint32_t ip) { return fmt::format("{}.{}.{}.{}", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, (ip >> 0) & 0xff); }

void dump_iph(iphdr *hdr) {
    fmt::print("ihl: {}, version: {}, tos: {}, tot_len: {}, id: {}, frag_off: {}, ttl: {}, proto: {}, check: {}, saddr: {}, daddr: {}\n", hdr->ihl,
               hdr->version, hdr->tos, ntohs(hdr->tot_len), ntohs(hdr->id), ntohs(hdr->frag_off), hdr->ttl, hdr->protocol, ntohs(hdr->check),
               ip_to_string(ntohl(hdr->saddr)), ip_to_string(ntohl(hdr->daddr)));
}

void dump_tcph(tcphdr *hdr) {
    fmt::print(
        "source: {}, dest: {}, seq: {}, ack_seq: {}, res1: {}, doff: {}, fin: {}, syn: {}, rst: {}, psh: {}, ack: {}, urg: {}, res2: {}, window: {}, check: "
        "{}, urg_ptr: {}\n",
        ntohs(hdr->source), ntohs(hdr->dest), ntohl(hdr->seq), ntohl(hdr->ack_seq), hdr->res1, hdr->doff, hdr->fin, hdr->syn, hdr->rst, hdr->psh, hdr->ack,
        hdr->urg, hdr->res2, ntohs(hdr->window), ntohs(hdr->check), ntohs(hdr->urg_ptr));
}

unsigned short compute_tcp_checksum(void *iph, void *tcph) {
    struct iphdr * pIph    = reinterpret_cast<iphdr *>(iph);
    struct tcphdr *tcphdrp = reinterpret_cast<tcphdr *>(tcph);

    unsigned long   sum       = 0;
    unsigned short  tcpLen    = ntohs(pIph->tot_len) - uint16_t(pIph->ihl * 4);
    unsigned short *ipPayload = reinterpret_cast<uint16_t *>(tcphdrp);
    // add the pseudo header
    // the source ip
    sum += (pIph->saddr >> 16) & 0xFFFF;
    sum += (pIph->saddr) & 0xFFFF;
    // the dest ip
    sum += (pIph->daddr >> 16) & 0xFFFF;
    sum += (pIph->daddr) & 0xFFFF;
    // protocol and reserved: 6
    sum += htons(6);
    // the length
    sum += htons(tcpLen);

    // add the IP payload
    // initialize checksum to 0
    //    tcphdrp->check = 0;
    while (tcpLen > 1) {
        sum += *ipPayload++;
        tcpLen -= 2;
    }
    // if any bytes left, pad the bytes and add
    if (tcpLen > 0) {
        // printf("+++++++++++padding, %dn", tcpLen);
        sum += ((*ipPayload) & htons(0xFF00));
    }
    // Fold 32-bit sum to 16 bits: add carrier to result
    while (sum >> 16) { sum = (sum & 0xffff) + (sum >> 16); }
    sum = ~sum;
    // set computation result
    return static_cast<uint16_t>(sum);
}

// static int build_vnet_header(void *header) {
//    struct virtio_net_hdr *vh = reinterpret_cast<virtio_net_hdr *>(header);

//    vh->hdr_len = ETH_HLEN + sizeof(struct iphdr) + sizeof(struct tcphdr);

//    bool cfg_use_csum_off = 1;
//    //	bool cfg_use_csum_off_bad = 0;
//    bool     cfg_use_gso = 1;
//    uint16_t cfg_mtu     = 1500;

//    if (cfg_use_csum_off) {
//        vh->flags |= VIRTIO_NET_HDR_F_NEEDS_CSUM;
//        vh->csum_start  = ETH_HLEN + sizeof(struct iphdr);
//        vh->csum_offset = __builtin_offsetof(struct tcphdr, check);

//        /* position check field exactly one byte beyond end of packet */
//        //		if (cfg_use_csum_off_bad)
//        //			vh->csum_start += sizeof(struct tcphdr) + cfg_payload_len -
//        //					  vh->csum_offset - 1;
//    }

//    if (cfg_use_gso) {
//        vh->gso_type = VIRTIO_NET_HDR_GSO_UDP;
//        vh->gso_size = cfg_mtu - sizeof(struct iphdr);
//    }

//    return sizeof(*vh);
//}

static int CNTR = 10;

static sockaddr_ll g_host_addr{};

// static int g_csocks[0xffff] = {0};
// static std::bitset<0xffff> g_fins(0);
[[maybe_unused]] static int g_sockets[0xffff]         = {0};
static uint32_t             g_port_timestamps[0xffff] = {0};

[[maybe_unused]] static int get_iface_addr(const char *if_name, int sock, sockaddr_ll &addr) {
    int   err;
    ifreq ifr{};
    memcpy(ifr.ifr_name, if_name, strlen(if_name) + 1);

    err = platform::syscall<int>(platform::SC::ioctl, sock, SIOCGIFINDEX, &ifr);
    if (err < 0) { return err; }

    int index = ifr.ifr_ifindex;

    err = platform::syscall<int>(platform::SC::ioctl, sock, SIOCGIFHWADDR, &ifr);
    if (err < 0) { return err; }

    memcpy(addr.sll_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    addr.sll_family   = AF_PACKET;
    addr.sll_ifindex  = index;
    addr.sll_protocol = htons(ETH_P_IP);

    return 0;
}

static int bind_socket(int sock, const char *if_name) {
    int err;

    ifreq ifr{};
    memcpy(ifr.ifr_name, if_name, strlen(if_name) + 1);
    err = platform::syscall<int>(platform::SC::ioctl, sock, SIOCGIFINDEX, &ifr);
    if (err < 0) { return err; }

    int ifidx = ifr.ifr_ifindex;

    sockaddr_ll addr{};

    err = platform::syscall<int>(platform::SC::ioctl, sock, SIOCGIFHWADDR, &ifr);
    if (err < 0) { return err; }

    memcpy(addr.sll_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    addr.sll_family   = PF_PACKET;
    addr.sll_protocol = htons(ETH_P_IP);
    addr.sll_ifindex  = ifidx;

    err = platform::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    if (err < 0) { return err; }

    g_host_addr = addr;

    return ifidx;
}

static int set_mtu(int sock, int mtu) {
    ifreq ifr{};
    ifr.ifr_mtu = mtu;
    return platform::syscall<int>(platform::SC::ioctl, sock, SIOCSIFMTU, &ifr);
}

struct PacketReq : public tpacket_req {
    PacketReq(unsigned block_size, unsigned blocks_count) {
        tp_frame_size = block_size;
        tp_block_size = block_size;
        //                while (tp_block_size < tp_frame_size) { tp_block_size <<= 1; }
        tp_block_nr = blocks_count;
        tp_frame_nr = blocks_count;
        //        tp_frame_size = TPACKET_ALIGN(TPACKET_HDRLEN + ETH_HLEN) + TPACKET_ALIGN(mtu);
        //        tp_block_size = static_cast<uint>(sysconf(_SC_PAGESIZE));
        //        while (tp_block_size < tp_frame_size) { tp_block_size <<= 1; }
        //        tp_block_nr = blocks_count;
        //        tp_frame_nr = tp_block_nr * framesPerBuffer();
    }

    unsigned framesPerBuffer() const { return tp_block_size / tp_frame_size; }

    size_t ringSize() const { return tp_block_nr * tp_block_size; }
};

struct RingBuffer {
    char *    data;
    char *    current_frame;
    PacketReq req;
    size_t    frame_idx = 0;

    RingBuffer(uint16_t mtu = 1 << 12, unsigned blocks_count = 64) : req(mtu, blocks_count) {}

    size_t size() const { return req.tp_block_nr * req.tp_block_size; }

    void nextFrame() {
        frame_idx     = (frame_idx + 1) % req.tp_frame_nr;
        current_frame = data + frame_idx * req.tp_block_size;
        //        fmt::print(stderr, "{}\n", (void *)current_frame);

        // Determine the location of the buffer which the next frame lies within.
        //        size_t buffer_idx = frame_idx / req.framesPerBuffer();
        //        char * buffer_ptr = data + buffer_idx * req.tp_block_size;

        //        // Determine the location of the frame within that buffer.
        //        size_t frame_idx_diff = frame_idx % req.framesPerBuffer();
        //        current_frame         = buffer_ptr + frame_idx_diff * req.tp_frame_size;
    }
};

struct FrameHdr {
    //    intmax_t tp : TPACKET_HDRLEN * 8 - sizeof(struct sockaddr_ll) * 8;
    //    struct {
    unsigned long tp_status;
    unsigned int  tp_len;

    unsigned int   tp_snaplen;
    unsigned short tp_mac;
    unsigned short tp_net;
    unsigned int   tp_sec;
    unsigned int   tp_usec;

    int pad1;

    //    } tp;
    //        tpacket_hdr tp;
    //    sockaddr_ll addr;

#if USE_VNET_HDR
    virtio_net_hdr vnet_hdr;
#endif

    ethhdr eth;

    //    TPACKET2_HDRLEN
    iphdr  iph;
    tcphdr tcph;
    //#if USE_TCP_OPTIONS
    //    char nop1 = 0x1;
    //    char nop2 = 0x1;
    //    char
    //#endif
} __attribute__((packed));

static RingBuffer rx_ring;
#if !USE_TX_RING
static RingBuffer tx_ring(1 << 12, 1);
#else
static RingBuffer tx_ring;
#endif

#include <pthread.h>

static void set_affinity(pthread_t th, int i) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);
    int rc = pthread_setaffinity_np(th, sizeof(cpu_set_t), &cpuset);
    if (rc < 0) { perror("pthread_setaffinity_np()"); }
}

#if USE_SEND_THREAD
[[noreturn]] static void send_thread(int tx_sock) {
    set_affinity(pthread_self(), 3);
    //    static uint64_t   tx_last_sent_ts   = 0;

#if !BUSY_WAIT
    pollfd pfd{};
    pfd.fd     = g_tx_raw_sock;
    pfd.events = POLLOUT;
#endif

    msghdr msg{};
    while (true) {
#if BUSY_WAIT
#else
        //        fmt::print("pool start\n");
        ::usleep(100);
//        int err = platform::syscall<int>(platform::SC::poll, &pfd, 1, -1);
//        if (err < 0) {
//            fmt::print(stderr, "poll(): {}\n", err);
//            break;
//        }
//        fmt::print("pool end\n");
#endif
        platform::sendmsg(tx_sock, &msg, MSG_DONTWAIT);
    }
}
#endif

using u16 = uint16_t;
static inline u16 __virtio16_to_cpu(bool little_endian, __virtio16 val) {
    if (little_endian)
        return val;
    else
        return ntohs(val);
}

static inline __virtio16 __cpu_to_virtio16(bool little_endian, u16 val) {
    if (little_endian)
        return val;
    else
        return htons(val);
}

static void debug_send(virtio_net_hdr vnet_hdr, size_t len) {
    size_t vnet_hdr_len = sizeof(vnet_hdr);

    //			int err = -EINVAL;
    if (len < vnet_hdr_len) {
        fmt::print("1\n");
        return;
    }
    len -= vnet_hdr_len;

    if ((vnet_hdr.flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)
        && (__virtio16_to_cpu(false, vnet_hdr.csum_start) + __virtio16_to_cpu(false, vnet_hdr.csum_offset) + 2 > __virtio16_to_cpu(false, vnet_hdr.hdr_len)))
        vnet_hdr.hdr_len = __cpu_to_virtio16(false, __virtio16_to_cpu(false, vnet_hdr.csum_start) + __virtio16_to_cpu(false, vnet_hdr.csum_offset) + 2);

    //    if ((vnet_hdr.flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) && (vnet_hdr.csum_start + vnet_hdr.csum_offset + 2 > vnet_hdr.hdr_len))
    //        vnet_hdr.hdr_len = vnet_hdr.csum_start + vnet_hdr.csum_offset + 2;

    if (__virtio16_to_cpu(false, vnet_hdr.hdr_len) > len) {
        fmt::print(stderr, "__virtio16_to_cpu(false, {}) = {} > {}\n", vnet_hdr.hdr_len, __virtio16_to_cpu(false, vnet_hdr.hdr_len), len);
        fmt::print("2\n");
        return;
    }

    if (__virtio16_to_cpu(false, vnet_hdr.hdr_len) > len) {
        fmt::print("3\n");
        return;
    }
}

static ssize_t do_send(int sock, FrameHdr *frame, size_t data_size, sockaddr_ll *) {
#if USE_SEND_THREAD
    tx_ring.nextFrame();
#elif USE_TX_RING
    tx_ring.nextFrame();
    return 0;
//    return CHECK_ERROR(platform::send(sock, nullptr, 0, kSendFlags));
#elif USE_VNET_HDR
    frame->vnet_hdr.flags       = VIRTIO_NET_HDR_F_NEEDS_CSUM | VIRTIO_NET_HDR_F_DATA_VALID;
    frame->vnet_hdr.hdr_len     = htons(sizeof(frame->eth) + sizeof(frame->iph) + sizeof(frame->tcph));
    frame->vnet_hdr.gso_type    = data_size > 0 ? VIRTIO_NET_HDR_GSO_TCPV4 | VIRTIO_NET_HDR_GSO_ECN : VIRTIO_NET_HDR_GSO_NONE;
    frame->vnet_hdr.gso_size    = data_size > 0 ? htons(1) : 0;
    frame->vnet_hdr.csum_start  = htons(ETH_HLEN + sizeof(struct iphdr));
    frame->vnet_hdr.csum_offset = htons(__builtin_offsetof(struct tcphdr, check));

    size_t sz = sizeof(frame->vnet_hdr) + ETH_HLEN + sizeof(struct iphdr) + sizeof(struct tcphdr) + data_size;
    debug_send(frame->vnet_hdr, sz);
    ssize_t err = platform::send(sock, &frame->vnet_hdr, sz, kSendFlags);
    if (err < 0) { fmt::print(stderr, "send(): {}\n", err); }
    return err;
#else
    size_t  sz  = frame->tp_len;
    ssize_t err = platform::send(sock, &frame->eth, sz, kSendFlags);
    if (err < 0) { fmt::print(stderr, "send(): {}\n", err); }
    return err;
#endif
}

static ssize_t handle_frame([[maybe_unused]] int tx_sock, char *buffer) {
    tpacket_hdr *tphdr = reinterpret_cast<tpacket_hdr *>(buffer);
    sockaddr_ll *caddr = reinterpret_cast<sockaddr_ll *>(buffer + TPACKET_HDRLEN - sizeof(struct sockaddr_ll));
    ethhdr *     eth   = reinterpret_cast<ethhdr *>(buffer + tphdr->tp_mac);
    iphdr *      iph   = reinterpret_cast<iphdr *>(buffer + tphdr->tp_net);

    if (iph->protocol != 6) return 0;

    tcphdr *tcph = reinterpret_cast<tcphdr *>(buffer + tphdr->tp_net + iph->ihl * 4);
    if (tcph->dest != htons(kPort)) return 0;

    [[maybe_unused]] char *data      = buffer + tphdr->tp_net + iph->ihl * 4 + tcph->doff * 4;
    uint16_t               data_size = ntohs(iph->tot_len) - uint16_t(tcph->doff * 4) - uint16_t(iph->ihl * 4);

    //    if (data_size <= 0) return;

    if (CNTR) {
        dump_tpacket_hdr(tphdr);
        dump_addr(caddr);
        dump_eth(eth);
        dump_iph(iph);
        dump_tcph(tcph);
    }

    if (CNTR) CNTR--;

#if USE_CUSTOM_HANDSHAKE
    if (tcph->fin == 1) {
        FrameHdr *frame = reinterpret_cast<FrameHdr *>(tx_ring.current_frame);

        memcpy(frame->eth.h_source, eth->h_dest, 6);
        memcpy(frame->eth.h_dest, eth->h_source, 6);

        frame->iph.saddr = iph->daddr;
        frame->iph.daddr = iph->saddr;

        frame->tcph.source  = tcph->dest;
        frame->tcph.dest    = tcph->source;
        frame->tcph.ack_seq = htonl(ntohl(tcph->seq) + 1);
        frame->tcph.seq     = tcph->ack_seq;

        frame->tcph.fin = 1;
        frame->tcph.ack = 1;
        frame->tcph.psh = 0;
        frame->iph.id   = 0;
        //        frame->tcph.window = htons(65483);

#if USE_TCP_OPTIONS
        char *opts = reinterpret_cast<char *>(&frame->tcph) + sizeof(tcphdr);
        memcpy(opts, "\x02\x04\xff\xd7\x04\x02\x08\x0a\x52\x1e\x65\x61\x52\x1e\x65\x61\x01\x03\x03\x07", 20);
        uint32_t tsval                           = *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(tcph) + sizeof(tcphdr) + 8);
        *reinterpret_cast<uint32_t *>(opts + 8)  = tsval;
        *reinterpret_cast<uint32_t *>(opts + 12) = tsval;
        frame->tcph.doff                         = 10;
#endif

        frame->iph.tot_len = htons(sizeof(FrameHdr) - offsetof(FrameHdr, iph) + (frame->tcph.doff - 5) * 4);

#if USE_VNET_HDR
        frame->iph.check = 0;
        frame->iph.check = htons(cksum_generic(reinterpret_cast<uint8_t *>(&frame->iph), iph->ihl * 4, 0));
#else
        frame->iph.check  = 0;
        frame->tcph.check = 0;
        frame->iph.check  = htons(cksum_generic(reinterpret_cast<uint8_t *>(&frame->iph), iph->ihl * 4, 0));
        frame->tcph.check = compute_tcp_checksum(&frame->iph, &frame->tcph);
#endif

        frame->tp_len    = sizeof(FrameHdr) - offsetof(FrameHdr, eth) + (frame->tcph.doff - 5) * 4;
        frame->tp_status = TP_STATUS_SEND_REQUEST;

        return do_send(tx_sock, frame, 0, caddr);
    }
    if (tcph->syn == 1) {
        FrameHdr *frame = reinterpret_cast<FrameHdr *>(tx_ring.current_frame);

        //        uint32_t ack_seq     = tcph->ack_seq;
        //        uint32_t new_ack_seq = ntohl(tcph->seq) + data_size;

        //                memcpy(frame->addr.h_source, eth->h_dest, 6);
        //                memcpy(frame->eth.h_dest, eth->h_source, 6);
        //        frame->addr = *caddr;

        memcpy(frame->eth.h_source, eth->h_dest, 6);
        memcpy(frame->eth.h_dest, eth->h_source, 6);

        frame->iph.saddr = iph->daddr;
        frame->iph.daddr = iph->saddr;

        frame->tcph.source  = tcph->dest;
        frame->tcph.dest    = tcph->source;
        frame->tcph.ack_seq = htonl(ntohl(tcph->seq) + 1);
        frame->tcph.seq     = tcph->ack_seq;

        frame->tcph.syn = 1;
        frame->tcph.fin = 0;
        frame->tcph.psh = 0;
        frame->tcph.ack = 1;
        frame->iph.id   = 0;
        //        frame->tcph.window = htons(65483);

#if USE_TCP_OPTIONS
        char *opts = reinterpret_cast<char *>(&frame->tcph) + sizeof(tcphdr);
        memcpy(opts, "\x02\x04\xff\xd7\x04\x02\x08\x0a\x52\x1e\x65\x61\x52\x1e\x65\x61\x01\x03\x03\x07", 20);
        uint32_t tsval                           = *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(tcph) + sizeof(tcphdr) + 8);
        *reinterpret_cast<uint32_t *>(opts + 8)  = tsval;
        *reinterpret_cast<uint32_t *>(opts + 12) = tsval;
        frame->tcph.doff                         = 10;
#endif

        frame->iph.tot_len = htons(sizeof(FrameHdr) - offsetof(FrameHdr, iph) + (frame->tcph.doff - 5) * 4);
        //        iph->check  = 0;
        //        tcph->check = 0;

#if USE_VNET_HDR
        frame->iph.check = 0;
        frame->iph.check = htons(cksum_generic(reinterpret_cast<uint8_t *>(&frame->iph), iph->ihl * 4, 0));
#else
        frame->iph.check  = 0;
        frame->tcph.check = 0;
        frame->iph.check  = htons(cksum_generic(reinterpret_cast<uint8_t *>(&frame->iph), iph->ihl * 4, 0));
        frame->tcph.check = compute_tcp_checksum(&frame->iph, &frame->tcph);
#endif

        frame->tp_len    = sizeof(FrameHdr) - offsetof(FrameHdr, eth) + (frame->tcph.doff - 5) * 4;
        frame->tp_status = TP_STATUS_SEND_REQUEST;
        //        tx_ring.nextFrame();
        return do_send(tx_sock, frame, 0, caddr);
    }

#endif

    if (data_size > 0) {
        g_port_timestamps[tcph->source] = tphdr->tp_sec;

        //        fmt::print("data: {} `{:.{}}`\n", data_size, data, data_size);

        FrameHdr *frame = reinterpret_cast<FrameHdr *>(tx_ring.current_frame);

        //        uint16_t resp_size   = static_cast<uint16_t>(response.size());
        uint32_t ack_seq = tcph->ack_seq;
        //        uint32_t new_ack_seq = ntohl(tcph->seq) + data_size;

        //                memcpy(frame->addr.h_source, eth->h_dest, 6);
        //                memcpy(frame->eth.h_dest, eth->h_source, 6);
        //        frame->addr = *caddr;

        memcpy(frame->eth.h_source, eth->h_dest, 6);
        memcpy(frame->eth.h_dest, eth->h_source, 6);

        frame->iph.saddr = iph->daddr;
        frame->iph.daddr = iph->saddr;

        frame->tcph.source  = tcph->dest;
        frame->tcph.dest    = tcph->source;
        frame->tcph.ack_seq = htonl(ntohl(tcph->seq) + data_size);
        frame->tcph.seq     = ack_seq;

#if USE_CUSTOM_HANDSHAKE
        //        frame->iph.id      = htons(5723);  // generated by dice roll :)
        frame->tcph.syn = 0;
        frame->tcph.psh = 1;
        frame->tcph.fin = 0;
        frame->tcph.ack = 1;

#endif

#if USE_TCP_OPTIONS
        char *    tcph_opts  = reinterpret_cast<char *>(tcph) + sizeof(tcphdr);
        uint32_t *timestamps = reinterpret_cast<uint32_t *>(tcph_opts + 4);
        uint32_t  tsval      = timestamps[0];
        uint32_t  tsecr      = timestamps[1];

        char *opts_dest  = reinterpret_cast<char *>(&frame->tcph) + sizeof(tcphdr);
        opts_dest[0]     = 0x1;
        opts_dest[1]     = 0x1;
        opts_dest[2]     = 8;
        opts_dest[3]     = 10;
        timestamps       = reinterpret_cast<uint32_t *>(opts_dest + 4);
        timestamps[1]    = tsval;
        timestamps[0]    = htonl(ntohl(tsecr));
        frame->tcph.doff = 8;
#endif

        char *dest_data = reinterpret_cast<char *>(&frame->tcph) + frame->tcph.doff * 4;
        memcpy(dest_data, response.data(), response.size());
        frame->iph.tot_len = htons(sizeof(FrameHdr) - offsetof(FrameHdr, iph) + static_cast<uint16_t>(response.size()) + (frame->tcph.doff - 5) * 4);

        //        frame->tcph.check = 0;
        //        frame->tcph.check = compute_tcp_checksum(&frame->iph, &frame->tcph);

#if USE_VNET_HDR
        frame->iph.check = 0;
        frame->iph.check = htons(cksum_generic(reinterpret_cast<uint8_t *>(&frame->iph), iph->ihl * 4, 0));
#else
        frame->iph.check  = 0;
        frame->tcph.check = 0;
        frame->iph.check  = htons(cksum_generic(reinterpret_cast<uint8_t *>(&frame->iph), iph->ihl * 4, 0));
        frame->tcph.check = compute_tcp_checksum(&frame->iph, &frame->tcph);
#endif
        //        uint32_t send_data_size = static_cast<uint32_t>(data - reinterpret_cast<char *>(eth)) + resp_size;

        if (CNTR) {
            //            fmt::print("sending to sock {} data size {} :\n", sock, send_data_size);
            dump_eth(&frame->eth);
            dump_iph(&frame->iph);
            dump_tcph(&frame->tcph);
        }
        //            fmt::print("msg->msg_namelen: {}, sizeof(struct sockaddr_ll): {}\n", caddr_len, sizeof(struct sockaddr_ll));
        //            fmt::print("(saddr->sll_halen + offsetof(struct sockaddr_ll, sll_addr)): {}\n", (caddr.sll_halen + offsetof(struct sockaddr_ll,
        //            sll_addr)));

        //            dump_eth(eth);
        //            dump_iph(iph);
        //            dump_tcph(tcph);

        //            caddr.sll_pkttype = PACKET_OUTGOING;

        //        memcpy(caddr->sll_addr, eth->h_dest, 6);
        //        ssize_t sent_size = platform::send(sock, eth, send_data_size, 0);

        //        ssize_t sent_size = platform::send(sock, eth, send_data_size, MSG_DONTWAIT);
        frame->tp_len    = sizeof(FrameHdr) - offsetof(FrameHdr, eth) + static_cast<uint32_t>(response.size()) + (frame->tcph.doff - 5) * 4;
        frame->tp_status = TP_STATUS_SEND_REQUEST;
        //        tx_ring.nextFrame();

        //        uint64_t current_ts = uint64_t(tphdr->tp_sec) * 1000000 + tphdr->tp_usec;

        return do_send(tx_sock, frame, response.size(), caddr);
        //            fmt::print(stderr, "----------------\nraw send(): {}\n", sent_size);
    }

    return 0;
}

static int fill_tx_ring(RingBuffer &tx) {
    FrameHdr tpl;

    //    tpl.addr = g_host_addr;
    //    tpl.tp.tp_mac     = offsetof(FrameHdr, eth);
    //    tpl.tp.tp_net     = offsetof(FrameHdr, iph);
    tpl.tp_status = TP_STATUS_SENDING;
    tpl.tp_len    = sizeof(FrameHdr) - offsetof(FrameHdr, eth) + static_cast<uint32_t>(response.size());
    //    tpl.tp.tp_snaplen = tpl.tp.tp_len;

    tpl.eth.h_proto = htons(ETH_P_IP);

    fmt::print(stderr, "payload: {}\n", response.size());
    tpl.iph.ihl     = 5;
    tpl.iph.version = 4;
    tpl.iph.tos     = 16;
    tpl.iph.tot_len = htons(sizeof(FrameHdr) - offsetof(FrameHdr, iph) + static_cast<uint16_t>(response.size()));

    tpl.iph.frag_off = htons(0x4000);
    tpl.iph.ttl      = 64;
    tpl.iph.protocol = IPPROTO_TCP;
    tpl.iph.check    = 0;

    tpl.tcph.source = htons(kPort);
    tpl.tcph.res1   = 0;
    tpl.tcph.doff   = 5;
    tpl.tcph.fin    = 0;
    tpl.tcph.syn    = 0;
    tpl.tcph.rst    = 0;
    tpl.tcph.psh    = 1;
    tpl.tcph.ack    = 1;
    tpl.tcph.urg    = 0;
    tpl.tcph.res2   = 0;
    tpl.tcph.window = htons(65483);

    do {
        memcpy(tx.current_frame, &tpl, sizeof(tpl));
        memcpy(tx.current_frame + sizeof(tpl), response.data(), response.size());
        tx.nextFrame();
    } while (tx.current_frame != tx.data);

    return 0;
}

static int create_packet_socket() {
    int err;
    int sock = platform::socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    fmt::print(stderr, "raw sock(): {}\n", sock);
    if (sock < 0) return sock;

    err = bind_socket(sock, "eth0");
    fmt::print(stderr, "bind_socket(eth0): {}\n", err);
    if (err < 0) {
        err = bind_socket(sock, "docker0");
        fmt::print(stderr, "bind_socket(docker0): {}\n", err);

        if (err < 0) {
            err = bind_socket(sock, "lo");
            fmt::print(stderr, "bind_socket(lo): {}\n", err);
        }
    }

    err = set_mtu(sock, 65536);
    fmt::print(stderr, "set_mtu(): {}\n", err);

    CHECK_ERROR(platform::setsockopt(sock, SOL_PACKET, PACKET_LOSS, 1));
    CHECK_ERROR(platform::setsockopt(sock, SOL_PACKET, PACKET_QDISC_BYPASS, 1));
    int timestamps = SOF_TIMESTAMPING_RAW_HARDWARE | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_SYS_HARDWARE
                     | SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_TX_SCHED;
    CHECK_ERROR(platform::setsockopt(sock, SOL_PACKET, PACKET_TIMESTAMP, timestamps));

    return sock;
}

static int socket_mmap_ring(int sock, int ring_type, RingBuffer &ring) {
    int err;

    err = platform::setsockopt(sock, SOL_PACKET, ring_type, &ring.req, sizeof(tpacket_req));
    if (err < 0) {
        fmt::print(stderr, "setsockopt(ring) failed: {}\n", err);
        return err;
    }

    union {
        long  err;
        char *ptr;
    } mmap_ret;

    mmap_ret.err = platform::syscall<long>(platform::SC::mmap, 0, ring.size(), PROT_READ | PROT_WRITE, MAP_SHARED, sock, 0);
    if (mmap_ret.err < 0 && mmap_ret.err > -4096) {
        fmt::print(stderr, "mmap() failed: {} \n", mmap_ret.err);
        return static_cast<int>(mmap_ret.err);
    }

    ring.current_frame = ring.data = mmap_ret.ptr;
    return 0;
}

#ifndef SO_TIMESTAMPING
#define SO_TIMESTAMPING 37
#define SCM_TIMESTAMPING SO_TIMESTAMPING
#endif

#ifndef SO_TIMESTAMPNS
#define SO_TIMESTAMPNS 35
#endif

#ifndef SIOCGSTAMPNS
#define SIOCGSTAMPNS 0x8907
#endif

#ifndef SIOCSHWTSTAMP
#define SIOCSHWTSTAMP 0x89b0
#endif

int hwtsInit(int sock, const char *interface) {
    struct ifreq           hwtstamp;
    struct hwtstamp_config hwconfig, hwconfig_requested;

    int so_timestamping_flags = 0;
    so_timestamping_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
    // so_timestamping_flags |= SOF_TIMESTAMPING_TX_SOFTWARE;		//Enabled for using SW TS
    so_timestamping_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
    // so_timestamping_flags |= SOF_TIMESTAMPING_RX_SOFTWARE;		//Enabled for using SW TS
    // so_timestamping_flags |= SOF_TIMESTAMPING_SOFTWARE;		//Enabled for using SW TS
    so_timestamping_flags |= SOF_TIMESTAMPING_SYS_HARDWARE;
    so_timestamping_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;

    memset(&hwtstamp, 0, sizeof(hwtstamp));
    strncpy(hwtstamp.ifr_name, interface, sizeof(hwtstamp.ifr_name));
    hwtstamp.ifr_data = *reinterpret_cast<caddr_t *>(&hwconfig);

    memset(&hwconfig, 0, sizeof(hwconfig));
    hwconfig.tx_type   = HWTSTAMP_TX_ON;
    hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;

    hwconfig_requested = hwconfig;

    if (ioctl(sock, SIOCSHWTSTAMP, &hwtstamp) >= 0) {
        fprintf(stderr,
                "SIOCSHWTSTAMP: tx_type %d requested, got %d; "
                "rx_filter %d requested, got %d\n",
                hwconfig_requested.tx_type, hwconfig.tx_type, hwconfig_requested.rx_filter, hwconfig.rx_filter);
    } else {
        perror("SIOCSHWTSTAMP: failed to enable hardware time stamping");
        // return FALSE;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &so_timestamping_flags, sizeof(so_timestamping_flags)) < 0) {
        perror("setsockopt SO_TIMESTAMPING");
        return false;
    }

    fprintf(stderr, "SO_TIMESTAMPING enabled\n");

    return true;
}

static void packet_handler() {
    //    set_affinity(pthread_self(), 1);

    int rx_sock = create_packet_socket();
    int tx_sock = create_packet_socket();

    //    struct packet_mreq mreq{};
    //    mreq.mr_ifindex = ifindex;
    //    mreq.mr_type = PACKET_MR_PROMISC;
    //    err = platform::setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    //    fmt::print(stderr, "raw setsockopt(PACKET_ADD_MEMBERSHIP): {}\n", err);

    //    err = platform::setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, "eth0", strlen("eth0") + 1);
    //    fmt::print(stderr, "raw setsockopt(SO_BINDTODEVICE): {}\n", err);

    //    hwtsInit(rx_sock, "docker0");

    int err;
    err = socket_mmap_ring(rx_sock, PACKET_RX_RING, rx_ring);
    if (err < 0) {
        fmt::print(stderr, "mmap(PACKET_RX_RING) failed: {}\n", err);
        return;
    }
#if USE_TX_RING
    err = socket_mmap_ring(tx_sock, PACKET_TX_RING, tx_ring);
    if (err < 0) {
        fmt::print(stderr, "mmap(PACKET_TX_RING) failed: {}\n", err);
        return;
    }
#else

#if USE_VNET_HDR
    err = platform::setsockopt(tx_sock, SOL_PACKET, PACKET_VNET_HDR, 1);
    fmt::print(stderr, "setsockopt(PACKET_VNET_HDR): {}\n", err);
#endif

    tx_ring.current_frame = tx_ring.data = reinterpret_cast<char *>(::aligned_alloc(8192, 8192));
#endif

    fmt::print(stderr, "filling tx ring\n");
    fill_tx_ring(tx_ring);
    fmt::print(stderr, "tx ring filled ok\n");

#if USE_SEND_THREAD
    std::thread send_th(send_thread, tx_sock);
#endif

#if !BUSY_WAIT
    pollfd pfd;
    pfd.fd     = rx_sock;
    pfd.events = POLLIN;
#endif

    int packets_in_row = 0, packets_in_row_max = 0;
    while (true) {
        char *                  frame_ptr = rx_ring.current_frame;
        tpacket_hdr *           tphdr     = reinterpret_cast<tpacket_hdr *>(frame_ptr);
        volatile unsigned long *status    = &tphdr->tp_status;

        while (!(__atomic_load_n(&tphdr->tp_status, __ATOMIC_RELAXED) & TP_STATUS_USER)) {
            //        while (!(*status & TP_STATUS_USER)) {
            packets_in_row = 0;
//            platform::send(tx_sock, nullptr, 0, kSendFlags);
#if BUSY_WAIT
//            __asm volatile("pause" ::: "memory");
#else
            err = platform::syscall<int>(platform::SC::poll, &pfd, 1, 1000);
            if (err < 0) {
                fmt::print(stderr, "poll(): {}\n", err);
                break;
            }
#endif
        }

#if DEBUG
        uint64_t start_tsc = rdtscp();
        ssize_t  rs        = handle_frame(tx_sock, frame_ptr);
        uint64_t end_tsc   = rdtscp();

        if (rs > 0) {
            uint64_t dt         = end_tsc - start_tsc;
            char     ts_type[4] = {'.', '.', '.', 0};

            // TP_STATUS_TS_SOFTWARE (1 << 29)
            // TP_STATUS_TS_SYS_HARDWARE (1 << 30)
            // TP_STATUS_TS_RAW_HARDWARE (1 << 31)
            if (tphdr->tp_status & (1UL << 31UL)) { ts_type[0] = 'R'; }
            if (tphdr->tp_status & (1UL << 30UL)) { ts_type[1] = 'Y'; }
            if (tphdr->tp_status & (1UL << 29UL)) { ts_type[2] = 'S'; }
            fmt::print(stderr, "dt: {:>10} {:3} {:>10} {:>10} {:>20} {:<5}\n", tphdr->tp_status, ts_type, tphdr->tp_sec, tphdr->tp_usec, dt, rs);
        }
#else
        handle_frame(tx_sock, frame_ptr);
#endif

        packets_in_row++;
        if (packets_in_row_max < packets_in_row) {
            packets_in_row_max = packets_in_row;
            fmt::print(stderr, "max packets_in_row: {}\n", packets_in_row_max);
        }

        tphdr->tp_status = TP_STATUS_KERNEL;

        rx_ring.nextFrame();
    }

    err = platform::close(rx_sock);
    fmt::print(stderr, "raw close(): {}\n", err);
}

#include <linux/filter.h>
#include <sys/resource.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

int main() {
    int err;

    set_affinity(pthread_self(), 0);
    //    struct rlimit l;
    //    l.rlim_cur = 4096;
    //    l.rlim_max = 4096;
    //    if (setrlimit(RLIMIT_NOFILE, &l) < 0) { fprintf(stderr, "setrlimit failed: %d (%s)\n", errno, strerror(errno)); }

    err = setvbuf(stdout, nullptr, _IOLBF, BUFSIZ);
    fmt::print(stderr, "setvbuf(): {} {}\n", err, errno);

    //    std::thread th(packet_handler);

    //    std::thread th;
    //    return 0;

    size_t n_fins = 0, n_fins_ack = 0;

    int sock = platform::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_TCP);
    fmt::print(stderr, "http socket(): {}\n", sock);
    if (sock < 0) { return 1; }

    int val = 1;
    err     = platform::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
    fmt::print(stderr, "http setsockopt(SO_REUSEADDR): {}\n", err);

    sockaddr_in addr{};
    addr.sin_port   = htons(kPort);
    addr.sin_family = AF_INET;

#if USE_CUSTOM_HANDSHAKE
    sock_filter code[] = {
        //        {0x06, 0, 0, 0x00000000},
        {0x28, 0, 0, 0x0000000c},  {0x15, 0, 8, 0x000086dd}, {0x30, 0, 0, 0x00000014},  {0x15, 2, 0, 0x00000084}, {0x15, 1, 0, 0x00000006},
        {0x15, 0, 17, 0x00000011}, {0x28, 0, 0, 0x00000036}, {0x15, 14, 0, 0x00000050}, {0x28, 0, 0, 0x00000038}, {0x15, 12, 13, 0x00000050},
        {0x15, 0, 12, 0x00000800}, {0x30, 0, 0, 0x00000017}, {0x15, 2, 0, 0x00000084},  {0x15, 1, 0, 0x00000006}, {0x15, 0, 8, 0x00000011},
        {0x28, 0, 0, 0x00000014},  {0x45, 6, 0, 0x00001fff}, {0xb1, 0, 0, 0x0000000e},  {0x48, 0, 0, 0x0000000e}, {0x15, 2, 0, 0x00000050},
        {0x48, 0, 0, 0x00000010},  {0x15, 0, 1, 0x00000050}, {0x6, 0, 0, 0x00040000},   {0x6, 0, 0, 0x00000000},

    };
    struct sock_fprog bpf {
        ARRAY_SIZE(code), code,
    };

    err = platform::setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(struct sock_fprog));
    fmt::print(stderr, "http SO_ATTACH_FILTER: {}\n", err);

    err = platform::setsockopt(sock, SOL_SOCKET, SO_LOCK_FILTER, 1);
    fmt::print(stderr, "http SO_LOCK_FILTER: {}\n", err);
#endif

    err = platform::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    fmt::print(stderr, "http bind(): {}\n", err);
    if (err < 0) { return 1; }

    err = platform::listen(sock, 512);
    fmt::print(stderr, "http listen(): {}\n", err);
    if (err < 0) { return 1; }

#if USE_CUSTOM_HANDSHAKE
//    err = platform::shutdown(sock, SHUT_RDWR);
//    fmt::print(stderr, "http shutdown(): {}\n", err);
//    if (err < 0) { return 1; }
#endif

    //    ::usleep(10 * 1000000);
    //    th.join();
    packet_handler();

    //    void *buffer = aligned_alloc(0x10000, 0x10000);

    /*
    for (;;) {
        sockaddr_in caddr;
        socklen_t   caddr_len = sizeof(caddr);

        //        fmt::print(stderr, "!!!! accepting .... \n");
        int csock = platform::accept(sock, reinterpret_cast<sockaddr *>(&caddr), &caddr_len);
        //        fmt::print(stderr, "!!!! accepted: .... {}\n", csock);

        if (csock < 0) {
            fmt::print(stderr, "http accept(): {}. calling gc\n", csock);

            while (true) {
                timespec ts;
                clock_gettime(CLOCK_REALTIME_COARSE, &ts);
                uint32_t current_secs   = static_cast<uint32_t>(ts.tv_sec);
                size_t   closed_sockets = 0;
                for (size_t i = 0; i < 0xffff; i++) {
                    if (g_port_timestamps[i] == 0) continue;
                    if (g_port_timestamps[i] + 10 < current_secs) {
                        //                        fmt::print("trying close port {} socket {}\n", i, g_sockets[i]);
                        if (g_sockets[i] == 0) continue;
                        n_fins++;
                        int close_err = platform::close(g_sockets[i]);
                        if (close_err >= 0) {
                            n_fins_ack++;
                            closed_sockets++;
                            g_port_timestamps[i] = 0;
                            g_sockets[i]         = 0;
                        } else {
                            fmt::print(stderr, "close err: {}\n", close_err);
                        }
                    }
                }

                if (closed_sockets > 0) { break; }

                usleep(1000000);
            }

            fmt::print(stderr, "n_fins: {}, n_fins_ack: {}\n", n_fins, n_fins_ack);

            continue;
        }

        g_sockets[caddr.sin_port] = csock;
        //        fmt::print("open: {} -> {}\n", caddr.sin_port, csock);

        if (CNTR) { fmt::print("Client connected: {}\n", csock); }

#if 0
        ssize_t sz = platform::recv(csock, buffer, 0x10000, 0);
        if (sz < 0) {
            fmt::print(stderr, "http recv(): {}\n", sz);
            break;
        }

        platform::send(csock, response.data(), response.size(), MSG_DONTWAIT);

        ::usleep(1500000);

        platform::close(csock);
#endif
    }

    err = platform::close(sock);
    fmt::print(stderr, "raw close(): {}\n", err);
*/
    //    th.join();
    return 0;
}

#if 0

    int             saddr_size, data_size;
    struct sockaddr saddr;

    unsigned char *buffer = (unsigned char *)malloc(65536);  // Its Big!

    logfile = stdout;
    if (logfile == NULL) { printf("Unable to create log.txt file."); }
    printf("Starting...\n");
    fflush(stdout);

    int sock_raw = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    fmt::print("sock_raw: {} {}\n", sock_raw, errno);
    fflush(stdout);
    if (sock_raw < 0) {
        perror("socket()");
        return 1;
    }

    //    int val = 1;
    //    if (setsockopt(sock_raw, IPPROTO_IP, IP_HDRINCL, &val, sizeof(val)) < 0) printf("Warning: Cannot set HDRINCL!\n");

    //    sockaddr_in addr;
    //    int         err;
    //    addr.sin_family = AF_INET;
    //    addr.sin_port   = htons(80);
    //    err             = bind(sock_raw, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
    //    fmt::print("bind(): {} {}\n", err, errno);
    //    fflush(stdout);
    //    if (err < 0) return 1;

    //    err = listen(sock_raw, 1024);
    //    fmt::print("listen(): {} {}\n", err, errno);
    //    fflush(stdout);
    //    if (err < 0) return 1;

    setsockopt(sock_raw, SOL_SOCKET, SO_BINDTODEVICE, "lo", strlen("lo") + 1);

    if (sock_raw < 0) {
        // Print the error with proper message
        perror("Socket Error");
        return 1;
    }

    while (1) {
        saddr_size = sizeof saddr;
        // Receive a packet
        data_size = recvfrom(sock_raw, buffer, 65536, 0, &saddr, (socklen_t *)&saddr_size);
        //        printf("data_size: %d\n", data_size);
        //        fflush(stdout);
        if (data_size < 0) {
            printf("Recvfrom error , failed to get packets\n");
            return 1;
        }
        // Now process the packet
        //        PrintData(buffer, data_size);
        print_tcp_packet(buffer, data_size);
        //        ProcessPacket(buffer, data_size);
    }
    close(sock_raw);
    printf("Finished");
    return 0;
}

void ProcessPacket(unsigned char *buffer, int size) {
    // Get the IP Header part of this packet , excluding the ethernet header
    struct iphdr *iph = (struct iphdr *)(buffer + sizeof(struct ethhdr));
    ++total;
    switch (iph->protocol)  // Check the Protocol and do accordingly...
    {
        //    case 1:  // ICMP Protocol
        //        ++icmp;
        //        print_icmp_packet(buffer, size);
        //        break;

        //    case 2:  // IGMP Protocol
        //        ++igmp;
        //        break;

    case 6:  // TCP Protocol
        ++tcp;

        print_tcp_packet(buffer, size);
        break;

        //    case 17:  // UDP Protocol
        //        ++udp;
        //        print_udp_packet(buffer, size);
        //        break;

        //    default:  // Some Other Protocol like ARP etc.
        //        ++others;
        //        break;
    }
    //    printf("TCP : %d   UDP : %d   ICMP : %d   IGMP : %d   Others : %d   Total : %d\r", tcp, udp, icmp, igmp, others, total);
}

void print_ethernet_header(unsigned char *Buffer, int Size) {
    struct ethhdr *eth = (struct ethhdr *)Buffer;

    fprintf(logfile, "\n");
    fprintf(logfile, "Ethernet Header\n");
    fprintf(logfile, "   |-Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3],
            eth->h_dest[4], eth->h_dest[5]);
    fprintf(logfile, "   |-Source Address      : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3],
            eth->h_source[4], eth->h_source[5]);
    fprintf(logfile, "   |-Protocol            : %u \n", (unsigned short)eth->h_proto);
}

void print_ip_header(unsigned char *Buffer, int Size) {
    //    print_ethernet_header(Buffer, Size);

    unsigned short iphdrlen;

    struct iphdr *iph = (struct iphdr *)(Buffer + sizeof(struct ethhdr));
    iphdrlen          = iph->ihl * 4;

    memset(&source, 0, sizeof(source));
    source.sin_addr.s_addr = iph->saddr;

    memset(&dest, 0, sizeof(dest));
    dest.sin_addr.s_addr = iph->daddr;

    fprintf(logfile, "\n");
    fprintf(logfile, "IP Header\n");
    fprintf(logfile, "   |-IP Version        : %d\n", (unsigned int)iph->version);
    fprintf(logfile, "   |-IP Header Length  : %d DWORDS or %d Bytes\n", (unsigned int)iph->ihl, ((unsigned int)(iph->ihl)) * 4);
    fprintf(logfile, "   |-Type Of Service   : %d\n", (unsigned int)iph->tos);
    fprintf(logfile, "   |-IP Total Length   : %d  Bytes(Size of Packet)\n", ntohs(iph->tot_len));
    fprintf(logfile, "   |-Identification    : %d\n", ntohs(iph->id));
    // fprintf(logfile , "   |-Reserved ZERO Field   : %d\n",(unsigned int)iphdr->ip_reserved_zero);
    // fprintf(logfile , "   |-Dont Fragment Field   : %d\n",(unsigned int)iphdr->ip_dont_fragment);
    // fprintf(logfile , "   |-More Fragment Field   : %d\n",(unsigned int)iphdr->ip_more_fragment);
    fprintf(logfile, "   |-TTL      : %d\n", (unsigned int)iph->ttl);
    fprintf(logfile, "   |-Protocol : %d\n", (unsigned int)iph->protocol);
    fprintf(logfile, "   |-Checksum : %d\n", ntohs(iph->check));
    fprintf(logfile, "   |-Source IP        : %s\n", inet_ntoa(source.sin_addr));
    fprintf(logfile, "   |-Destination IP   : %s\n", inet_ntoa(dest.sin_addr));
}

void print_tcp_packet(unsigned char *Buffer, int Size) {
    unsigned short iphdrlen;

    struct iphdr *iph = (struct iphdr *)(Buffer + sizeof(struct ethhdr));
    iphdrlen          = iph->ihl * 4;

    struct tcphdr *tcph = (struct tcphdr *)(Buffer + iphdrlen + sizeof(struct ethhdr));

    if (tcph->dest != htons(80) && tcph->source != htons(80)) return;

    //    if (tcph->source == htons(9229) || tcph->dest == htons(9229)) return;

    int header_size = sizeof(struct ethhdr) + iphdrlen + tcph->doff * 4;
    //    int header_size = iphdrlen + tcph->doff * 4;

    unsigned char *data_buf  = (unsigned char *)(Buffer + header_size);
    int            data_size = Size - header_size;

    //    if (data_size < 4) return;

    if (CNTR-- == 0) exit(0);
    //    if (data_size < 4) return;

    fprintf(logfile, "\n\n***********************TCP Packet*************************\n");

    print_ip_header(Buffer, Size);

    fprintf(logfile, "\n");
    fprintf(logfile, "TCP Header\n");
    fprintf(logfile, "   |-Source Port      : %u\n", ntohs(tcph->source));
    fprintf(logfile, "   |-Destination Port : %u\n", ntohs(tcph->dest));
    fprintf(logfile, "   |-Sequence Number    : %u\n", ntohl(tcph->seq));
    fprintf(logfile, "   |-Acknowledge Number : %u\n", ntohl(tcph->ack_seq));
    fprintf(logfile, "   |-Header Length      : %d DWORDS or %d BYTES\n", (unsigned int)tcph->doff, (unsigned int)tcph->doff * 4);
    // fprintf(logfile , "   |-CWR Flag : %d\n",(unsigned int)tcph->cwr);
    // fprintf(logfile , "   |-ECN Flag : %d\n",(unsigned int)tcph->ece);
    fprintf(logfile, "   |-Urgent Flag          : %d\n", (unsigned int)tcph->urg);
    fprintf(logfile, "   |-Acknowledgement Flag : %d\n", (unsigned int)tcph->ack);
    fprintf(logfile, "   |-Push Flag            : %d\n", (unsigned int)tcph->psh);
    fprintf(logfile, "   |-Reset Flag           : %d\n", (unsigned int)tcph->rst);
    fprintf(logfile, "   |-Synchronise Flag     : %d\n", (unsigned int)tcph->syn);
    fprintf(logfile, "   |-Finish Flag          : %d\n", (unsigned int)tcph->fin);
    fprintf(logfile, "   |-Window         : %d\n", ntohs(tcph->window));
    fprintf(logfile, "   |-Checksum       : %d\n", ntohs(tcph->check));
    fprintf(logfile, "   |-Urgent Pointer : %d\n", tcph->urg_ptr);
    fprintf(logfile, "\n");
    fprintf(logfile, "                        DATA Dump                         ");
    fprintf(logfile, "\n");

    //    fprintf(logfile, "IP Header\n");
    //    PrintData(Buffer, iphdrlen);

    fprintf(logfile, "TCP Header\n");
    PrintData(Buffer + iphdrlen, tcph->doff * 4);

    fprintf(logfile, "Data Payload\n");
    PrintData(data_buf, data_size);
    fflush(stdout);

    //    fprintf(logfile, "\n###########################################################");
}

void PrintData(unsigned char *data, int Size) {
    int i, j;
    printf("Size: %d\n", Size);
    for (i = 0; i < Size; i++) {
        if (i != 0 && i % 16 == 0)  // if one line of hex printing is complete...
        {
            fprintf(logfile, "         ");
            for (j = i - 16; j < i; j++) {
                if (data[j] >= 32 && data[j] <= 128)
                    fprintf(logfile, "%c", (unsigned char)data[j]);  // if its a number or alphabet

                else
                    fprintf(logfile, ".");  // otherwise print a dot
            }
            fprintf(logfile, "\n");
        }

        if (i % 16 == 0) fprintf(logfile, "   ");
        fprintf(logfile, " %02X", (unsigned int)data[i]);

        if (i == Size - 1)  // print the last spaces
        {
            for (j = 0; j < 15 - i % 16; j++) {
                fprintf(logfile, "   ");  // extra spaces
            }

            fprintf(logfile, "         ");

            for (j = i - i % 16; j <= i; j++) {
                if (data[j] >= 32 && data[j] <= 128) {
                    fprintf(logfile, "%c", (unsigned char)data[j]);
                } else {
                    fprintf(logfile, ".");
                }
            }

            fprintf(logfile, "\n");
        }
    }
}
#endif
/*

MYTRACE(platform::socket(AF_INET, SOCK_RAW, 0));
MYTRACE(platform::socket(AF_PACKET, SOCK_PACKET, IPPROTO_TCP));
MYTRACE(platform::socket(AF_PACKET, SOCK_PACKET, 0));
MYTRACE(platform::socket(AF_INET, SOCK_PACKET, IPPROTO_TCP));
MYTRACE(platform::socket(AF_INET, SOCK_PACKET, 0));
MYTRACE(platform::socket(AF_PACKET, SOCK_RAW, IPPROTO_TCP));
MYTRACE(platform::socket(AF_PACKET, SOCK_RAW, 0));

return 0;
if (fd < 0) { fd = platform::socket(AF_INET, SOCK_RAW, 0); }
fmt::print("socket(): {}\n", fd);
fflush(stdout);
if (fd < 0) return 1;

addr.sin_family = AF_INET;
addr.sin_port   = htons(21285);
err             = platform::bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
fmt::print("bind(): {}\n", err);
fflush(stdout);
if (err < 0) return 1;

int qlen = 5;
platform::setsockopt(fd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));

err = platform::listen(fd, 512);
fmt::print("listen(): {}\n", err);
fflush(stdout);
if (err < 0) return 1;

sockaddr_in caddr;
socklen_t   caddr_len;

int pipes[2];
::pipe2(pipes, O_DIRECT);

void *resp_buf;
posix_memalign(&resp_buf, 4096, 4096);
if (resp_buf == nullptr) { assert(resp_buf); }

fmt::print("aaa\n");
fflush(stdout);

std::string resp(
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Content-Length: 2\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "{}");
memcpy(resp_buf, resp.data(), 4096);

iovec resp_vec;
resp_vec.iov_len  = 4096;
resp_vec.iov_base = resp_buf;

for (;;) {
    int cfd = platform::accept4(fd, reinterpret_cast<sockaddr *>(&caddr), &caddr_len, 0);

    vmsplice(pipes[1], &resp_vec, 1, SPLICE_F_GIFT);
    //        fmt::print("vmsplice(): {}\n", sss);
    //        fflush(stdout);

    if (cfd < 0) {
        fmt::print("accept(): {}\n", cfd);
        fflush(stdout);
        return 1;
    }

    std::string buf(8192, ' ');
    ssize_t     sz = platform::recv(cfd, buf.data(), buf.size(), 0);

    if (sz < 0) {
        fmt::print("read(): {}\n", sz);
        fflush(stdout);
        return 1;
    }

    sz = splice(pipes[0], nullptr, cfd, nullptr, resp.length(), SPLICE_F_MOVE);
    //        fmt::print("splice(): {}\n", sz);
    //        fflush(stdout);
    //        sz = platform::send(cfd, resp, strlen(resp), 0);
    if (sz < 0) return 1;

    platform::shutdown(cfd, SHUT_RDWR);
}
*/
