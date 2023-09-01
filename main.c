/* main.c
**
** Configures BT-0240 Bluetooth RS232C Adapter
**
** License: GPL
**
** Emard
**
*/
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
// #include <sys/socket.h>
#include <bluetooth/bluetooth.h> /* address parser */
#include <bluetooth/rfcomm.h>

#include "bt0240.h"
#include "cmdline.h"

struct gengetopt_args_info args_info;
struct gengetopt_args_info *args = &args_info;

int fd;
char *device = "/dev/ttyS0";

int main(int argc, char **argv) 
{
  int i;
  struct bt_uart btuart;
  struct bt_role btrole;
  struct bt_identity btname;
  struct bt_security1 btsecurity;
  char string[256];
  char *peer = NULL;
  char *pin = NULL;
  char *name;
  int bps;
  char parity;
  int stopbits;
  int master = 0;
  int discoverable = 1;
  int set_role = 0, set_security = 0, set_uart = 0, set_name = 0;
  struct sockaddr_rc mac = { 0 };
  char mac_str[100];

  cmdline_parser(argc, argv, args);
  verbose = args->verbose_given ? 1 : 0;
  bt0240->name = args->device_arg;

  #if 0
  if(bt0240->name[0] == '/')
  {
    if(dev_lock(bt0240->name))
    {
      perror("can't get hold of device lock");
      return 1;
    }
  }
  #endif

  fd = bt_open(bt0240);

  if(fd < 0)
  {
    perror("can't open device");
    return 1;
  }

  if(args->peer_given || args->reset_given)
  {
    if(strcasecmp("any", args->peer_arg) == 0)
    {
      peer = NULL;
      set_role = 1;
    }
    if(strlen(args->peer_arg) == 12)
    {
      peer = args->peer_arg;
      set_role = 1;
    }
    if(strlen(args->peer_arg) == 17)
    {
      str2ba(args->peer_arg, &mac.rc_bdaddr);
      sprintf(mac_str, "%02x%02x%02x%02x%02x%02x", 
              mac.rc_bdaddr.b[5], mac.rc_bdaddr.b[4], mac.rc_bdaddr.b[3],
              mac.rc_bdaddr.b[2], mac.rc_bdaddr.b[1], mac.rc_bdaddr.b[0]);
      peer = mac_str;
      set_role = 1;
    }
  }
  
  if(args->discoverable_given || args->reset_given)
  {
    discoverable = args->discoverable_arg > 0 ? 1 : 0;
    set_role = 1;
  }

  if(args->role_given || args->reset_given)
  {
    if(strcasecmp("master", args->role_arg) == 0)
    {
      master = 1;
      set_role = 1;
    }
    if(strcasecmp("slave", args->role_arg) == 0)
    {
      master = 0;
      set_role = 1;
    }
  }

  if(args->pin_given || args->reset_given)
  {
    if(strcasecmp("off", args->pin_arg) == 0)
    {
      pin = NULL;
      set_security = 1;
    }
    if(strlen(args->pin_arg) >= 4 && strlen(args->pin_arg) <= 12)
    {
      pin = args->pin_arg;
      set_security = 1;
    }
  }
  
  if(args->bps_given || args->reset_given)
  {
    int i;

    for(i = 0; i < bt_rate2int_n; i++)
    {
      if(args->bps_arg == bt_rate2int[i])
      {
        bps = args->bps_arg;
        set_uart = 1;
      }
    }
  }

  if(args->parity_given || args->reset_given)
  {
    if(strcasecmp("none", args->parity_arg) == 0 ||
       strcasecmp("n", args->parity_arg) == 0)
    {
      parity = 'N';
      set_uart = 1;
    }
    if(strcasecmp("even", args->parity_arg) == 0 ||
       strcasecmp("e", args->parity_arg) == 0)
    {
      parity = 'E';
      set_uart = 1;
    }
    if(strcasecmp("odd", args->parity_arg) == 0 ||
       strcasecmp("o", args->parity_arg) == 0)
    {
      parity = 'O';
      set_uart = 1;
    }
  }

  if(args->stopbits_given || args->reset_given)
  {
    if(args->stopbits_arg >= 1 && args->stopbits_arg <= 2)
    {
      stopbits = args->stopbits_arg;
      set_uart = 1;
    }
  }

  if(args->name_given || args->reset_given)
  {
    if(strlen(args->name_arg) >= 1 && strlen(args->name_arg) <= 31)
    {
      name = args->name_arg;
      set_name = 1;
    }
  }

  if(set_role)
    bt_set_role(bt0240, master, discoverable, peer);

  if(set_security)
    bt_set_security(bt0240, pin);

  if(set_uart)
    bt_set_uart(bt0240, bps, parity, stopbits);

  if(set_name)
  {
    printf("Warning: before changing the name next time, power cycle BT-0240\n");
    bt_set_identity(bt0240, name);
  }

  if(args->list_given)
  for(i = 0; i < 1; i++)
  {
    if(bt_read_identity(bt0240, &btname))
    {
      bt_copystring((char *) &(btname.len), string, sizeof(btname.name));
      printf("%-12s: %s\n", "Device name", string);
    }
    else
      printf("%-12s: *READ ERROR*\n", "Device name");

    if(bt_read_security(bt0240, &btsecurity))
    {
      bt_copystring((char *) &(btsecurity.pin_len), string, sizeof(btsecurity.pin));
      printf("%-12s: %s\n", "Encryption", (int)btsecurity.encryption > 0 ? "Yes" : "No");
      if((int)btsecurity.encryption > 0)
        printf("%-12s: %s\n", "PIN", string);
    }
    else
      printf("%-12s: *READ ERROR*\n%-12s: *READ ERROR*\n", "Encryption", "PIN");

    if(bt_read_role(bt0240, &btrole))
    {
      strcpy(string, "xx:xx:xx:xx:xx:xx");
      if( (btrole.role & ROLE_CONTACT_PEER) > 0)
      {
        string[0]  = btrole.peer[0];
        string[1]  = btrole.peer[1];

        string[3]  = btrole.peer[2];
        string[4]  = btrole.peer[3];

        string[6]  = btrole.peer[4];
        string[7]  = btrole.peer[5];

        string[9]  = btrole.peer[6];
        string[10] = btrole.peer[7];

        string[12] = btrole.peer[8];
        string[13] = btrole.peer[9];

        string[15] = btrole.peer[10];
        string[16] = btrole.peer[11];
      }    
      printf("%-12s: %s\n%-12s: %s\n%-12s: %s\n",
             "Role", btrole.role & ROLE_MASTER ? "Master" : "Slave",
             "Discoverable", btrole.role & ROLE_DISCOVERABLE ? "Yes" : "No",
             "Peer", btrole.role & ROLE_CONTACT_PEER ? string : "Any" );
    }
    else
      printf("%-12s: *READ ERROR*\n%-12s: *READ ERROR*\n%-12s: *READ ERROR*\n",
             "Role", "Discoverable", "Peer");

    if(bt_read_uart(bt0240, &btuart))
    {
      printf("%-12s: %d\n%-12s: %c\n%-12s: %d\n",
             "BPS Rate", bt_rate2int[(int) btuart.rate],
             "Parity", bt_parity2char[(int) btuart.parity],
             "Stopbits", bt_stopbits2int[(int) btuart.stopbits]);
    }
    else
      printf("%-12s: *READ ERROR*\n%-12s: *READ ERROR*\n%-12s: *READ ERROR*\n",
             "BPS Rate",
             "Parity",
             "Stopbits");
  }

  bt_close(bt0240);

  #if 0
  if(bt0240->name[0] == '/')
  {
    dev_unlock(bt0240->name, getpid());
  }
  #endif
/*  sleep(1); */

  return 0;
}
