
/* hwscan front end
   Copyright 2004 by SUSE (<adrian@suse.de>) */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "init_message.h"

#define TIMEOUT 2
#define LONG_TIMEOUT 0
#define BUFFERS 1024

int main( int argc, char **argv )
{
        int ret, i;
	key_t key = KEY;
	int msgid;
	int mode = 0;
	int dev_nr = 0;
	int lines = 0;
	int block, usb, firewire, pci;
	int dev_last_state[BUFFERS];
	int dev_counter[BUFFERS];
	char * command_device[NR_COMMANDS][BUFFERS];
	time_t command_device_last[NR_COMMANDS][BUFFERS];
	time_t last;
	char **commands;
	char **devices;
	char buffer[32];
	message m;

	// are we running already, maybe ?
	{
		do {
			ssize_t r;
			char b[1024];
			char link[1024];
			int fd = open( PID_FILE, O_RDONLY );
			if ( fd >= 0 && (r=read(fd,b,1023)) > 0 ){
			        close(fd);
			        b[r]='\0';
			        snprintf(link, 1023, "/proc/%s/exe", b);
			        if ( (r=readlink( link, b, 1023 )) > 0 ){
			                b[r]='\0';
			                if ( r<8 )
			                        unlink(PID_FILE);
			                else if ( strcmp("/hwscand", b+strlen(b)-8) )
			                        unlink(PID_FILE);
					else
						exit(1);
			        }else
		                        unlink(PID_FILE);
		        }else if ( fd >= 0 )
	                        unlink(PID_FILE);
		} while ( 0 > (ret = open( PID_FILE, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR ) ) );
		sprintf(buffer, "%d", getpid());
		if ( ret < 0 || write(ret,buffer,strlen(buffer)) <= 0 ){
			perror("hwscand: unable to write pid file "PID_FILE);
			exit(1);
		}
		close(ret);
	}

	// initialize ...
	for ( i=0; i<NR_COMMANDS; i++ ){
		command_device[i][0] = 0;
		command_device_last[i][0] = 1;
	}

	last=block=usb=firewire=pci=0;
	commands = (char**) malloc( BUFFERS * sizeof(char*) );
	devices  = (char**) malloc( BUFFERS * sizeof(char*) );

	msgid = msgget(key, IPC_CREAT | 0600);
        if (msgid < 0) {
		perror("msgget");
		exit(1);
        }

	while (1) {
		if ( last || dev_nr )
			mode = IPC_NOWAIT;
		else
			mode = 0;

		if( msgrcv(msgid, &m, MESSAGE_BUFFER, 1, mode) >= 0 ){
			char *p = m.mtext;

			if ( p == 0 ){
				fprintf( stderr, "hwscand: error, zero sized message\n" );
			}else{
				if ( p[0] == 'S' && strlen(p) > 1 ){
					// scan calls
					char z[2];
					int c;
					z[0] = *(p+1);
					z[1] = '\0';
					c = atoi(z);
					if ( c < NR_COMMANDS ){
						if ( ! command_with_device[c] ){
							last = time(0L);
							if ( LONG_TIMEOUT+command_device_last[c][0] < time(0L) )
								command_device_last[c][0] = 0;
						}else
						for ( i=0; i<BUFFERS; i++ ){
							if ( !command_device[c][i] ){
								last = time(0L);
								command_device[c][i] = strdup(p+2);
								command_device[c][i+1] = 0;
								command_device_last[c][i] = 0;
								break;
							}else if ( !strcmp(command_device[c][i], p+2) ){
								last = time(0L);
								if ( LONG_TIMEOUT+command_device_last[c][i] < time(0L) )
									command_device_last[c][i] = 0;
								break;
							}
						}
					}
				}
				if ( p[0] == 'C' && lines < BUFFERS ){
					last = time(0L);
					// config calls
					commands[lines] = strdup(p+1);
					lines++;	
				}
				if ( p[0] == 'A' && dev_nr < BUFFERS ){	
					// add scan devices
					devices[dev_nr]        = strdup(p+1);
					dev_last_state[dev_nr] = 0;
					dev_counter[dev_nr]    = 0;
					dev_nr++;
				}
				if ( p[0] == 'R' && dev_nr < BUFFERS ){	
					for ( i=0; i<dev_nr; i++ ){
						if ( !strcmp(p+1, devices[i]) ){
							int j;
							free(devices[i]);
							for ( j=i; j+1<dev_nr; j++ ){
								devices[j]        = devices[j+1];
								dev_last_state[j] = dev_last_state[j+1];
								dev_counter[j]    = dev_counter[j+1];
							}
							dev_nr--;
						}
					}
				}
			}
#if DEBUG
				printf("CALL RECEIVED %s\n", p);
#endif
		}else{
			// we do this only in scanning mode ...

			sleep(1);
			for ( i=0; i<dev_nr; i++ ){
				if (dev_counter[i]<0) continue;
				dev_counter[i]++;
				if ( dev_counter[i] > 5 ){
					int fd;
					char buf[MESSAGE_BUFFER];
					dev_counter[i] = 0;
					fd = open( devices[i], O_RDONLY );
					strcpy( buf, "/sbin/hwscan --fast --partition --only=");
					strcat( buf, devices[i] );
					if ( fd < 0 ){
						if ( dev_last_state[i] )
							system(buf);
						dev_last_state[i] = 0;
					}else{
						if ( dev_last_state[i] == 0)
							system(buf);
						dev_last_state[i] = 1;
						close(fd);
					}
				}
			}
		}
		
		if ( last && (last+TIMEOUT <= time(0L)) ){
			char buf[MESSAGE_BUFFER * NR_COMMANDS];
			int run_really = 0;

			last=0;
			strcpy( buf, "/sbin/hwscan --fast --boot --silent" );
			for ( i=0; i<NR_COMMANDS; i++ ){
				if ( command_with_device[i] == 0 &&
				     command_device_last[i][0] == 0 ){
					command_device_last[i][0] = time(0L);
					strcat( buf, " --");
					strcat( buf, command_args[i] );
				 	run_really = 1;
				} else {
					int j;
					int commappended = 0;

					for ( j=0; j<BUFFERS; j++ ){
						if ( !command_device[i][j] )
							break;
						if ( command_device_last[i][j] == 0 ){
							if (!commappended) {
								strcat( buf, " --");
								strcat( buf, command_args[i] );
								commappended = 1;
						        }
							strcat( buf, " --only=" );
							strcat( buf, command_device[i][j] );
							command_device_last[i][j] = time(0L);
							run_really = 1;
							if (strlen(buf) > sizeof(buf) - MESSAGE_BUFFER)
								break;
						}
					}
				}
				if (strlen(buf) > sizeof(buf) - MESSAGE_BUFFER) {
					last = time(0L);	/* call me again */
					break;
				}
			}

			if ( run_really ){
#if DEBUG
				printf("RUN %s\n", buf);
#endif
				system(buf);
#if DEBUG				
				printf("RUN quit %s\n", buf);
#endif
			}
			if ( lines ){
				for (i=0; i<lines; i++){
#if DEBUG
				printf("CALL DIRECT %s\n", commands[i]);
#endif
					system(commands[i]);
#if DEBUG
				printf("CALL quit %s\n", commands[i]);
#endif
					free(commands[i]);
				}
				lines=0;
			}
		}
	}

	return 0;
}
