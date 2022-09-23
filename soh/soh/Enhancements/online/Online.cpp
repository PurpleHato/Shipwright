#include "Online.h"
#include <spdlog/spdlog.h>
#include <libultraship/Lib/Fast3D/U64/PR/ultra64/gbi.h>

#define MAX_CLIENTS 32

extern "C" void SetLinkPuppetData(Ship::Online::OnlinePacket * packet, u8 player_id);
extern "C" void Rupees_ChangeBy(s16 rupeeChange);
extern "C" u8 rupeesReceived;

namespace Ship {
namespace Online {

void InitOnline(char* ipAddr, int port)
{
    if (SDLNet_Init() == -1) {
        SPDLOG_INFO("SDLNet_Init: %s\n", SDLNet_GetError());
        return;
    }

    if (ipAddr == nullptr) {
        server.CreateServer(port);
    } else {
        client.CreateClient(ipAddr, port);
    }
}

void Server::RunServer() {
    SPDLOG_INFO("Server Started!\n");

    while (!clientConnected) {
        /* try to accept a connection */
        clientSocket = SDLNet_TCP_Accept(serverSocket);

        if (!clientSocket) {
            SPDLOG_INFO("Searching for Client...\n");
            SDL_Delay(1000);
            continue;
        }

        clientConnected = true;
    }

    /* get the clients IP and port number */
    IPaddress* remoteip;
    remoteip = SDLNet_TCP_GetPeerAddress(clientSocket);
    if (!remoteip) {
        SPDLOG_INFO("SDLNet_TCP_GetPeerAddress: %s\n", SDLNet_GetError());
        return;
    }

    /* print out the clients IP and port number */
    Uint32 ipaddr;
    ipaddr = SDL_SwapBE32(remoteip->host);

    SPDLOG_INFO("Server connected to Client!\n");

    while (serverOpen) {
        /* read the buffer from client */
        char message[1024];
        int len = SDLNet_TCP_Recv(clientSocket, message, 1024);
        if (!len) {
            SPDLOG_INFO("SDLNet_TCP_Recv: %s\n", SDLNet_GetError());
        }

        SPDLOG_INFO("Received: %.*s\n", len, message);
    }

    SDLNet_TCP_Close(clientSocket);
}

void Server::CreateServer(int serverPort) {
    IPaddress ip;

    SPDLOG_INFO("Starting server...\n");

    if (SDLNet_ResolveHost(&ip, NULL, serverPort) == -1) {
        SPDLOG_INFO("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
        return;
    }

    serverSocket = SDLNet_TCP_Open(&ip);

    if (!serverSocket) {
        SPDLOG_INFO("SDLNet_TCP_Open: %s\n", SDLNet_GetError());
        return;
    }

    serverOpen = true;

    onlineThread = std::thread(&Server::RunServer, this);
}

void Client::CreateClient(char* ipAddr, int port) {
    SPDLOG_INFO("Starting client...\n");
    IPaddress ip;

    if (SDLNet_ResolveHost(&ip, ipAddr, port) == -1) {
        SPDLOG_INFO("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
        return;
    }

    clientSocket = SDLNet_TCP_Open(&ip);
    if (!clientSocket) {
        SPDLOG_INFO("SDLNet_TCP_Open: %s\n", SDLNet_GetError());
        return;
    }

    clientConnected = true;

    onlineThread = std::thread(&Client::RunClient, this);
}

void Client::RunClient() {
    SPDLOG_INFO("Client Connected to Server!\n");

    while (clientConnected) {
        char message[1024] = "Hello World!";
        int len = strlen(message);

        /* strip the newline */
        message[len - 1] = '\0';

        if (len) {
            int result;

            /* print out the message */
            SPDLOG_INFO("Sending: %.*s\n", len, message);

            result = SDLNet_TCP_Send(clientSocket, message, len); /* add 1 for the NULL */

            if (result < len)
                SPDLOG_INFO("SDLNet_TCP_Send: %s\n", SDLNet_GetError());
        }
    }

    SDLNet_TCP_Close(clientSocket);
}

} // namespace Online
} // namespace Ship