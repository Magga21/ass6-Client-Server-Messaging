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

#include <ctime>

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <cstring>

#include <thread>
#include <map>

#include <unistd.h>

// Checks if user input is a valid command
bool validateMessage(std::string input)
{
    size_t firstComma = input.find(',');
    // Extract command from input
    std::string command = input.substr(0, firstComma);

    if (input == "STATUSREQ")
    {
        return true;
    }
    else if (command == "GETMSG")
    {
        // Checks that a comma seperates GETMSG from the username
        if (firstComma == std::string::npos)
        {
            return false;
        }

        std::string username = input.substr(firstComma + 1);

        // Make sure there are no additional commas
        if (username.find(',') != std::string::npos)
        {
            return false;
        }

        if (!username.empty())
        {
            return true;
        }
    }
    else if (command == "SENDMSG")
    {
        std::stringstream ss(input);
        std::string part;
        std::vector<std::string> parts;

        // Splits the first three parts by commas (in case the message part has more commas)
        for (int i = 0; i < 3; i++)
        {
            // Stops the loop if input is missing required parts
            if (!std::getline(ss, part, ','))
            {
                break;
            }
            parts.push_back(part);
        }

        // Keeps the remaining part as the message
        if (parts.size() == 3)
        {
            if (std::getline(ss, part))
            {
                parts.push_back(part);
            }
        }

        // Checks if input containes these parts: SENDMSG,<from>,<to>,<message>
        if (parts.size() != 4)
        {
            return false;
        }

        // Checks if each part contains data
        for (size_t i = 0; i < parts.size(); i++)
        {
            if (parts[i].empty())
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

/*
Time stamp helper function 
*/
// Returns a timestamp for current date and time
std::string getTimestamp()
{
    std::time_t timestamp = std::time(nullptr);
    return std::ctime(&timestamp);
}


// Process complete frames and keeps incomplete data in the buffer
bool processReceivedData(std::string &receiveBuffer)
{
    const char SOH = 0x01;
    const char STX = 0x02;
    const char ETX = 0x03;

    while (receiveBuffer.size() >= 5)
    {
        // Start of frame
        if (receiveBuffer[0] != SOH)
        {
            std::cerr << "Invalid frame: missing SOH" << std::endl;
            return false;
        }

        // Read the two byte length in network byte order
        uint16_t networkLength;
        std::memcpy(&networkLength, receiveBuffer.data() + 1, sizeof(networkLength));

        uint16_t length = ntohs(networkLength);

        // A valid frame needs contains at least 5 bytes
        if (length < 5)
        {
            std::cerr << "Invalid frame length" << std::endl;
            return false;
        }

        // Wait for more data if the frame is incomplete
        if (receiveBuffer.size() < length)
        {
            break;
        }

        // Check the remaining framing characters
        if (receiveBuffer[3] != STX || receiveBuffer[length - 1] != ETX)
        {
            std::cerr << "Invalid frame structure" << std::endl;
            return false;
        }

        // Extract the command or response from the frame
        std::string response = receiveBuffer.substr(4, length - 5);

        std::cout << getTimestamp();
        std::cout << "Received: " << response << std::endl;

        receiveBuffer.erase(0, length);
    }

    return true;
}



/*
OPEN SOCKET FUNCTION -Reuse our old ones? 
TODO: Rewrite function, currently edited from template, might just keep it
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
		
	std::map<int, std::string> receiveBuffers;

	fds.push_back(pfd);
	
	struct sockaddr_in client;
	socklen_t clientLen = sizeof(client);

	// Create and write into the log file with the append command
	std::ofstream logFile("server.log", std::ios::app);
	if (!logFile.is_open())
	{
		std::cerr << "Failed to open log file!" << std::endl;
	}

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
				std::cerr << "Failed to accept socket! Braking! " << std::endl;
				close(sock);
				exit(1);
				
			}
			pollfd clientfds;
			clientfds.fd = yee;
			clientfds.events = POLLIN;
		
			fds.push_back(clientfds);


		}
	
	size_t i = 1;
	
	while (i < fds.size())
	{
		if(fds[i].revents & POLLIN)
		{
			char buffer[5000];

			int msgs = recv(fds[i].fd, buffer, sizeof(buffer), 0);
			
			if(msgs < 0)
			{
				perror("recv failed");
			}

			else if (msgs == 0)
			{
				receiveBuffers.erase(fds[i].fd);
				close(fds[i].fd);
				fds.erase(fds.begin() + i);
				continue;

			}

			else
			{
				receiveBuffers[fds[i].fd].append(buffer, msgs);

				if(!processReceivedData(receiveBuffers[fds[i].fd]))
				{
					//TODO:
					std::cout << "Handle error goes here" << std:: endl;
				}

				std::string timeStamp(getTimestamp()); 

				std::string message(buffer, msgs);

				timeStamp.pop_back();

				logFile << "[" << timeStamp << "] RECEIVED: " << message << std::endl;

			}

		}
		i++; 
	}

	}
	
	}
	
	logFile.close();
	close(sock);
	return 0;
}
