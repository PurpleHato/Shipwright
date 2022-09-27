#pragma once

#include <thread>
#include <mutex>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <future>

#include <SDL_net.h>

namespace Ship {
namespace Online {

typedef struct {
    float x, y, z;
} Vec3float; // size = 0x0C

typedef struct {
    int16_t x, y, z;
} Vec3short; // size = 0x0C

typedef struct {
    Vec3float pos;
    Vec3short rot;
} PosRotOnline;

typedef struct OnlinePacket {
    uint8_t player_id;
    uint16_t rupeeAmountChanged;
    PosRotOnline posRot;
    uint8_t biggoron_broken;

    // SkelAnime Data
    Vec3short jointTable[0x16];

    uint8_t sheathType;
    uint8_t shieldType;
    uint8_t leftHandType;
    uint8_t rightHandType;

    int16_t faceType;
    uint8_t tunicType;
    uint8_t bootsType;

    uint8_t didDamage;
} OnlinePacket;

void InitOnline(char* ipAddr, int port);
void SendPacketMessage(OnlinePacket* packet, TCPsocket* sendTo);

class Server {
  private:
    std::thread onlineThread;

  public:
    IPaddress ip;
    int port;

    bool serverOpen = false;
    bool clientConnected = false;

    TCPsocket serverSocket;
    TCPsocket clientSocket;

    uint8_t my_player_id = 0;

    ~Server();
    void CreateServer(int serverPort);
    void RunServer();
};

class Client {
  private:
    std::thread onlineThread;

  public:
    IPaddress ip;
    int port;

    TCPsocket clientSocket;

    uint8_t my_player_id = 0;

    bool clientConnected = false;

    ~Client();
    void CreateClient(char* ipAddr, int port);
    void RunClient();
};

OnlinePacket serverPacket;
OnlinePacket clientPacket;

Server server = Server();
Client client = Client();

} // namespace Online
} // namespace Ship