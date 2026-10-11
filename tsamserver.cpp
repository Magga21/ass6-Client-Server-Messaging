//sup

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <list>
#include <poll.h>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

/*
OPEN SOCKET FUNCTION -Reuse our old ones? 
TODO: Rewrite function, currently only a template
*/

int open_socket(int portno)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.

   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
     perror("Failed to open socket");
    return(-1);
   }

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
      perror("Failed to set SO_REUSEADDR:");
   }
   set = 1; 

   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = INADDR_ANY;
   sk_addr.sin_port        = htons(portno);

   // Bind to socket to listen for connections from clients

   if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
   {
      perror("Failed to bind to socket:");
	  close(sock);
      return(-1);
   }

	return(sock);
}

/*
LISTEN TO PORT FUNC- again reuse prior?
Might not need one
*/




/*

Structure suggested by AI
 +-- poll()
         Waits for socket activity
         |
         +-- accept()
         |      Accepts new clients
         |
         +-- recv()
                Receives client data
                |
                +-- clientCommand()
                       Interprets commands
                       |
                       +-- sendMessage()
                              Sends messages

*/

/*
---------------
	MAIN
---------------
*/

int main(int argc, const char *argv[])
{
 // Make sure the correct number of command line arguments was given
    if (argc != 2)
    {
        std::cerr << "Invalid number of arguments! Need: ./tsamserver <port>" << std::endl;
        exit(1);
    }

    // Store the port number from the command line
    int port = std::stoi(argv[1]);

	int sock;
	if((sock = open_socket(port)) < 0)
	{
		std::cerr << "Failed to open socket! Braking! " << std::endl;
		exit(1);
	}

	// TODO: See if needs to be changed later on

	if((listen(sock, 5))< 0)
	{
		std::cerr << "Failed to listen to socket! Braking! " << std::endl;
		close(sock);
		exit(1);
	}

	std::vector<pollfd> fds;
	pollfd pfd;
	pfd.fd = sock;
	pfd.events = POLLIN;
	int timeout_msecs = 500;
	int ret;
		int i;

	fds.push_back(pfd);
	
	struct sockaddr_in client;
	socklen_t clientLen = sizeof(client);
	while (true){
	ret = poll(fds.data(), fds.size(), timeout_msecs);
	
	if(ret < 0)
	{
		std::cerr << "Failed to listen to socket! Braking! " << std::endl;
		close(sock);
		exit(1);
	}

	else if(ret == 0)
	{
		continue;

	}

	else
	{
		if((fds[0].revents & POLLIN) > 0 ){
			int yee;

			if((yee = accept(sock, (struct sockaddr*)&client, &clientLen)) < 0)
			{
				// TODO: 

			}
		}
	}
	
	}
	//TODO: Bind(), and other stuff

	close(sock);
	return 0;
}
