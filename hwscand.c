
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
	time_t last;
	char **commands;
	char **devices;
	char buffer[32];
	message m;

	last=block=usb=firewire=pci=0;
	commands = (char**) malloc( BUFFERS * sizeof(char*) );
	devices  = (char**) malloc( BUFFERS * sizeof(char*) );

	ret = open( PID_FILE, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );
	sprintf(buffer, "%d", getpid());
	if ( ret < 0 || write(ret,buffer,strlen(buffer)) <= 0 ){
		perror("hwscand: unable to write pid file "PID_FILE);
		exit(1);
	}
	close(ret);

	while (1) {
		if ( last || dev_nr )
			mode = IPC_NOWAIT;
		else
			mode = 0;

		msgid = msgget(key, 0);
		if( (msgid >= 0) && msgrcv(msgid, &m, MESSAGE_BUFFER, 1, mode) >= 0 ){
			char *p = m.mtext;
			if ( p == 0 ){
				fprintf( stderr, "hwscand: error, zero sized message\n" );
			}else{
				if ( p[0] == 'S' && strlen(p) == 5 ){	
					last = time(0L);
					// scan calls
					if ( p[1] == 'X' )
						block = 1;
					if ( p[2] == 'X' )
						usb = 1;
					if ( p[3] == 'X' )
						firewire = 1;
					if ( p[4] == 'X' )
						pci = 1;
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
		}else{
			// we do this only in scanning mode ...

			sleep(1);
			for ( i=0; i<dev_nr; i++ ){
				if (dev_counter[i]<0) continue;
				dev_counter[i]++;
				if ( dev_counter[i] > 5 ){
					int fd;
					dev_counter[i] = 0;
					fd = open( devices[i], O_RDONLY );
					if ( fd < 0 ){
						if ( dev_last_state[i] )
							system("/usr/sbin/hwscan --partition");
						dev_last_state[i] = 0;
					}else{
						char c;
						if ( read(fd, &c, 1)>0 ){
							if ( dev_last_state[i] == 0)
								system("/usr/sbin/hwscan --partition");

							dev_last_state[i] = 1;
						}else{
							dev_last_state[i] = 0;
						}
						close(fd);
					}
				}
			}
		}
		
		if ( last && (last+TIMEOUT < time(0L)) ){
			if ( block||usb||firewire||pci ){
				char buf[MESSAGE_BUFFER];
				strcpy( buf, "/usr/sbin/hwscan --fast --boot --silent " );
				if ( block )
					strcat( buf, "--block " );
				if ( usb )
					strcat( buf, "--usb " );
				if ( firewire )
					strcat( buf, "--firewire " );
				if ( pci )
					strcat( buf, "--pci " );
				system(buf);
				block=usb=firewire=pci=0;
			}
			if ( lines ){
				for (i=0; i<lines; i++){
					system(commands[i]);
					free(commands[i]);
				}
				lines=0;
			}
			last=0;
		}
	}

	return 0;
}
