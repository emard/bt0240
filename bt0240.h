// #include <lockdev.h>
/* struct for exchanging messages with bt0240 adapter */

/* commands that can be sent to bt0240 */
#define WRITE_ROLE 1
#define READ_ROLE 2
#define LEN_ROLE_PEER 12
#define ROLE_MASTER 1
#define ROLE_DISCOVERABLE 2
#define ROLE_CONTACT_PEER 4
#define WRITE_ROLE_COMMIT 3

#define WRITE_SECURITY 4
#define READ_SECURITY 5
#define LEN_SECURITY_PIN 12
#define WRITE_SECURITY_COMMIT 6

#define WRITE_IDENTITY 7
#define READ_IDENTITY 8
#define LEN_IDENTITY_NAME 31
#define LEN_IDENTITY_NAME2 31
#define WRITE_IDENTITY_COMMIT 9

#define WRITE_UART 10
#define READ_UART 11
#define UART_4800 0
#define UART_9600 1
#define UART_19200 2
#define UART_38400 3
#define UART_57600 4
#define UART_115200 5
#define UART_230400 6
#define UART_460800 7
#define UART_921600 8
#define UART_1382400 9
#define UART_NONE 0
#define UART_ODD 1
#define UART_EVEN 2
#define UART_1STOP 0
#define UART_2STOP 1
#define WRITE_UART_COMMIT 12

struct bt_commit {
  char command;
  char len;
  char data[1];
};

struct bt_command {
  char command;
  char len;
};

struct bt_role {
  char command;
  char len;
  char role;     /* see ROLE_* flags */ 
  char peer[LEN_ROLE_PEER];
};

struct bt_uart {
  char command;
  char len;
  char rate;
  char stopbits;
  char parity;
};

struct bt_identity {
  char command;
  char len;
  char name[LEN_IDENTITY_NAME];
};

struct bt_identity2 {
  char command;
  char len;
  char name[LEN_IDENTITY_NAME2];
};

struct bt_security1 {
  char command;
  char len;
  char encryption;
  char pin_len;
  char pin[LEN_SECURITY_PIN];
};

struct bt0240 {
  int fd; /* open device file */
  char *name; /* device name */
  int connect;
};

extern int bt_rate2int[], bt_rate2int_n;
extern char bt_parity2char[];
extern int bt_stopbits2int[];
#if 0
extern int fd;
extern char *device;
#endif
extern struct bt0240 *bt0240;
extern int verbose;

int bt_open(struct bt0240 *bt);
int bt_close(struct bt0240 *bt);

int bt_copystring(char *packet, char *string, int maxlen);

int bt_read_security(struct bt0240 *bt, struct bt_security1 *security);
int bt_read_identity(struct bt0240 *bt, struct bt_identity *name);
int bt_read_uart(struct bt0240 *bt, struct bt_uart *uart);
int bt_read_role(struct bt0240 *bt, struct bt_role *role);

int bt_set_identity(struct bt0240 *bt, char *name);
int bt_set_uart(struct bt0240 *bt, int rate, char parity, int stopbits);
int bt_set_security(struct bt0240 *bt, char *pin);
int bt_set_role(struct bt0240 *bt, int master, int discoverable, char *peer);
