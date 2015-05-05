#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>

#define WIIMOTE_ACCELEROMETER (1 << 0)
#define WIIMOTE_BUTTONS (1 << 1)

typedef struct Wiimote {
  int button_a;
  int button_b;
  int button_1;
  int button_2;
  int button_plus;
  int button_minus;
  int button_up;
  int button_down;
  int button_left;
  int button_right;
  int button_home;
} Wiimote;

typedef struct Server {
  unsigned short id;
  unsigned short port;
  unsigned char index;
  char name[256];
  unsigned char name_len;
  int sock;
  int broadcast_sock;
  unsigned char broadcast_buffer[512];
} Server;

void dump_state(const Wiimote *wm) {
  printf("\033[2J"
      "Up: %d\n"
      "Down: %d\n"
      "Left: %d\n"
      "Right: %d\n"
      "Plus: %d\n"
      "Minus: %d\n"
      "1: %d\n"
      "2: %d\n"
      "A: %d\n"
      "B: %d\n"
      "Home: %d\n",
      wm->button_up, wm->button_down, wm->button_left, wm->button_right,
      wm->button_plus, wm->button_minus,
      wm->button_1, wm->button_2,
      wm->button_a, wm->button_b, wm->button_home);
}

void build_broadcast_buffer(Server *srv) {
  unsigned char *buffer = srv->broadcast_buffer;
  unsigned short *id = (unsigned short *) (buffer+1);
  unsigned short *port = (unsigned short *) (buffer+4);

  buffer[0] = 0xdf;
  *id = htons(srv->id);
  buffer[3] = srv->index;
  *port = htons(srv->port);
  buffer[6] = srv->name_len;
  strncpy((char *) buffer+7, srv->name, 255);
}

void broadcast(const Server *srv) {
  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(4431);
  addr.sin_addr.s_addr =  INADDR_BROADCAST;

  sendto(srv->broadcast_sock, (const void *) srv->broadcast_buffer,
      srv->name_len+7, 0, (struct sockaddr *) &addr, sizeof(addr));
}

int main(int argc, char *argv[]) {
  struct sockaddr_in saddr;
  struct timeval timeout;
  unsigned char in_buffer[46];
  Server srv;
  Wiimote wm;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s PORT [INDEX] [NAME]\n", argv[0]);
    return 1;
  }
  srand(time(0));

  srv.id = (unsigned short) rand();
  srv.port = atoi(argv[1]);
  srv.index = (argc >= 3? atoi(argv[2]) : 0);
  strncpy(srv.name, (argc >= 4? argv[3] : "UDPWii Tester"), 255);
  srv.name[255] = 0;
  srv.name_len = strlen(srv.name);
  build_broadcast_buffer(&srv);

  memset(&wm, 0, sizeof(wm));

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(srv.port);
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);

  srv.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (srv.sock == -1) {
    fprintf(stderr, "Failed to open UDP socket\n");
    return 2;
  }
  if (bind(srv.sock, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
    fprintf(stderr, "Failed to bind to port %d\n", srv.port);
    return 3;
  }

  srv.broadcast_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (srv.broadcast_sock == -1) {
    fprintf(stderr, "Failed to open broadcast socket\n");
    return 4;
  }
  {
    int broad = 1;
    if (setsockopt(srv.broadcast_sock, SOL_SOCKET, SO_BROADCAST,
          (const void *) &broad, sizeof(broad)) == -1) {
      fprintf(stderr, "Failed to enable broadcasting\n");
      return 5;
    }
  }

  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  for (;;) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(srv.sock, &read_fds);

    select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);

    if (FD_ISSET(srv.sock, &read_fds)) {
      int read = recv(srv.sock, in_buffer, sizeof(in_buffer), 0);
      int i;
      for (i = 0; i < read; i++) {
        printf("%02X ", in_buffer[i]);
      }
      putchar('\n');
    }
    else {
      broadcast(&srv);
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
    }
  }

  close(srv.sock);
  close(srv.broadcast_sock);

  return 0;
}
