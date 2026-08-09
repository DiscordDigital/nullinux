/*********************************************************************
 * Minimal DHCP client
 *
 * What it does:
 *   1. Open a UDP socket on the client-side DHCP port (68).
 *   2. Build a DHCPDISCOVER packet and broadcast it on the local
 *      network.
 *   3. Use mac address of interface passed as argument.
 *   4. Wait (with a 5-second timeout) for a DHCPOFFER reply.
 *   5. When a reply arrives, extract the offered IP address, subnet
 *      mask, default gateway and DNS servers and print them in a
 *      format that can be `eval`-ed by a shell script.
 *
 * The code is deliberately tiny - it only implements the parts of
 * DHCP that are needed to obtain a lease.
 *
 * It is **not** a complete DHCP client.
 *********************************************************************/

#include <stdio.h>          // printf, fprintf
#include <string.h>         // memset, memcpy
#include <stdlib.h>         // exit, malloc, free
#include <unistd.h>         // close
#include <arpa/inet.h>      // htons, htonl, ntohs, ntohl, inet_ntoa
#include <sys/socket.h>     // socket, bind, sendto, recvfrom, setsockopt
#include <netinet/in.h>     // struct sockaddr_in, IPPROTO_UDP
#include <time.h>           // time()
#include <errno.h>          // errno, strerror
#include <net/if.h>        // struct ifreq
#include <sys/ioctl.h>     // ioctl

/* -----------------------------------------------------------------
 * DHCP constants (taken from the RFC)
 * ----------------------------------------------------------------- */
#define DHCP_CLIENT_PORT 68     /* UDP port a client listens on */
#define DHCP_SERVER_PORT 67     /* UDP port a server listens on */
#define DHCP_MAGIC 0x63825363   /* DHCP "magic cookie" - marks start of options */

/* DHCP message types (option 53) */
#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPACK 5

/* -----------------------------------------------------------------
 * The DHCP packet layout (RFC 2131, section 2)
 *
 * Only the fields we need are filled in - everything else is zeroed.
 * ----------------------------------------------------------------- */
struct dhcp_msg {
    uint8_t op;           /* Message opcode/type: 1 = BOOTREQUEST, 2 = BOOTREPLY */
    uint8_t htype;        /* Hardware address type: 1 = Ethernet */
    uint8_t hlen;         /* Hardware address length: 6 for Ethernet MACs */
    uint8_t hops;         /* Number of relay hops - set to 0 by a client */
    uint32_t xid;         /* Transaction ID, a random number chosen by the client */
    uint16_t secs;        /* Seconds elapsed since client began address acquisition */
    uint16_t flags;       /* Flags (bit 0 = broadcast flag) */
    uint32_t ciaddr;      /* Client IP address (filled in only if already has an address) */
    uint32_t yiaddr;      /* 'Your' (client) IP address - filled in by the server */
    uint32_t siaddr;      /* Next server IP address (used by DHCP relay agents) */
    uint32_t giaddr;      /* Relay agent IP address */
    uint8_t chaddr[16];   /* Client hardware address (MAC) - first 6 bytes used */
    uint8_t sname[64];    /* Optional server host name (not used) */
    uint8_t file[128];    /* Boot file name (not used) */
    uint32_t magic;       /* DHCP magic cookie - must be 0x63825363 */
    uint8_t options[312]; /* Variable-length options field (max 312 bytes) */
};

/* -----------------------------------------------------------------
 * Helper macros for option parsing - they make the code easier to read.
 * ----------------------------------------------------------------- */
#define OPTION_END          255   /* End of options marker */
#define OPTION_MESSAGE_TYPE  53   /* DHCP Message Type option */
#define OPTION_SUBNET_MASK    1   /* Subnet mask option */
#define OPTION_ROUTER         3   /* Default gateway option */
#define OPTION_DNS            6   /* DNS server list option */

/* -----------------------------------------------------------------
 * Main program
 * ----------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    if (argc < 2 || argv[1][0] == '\0') {
        printf("export error=\"No network interface specified\"\n");
        return 1;
    }
    
    char *netdev = argv[1];  // the interface name, later used when obtaining the mac
    
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/net/%s", netdev);

    // Exit the program early, when network interface doesn't exist
    if (access(path, F_OK) != 0) {
        printf("export error=\"Network interface '%s' not found\"\n", netdev);
        return 1;
    }
    
    /* -------------------------------------------------------------
     * 1) Create a UDP socket that we will use for both sending the
     *    broadcast DISCOVER and receiving the OFFER.
     * ------------------------------------------------------------- */
    int sock;
    struct sockaddr_in addr;
    struct dhcp_msg msg, reply;
    socklen_t addr_len = sizeof(addr);
    unsigned char *opt;
    int len;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        printf("export error=\"%s\"\n", strerror(errno));
        return 1;
    }

    /* -------------------------------------------------------------
     * 2) Enable broadcasting on the socket.  Without this the kernel
     *    would reject packets sent to 255.255.255.255.
     * ------------------------------------------------------------- */
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    /* -------------------------------------------------------------
     * 3) Bind the socket to the client DHCP port (68) on all local
     *    interfaces.  The client must listen on this port to receive
     *    replies from the server.
     * ------------------------------------------------------------- */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DHCP_CLIENT_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("export error=\"%s\"\n", strerror(errno));
        return 1;
    }

    /* -------------------------------------------------------------
     * 4) Build the DHCPDISCOVER packet.
     *
     *    Only the mandatory fields are filled; everything else is
     *    left as zero (thanks to the memset).  The transaction ID
     *    (xid) is generated from the current time - this is not
     *    cryptographically strong but is OK for this purpose.
     * ------------------------------------------------------------- */
    memset(&msg, 0, sizeof(msg));    // Zero-fill the whole structure

    msg.op = 1;                      // BOOTREQUEST
    msg.htype = 1;                   // Ethernet
    msg.hlen = 6;                    // MAC address length
    msg.xid = htonl(time(NULL));     // Transaction ID (network order)

    /* -------------------------------------------------------------
     * 5) Get the hardware mac address
     * ------------------------------------------------------------- */
    struct ifreq ifr;
    int fd;
    const char *iface = netdev;
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        printf("export error=\"%s\"\n", strerror(errno));
        return 1;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        printf("export error=\"%s\"\n", strerror(errno));
        close(fd);
        return 1;
    }
    
    memcpy(msg.chaddr, ifr.ifr_hwaddr.sa_data, 6); // set the first 6 bytes
    close(fd);

    /* The broadcast flag tells the server to broadcast its reply.
     * Bit 15 (0x8000) of the flags field is defined as the broadcast flag.
     */
    msg.flags = htons(0x8000);

    /* Magic cookie - tells the receiver that the following bytes are
     * DHCP options.
     */
    msg.magic = htonl(DHCP_MAGIC);

    /* -----------------------------------------------------------------
     * Fill the options field.
     *
     *   * Option 53 - DHCP Message Type = DHCPDISCOVER
     *   * Option 255 - End of options
     *
     * In a full client we would also send a Parameter Request List,
     * Host Name, Client Identifier, etc.  For the minimal client we only
     * need the message type.
     * ----------------------------------------------------------------- */
    opt = msg.options;
    *opt++ = OPTION_MESSAGE_TYPE; // 53
    *opt++ = 1;                   // length = 1 byte
    *opt++ = DHCPDISCOVER;        // value = 1 (DISCOVER)

    /* End option - tells the server there are no more options */
    *opt++ = OPTION_END;          // 255

    /* -------------------------------------------------------------
     * 6) Broadcast the DISCOVER packet to the well-known DHCP server
     *    port (67) on the broadcast address (255.255.255.255).
     * ------------------------------------------------------------- */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DHCP_SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_BROADCAST;

    if (sendto(sock, &msg, sizeof(msg), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("export error=\"%s\"\n", strerror(errno));
        return 1;
    }

    /* -------------------------------------------------------------
     * 7) Set a receive timeout of 5 seconds.  If no OFFER arrives,
     *    recvfrom() will return -1 with errno == EAGAIN/EWOULDBLOCK.
     * ------------------------------------------------------------- */
    struct timeval tv = {5, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* -------------------------------------------------------------
     * 8) Wait for a reply.  The loop is kept so that we can ignore
     *    packets that are not DHCP (wrong magic cookie) or that are
     *    not an OFFER.
     * ------------------------------------------------------------- */
    while (1) {
        len = recvfrom(sock, &reply, sizeof(reply), 0, (struct sockaddr *)&addr, &addr_len);
        if (len < 0) {
            /* Timeout or other error - report it and exit. */
            printf("export error=\"%s\"\n", strerror(errno));
            return 1;
        }

        /* Basic sanity check: the packet must contain the DHCP magic
         * cookie.  If it does not, it is either not a DHCP packet or
         * it is corrupted - just ignore it and keep waiting. */
        if (ntohl(reply.magic) != DHCP_MAGIC)
            continue;

        /* ---------------------------------------------------------
         * 9) Parse the options field looking for the data we care
         *    about.  The DHCP options are TLV-encoded:
         *
         *        Type  Length  Value...
         *
         *    We walk the array until we see the END marker (255)
         *    or run out of space.
         * --------------------------------------------------------- */

        // If we reach this point we have a valid DHCP packet - clear any previous error
        printf("unset error\n");

        for (opt = reply.options; *opt != OPTION_END; opt += opt[1] + 2) {
            // IP Address
            if (*opt == OPTION_MESSAGE_TYPE && opt[2] == DHCPOFFER) {
                struct in_addr ip = { reply.yiaddr };
                printf("export address=%s\n", inet_ntoa(ip));
            }

            // Subnet mask (CIDR)
            if (*opt == OPTION_SUBNET_MASK && opt[1] == 4) {
                int mask = ntohl(*(uint32_t *)(opt + 2));
                int cidr = 0;
                while (mask & 0x80000000) { cidr++; mask <<= 1; }
                printf("export cidr=%d\n", cidr);
            }

            // Router Gateway
            if (*opt == OPTION_ROUTER && opt[1] == 4) {
                struct in_addr gw = { *(uint32_t *)(opt + 2) };
                printf("export gateway=%s\n", inet_ntoa(gw));
            }

            // DNS Options
            if (*opt == OPTION_DNS) {  // DNS servers
                int num = opt[1] / 4;  // 4 bytes per IP
                for (int i = 0; i < num; i++) {
                    struct in_addr dns = { *(uint32_t *)(opt + 2 + i*4) };
                    printf("export dns%d=%s\n", i + 1, inet_ntoa(dns));
                }
            }
        }
        break;
    }

    close(sock);
    return 0;
}
