
/* hwscan front end
   Copyright 2004 by SUSE (<adrian@suse.de>) */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "init_message.h"

int main( int argc, char **argv )
{
	int ret;
	unsigned short i;
        key_t key = KEY;
        int msgid;
	message m;
	char *device = argv[2];

	if ( argc < 2 ){
		fprintf( stderr, "help: hwscanqueue hwscan-commands\n" );
		fprintf( stderr, "help: commands:\n" );
		for ( i=0; i<NR_COMMANDS; i++ ){
			fprintf( stderr, "      --%s", command_args[i]  );
			if ( command_with_device[i] )
				fprintf( stderr, " device" );
			fprintf( stderr, "\n");
		}
		fprintf( stderr, "      --avail=yes/no id\n" );
		fprintf( stderr, "      --scan=device\n" );
		fprintf( stderr, "      --stop=device\n" );
		exit(1);
	}

	if ( !strncmp("--cfg=", argv[1], 6) && argc>2 )
		snprintf( m.mtext, MESSAGE_BUFFER, "C/sbin/hwscan %s %s", argv[1], argv[2]  );
	else if ( !strncmp("--avail=", argv[1], 8) && argc>2 )
		snprintf( m.mtext, MESSAGE_BUFFER, "C/sbin/hwscan %s %s", argv[1], argv[2]  );
	else if ( !strncmp("--scan=", argv[1], 7) )
		snprintf( m.mtext, MESSAGE_BUFFER, "A%s", argv[1]+7 );
	else if ( !strncmp("--stop=", argv[1], 7) )
		snprintf( m.mtext, MESSAGE_BUFFER, "R%s", argv[1]+7 );
	else if ( !strncmp("--", argv[1], 2) ){
		for ( i=0; i<NR_COMMANDS; i++ ){
			if ( !strcmp(argv[1]+2,command_args[i]) ){
#if DEBUG
				printf("COMMAND %s\n", command_args[i] );
#endif
				snprintf( m.mtext, MESSAGE_BUFFER, "S%d", i );
				if (command_with_device[i]){
					if ( !device ){
						fprintf(stderr, "need a device for this command\n");
						exit(1);
					}
					strncat( m.mtext, device, MESSAGE_BUFFER-3 );
				}
				break;
			}
		}
		if ( i>=NR_COMMANDS ){
			fprintf(stderr, "unknown command\n");
			exit(1);
		}
	}else
		exit(1);

        if ( (msgid = msgget(key, IPC_CREAT | 0600)) < 0 ){
		perror("unable to init.");
                exit(1);
        }
	m.mtype = 1;
	ret = msgsnd( msgid, &m, MESSAGE_BUFFER, IPC_NOWAIT);
#if DEBUG
	printf("SEND %s, return %d\n", m.mtext, ret );
#endif

	if ( ret < 0 )
		perror("message send failed");
	else{
		// success ... start hwscand, if it is not yet running
		ssize_t r;
		char buffer[1024];
		char link[1024];
		int fd = open( PID_FILE, O_RDONLY );
        	if ( fd >= 0 && (r=read(fd,buffer,1023)) > 0 ){
			close(fd);
			buffer[r]='\0';
			snprintf(link, 1023, "/proc/%s/exe", buffer);
			if ( (r=readlink( link, buffer, 1023 )) > 0 ){
				buffer[r]='\0';
				if ( r<8 )
					fd=-1;
				else if ( strcmp("/hwscand", buffer+strlen(buffer)-8) )
					fd=-1;
			}else
				fd=-1;
		}else
			fd=-1;

		if ( fd < 0 ){
			pid_t pid;
			signal(SIGCHLD,SIG_IGN);
			pid=fork();
			if (pid==0){
				/* Change directory to allow clean shut-down */
				chdir("/");
				/* Close std fds */
				close(0);
				close(1);
				close(2);
				/* Start hwscand */
				execve("/sbin/hwscand", 0, 0);
			}
		}
	}

	exit(ret);
}

