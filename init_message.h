#define MESSAGE_BUFFER 1024
#define KEY 8024;
#define PID_FILE "/var/run/hwscand.pid"

// WARNING NEEDS TO BE <= 9
#define NR_COMMANDS 4
// WARNING NEEDS TO BE <= 9
static const char *command_args[] = { "block", "usb", "firewire", "pci" };
static const int command_with_device[] = { 1, 0, 0, 0 };

typedef struct msgbuf {
        long mtype;
        char mtext[MESSAGE_BUFFER+1];
} message;

#define DEBUG 1

