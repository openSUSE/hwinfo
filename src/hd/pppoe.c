

// gcc test2.c -o test2 -Wall -O2 -std=c99


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <linux/if.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>

#include "hd.h"
#include "hd_int.h"
#include "pppoe.h"

static hd_data_t *hd_data;

/* Ethernet Frame Types */
#define ETH_PPPOE_DISCOVERY	0x8863
#define ETH_PPPOE_SESSION	0x8864

/* PPPoE Codes */
#define CODE_PADI		0x09
#define CODE_PADO		0x07
#define CODE_PADR		0x19
#define CODE_PADS		0x65
#define CODE_PADT		0xA7

/* PPPoE Tags */
#define TAG_END_OF_LIST		0x0000
#define TAG_SERVICE_NAME	0x0101
#define TAG_AC_NAME		0x0102
#define TAG_HOST_UNIQ		0x0103
#define TAG_AC_COOKIE		0x0104
#define TAG_VENDOR_SPECIFIC	0x0105
#define TAG_RELAY_SESSION_ID	0x0110
#define TAG_SERVICE_NAME_ERROR	0x0201
#define TAG_AC_SYSTEM_ERROR	0x0202
#define TAG_GENERIC_ERROR	0x0203

/* Number of PADI Attempts */
#define MAX_PADI_ATTEMPTS	2

/* Timeout for PADO */
#define PADO_TIMEOUT		3

/* A PPPoE Packet, including Ethernet headers */
typedef struct PPPoEPacketStruct {
    struct ethhdr ethHdr;	/* Ethernet header */
#ifdef PACK_BITFIELDS_REVERSED
    unsigned int type:4;	/* PPPoE Type (must be 1) */
    unsigned int ver:4;		/* PPPoE Version (must be 1) */
#else
    unsigned int ver:4;		/* PPPoE Version (must be 1) */
    unsigned int type:4;	/* PPPoE Type (must be 1) */
#endif
    unsigned int code:8;	/* PPPoE code */
    unsigned int session:16;	/* PPPoE session */
    unsigned int length:16;	/* Payload length */
    unsigned char payload[ETH_DATA_LEN];	/* A bit of room to spare */
} PPPoEPacket;

/* Header size of a PPPoE packet */
#define PPPOE_OVERHEAD 6	/* type, code, session, length */

#define HDR_SIZE (sizeof (struct ethhdr) + PPPOE_OVERHEAD)
#define MAX_PPPOE_PAYLOAD (ETH_DATA_LEN - PPPOE_OVERHEAD)

/* PPPoE Tag */

typedef struct PPPoETagStruct {
    unsigned int type:16;	/* tag type */
    unsigned int length:16;	/* Length of payload */
    unsigned char payload[ETH_DATA_LEN];	/* A LOT of room to spare */
} PPPoETag;

/* Header size of a PPPoE tag */
#define TAG_HDR_SIZE 4


/* Function passed to parse_packet. */
typedef void parse_func (uint16_t type, uint16_t len,
			 unsigned char* data, void* extra);


/* Keep track of the state of a connection -- collect everything in
   one spot */

typedef struct PPPoEConnectionStruct {
    int received_pado;		/* Where we are in discovery */
    int fd;			/* Raw socket for discovery frames */
    unsigned char my_mac[ETH_ALEN];	/* My MAC address */
    unsigned char peer_mac[ETH_ALEN];	/* Peer's MAC address */
    char* ifname;		/* Interface name */
    hd_t *hd;
} PPPoEConnection;

/* Structure used to determine acceptable PADO packet */
struct PacketCriteria {
    PPPoEConnection* conn;
    int acNameOK;
    int serviceNameOK;
    int error;
};


int
check_room (unsigned char* cursor, unsigned char* start, uint16_t len)
{
    if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) {
	ADD2LOG ("Would create too-long packet\n");
	return 0;
    }
    return 1;
}


/* True if Ethernet address is broadcast or multicast */
#define NOT_UNICAST(e) ((e[0] & 0x01) != 0)
#define BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) == 0xFF)
#define NOT_BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) != 0xFF)


int
parse_packet (PPPoEPacket* packet, parse_func* func, void* extra)
{
    uint16_t len = ntohs (packet->length);
    unsigned char* curTag;
    uint16_t tagType, tagLen;

    if (packet->ver != 1) {
	ADD2LOG ("invalid PPPoE version (%d)\n", (int) packet->ver);
	return 0;
    }

    if (packet->type != 1) {
	ADD2LOG ("invalid PPPoE type (%d)\n", (int) packet->type);
	return 0;
    }

    /* Do some sanity checks on packet. */
    if (len > ETH_DATA_LEN - 6) { /* 6-byte overhead for PPPoE header */
	ADD2LOG ("invalid PPPoE packet length (%u)\n", len);
	return 0;
    }

    /* Step through the tags. */
    curTag = packet->payload;
    while (curTag - packet->payload < len) {
	/* Alignment is not guaranteed, so do this by hand. */
	tagType = (((uint16_t) curTag[0]) << 8) + (uint16_t) curTag[1];
	tagLen = (((uint16_t) curTag[2]) << 8) + (uint16_t) curTag[3];
	if (tagType == TAG_END_OF_LIST)
	    break;
	if ((curTag - packet->payload) + tagLen + TAG_HDR_SIZE > len) {
	    ADD2LOG ("Invalid PPPoE tag length (%u)\n", tagLen);
	    return 0;
	}
	func (tagType, tagLen, curTag + TAG_HDR_SIZE, extra);
	curTag = curTag + TAG_HDR_SIZE + tagLen;
    }

    return 1;
}


int
open_interfaces (int n, PPPoEConnection* conns)
{
    int ret = 0, i;

    for (i = 0; i < n; i++)
    {
	PPPoEConnection* conn = &conns[i];

	conn->fd = socket (PF_PACKET, SOCK_RAW, htons (ETH_PPPOE_DISCOVERY));
	if (conn->fd < 0) {
	    ADD2LOG ("socket: %m\n");
	    continue;
	}

	ADD2LOG ("%s %d\n", conn->ifname, conn->fd);

	int one = 1;
	if (setsockopt (conn->fd, SOL_SOCKET, SO_BROADCAST, &one,
			sizeof (one)) < 0) {
	    ADD2LOG ("setsockopt: %m\n");
	    goto error;
	}

	/* Fill in hardware address */
	struct ifreq ifr;
	struct sockaddr_ll sa;
	memset (&sa, 0, sizeof (sa));
	strncpy (ifr.ifr_name, conn->ifname, sizeof (ifr.ifr_name));
	if (ioctl (conn->fd, SIOCGIFHWADDR, &ifr) < 0) {
	    ADD2LOG ("ioctl (SIOCGIFHWADDR): %m\n");
	    goto error;
	}

	memcpy (conn->my_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
	    ADD2LOG ("Interface %s is not Ethernet\n", conn->ifname);
	    goto error;
	}

	if (NOT_UNICAST (conn->my_mac)) {
	    ADD2LOG ("Interface %s has broadcast/multicast MAC address?\n",
		    conn->ifname);
	    goto error;
	}

	/* Sanity check on MTU */
	strncpy (ifr.ifr_name, conn->ifname, sizeof (ifr.ifr_name));
	if (ioctl (conn->fd, SIOCGIFMTU, &ifr) < 0) {
	    ADD2LOG ("ioctl(SIOCGIFMTU): %m\n");
	    goto error;
	}
	if (ifr.ifr_mtu < ETH_DATA_LEN) {
	    ADD2LOG ("interface has to low MTU\n");
	    goto error;
	}

	/* Get interface index */
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons (ETH_PPPOE_DISCOVERY);
	strncpy (ifr.ifr_name, conn->ifname, sizeof (ifr.ifr_name));
	if (ioctl (conn->fd, SIOCGIFINDEX, &ifr) < 0) {
	    ADD2LOG ("ioctl(SIOCFIGINDEX): Could not get interface index\n");
	    goto error;
	}
	sa.sll_ifindex = ifr.ifr_ifindex;

	/* We're only interested in packets on specified interface */
	if (bind (conn->fd, (struct sockaddr *) &sa, sizeof (sa)) < 0) {
	    ADD2LOG ("bind: %m\n");
	    goto error;
	}

	ret = 1;
	continue;

error:

	close (conn->fd);
	conn->fd = -1;

    }

    return ret;
}


void
close_intefaces (int n, PPPoEConnection* conns)
{
    int i;

    for (i = 0; i < n; i++)
    {
	PPPoEConnection* conn = &conns[i];

	if (conn->fd != -1) {
	    close (conn->fd);
	    conn->fd = -1;
	}
    }
}


int
send_packet (int fd, PPPoEPacket* pkt, int size)
{
    if (send (fd, pkt, size, 0) < 0) {
	ADD2LOG ("send %d\n", fd);
	return 0;
    }

    return 1;
}


int
receive_packet (int fd, PPPoEPacket* pkt, int* size)
{
    if ((*size = recv (fd, pkt, sizeof (PPPoEPacket), 0)) < 0) {
	ADD2LOG ("recv %d\n", fd);
	return 0;
    }

    return 1;
}


/**********************************************************************
*%FUNCTION: parseForHostUniq
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data.
* extra -- user-supplied pointer.  This is assumed to be a pointer to int.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* If a HostUnique tag is found which matches our PID, sets *extra to 1.
***********************************************************************/
void
parseForHostUniq (uint16_t type, uint16_t len, unsigned char* data, void* extra)
{
    int* val = (int*) extra;
    if (type == TAG_HOST_UNIQ && len == sizeof (pid_t)) {
	pid_t tmp;
	memcpy (&tmp, data, len);
	if (tmp == getpid ()) {
	    *val = 1;
	}
    }
}


/**********************************************************************
*%FUNCTION: packetIsForMe
*%ARGUMENTS:
* conn -- PPPoE connection info
* packet -- a received PPPoE packet
*%RETURNS:
* 1 if packet is for this PPPoE daemon; 0 otherwise.
*%DESCRIPTION:
* If we are using the Host-Unique tag, verifies that packet contains
* our unique identifier.
***********************************************************************/
int
packetIsForMe (PPPoEConnection* conn, PPPoEPacket* packet)
{
    /* If packet is not directed to our MAC address, forget it. */
    if (memcmp (packet->ethHdr.h_dest, conn->my_mac, ETH_ALEN))
	return 0;

    /* Check for HostUniq tag. */
    int forMe = 0;
    parse_packet (packet, parseForHostUniq, &forMe);
    return forMe;
}


/**********************************************************************
*%FUNCTION: parsePADOTags
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data.  Should point to a PacketCriteria structure
*          which gets filled in according to selected AC name and service
*          name.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks interesting tags out of a PADO packet
***********************************************************************/
void
parsePADOTags (uint16_t type, uint16_t len, unsigned char* data, void* extra)
{
    struct PacketCriteria* pc = (struct PacketCriteria*) extra;
    PPPoEConnection *conn = pc->conn;

    switch (type) {
	case TAG_AC_NAME:
	    pc->acNameOK = 1;
	    ADD2LOG ("%s: service-name: %.*s\n", conn->ifname, (int) len, data);
	    break;
	case TAG_SERVICE_NAME:
	    pc->serviceNameOK = len == 0;
	    break;
	case TAG_SERVICE_NAME_ERROR:
	    ADD2LOG ("PADO: Service-Name-Error: %.*s\n", (int) len, data);
	    pc->error = 1;
	    break;
	case TAG_AC_SYSTEM_ERROR:
	    ADD2LOG ("PADO: System-Error: %.*s\n", (int) len, data);
	    pc->error = 1;
	    break;
	case TAG_GENERIC_ERROR:
	    ADD2LOG ("PADO: Generic-Error: %.*s\n", (int) len, data);
	    pc->error = 1;
	    break;
    }
}


int
sendPADI (int n, PPPoEConnection* conns)
{
    int ret = 0, i;

    for (i = 0; i < n; i++)
    {
	PPPoEConnection* conn = &conns[i];

	if (conn->fd == -1 || conn->received_pado)
	    continue;

	PPPoEPacket packet;
	unsigned char* cursor = packet.payload;
	PPPoETag* svc = (PPPoETag*) (&packet.payload);
	uint16_t namelen = 0;
	uint16_t plen;

	namelen = 0;
	plen = TAG_HDR_SIZE + namelen;
	if (!check_room (cursor, packet.payload, TAG_HDR_SIZE))
	    continue;

	/* Set destination to Ethernet broadcast address */
	memset (packet.ethHdr.h_dest, 0xFF, ETH_ALEN);
	memcpy (packet.ethHdr.h_source, conn->my_mac, ETH_ALEN);

	packet.ethHdr.h_proto = htons (ETH_PPPOE_DISCOVERY);
	packet.ver = 1;
	packet.type = 1;
	packet.code = CODE_PADI;
	packet.session = 0;

	svc->type = TAG_SERVICE_NAME;
	svc->length = htons (0);
	if (!check_room (cursor, packet.payload, namelen + TAG_HDR_SIZE))
	    continue;

	cursor += namelen + TAG_HDR_SIZE;

	PPPoETag hostUniq;
	pid_t pid = getpid ();
	hostUniq.type = htons (TAG_HOST_UNIQ);
	hostUniq.length = htons (sizeof (pid));
	memcpy (hostUniq.payload, &pid, sizeof (pid));
	if (!check_room (cursor, packet.payload, sizeof (pid) + TAG_HDR_SIZE))
	    continue;
	memcpy (cursor, &hostUniq, sizeof (pid) + TAG_HDR_SIZE);
	cursor += sizeof (pid) + TAG_HDR_SIZE;
	plen += sizeof (pid) + TAG_HDR_SIZE;

	packet.length = htons (plen);

	ADD2LOG ("sending padi for %s\n", conn->ifname);

	if (send_packet (conn->fd, &packet, (int) (plen + HDR_SIZE)))
	    ret = 1;
    }

    return ret;
}


int
waitForPADO (int n, PPPoEConnection* conns)
{
    int r, i, all, len;
    PPPoEPacket packet;
    struct PacketCriteria pc;

    struct timeval tv;
    tv.tv_sec = PADO_TIMEOUT;
    tv.tv_usec = 0;

    while (1)
    {
	fd_set readable;
	FD_ZERO (&readable);

	for (i = 0; i < n; i++)
	    if (conns[i].fd != -1)
		FD_SET (conns[i].fd, &readable);

	do {
	    r = select (FD_SETSIZE, &readable, NULL, NULL, &tv);
	} while (r == -1 && errno == EINTR);

	if (r < 0) {
	    ADD2LOG ("select: %m\n");
	    return 0;
	}

	if (r == 0) {
	    ADD2LOG ("timeout\n");
	    return 0;
	}

	for (i = 0; i < n; i++)
	{
	    PPPoEConnection* conn = &conns[i];

	    if (conn->fd == -1 || !FD_ISSET (conn->fd, &readable))
		continue;

	    pc.conn = conn;
	    pc.acNameOK = 0;
	    pc.serviceNameOK = 0;
	    pc.error = 0;

	    /* Get the packet */
	    if (!receive_packet (conn->fd, &packet, &len))
		continue;

	    /* Check length */
	    if (ntohs (packet.length) + HDR_SIZE > len) {
		ADD2LOG ("Bogus PPPoE length field (%u)\n",
			(unsigned int) ntohs (packet.length));
		continue;
	    }

	    /* If it's not for us, loop again */
	    if (!packetIsForMe (conn, &packet))
		continue;

	    if (packet.code != CODE_PADO)
		continue;

	    if (NOT_UNICAST (packet.ethHdr.h_source)) {
		ADD2LOG ("Ignoring PADO packet from non-unicast MAC address\n");
		continue;
	    }

	    parse_packet (&packet, parsePADOTags, &pc);

	    if (!pc.acNameOK) {
		ADD2LOG ("wrong or missing AC-Name tag\n");
		continue;
	    }

	    if (!pc.serviceNameOK) {
		ADD2LOG ("wrong or missing Service-Name tag\n");
		continue;
	    }

	    if (pc.error) {
		ADD2LOG ("Ignoring PADO packet with error tag\n");
		continue;
	    }

	    memcpy (conn->peer_mac, packet.ethHdr.h_source, ETH_ALEN);
	    ADD2LOG ("receive pado for %s\n", conn->ifname);
	    conn->received_pado = 1;
	}

	all = 1;
	for (i = 0; i < n; i++)
	    if (conns[i].fd != -1 && !conns[i].received_pado)
		all = 0;
	if (all)
	    return 1;
    }
}


void
discovery (int n, PPPoEConnection* conns)
{
    int a;

    if (open_interfaces (n, conns))
    {
	for (a = 0; a < MAX_PADI_ATTEMPTS; a++)
	{
	    ADD2LOG ("attemp %d\n", a + 1);

	    if (!sendPADI (n, conns))
		break;

	    if (waitForPADO (n, conns))
		break;
	}
    }

    close_intefaces (n, conns);
}


void hd_scan_pppoe(hd_data_t *hd_data2)
{
  hd_t *hd;
  int cnt, interfaces;
  PPPoEConnection *conn;

  hd_data = hd_data2;

  if(!hd_probe_feature(hd_data, pr_pppoe)) return;

  hd_data->module = mod_pppoe;

  PROGRESS(1, 0, "looking for pppoe");

  for(interfaces = 0, hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_network_interface &&
      hd->unix_dev_name &&
      !strncmp(hd->unix_dev_name, "eth", 3)
    ) {
      interfaces++;
    }
  }

  if(!interfaces) return;

  conn = new_mem(interfaces * sizeof *conn);

  for(cnt = 0, hd = hd_data->hd; hd && cnt < interfaces; hd = hd->next) {
    if(
      hd->base_class.id == bc_network_interface &&
      hd->unix_dev_name &&
      !strncmp(hd->unix_dev_name, "eth", 3)
    ) {
      conn[cnt].hd = hd;
      conn[cnt].fd = -1;
      conn[cnt].ifname = hd->unix_dev_name;
      cnt++;
    }
  }

  PROGRESS(2, 0, "discovery");

  discovery(interfaces, conn);

  for(cnt = 0; cnt < interfaces; cnt++) {
    conn[cnt].hd->is.pppoe = 0;

    if(conn[cnt].received_pado) {
      conn[cnt].hd->is.pppoe = 1;
      ADD2LOG(
        "pppoe %s: my mac %02x:%02x:%02x:%02x:%02x:%02x, "
        "peer mac %02x:%02x:%02x:%02x:%02x:%02x\n",
        conn[cnt].ifname,
        conn[cnt].my_mac[0], conn[cnt].my_mac[1], conn[cnt].my_mac[2],
        conn[cnt].my_mac[3], conn[cnt].my_mac[4], conn[cnt].my_mac[5],
        conn[cnt].peer_mac[0], conn[cnt].peer_mac[1], conn[cnt].peer_mac[2],
        conn[cnt].peer_mac[3], conn[cnt].peer_mac[4], conn[cnt].peer_mac[5]
      );
    }
  }
}

