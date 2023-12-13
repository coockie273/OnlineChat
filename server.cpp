#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <algorithm>
#include <errno.h>
#include <map>
#include <iostream>
#include <sstream>

#define DAEMON_NAME "chat_server"
#define WORKING_DIRECTORY  "/"
#define PORT 8080

using namespace std;

class Client {

private:
    string name;
    int message_count;

public:

    Client() {
        name = "Client";
        message_count = 0;
    }

    Client(const string& n) : name(n), message_count(0) {}

    string get_name() const {
        return name;
    }

    int get_count() const {
        return message_count;
    }

    void add_message() {
        message_count++;
    }

    void set_name(const string& n) {
        name = n;
    }
};

map<int, Client> clients;

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
            syslog(LOG_INFO, "Chat server was terminated");
            closelog();
            exit(0);
            break;

    }

}

string clear_string(string s) {

    string res;
    remove_copy_if(s.begin(), s.end(), back_inserter(res), [](unsigned char c) {return c < 32;});
    return res;

}


int find_client(string name) {


    for (const auto& client : clients) {

        if (client.second.get_name() == name) {

            return client.first;

        }
    }

    return -1;
}


void handle_message(string message, int sock, Client sender) {

    syslog(LOG_INFO, "%s", message.c_str());

    istringstream iss(message);

    string command;

    iss >> command;

    if (command == "/members_count") {

        char buf[1024];
        sprintf(buf, "Number of members on chat: %ld \n", clients.size());
        send(sock, buf, strlen(buf), 0);

    } else if (command == "/close") {

        const char* response = "Disconnect from online chat \n";
        send(sock, response, strlen(response), 0);
        clients.erase(sock);

    } else if (command == "/members_list") {

        string response = "List of members in chat:\n";

        for (const auto& client : clients) {

            response += client.second.get_name() + "\n";

        }

        syslog(LOG_INFO, response.c_str());
        send(sock, response.c_str(), strlen(response.c_str()), 0);

    } else if (command == "/message_all") {

        string mes;

        getline(iss, mes);

        mes = mes.substr(1);
        string response = "Message from " + sender.get_name() + " : " + mes + "\n";

        for (const auto& client : clients) {

            if (client.first != sock) send(client.first, response.c_str(), strlen(response.c_str()), 0);

        }

    } else if (command == "/message") {

        string rec, mes;

        iss >> rec;

        getline(iss, mes);

        mes = mes.substr(1);

        syslog(LOG_INFO, "%s %s", rec.c_str(), mes.c_str());

        int rec_sock = find_client(rec);

        if (rec_sock == -1) {

           const char* response = "Invalid nickname of receiver\n";
           send(sock, response, strlen(response), 0);

           return;
        }

        string response = "Message from " + sender.get_name() + " : " + mes + "\n";
        send(rec_sock, response.c_str(), strlen(response.c_str()), 0);

    } else {

        const char* mes = "Unknown command\n";

        send(sock, mes, strlen(mes), 0);

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

    syslog(LOG_INFO, "Server for chat was started");
    listen(listener, 10);

    while(1) {

        fd_set readset;
	FD_ZERO(&readset);
	FD_SET(listener, &readset);
        for (const auto& client : clients) {

            FD_SET(client.first, &readset);

        }

	timeval timeout;

	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	int mx;
	if (clients.empty()) {
            mx = listener;
        } else {
            mx = max(listener, clients.rbegin()->first);
        }

	if (select(mx + 1, &readset, NULL, NULL, &timeout) < 0) {
		syslog(LOG_ERR, "Failed to select sockets %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (FD_ISSET(listener, &readset)) {

		int sock = accept(listener, NULL, NULL);

                syslog(LOG_INFO, "new client");

         	if (sock < 0) {

	            syslog(LOG_ERR, "Failed to accept socket");
          	    exit(EXIT_FAILURE);

		}

		fcntl(sock, F_SETFL, O_NONBLOCK);
                const char* request = "Welcome to online chat, what is your nickname? \n";
                send(sock, request, strlen(request), 0);
		clients[sock] = Client();
        }

	for (auto& client : clients) {

   	    if (FD_ISSET(client.first, &readset)) {

                char buf[1024];
	        recv(client.first, buf, 1024, 0);
                string message = clear_string(string(buf));
                syslog(LOG_INFO, "New message from client: %s", buf);
	        if (! client.second.get_count()) {
                    client.second.set_name(message);
                    client.second.add_message();
                    const char* request = "Hello! \n";
                    send(client.first, request, strlen(request), 0);
                    continue;
                }
                handle_message(message, client.first, client.second);
                memset(buf, 0, sizeof(buf));
            }

	}

    }

    return 0;
}

