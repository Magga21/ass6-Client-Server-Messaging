#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <ctime>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <exception>

/*
--------------------
TCP HELPER FUNCTIONS
--------------------
*/

// Checks if user input is a valid command
bool validateMessage(std::string input)
{
    size_t firstComma = input.find(',');
    // Extracts command from input
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
            // Stops the loop if input is missing parts
            if (!std::getline(ss, part, ','))
            {
                break;
            }
            parts.push_back(part);
        }

        // Keepd the remaining part as the message
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
            // Retries send() if system call was interrupted
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

        totalSent += bytesSent;
    }

    return true;
}

// Returns a timestamp for current date and time
std::string getTimestamp()
{
    std::time_t timestamp = std::time(nullptr);
    return std::ctime(&timestamp);
}

// Process response and handle incomplete data
bool processResponse(std::string &receiveBuffer)
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
            std::cerr << "Invalid frame! Missing SOH" << std::endl;
            return false;
        }

        // Find the length of the response
        uint16_t networkLength;
        std::memcpy(&networkLength, receiveBuffer.data() + 1, sizeof(networkLength));

        uint16_t length = ntohs(networkLength);

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
            std::cerr << "Invalid frame structure!" << std::endl;
            return false;
        }

        // Extract the response
        std::string response = receiveBuffer.substr(4, length - 5);

        std::cout << getTimestamp();
        std::cout << "Received: " << response << std::endl;

        receiveBuffer.erase(0, length);
    }

    return true;
}

/*
-----------
TCP CLIENT
-----------
Connects to a server, monitors input and socket activity and sends formatted commands.
*/

int main(int argc, const char *argv[])
{

    // Makes sure the correct number of command line arguments was given
    if (argc != 3)
    {
        std::cerr << "Invalid number of arguments. Need: ./tsamclient <IPaddress> <port>" << std::endl;
        exit(1);
    }

    // Stores the IP address and validate the port number
    const char *ipaddr = argv[1];
    int port;

    try
    {
        port = std::stoi(argv[2]);
    }
    catch (const std::exception &)
    {
        std::cerr << "Invalid port number" << std::endl;
        exit(1);
    }

    if (port < 1 || port > 65535)
    {
        std::cerr << "Port must be an integer between 1 and 65535" << std::endl;
        exit(1);
    }

    // Creates TCP socket
    int client_sockfd;
    if ((client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Error creating socket");
        exit(1);
    }

    // Sets up the server IPv4 address and port
    struct sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    // Converts IP address to its binary representation
    if (inet_pton(AF_INET, ipaddr, &servaddr.sin_addr) < 1)
    {
        std::cerr << "Invalid IP address of address family: " << ipaddr << std::endl;
        exit(1);
    }

    // Connects the client socket to the server socket
    if (connect(client_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Connection with the server failed");
        exit(1);
    }

    std::cout << "Successfully connected to server!" << std::endl;

    // Monitors user input and incoming data from the server
    std::vector<struct pollfd> monitored_fds;
    monitored_fds.push_back({STDIN_FILENO, POLLIN, 0});  // user input
    monitored_fds.push_back({client_sockfd, POLLIN, 0}); // server

    char buffer[5001];
    ssize_t bytesReceived;
    bool finished = false;

    std::string receiveBuffer;

    while (!finished)
    {
        // Waits for input from user or server
        int ready_fds = poll(monitored_fds.data(), monitored_fds.size(), -1);

        if (ready_fds < 0)
        {
            // Retries if poll() was interrupted
            if (errno == EINTR)
            {
                continue;
            }

            perror("poll() failed");
            finished = true;
            continue;
        }

        bool inputReady = monitored_fds[0].revents & POLLIN;

        if (inputReady)
        {
            std::string message;

            if (!std::getline(std::cin, message))
            {
                finished = true;
            }
            else if (message.size() + 5 > 5000)
            {
                std::cerr << "Command is too long. Commands can be up to a length of 5000 bytes" << std::endl;
            }
            else if (!validateMessage(message))
            {
                std::cerr << "Invalid command!\n"
                          << "Allowed commands:\n"
                          << "SENDMSG,<from>,<to>,<message>\n"
                          << "GETMSG,<username>\n"
                          << "STATUSREQ\n"
                          << std::endl;
            }
            else
            {
                std::string formattedMessage = formatMessage(message);

                if (!sendMessage(client_sockfd, formattedMessage))
                {
                    finished = true;
                }
                else
                {
                    std::cout << getTimestamp();
                    std::cout << "Sent: " << message << std::endl;
                }
            }
        }

        // Checks for socket errors
        bool socketError = monitored_fds[1].revents & (POLLERR | POLLNVAL);

        if (socketError)
        {
            std::cerr << "Server socket error" << std::endl;
            finished = true;
        }

        bool socketReady = monitored_fds[1].revents & (POLLIN | POLLHUP);

        if (!finished && socketReady)
        {
            bytesReceived = recv(client_sockfd, buffer, sizeof(buffer), 0);

            if (bytesReceived == 0)
            {
                if (!receiveBuffer.empty())
                {
                    std::cerr << "Server disconnected with an incomplete message" << std::endl;
                }
                else
                {
                    std::cerr << "Server disconnected" << std::endl;
                }

                finished = true;
            }
            else if (bytesReceived < 0)
            {
                // Retries if recv() was interrupted
                if (errno != EINTR)
                {
                    perror("Error receiving");
                    finished = true;
                }
            }
            else
            {
                receiveBuffer.append(buffer, bytesReceived);

                if (!processResponse(receiveBuffer))
                {
                    finished = true;
                }
            }
        }
    }
    close(client_sockfd);
    return 0;
}