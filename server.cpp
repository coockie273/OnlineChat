#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <algorithm>
#include <set>

#define DAEMON_NAME "chat_server"
#define WORKING_DIRECTORY  "/"
#define PORT 8080


void daemonize() {

    pid_t pid;
    pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Forking failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    pid_t sid = setsid();

    if (sid < 0) {
        syslog(LOG_ERR, "Failed to create SID");
        exit(EXIT_FAILURE);
    }

    if (chdir(WORKING_DIRECTORY) < 0) {
        syslog(LOG_ERR, "Failed to change the working directory");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

}


void signal_handler(int sig) {

    switch(sig) {

        case SIGTERM:
            syslog(LOG_INFO, "Char server was terminated");
            closelog();
            exit(0);
            break;

    }

}



int main() {

    openlog(DAEMON_NAME, LOG_PID, LOG_USER);

    signal(SIGTERM, signal_handler);

    daemonize();

    struct sockaddr_in addr;

    int listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener < 0) {
        syslog(LOG_ERR, "Failed to create listener socket");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {

       syslog(LOG_ERR, "Failed to bind listener");
       exit(EXIT_FAILURE);

    }

    syslog(LOG_INFO, "Web server was started");
    listen(listener, 10);
	
	set<int> clients;
	
	clients.clear()

    while(1) {

        fd_set readset;
		FD_ZERO(&readset);
		FD_SET(listener, &readset);
        for (set<int>::iterator it = clients.begin(); it != clients.end(); it++) FD_SET(*it, &readset);
		
		timeval timeout;
		
		timeout.tv_sec = 15;
		timeout.tv_usec = 0;
		
		int mx = max(listener, *max_element(clients(clients.begin(), clients.end())));
		
		if (select(mx + 1, &readset, NULL, NULL, &timeout) <= 0) {
			
			syslog(LOG_ERR, "Failed to select sockets");
			exit(EXIT_FAILURE);
			
		}
		
		if (FD_ISSET(listener, &readset)) {
			
			int sock = accept(listener, NULL, NULL);
			
			if (sock < 0) {
				
				syslog(LOG_ERR, "Failed to accept socket");
			    exit(EXIT_FAILURE);
				
			}
			
			fcntl(sock, F_SETFL, O_NONBLOCK);
			clients.insert(sock);
			
			for (set<int>::iterator it = clients.begin(); it != clients.end(); it++) {
				
				if (FD_ISSET(*it, &readset)) {
					
					int bytes_read = recv(*it, buf, 1024, 0);
					
					if (bytes_read <= 0) {
			
						close(*it);
						clients.erase(*it);
						
						continue;
						
					}
					
					send(*it, buf, bytes_read, 0);
				}
			}
		}
    }

    return 0;
}