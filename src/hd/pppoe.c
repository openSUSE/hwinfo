
/*
 *  License: GPL
 *
 *  Much inspired by rp-pppoe from Roaring Penguin Software Inc.
 *  which itself is inspired by earlier code from Luke Stras.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
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

/**
 * @defgroup PPPOEint PPPoE devices (DSL)
 * @ingroup libhdDEVint
 * @brief PPPoE devices scan functions
 *
 * @{
 */

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

/* Number of Attempts */
#define MAX_ATTEMPTS		2

/* Timeout for PADO Packets */
#define PADO_TIMEOUT		3

/* A PPPoE Packet, including Ethernet headers */
typedef struct PPPoEPacketStruct {
    struct ethhdr ethHdr;	/* Ethernet header */
    unsigned int ver:4;		/* PPPoE Version (must be 1) */
    unsigned int type:4;	/* PPPoE Type (must be 1) */
    unsigned int code:8;	/* PPPoE code */
    unsigned int session:16;	/* PPPoE session */
    unsigned int length:16;	/* Payload length */
    unsigned char payload[ETH_DATA_LEN];	/* A bit of room to spare */
} PPPoEPacket;

/* Header size of a PPPoE Packet */
#define PPPOE_OVERHEAD 6	/* type, code, session, length */
#define HDR_SIZE (sizeof (struct ethhdr) + PPPOE_OVERHEAD)
#define MAX_PPPOE_PAYLOAD (ETH_DATA_LEN - PPPOE_OVERHEAD)

/* PPPoE Tag */
typedef struct PPPoETagStruct {
    unsigned int type:16;	/* tag type */
    unsigned int length:16;	/* Length of payload */
    unsigned char payload[ETH_DATA_LEN];	/* A LOT of room to spare */
} PPPoETag;

/* Header size of a PPPoE Tag */
#define TAG_HDR_SIZE 4

/* Function passed to parse_packet */
typedef void parse_func (uint16_t type, uint16_t len,
			 unsigned char* data, void* extra);

/* Keep track of the state of a connection */
typedef struct PPPoEConnectionStruct {
    char* ifname;		/* Interface name */
    int fd;			/* Raw socket for discovery frames */
    int received_pado;		/* Where we are in discovery */
    unsigned char my_mac[ETH_ALEN];	/* My MAC address */
    unsigned char peer_mac[ETH_ALEN];	/* Peer's MAC address */
    hd_t *hd;
} PPPoEConnection;

/* Structure used to determine acceptable PADO packet */
typedef struct PacketCriteriaStruct {
    PPPoEConnection* conn;
    int acname_ok;
    int servicename_ok;
    int error;
} PacketCriteria;

/* True if Ethernet address is broadcast or multicast */
#define NOT_UNICAST(e) ((e[0] & 0x01) != 0)


static int
check_room (PPPoEConnection* conn, unsigned char* cursor, unsigned char* start,
	    uint16_t len)
{
    if (cursor - start + len > MAX_PPPOE_PAYLOAD) {
	ADD2LOG ("%s: Would create too-long packet\n", conn->ifname);
	return 0;
    }
    return 1;
}


static int
parse_packet (PPPoEConnection* conn, PPPoEPacket* packet, parse_func* func,
	      void* extra)
{
    uint16_t len = ntohs (packet->length);
    unsigned char* curTag;
    uint16_t tagType, tagLen;

    if (packet->ver != 1) {
	ADD2LOG ("%s: Invalid PPPoE version (%d)\n", conn->ifname,
		 (int) packet->ver);
	return 0;
    }

    if (packet->type != 1) {
	ADD2LOG ("%s: Invalid PPPoE type (%d)\n", conn->ifname,
		 (int) packet->type);
	return 0;
    }

    /* Do some sanity checks on packet. */
    if (len > ETH_DATA_LEN - 6) { /* 6-byte overhead for PPPoE header */
	ADD2LOG ("%s: Invalid PPPoE packet length (%u)\n", conn->ifname, len);
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
	    ADD2LOG ("%s: Invalid PPPoE tag length (%u)\n", conn->ifname,
		     tagLen);
	    return 0;
	}
	func (tagType, tagLen, curTag + TAG_HDR_SIZE, extra);
	curTag = curTag + TAG_HDR_SIZE + tagLen;
    }

    return 1;
}


static int
open_interfaces (int n, PPPoEConnection* conns)
{
    int ret = 0, i;

    for (i = 0; i < n; i++)
    {
	PPPoEConnection* conn = &conns[i];

	conn->fd = socket (PF_PACKET, SOCK_RAW, htons (ETH_PPPOE_DISCOVERY));
	if (conn->fd < 0) {
	    ADD2LOG ("%s: socket failed: %m\n", conn->ifname);
	    continue;
	}

	int one = 1;
	if (setsockopt (conn->fd, SOL_SOCKET, SO_BROADCAST, &one,
			sizeof (one)) < 0) {
	    ADD2LOG ("%s: setsockopt failed: %m\n", conn->ifname);
	    goto error;
	}

	/* Fill in hardware address */
	struct ifreq ifr = {};
	struct sockaddr_ll sa;
	memset (&sa, 0, sizeof (sa));
	strncpy (ifr.ifr_name, conn->ifname, sizeof (ifr.ifr_name) - 1);
	if (ioctl (conn->fd, SIOCGIFHWADDR, &ifr) < 0) {
	    ADD2LOG ("%s: ioctl (SIOCGIFHWADDR) failed: %m\n", conn->ifname);
	    goto error;
	}

	memcpy (conn->my_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
	    ADD2LOG ("%s: Interface is not ethernet\n", conn->ifname);
	    goto error;
	}

	if (NOT_UNICAST (conn->my_mac)) {
	    ADD2LOG ("%s: Interface has broadcast/multicast MAC address?\n",
		    conn->ifname);
	    goto error;
	}

	/* Sanity check on MTU */
	strncpy (ifr.ifr_name, conn->ifname, sizeof (ifr.ifr_name) - 1);
	if (ioctl (conn->fd, SIOCGIFMTU, &ifr) < 0) {
	    ADD2LOG ("%s: ioctl (SIOCGIFMTU) failed: %m\n", conn->ifname);
	    goto error;
	}
	if (ifr.ifr_mtu < ETH_DATA_LEN) {
	    ADD2LOG ("%s: Interface has too low MTU\n", conn->ifname);
	    goto error;
	}

	/* Skip interfaces that have the SLAVE flag set */
	strncpy (ifr.ifr_name, conn->ifname, sizeof (ifr.ifr_name) - 1);
	if (ioctl (conn->fd, SIOCGIFFLAGS, &ifr) < 0) {
		ADD2LOG ("%s: ioctl (SIOCGIFFLAGS) failed: %m\n", conn->ifname);
		goto error;
	}
	if (ifr.ifr_ifru.ifru_flags & IFF_SLAVE) {
		ADD2LOG ("%s: Interface has SLAVE flag set\n", conn->ifname);
		goto error;
	}

	/* Get interface index */
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons (ETH_PPPOE_DISCOVERY);
	strncpy (ifr.ifr_name, conn->ifname, sizeof (ifr.ifr_name) - 1);
	if (ioctl (conn->fd, SIOCGIFINDEX, &ifr) < 0) {
	    ADD2LOG ("%s: ioctl (SIOCFIGINDEX) failed: Could not get interface "
		     "index\n", conn->ifname);
	    goto error;
	}
	sa.sll_ifindex = ifr.ifr_ifindex;

	/* We're only interested in packets on specified interface */
	if (bind (conn->fd, (struct sockaddr*) &sa, sizeof (sa)) < 0) {
	    ADD2LOG ("%s: bind failed: %m\n", conn->ifname);
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


static void
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


static int
send_packet (int fd, PPPoEPacket* pkt, size_t size)
{
    if (send (fd, pkt, size, 0) < 0) {
	ADD2LOG ("send failed: %m\n");
	return 0;
    }

    return 1;
}


static int
receive_packet (int fd, PPPoEPacket* pkt, size_t* size)
{
    int r = recv (fd, pkt, sizeof (PPPoEPacket), 0);
    if (r < 0) {
	ADD2LOG ("recv failed: %m\n");
	return 0;
    }

    *size = r;
    return 1;
}


static void
parse_hostuniq (uint16_t type, uint16_t len, unsigned char* data, void* extra)
{
    if (type == TAG_HOST_UNIQ && len == sizeof (pid_t)) {
	pid_t tmp;
	memcpy (&tmp, data, len);
	if (tmp == getpid ()) {
	    int* val = (int*) extra;
	    *val = 1;
	}
    }
}


static int
packet_for_me (PPPoEConnection* conn, PPPoEPacket* packet)
{
    /* If packet is not directed to our MAC address, forget it. */
    if (memcmp (packet->ethHdr.h_dest, conn->my_mac, ETH_ALEN))
	return 0;

    /* Check for HostUniq tag. */
    int for_me = 0;
    parse_packet (conn, packet, parse_hostuniq, &for_me);
    return for_me;
}


static void
parse_pado_tags (uint16_t type, uint16_t len, unsigned char* data, void* extra)
{
    PacketCriteria* pc = (PacketCriteria*) extra;
    PPPoEConnection *conn = pc->conn;

    switch (type) {
	case TAG_AC_NAME:
	    pc->acname_ok = 1;
	    ADD2LOG ("%s: Service-Name is: %.*s\n", conn->ifname, (int) len,
		     data);
	    break;
	case TAG_SERVICE_NAME:
	    pc->servicename_ok = len == 0;
	    break;
	case TAG_SERVICE_NAME_ERROR:
	    ADD2LOG ("%s: Service-Name-Error: %.*s\n", conn->ifname, (int) len,
		     data);
	    pc->error = 1;
	    break;
	case TAG_AC_SYSTEM_ERROR:
	    ADD2LOG ("%s: System-Error: %.*s\n", conn->ifname, (int) len, data);
	    pc->error = 1;
	    break;
	case TAG_GENERIC_ERROR:
	    ADD2LOG ("%s: Generic-Error: %.*s\n", conn->ifname, (int) len, data);
	    pc->error = 1;
	    break;
    }
}


static int
send_padi (int n, PPPoEConnection* conns)
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
	if (!check_room (conn, cursor, packet.payload, TAG_HDR_SIZE))
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
	if (!check_room (conn, cursor, packet.payload, namelen + TAG_HDR_SIZE))
	    continue;

	cursor += namelen + TAG_HDR_SIZE;

	PPPoETag hostUniq;
	pid_t pid = getpid ();
	hostUniq.type = htons (TAG_HOST_UNIQ);
	hostUniq.length = htons (sizeof (pid));
	memcpy (hostUniq.payload, &pid, sizeof (pid));
	if (!check_room (conn, cursor, packet.payload, sizeof (pid) + TAG_HDR_SIZE))
	    continue;
	memcpy (cursor, &hostUniq, sizeof (pid) + TAG_HDR_SIZE);
	cursor += sizeof (pid) + TAG_HDR_SIZE;
	plen += sizeof (pid) + TAG_HDR_SIZE;

	packet.length = htons (plen);

	ADD2LOG ("%s: Sending PADI packet\n", conn->ifname);

	if (send_packet (conn->fd, &packet, (int) (plen + HDR_SIZE)))
	    ret = 1;
    }

    return ret;
}


static int
wait_for_pado (int n, PPPoEConnection* conns)
{
    int r, i, all;
    size_t len;
    fd_set readable;
    PPPoEPacket packet;
    PacketCriteria pc;
    struct timeval tv_end;

    while (1)
    {
	FD_ZERO (&readable);
	for (i = 0; i < n; i++)
	    if (conns[i].fd != -1)
		FD_SET (conns[i].fd, &readable);

        /* don't rely on select() updating its timeout arg */

        gettimeofday (&tv_end, NULL);
        timeradd (&tv_end, &((struct timeval) { tv_sec: PADO_TIMEOUT }), &tv_end);

	do {
            struct timeval tv;

            r = 0;

            gettimeofday (&tv, NULL);
            timersub (&tv_end, &tv, &tv);

            if (timercmp (&tv, &((struct timeval) {}), <=))
                break;

	    r = select (FD_SETSIZE, &readable, NULL, NULL, &tv);
	} while (r == -1 && errno == EINTR);

	if (r < 0) {
	    ADD2LOG ("select: %m\n");
	    return 0;
	}

	if (r == 0) {
	    ADD2LOG ("Timeout waiting for PADO packets\n");
	    return 0;
	}

	for (i = 0; i < n; i++)
	{
	    PPPoEConnection* conn = &conns[i];

	    if (conn->fd == -1 || !FD_ISSET (conn->fd, &readable))
		continue;

	    pc.conn = conn;
	    pc.acname_ok = 0;
	    pc.servicename_ok = 0;
	    pc.error = 0;

	    /* Get the packet */
	    if (!receive_packet (conn->fd, &packet, &len))
		continue;

	    /* Check length */
	    if (ntohs (packet.length) + HDR_SIZE > len) {
		ADD2LOG ("%s: Bogus PPPoE length field (%u)\n", conn->ifname,
			(unsigned int) ntohs (packet.length));
		continue;
	    }

	    /* If it's not for us, loop again */
	    if (!packet_for_me (conn, &packet))
		continue;

	    if (packet.code != CODE_PADO)
		continue;

	    if (NOT_UNICAST (packet.ethHdr.h_source)) {
		ADD2LOG ("%s: Ignoring PADO packet from non-unicast MAC "
			 "address\n", conn->ifname);
		continue;
	    }

	    parse_packet (conn, &packet, parse_pado_tags, &pc);

	    if (!pc.acname_ok) {
		ADD2LOG ("%s: Wrong or missing AC-Name tag\n", conn->ifname);
		continue;
	    }

	    if (!pc.servicename_ok) {
		ADD2LOG ("%s: Wrong or missing Service-Name tag\n",
			 conn->ifname);
		continue;
	    }

	    if (pc.error) {
		ADD2LOG ("%s: Ignoring PADO packet with some Error tag\n",
			 conn->ifname);
		continue;
	    }

	    memcpy (conn->peer_mac, packet.ethHdr.h_source, ETH_ALEN);
	    ADD2LOG ("%s: Received correct PADO packet\n", conn->ifname);
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


static void
discovery (int n, PPPoEConnection* conns)
{
    int a;

    if (open_interfaces (n, conns))
    {
	for (a = 0; a < MAX_ATTEMPTS; a++)
	{
	    ADD2LOG ("Attempt number %d\n", a + 1);

	    if (!send_padi (n, conns))
		break;

	    if (wait_for_pado (n, conns))
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
      hd->sub_class.id == sc_nif_ethernet &&
      hd->unix_dev_name
    ) {
      interfaces++;
    }
  }

  if(!interfaces) return;

  conn = new_mem(interfaces * sizeof *conn);

  for(cnt = 0, hd = hd_data->hd; hd && cnt < interfaces; hd = hd->next) {
    if(
      hd->base_class.id == bc_network_interface &&
      hd->sub_class.id == sc_nif_ethernet &&
      hd->unix_dev_name
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

/** @} */

