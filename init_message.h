#define MESSAGE_BUFFER 1024
#define KEY 8024;
#define PID_FILE "/var/run/hwscand.pid"

typedef struct msgbuf {
        long mtype;
        char mtext[MESSAGE_BUFFER+1];
} message;


