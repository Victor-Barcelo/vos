# Networking Libraries for VOS

Lightweight TCP/IP stacks and networking utilities for embedded/hobby OS development.

**Note:** VOS currently does not have network hardware drivers. These libraries would
require implementing a network driver (e.g., NE2000, RTL8139, Intel E1000, or virtio-net).

---

## Lightweight TCP/IP Stacks

### uIP - Ultra Lightweight
- **URL:** https://github.com/adamdunkels/uip
- **License:** BSD
- **Size:** <10KB code
- **Author:** Adam Dunkels (Contiki OS)
- **Features:**
  - Minimal RAM usage (~2KB)
  - TCP and UDP
  - ARP, ICMP
  - Single-threaded, event-driven

```c
#include "uip.h"

// Initialize
uip_init();
uip_ipaddr_t ipaddr;
uip_ipaddr(ipaddr, 192, 168, 1, 100);
uip_sethostaddr(ipaddr);

// Main loop
while (1) {
    // Check for incoming packet
    uip_len = network_read(uip_buf, UIP_BUFSIZE);

    if (uip_len > 0) {
        if (BUF->type == htons(UIP_ETHTYPE_IP)) {
            uip_input();
            if (uip_len > 0)
                network_write(uip_buf, uip_len);
        } else if (BUF->type == htons(UIP_ETHTYPE_ARP)) {
            uip_arp_arpin();
            if (uip_len > 0)
                network_write(uip_buf, uip_len);
        }
    }

    // Periodic processing
    for (int i = 0; i < UIP_CONNS; i++) {
        uip_periodic(i);
        if (uip_len > 0)
            network_write(uip_buf, uip_len);
    }
}

// Application callback
void my_app_call(void) {
    if (uip_connected()) {
        // New connection
    }
    if (uip_newdata()) {
        // Data received: uip_appdata, uip_datalen()
    }
    if (uip_acked()) {
        // Previous data was acknowledged
    }
    if (uip_poll()) {
        // Can send data
        uip_send("Hello", 5);
    }
    if (uip_closed() || uip_aborted() || uip_timedout()) {
        // Connection closed
    }
}
```

### lwIP - Lightweight IP
- **URL:** https://savannah.nongnu.org/projects/lwip/
- **License:** BSD
- **Size:** ~40KB code
- **Features:**
  - Full TCP/IP stack
  - IPv4 and IPv6
  - DHCP, DNS, SNMP
  - Raw API, Netconn API, BSD Sockets API
  - More RAM needed (~16KB+)

```c
#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

// Initialize
lwip_init();

// Set up network interface
struct netif netif;
ip4_addr_t ipaddr, netmask, gateway;
IP4_ADDR(&ipaddr, 192, 168, 1, 100);
IP4_ADDR(&netmask, 255, 255, 255, 0);
IP4_ADDR(&gateway, 192, 168, 1, 1);

netif_add(&netif, &ipaddr, &netmask, &gateway, NULL, mydriver_init, ethernet_input);
netif_set_default(&netif);
netif_set_up(&netif);

// TCP Server example
struct tcp_pcb* pcb = tcp_new();
tcp_bind(pcb, IP_ADDR_ANY, 80);
pcb = tcp_listen(pcb);
tcp_accept(pcb, my_accept_callback);

// Main loop
while (1) {
    my_netif_poll(&netif);  // Check for packets
    sys_check_timeouts();    // Process timers
}
```

### picoTCP
- **URL:** https://github.com/tass-belgium/picotcp
- **License:** GPL (commercial available)
- **Features:** Modular, good for embedded

---

## Network Driver Requirements

To use any TCP/IP stack, VOS needs a network driver:

### Common Emulated NICs

| NIC | Complexity | Notes |
|-----|------------|-------|
| **NE2000** | Simple | ISA, old but simple |
| **RTL8139** | Medium | PCI, common in QEMU |
| **Intel E1000** | Medium | PCI, QEMU default |
| **virtio-net** | Easy | QEMU paravirtualized |

### Driver Interface
```c
// What a network driver needs to provide:
typedef struct {
    uint8_t mac[6];

    // Send packet (returns 0 on success)
    int (*send)(const uint8_t* data, size_t len);

    // Receive packet (returns length, 0 if none)
    size_t (*recv)(uint8_t* buffer, size_t max_len);

    // Check if packet available
    bool (*has_packet)(void);
} NetDriver;

// Example NE2000 skeleton
int ne2000_init(uint16_t iobase, uint8_t irq);
int ne2000_send(const uint8_t* data, size_t len);
size_t ne2000_recv(uint8_t* buffer, size_t max_len);
```

### virtio-net (Easiest for QEMU)
```c
// virtio-net uses memory-mapped virtqueues
// Much simpler than real hardware

#define VIRTIO_NET_QUEUE_RX 0
#define VIRTIO_NET_QUEUE_TX 1

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtq_desc;

// Initialize virtqueues
// Add buffers to RX queue
// Poll for received packets
// Add packets to TX queue for sending
```

---

## Serial Networking (Already Possible!)

VOS has serial port support. SLIP (Serial Line IP) is the simplest way to get networking:

### SLIP Protocol
- Send IP packets over serial
- Escape special bytes (END=0xC0, ESC=0xDB)
- Connect to Linux host with `slattach`

```c
#define SLIP_END     0xC0
#define SLIP_ESC     0xDB
#define SLIP_ESC_END 0xDC
#define SLIP_ESC_ESC 0xDD

void slip_send(const uint8_t* packet, size_t len) {
    serial_write_byte(SLIP_END);

    for (size_t i = 0; i < len; i++) {
        switch (packet[i]) {
            case SLIP_END:
                serial_write_byte(SLIP_ESC);
                serial_write_byte(SLIP_ESC_END);
                break;
            case SLIP_ESC:
                serial_write_byte(SLIP_ESC);
                serial_write_byte(SLIP_ESC_ESC);
                break;
            default:
                serial_write_byte(packet[i]);
        }
    }

    serial_write_byte(SLIP_END);
}

size_t slip_recv(uint8_t* buffer, size_t max_len) {
    size_t len = 0;
    bool escape = false;

    while (len < max_len) {
        int c = serial_read_byte();
        if (c < 0) continue;

        if (escape) {
            if (c == SLIP_ESC_END) buffer[len++] = SLIP_END;
            else if (c == SLIP_ESC_ESC) buffer[len++] = SLIP_ESC;
            escape = false;
        } else if (c == SLIP_ESC) {
            escape = true;
        } else if (c == SLIP_END) {
            if (len > 0) return len;
        } else {
            buffer[len++] = c;
        }
    }
    return len;
}
```

**Host setup (Linux):**
```bash
# Create SLIP interface
sudo slattach -s 115200 -p slip /dev/ttyUSB0 &
sudo ifconfig sl0 192.168.2.1 pointopoint 192.168.2.2 up

# Enable IP forwarding
sudo sysctl net.ipv4.ip_forward=1
sudo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```

---

## HTTP Client (Simple)

Without full sockets, a minimal HTTP client:

```c
typedef struct {
    char host[64];
    uint16_t port;
    char path[256];
} URL;

bool parse_url(const char* url, URL* out) {
    // Parse "http://host:port/path"
    if (strncmp(url, "http://", 7) != 0) return false;
    const char* p = url + 7;

    // Extract host
    char* colon = strchr(p, ':');
    char* slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
        strncpy(out->host, p, colon - p);
        out->host[colon - p] = '\0';
        out->port = atoi(colon + 1);
    } else {
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        strncpy(out->host, p, len);
        out->host[len] = '\0';
        out->port = 80;
    }

    if (slash) strcpy(out->path, slash);
    else strcpy(out->path, "/");

    return true;
}

// Simple HTTP GET (using uIP or lwIP)
void http_get(const char* url) {
    URL u;
    parse_url(url, &u);

    // Connect to server
    tcp_connect(u.host, u.port);

    // Send request
    char request[512];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        u.path, u.host);
    tcp_send(request, strlen(request));

    // Read response
    char buffer[4096];
    while (tcp_recv(buffer, sizeof(buffer)) > 0) {
        // Process response
    }
}
```

---

## DNS Resolution (Simple)

```c
// DNS query structure
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

// Simple DNS lookup
uint32_t dns_resolve(const char* hostname, uint32_t dns_server) {
    uint8_t query[512];
    dns_header_t* hdr = (dns_header_t*)query;

    hdr->id = rand();
    hdr->flags = htons(0x0100);  // Standard query
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    // Encode hostname
    uint8_t* p = query + sizeof(dns_header_t);
    const char* label = hostname;
    while (*label) {
        const char* dot = strchr(label, '.');
        size_t len = dot ? (size_t)(dot - label) : strlen(label);
        *p++ = len;
        memcpy(p, label, len);
        p += len;
        label = dot ? dot + 1 : "";
    }
    *p++ = 0;

    // Query type A, class IN
    *p++ = 0; *p++ = 1;  // Type A
    *p++ = 0; *p++ = 1;  // Class IN

    // Send UDP query to DNS server port 53
    udp_send(dns_server, 53, query, p - query);

    // Receive response and parse
    // ... parse answer section for IP address
}
```

---

## Useful Network Utilities

### TFTP Client (Simple File Transfer)
```c
// TFTP is UDP-based, very simple protocol
#define TFTP_PORT 69
#define TFTP_RRQ  1
#define TFTP_WRQ  2
#define TFTP_DATA 3
#define TFTP_ACK  4
#define TFTP_ERR  5

void tftp_get(const char* server, const char* filename, uint8_t* buffer) {
    // Send read request
    uint8_t rrq[512];
    rrq[0] = 0; rrq[1] = TFTP_RRQ;
    strcpy((char*)rrq + 2, filename);
    strcpy((char*)rrq + 2 + strlen(filename) + 1, "octet");
    udp_send(server, TFTP_PORT, rrq, 4 + strlen(filename) + strlen("octet"));

    // Receive data blocks
    uint16_t block = 1;
    size_t offset = 0;
    while (1) {
        uint8_t response[516];
        size_t len = udp_recv(response, sizeof(response));

        if (response[1] == TFTP_DATA) {
            memcpy(buffer + offset, response + 4, len - 4);
            offset += len - 4;

            // Send ACK
            uint8_t ack[4] = {0, TFTP_ACK, block >> 8, block & 0xFF};
            udp_send(server, TFTP_PORT, ack, 4);
            block++;

            if (len < 516) break;  // Last block
        }
    }
}
```

---

## Implementation Priority

| Priority | Component | Difficulty | Notes |
|----------|-----------|------------|-------|
| 1 | **SLIP driver** | Easy | Uses existing serial |
| 2 | **uIP stack** | Easy | Minimal, well-documented |
| 3 | **TFTP client** | Easy | Simple file transfer |
| 4 | **DNS client** | Easy | Basic resolution |
| 5 | **HTTP client** | Medium | Needs TCP working |
| 6 | **virtio-net** | Medium | For QEMU |
| 7 | **RTL8139** | Medium | Real hardware |

---

## See Also

- [system_libraries.md](system_libraries.md) - System utilities
- VOS serial driver: `kernel/serial.c`
