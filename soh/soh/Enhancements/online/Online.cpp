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

void SendPacketMessage(OnlinePacket* packet, TCPsocket* sendTo) {
    if (*sendTo != nullptr) {
        SDLNet_TCP_Send(*sendTo, packet, sizeof(OnlinePacket));
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

    SDLNet_SocketSet ss = SDLNet_AllocSocketSet(2);
    SDLNet_TCP_AddSocket(ss, clientSocket);

    while (serverOpen) {
        int check = SDLNet_CheckSockets(ss, ~0);

        if (SDLNet_SocketReady(clientSocket)) {
            int len = SDLNet_TCP_Recv(clientSocket, &serverPacket, sizeof(OnlinePacket));
            if (!len) {
                SPDLOG_INFO("SDLNet_TCP_Recv: %s\n", SDLNet_GetError());
                serverOpen = false;
            } else if (len > 0) {
                SetLinkPuppetData(&serverPacket, 0);
                SPDLOG_INFO("Received: {0}", serverPacket.posRot.pos.x);
            } else {
                serverOpen = false;
            }
        }
    }
}

Server::~Server() {
    SDLNet_TCP_Close(clientSocket);
    SDLNet_TCP_Close(serverSocket);
    onlineThread.join();
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

Client::~Client() {
    SDLNet_TCP_Close(clientSocket);
    onlineThread.join();
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

    SDLNet_SocketSet ss = SDLNet_AllocSocketSet(2);
    SDLNet_TCP_AddSocket(ss, clientSocket);

    while (clientConnected) {
        int check = SDLNet_CheckSockets(ss, ~0);

        if (SDLNet_SocketReady(clientSocket)) {
            int len = SDLNet_TCP_Recv(clientSocket, &clientPacket, sizeof(OnlinePacket));
            if (!len) {
                SPDLOG_INFO("SDLNet_TCP_Recv: %s\n", SDLNet_GetError());
                clientConnected = false;
            } else if (len > 0) {
                SetLinkPuppetData(&clientPacket, 0);
                SPDLOG_INFO("Received: {0}", clientPacket.posRot.pos.x);
            } else {
                clientConnected = false;
            }
        }
    }
}

} // namespace Online
} // namespace Ship