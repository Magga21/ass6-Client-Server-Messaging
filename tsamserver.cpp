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

// Formats message according to communication protocol: <SOH><length><STX><command><ETX>
std::string formatMessage(std::string message)
{
    const char SOH = 0x01;
    const char STX = 0x02;
    const char ETX = 0x03;

    uint16_t length = message.size() + 5;
    uint16_t networkLength = htons(length);

    std::string messageToServer;

    messageToServer.push_back(SOH);
    messageToServer.append(reinterpret_cast<const char *>(&networkLength), sizeof(networkLength));
    messageToServer.push_back(STX);
    messageToServer.append(message);
    messageToServer.push_back(ETX);

    return messageToServer;
}

// Sends the message and handles partial sends
bool sendMessage(int sockfd, const std::string &formattedMessage)
{
    size_t totalSent = 0;

    // Keep sending until all bytes of message have been sent
    while (totalSent < formattedMessage.size())
    {
        ssize_t bytesSent = send(sockfd, formattedMessage.data() + totalSent, formattedMessage.size() - totalSent, MSG_NOSIGNAL);

        if (bytesSent < 0)
        {
            // Retries send if system call was interrupted
            if (errno == EINTR)
            {
                continue;
            }

            perror("send() failed");
            return false;
        }

        if (bytesSent == 0)
        {
            std::cerr << "Connection closed while sending" << std::endl;
            return false;
        }

        // Update total number of bytes that have been sent successfully
        totalSent += static_cast<size_t>(bytesSent);
    }

    return true;
}


// Process response and handle incomplete data
bool processResponse(std::string &receiveBuffer, std::vector<std::string> &commands)
{
    const char SOH = 0x01;
    const char STX = 0x02;
    const char ETX = 0x03;

	// Start checking the response data
    while (receiveBuffer.size() >= 5)
    {
        // Check start frame
        if (receiveBuffer[0] != SOH)
        {
            std::cerr << "Invalid frame: missing SOH" << std::endl;
            return false;
        }

        // Find the length of the response
        uint16_t networkLength;
        std::memcpy(&networkLength, receiveBuffer.data() + 1, sizeof(networkLength));

        uint16_t length = ntohs(networkLength);

        // A valid frame needs contains at least 5 bytes
        if (length < 5)
        {
            std::cerr << "Invalid frame length!" << std::endl;
            return false;
        }

        // Wait for more data if entire message hasn't arrived
        if (receiveBuffer.size() < length)
        {
            break;
        }

        // Check remaining frames
        if (receiveBuffer[3] != STX || receiveBuffer[length - 1] != ETX)
        {
            std::cerr << "Invalid frame structure" << std::endl;
            return false;
        }

        // Extract the response
        std::string response = receiveBuffer.substr(4, length - 5);

		commands.push_back(response);

        std::cout << getTimestamp();
        std::cout << "Received: " << response << std::endl;

        receiveBuffer.erase(0, length);
    }

    return true;
}



/*
OPEN SOCKET FUNCTION 
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

	std::map<std::string, std::vector<std::string>> storedMessages;

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

				std::vector<std::string> commands;

				if(!processResponse(receiveBuffers[fds[i].fd], commands))
				{
					std::cout << "Invalid frame received" << std:: endl;

				}

				for(const std::string &command : commands)
				{
					if(!validateMessage(command))
					{
						std::cerr << "Invalid command: " << command << std::endl;
						continue;
					}
					std::cout << "Valid command: " << command << std::endl;
					if (command.rfind("SENDMSG,", 0) == 0)
					{
						std::stringstream ss(command);
						std::string part;
						std::vector<std::string> parts;

						// Splits the first three parts by commas (in case the message part has more commas)
						for (int i = 0; i < 3; i++)
						{
							// Stops the loop if input is missing parts
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

						std::string storedMsg = parts[1] + "," + parts[3];

						storedMessages[parts[2]].push_back(storedMsg);
						}
					}
					else if (command.rfind("GETMSG,", 0) == 0)
					{
						std::string username = command.substr(7);
						auto it = storedMessages.find(username);
						if (it != storedMessages.end() && !it->second.empty())
						{
							std::string msgs = it->second.front();

							std::string framedMessage = formatMessage(msgs);

							if(sendMessage(fds[i].fd, framedMessage))
							{
								std::cout << msgs << std::endl;

								it->second.erase(it->second.begin());
							}
							else{
								std::cerr << "Failed to send message" << std::endl;
							}
						}
						else
						{	
							std:: string msgs("");
							msgs.append ("No messages found for " + username);
							std::string framedMessage = formatMessage(msgs);
							sendMessage(fds[i].fd, framedMessage);
						}
					}
					else // Command == STATUSREQ
					{
						std:: string outputmsgs("");
						for(const auto &entry : storedMessages)
							if(entry.second.size() > 0)
							{
								std::string numofmsgs = std::to_string(entry.second.size());
								if (!outputmsgs.empty())
								{
									outputmsgs += ",";
								}
								outputmsgs.append(entry.first + ":" + numofmsgs);
								
							}
						if (outputmsgs.empty())
						{
							outputmsgs = "EMPTY";
						}
						std::string framedMessage = formatMessage(outputmsgs);
						sendMessage(fds[i].fd, framedMessage);
					}
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
