#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <malloc.h>  /* malloc() */
#include <sys/ioctl.h>
// #include <lockdev.h> /* serial device lock */
#include <sys/socket.h>
#include <bluetooth/bluetooth.h> /* address parser */
#include <bluetooth/rfcomm.h>

#include "bt0240.h"

int bt_rate2int[] = {9600, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1382400};
int bt_rate2int_n = sizeof(bt_rate2int) / sizeof(bt_rate2int[0]);
char bt_parity2char[] = {'N', 'O', 'E' };
int bt_stopbits2int[] = {1, 2};

int verbose = 0;

struct bt0240 bt0240_device;
struct bt0240 *bt0240 = &bt0240_device;

/* opens device, 
** sets serial parameters 
** and returns file descriptor id 
*/
int bt_open_serial(struct bt0240 *bt)
{
  int fd = -1;
  struct termios options;
  int retry = 0;
  char *device_name = bt->name;

  bt->fd = -1;
  for(retry = 0; fd < 0 && retry < 3; retry++)
  {
    fd = open(device_name, O_RDWR | O_NOCTTY | O_SYNC); /* O_NDELAY or O_NONBLOCK */
    if(fd >= 0)
      break;
    sleep(2);
  }
  bt->fd = fd;
  
  if (fd == -1) {
    perror("open_bt: Unable to open device");
    return -1;
  }
  
  fcntl(fd, F_SETFL, 0);
  
  /* Get the current options for the port */
  tcgetattr(fd, &options);

  /* Set the baud rates to 115200 */
  cfsetispeed(&options, B115200);
  cfsetospeed(&options, B115200);

  /* Enable the receiver and set local mode */
  options.c_cflag |= (CLOCAL | CREAD);

  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;

  options.c_iflag |= IGNBRK;
  options.c_iflag &= ~IGNBRK;
  options.c_iflag &= ~ICRNL;
  options.c_iflag &= ~INLCR;
  options.c_iflag &= ~IMAXBEL;
  options.c_iflag &= ~IXON;
  options.c_iflag &= ~IXOFF;

  options.c_oflag &= ~OPOST;
  options.c_oflag &= ~ONLCR;

  options.c_lflag &= ~ISIG;
  options.c_lflag &= ~ICANON;
  options.c_lflag &= ~IEXTEN;
  options.c_lflag &= ~ECHO;
  options.c_lflag &= ~ECHOE;
  options.c_lflag &= ~ECHOK;
  options.c_lflag |= NOFLSH;
  options.c_lflag &= ~ECHOCTL;
  options.c_lflag &= ~ECHOKE;

  options.c_cc[VTIME] = 0;
  options.c_cc[VMIN] = 1;
  /* Set the new options for the port */
  tcsetattr(fd, TCSANOW, &options);

  usleep(100000); /* port open grace time */

  return fd;
}

int bt_open_bluetooth(struct bt0240 *bt)
{
  struct sockaddr_rc addr = { 0 };
  int status;

  /* allocate a socket */
  bt->fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
  bt->connect = 0;

  if(bt->fd < 0)
  {
    perror("socket");
    return -1;
  }
  /* set the connection parameters (who to connect to) */
  addr.rc_family = AF_BLUETOOTH;
  addr.rc_channel = (uint8_t) 1;
  str2ba(bt->name, &addr.rc_bdaddr); 

  /* connect to server (bluetooth slave) */
  status = connect(bt->fd, (struct sockaddr *)&addr, sizeof(addr));
   
  if(status < 0)
  {
    /* perror("connect"); */
    return -1;
  }

  bt->connect = 1;
  return bt->fd;
}


int bt_close_serial(struct bt0240 *bt)
{
  if(bt->fd >= 0)
    close(bt->fd);

  return 0;
}

int bt_close_bluetooth(struct bt0240 *bt)
{
  if(bt->fd >= 0)
  {
    if(bt->connect)
      shutdown(bt->fd, SHUT_RDWR);
    close(bt->fd);
  }
  return 0;
}

int bt_close(struct bt0240 *bt)
{
  if(bt->name == NULL)
    return -1;

  if(bt->name[0] == '/')
    return bt_close_serial(bt);

  return bt_close_bluetooth(bt);  
}

int bt_open(struct bt0240 *bt)
{
  int i, retval;
  if(bt->name == NULL)
    return -1;

  if(bt->name[0] == '/')
    return bt_open_serial(bt);

  for(i = 0; i < 3; i++)
  {
    retval = bt_open_bluetooth(bt);
    if(retval >= 0)
      return retval;
    bt_close_bluetooth(bt);
    sleep(2);
  }
  return -1;
}

/* return value: */
/* <0 if error */
/* >=0 sucess, length of entire packet */
int bt_read_raw(struct bt0240 *bt, char *packet, int maxlen)
{
  char *command, *arg_len, *arg;
  int i, n, skip; /* i-retries, n - byte pointer */
  fd_set rfds;
  struct timeval tv;
  int retval;
  char expect;
  int fd = bt->fd;

  if(packet == NULL || maxlen == 0)
    return 0;
  
  expect = packet[0];

  /* first we want to read initially expected byte */
  for(i = 0, n = 0; i < 10 && n < 2; i++)
  {
    /* Watch fd to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    /* Wait up to 0.5 seconds. */
    tv.tv_sec = 1;
    tv.tv_usec = 300000;
  
    retval = select(fd+1, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */
  
    if (retval == -1)
    {
      if(verbose)
        printf("select error\n");
      return -1;
    }
    else if (retval)
    {
      // printf("Header byte #%d is available now.\n", n);
    }
    /* FD_ISSET(fd, &rfds) will be true. */
    else
    {
      if(verbose)
        printf("timeout in header\n");
      return -1; // printf("No data within five seconds.\n");
    }    
    // fcntl(fd, F_SETFL, FNDELAY); /* don't block serial read */
    
    retval = read(fd, &(packet[n]), 1);
    // printf("char %d=%02x\n", n, packet[n]);
    if(retval > 0 && packet[0] == expect)
    {
      n++;
      if(n == 2 && packet[1] + 2 != maxlen) 
        n = 0; /* length doesn't match expected maxlen */
    }
  }
  skip = i;

  command = packet;
  arg_len = packet+1;
  arg     = packet+2;

  if(n < 0)
    return -1;

  if(n == 2 && *arg_len > 0 && *arg_len + 2 <= maxlen)
  {
    {
      /* Watch fd to see when it has input. */
      FD_ZERO(&rfds);
      FD_SET(fd, &rfds);

      /* Wait up to 0.2 seconds. */
      tv.tv_sec = 0;
      tv.tv_usec = 200000;
  
      retval = select(fd+1, &rfds, NULL, NULL, &tv);
      /* Don't rely on the value of tv now! */
    
      if (retval == -1)
      {
        if(verbose)
          printf("select error\n");
        return -1; // perror("select()");
      }
      else if (retval)
      {
        // printf("Data is available now.\n");
      }
      /* FD_ISSET(fd, &rfds) will be true. */
      else
      {
        if(verbose)
          printf("timeout in payload\n");
        return -1; // printf("No data within five seconds.\n");
      }
      usleep(96*maxlen); /* calculate 96us character time (70ms should be enoug) */
      n += read(fd, arg, *arg_len);
    }
  }
  
  if(verbose)
  {
    printf("skipped %d, ", skip-2);
    for(i = 0; i < n; i++)
      printf("%02x ", packet[i]);
    printf("n=%d bytes\n", n);
  }
  return n;
}

int bt_write_raw(struct bt0240 *bt, char *packet, int maxlen)
{
  int fd = bt->fd;
  fcntl(fd, F_SETFL, 0); /* block serial write */
  return write(fd, packet, maxlen);
}

int bt_copystring(char *packet, char *string, int maxlen)
{
  int len = 0;
  if((int)packet[0] > 0)
    len = (int)packet[0] < maxlen ? (int)packet[0] : maxlen;
  memcpy(string, packet + 1, len);
  string[len] = '\0';
  return len;
}

int flushstream(int fd)
{
  fd_set rfds;
  struct timeval tv;
  int retval, bytes;
  char packet[100];

  for(bytes = 1; bytes > 0;)
  {
    /* Watch fd to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    /* Wait up to 0.3 seconds for quiet line. */
    tv.tv_sec = 0;
    tv.tv_usec = 300000;
    
    retval = select(fd+1, &rfds, NULL, NULL, &tv);

    bytes = 0;
    if(retval)
      bytes = read(fd, packet, 1);
  }
  return 0;
}

int bt_send_receive(struct bt0240 *bt, char *query, int querylen, char *answer, int answerlen, int verify)
{
  int retry = 0;
  int read = 0;
  int result = 0;
  char *answer2 = NULL;
  int equal = 0, require_equal = 0;
  int fd = bt->fd;

  if(verbose)
    printf("send_receive\n");
  if(verify)
    answer2 = malloc(answerlen);

  if(answer2 != NULL && answerlen > 0)
  {
    if(verbose)
      printf("verify reads\n");
    memcpy(answer2, answer, answerlen);
    /* spoil beginning and the end */
    answer2[0]++;
    answer2[answerlen - 1]--;
    require_equal = 1; /* requre 2 consecutive answers equal */
  }

  for(retry = 0; retry < 10; retry++)
  {
    if(verbose)
      if(retry >= 0)
        printf("RETRY %d EQUAL=%d\n", retry, equal);
    if(retry == 5)
    {
      bt_close(bt);
      if(verbose)
        printf("Reopening port to revive the line\n");
      sleep(5);
      fd = bt_open(bt);
    }

#if 0
    tcflush(fd, TCIOFLUSH);
#endif
    flushstream(fd);
    if(bt_write_raw(bt, query, querylen) < 0)
    {
      bt_close(bt);
      if(verbose)
        printf("Reopening port to revive the line\n");
      sleep(1);
      fd = bt_open(bt);
      // goto bailout;
    }
    read = bt_read_raw(bt, answer, answerlen);
    equal++;
    if(read < answerlen)
      equal = 0;
    if(answer2 != NULL)
    {
      if(memcmp(answer, answer2, answerlen) != 0)
        equal = 0;
      memcpy(answer2, answer, answerlen);
    }
    if(equal >= require_equal)
      goto complete;
  }
  if(equal < require_equal)
    goto bailout;
  /* printf("\n"); */

 complete:;
  result = 1;

 bailout:;
  if(answer2 != NULL)
    free(answer2);
  return result;
}

int bt_command_receive(struct bt0240 *bt, char command, char *answer, int answerlen)
{
  struct bt_command btcommand;

  btcommand.command = command;
  btcommand.len = 0;
  answer[0] = command;
  return bt_send_receive(bt, (char *) &btcommand, sizeof(btcommand), answer, answerlen, 1);
}

int bt_read_security(struct bt0240 *bt, struct bt_security1 *security)
{
  return bt_command_receive(bt, READ_SECURITY, (char *)security, sizeof(*security) );
}

int bt_read_identity(struct bt0240 *bt, struct bt_identity *name)
{
  return bt_command_receive(bt, READ_IDENTITY, (char *)name, sizeof(*name) );
}

int bt_read_uart(struct bt0240 *bt, struct bt_uart *uart)
{
  return bt_command_receive(bt, READ_UART, (char *)uart, sizeof(*uart) );
}

int bt_read_role(struct bt0240 *bt, struct bt_role *role)
{
  return bt_command_receive(bt, READ_ROLE, (char *)role, sizeof(*role) );
}

int bt_read_change_verify(struct bt0240 *bt, char *send, int len_send, char *receive, int len_receive)
{
  int result;
  /* TODO:
  ** read current state (read if needed, should have 'dirty' flag),
  ** compare read data with the requested
  ** modify only if the change is needed.
  ** return status if the operation succeeded (set dirty flag to 0)
  */
  result = bt_send_receive(bt, send, len_send, receive, len_receive, 0);
  return 0;
}

int bt_set_identity(struct bt0240 *bt, char *name)
{
  struct bt_identity identity;
  struct bt_commit commit;
  int len;
  len = (char) strlen(name);
  if(len < 0)
    len = 0;
  if(len > LEN_IDENTITY_NAME)
    len = LEN_IDENTITY_NAME;
  identity.command = WRITE_IDENTITY;
  identity.len = (char) LEN_IDENTITY_NAME;
  memset(identity.name, 0, LEN_IDENTITY_NAME);
  memcpy(identity.name, name, len);

  commit.command = (char) WRITE_IDENTITY_COMMIT;

#if 0
  printf("Warning: after setting device name, power cycle the BT-0240\n");
#endif
  return bt_read_change_verify(bt, (char *) &identity, sizeof(identity), (char *) &commit, sizeof(commit));
}

int bt_set_uart(struct bt0240 *bt, int rate, char parity, int stopbits)
{
  struct bt_uart uart;
  struct bt_commit commit;
  int i;
  int uart_rate = 5, uart_parity = 0, uart_stopbits = 0;

  for(i = 1; i < bt_rate2int_n; i++)
  {
    if(bt_rate2int[i] == rate)
      uart_rate = i;
  }

  switch(parity)
  {
  case 'N':
  case 'n':
    uart_parity = 0;
    break;
  case 'O':
  case 'o':
    uart_parity = 1;
    break;
  case 'E':
  case 'e':
    uart_parity = 2;
    break;
  }
  if(stopbits >= 1 && stopbits <= 2)
    uart_stopbits = stopbits-1;

  uart.command = WRITE_UART;
  uart.len = 3;
  uart.rate = uart_rate;
  uart.parity = uart_parity;
  uart.stopbits = uart_stopbits;

  commit.command = (char) WRITE_UART_COMMIT;

  return bt_read_change_verify(bt, (char *) &uart, sizeof(uart), (char *) &commit, sizeof(commit));
}

int bt_set_security(struct bt0240 *bt, char *pin)
{
  struct bt_security1 security;
  struct bt_commit commit;
  int len;
  
  security.command = (char) WRITE_SECURITY;
  security.len = sizeof(security) - 2;
  security.encryption = pin != NULL ? 1 : 0;

  security.pin_len = (char) 0;
  memset(security.pin, 0, LEN_SECURITY_PIN);
  if(pin != NULL)
  {
    len = (char) strlen(pin);
    if(len < 0)
      len = 0;
    if(len > LEN_SECURITY_PIN)
      len = LEN_SECURITY_PIN;
    security.pin_len = (char) len;
    memcpy(security.pin, pin, len);
  }
  commit.command = (char) WRITE_SECURITY_COMMIT;  

  return bt_read_change_verify(bt, (char *) &security, sizeof(security), (char *) &commit, sizeof(commit));
}

int bt_set_role(struct bt0240 *bt, int master, int discoverable, char *peer)
{
  struct bt_role role;
  struct bt_commit commit;

  memset(role.peer, 0, LEN_ROLE_PEER);
  if(peer != NULL)
  {
    if(strlen(peer) == LEN_ROLE_PEER)
      memcpy(role.peer, peer, LEN_ROLE_PEER);
    else
      peer = NULL;
  }

  role.role = 
    (master > 0 ? ROLE_MASTER : 0) | 
    (discoverable > 0 ? ROLE_DISCOVERABLE : 0) | 
    (peer != NULL ? ROLE_CONTACT_PEER : 0);

  role.command = (char) WRITE_ROLE;
  role.len = sizeof(role) - 2;
  commit.command = (char) WRITE_ROLE_COMMIT;
  
  return bt_read_change_verify(bt, (char *) &role, sizeof(role), (char *) &commit, sizeof(commit));
}
