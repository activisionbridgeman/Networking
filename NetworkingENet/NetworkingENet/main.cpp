// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
// Allows two clients to communicate with each other through a central server
//

#include <enet/enet.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace std;

ENetAddress address;
ENetHost* server = nullptr;
ENetHost* client = nullptr;

int peerCount = 0;

std::thread ServerThread;
std::thread ClientThread;
std::thread QuitServerThread;

bool quit = false;

string randomMessages[11] = { "Get out!", "You're scaring me", "That's dumb", "What? Why?", "Please no", 
"Hello, is anybody out there?", "Leave!", "Duh", "Wait no!", "I don't trust you.", "Huh?"};

int randomTimes[10] = { 5, 6, 9, 13, 16, 20, 24, 30, 32, 40 }; // in seconds

bool CreateServer() 
{
    /* Bind the server to the default localhost.     */
    /* A specific host address can be specified by   */
    /* enet_address_set_host (& address, "x.x.x.x"); */
    address.host = ENET_HOST_ANY;

    /* Bind the server to port 1234. */
    address.port = 1234;

    server = enet_host_create(&address /* the address to bind the server host to */,
        32      /* allow up to 32 clients and/or outgoing connections */,
        2      /* allow up to 2 channels to be used, 0 and 1 */,
        0      /* assume any amount of incoming bandwidth */,
        0      /* assume any amount of outgoing bandwidth */);

    peerCount = server->peerCount;

    return server != nullptr;
}

bool CreateClient() 
{
    client = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);

    return client != nullptr;
}

// Get input continuously for message to send
void GetInput(ENetHost* user, string userName) 
{
    while (!quit) 
    {
        string message;
        getline(cin, message);

        if (message == "quit") 
        {
            quit = true;
            cout << "Exited chat" << endl;
        }

        else if (!message.empty())
        {
            /* Create a reliable packet of size 7 containing "packet\0" */
            message = userName + ": " + message;
            ENetPacket* packet = enet_packet_create(message.c_str(),
                strlen(message.c_str()) + 1,
                ENET_PACKET_FLAG_RELIABLE);

            /* Send the packet to the peer over channel id 0. */
            enet_host_broadcast(user, 0, packet);

            enet_host_flush(user);
        }
    }
}

// Send random messages from server at random times to clients
void SendRandomMessages(ENetHost* user, string userName) 
{
    while (!quit) 
    {
        int num = rand() % randomMessages->size();
        string message = userName + ": " + randomMessages[num];
        ENetPacket* packet = enet_packet_create(message.c_str(),
            strlen(message.c_str()) + 1,
            ENET_PACKET_FLAG_RELIABLE);

        /* Send the packet to the peer over channel id 0. */
        enet_host_broadcast(user, 0, packet);

        enet_host_flush(user);

        num = rand() % sizeof(randomTimes) / sizeof(randomTimes[0]);

        int timeCount = 0;

        while (!quit && timeCount < randomTimes[num])
        {
            this_thread::sleep_for(chrono::seconds(1));
            timeCount++;
        }
    }
}

// Allow user to quit server by typing 'quit'
void QuitServer() 
{
    while (!quit)
    {
        string message;
        getline(cin, message);

        if (message == "quit")
        {
            quit = true;
            cout << "Exited server" << endl;
        }
    }
}

int main(int argc, char** argv)
{
    if (enet_initialize() != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        cout << "An error occurred while initializing ENet." << endl;
        return EXIT_FAILURE;
    }

    atexit(enet_deinitialize);

    cout << "1) Create Server " << endl;
    cout << "2) Create Client " << endl;

    int userInput;
    cin >> userInput;

    if (userInput == 1) 
    {
        if (!CreateServer())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet server host.\n");
            exit(EXIT_FAILURE);
        }

        string serverName;

        cout << "Enter name: ";
        cin >> serverName;

        cout << "Enter 'quit' at any time to close the server" << endl;

        cout << "Connecting..." << endl;

        ServerThread = std::thread (SendRandomMessages, server, serverName);
        QuitServerThread = std::thread(QuitServer);

        while (!quit) {
            ENetEvent event;

            /* Wait up to 1000 milliseconds for an event. */
            while (enet_host_service(server, &event, 1000) > 0)
            {
                switch (event.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                    cout << serverName << " connected from "
                        << event.peer->address.host
                        << ":" << event.peer->address.port
                        << endl;

                    /* Store any relevant client information here. */
                    event.peer->data = (void*)"Client information";

                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    cout << (char*)event.packet->data
                        << endl;

                    for (int i = 0; i < peerCount; i++) 
                    {
                        ENetPeer* peer = server->peers + i;
                        if (peer->connectID != event.peer->connectID) 
                        {
                            enet_peer_send(peer, 0, event.packet);
                        }
                    }

                    enet_host_flush(server);

                    /* Clean up the packet now that we're done using it. */
                    enet_packet_destroy(event.packet);

                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    cout << (char*)event.peer->data << " disconnected." << endl;
                    /* Reset the peer's client information. */
                    event.peer->data = NULL;
                }
            }
        }

        ServerThread.join();
        QuitServerThread.join();
    }

    else if (userInput == 2) 
    {
        if (!CreateClient())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet client host.\n");
            exit(EXIT_FAILURE);
        }

        string clientName;
        cout << "Enter name: ";
        cin >> clientName;

        cout << "Connecting..." << endl;

        ENetAddress address;
        ENetEvent event;
        ENetPeer* peer;

        /* Connect to 127.0.0.1 */
        enet_address_set_host(&address, "127.0.0.1");
        address.port = 1234;

        /* Initiate the connection, allocating the two channels 0 and 1. */
        peer = enet_host_connect(client, &address, 2, 0);

        if (peer == NULL)
        {
            fprintf(stderr,
                "No available peers for initiating an ENet connection.\n");
            exit(EXIT_FAILURE);
        }

        /* Wait up to 5 seconds for the connection attempt to succeed. */
        if (enet_host_service(client, &event, 5000) > 0 &&
            event.type == ENET_EVENT_TYPE_CONNECT)
        {
            cout << "Connection to 127.0.0.1:1234 succeeded." << endl;
            cout << "Type in a message to send! Type 'quit' to exit." << endl;
        }

        else
        {
            /* Either the 5 seconds are up or a disconnect event was */
            /* received. Reset the peer in the event the 5 seconds   */
            /* had run out without any significant event.            */
            enet_peer_reset(peer);
            cout << "Connection to 127.0.0.1:1234 failed." << endl;
        }

        ClientThread = std::thread(GetInput, client, clientName);

        while (!quit) 
        {
            ENetEvent event;

            /* Wait up to 1000 milliseconds for an event. */
            while (enet_host_service(client, &event, 1000) > 0)
            {
                switch (event.type) 
                {
                case ENET_EVENT_TYPE_RECEIVE:
                    cout << (char*)event.packet->data<< endl;

                    /* Clean up the packet now that we're done using it. */
                    enet_packet_destroy(event.packet);

                    break;
                }
            }
        }

        ClientThread.join();
    }
    else 
    {
        cout << "Invalid Input" << endl;
    }

    if (server != nullptr) 
    {
        enet_host_destroy(server);
    }
    
    if (client != nullptr) 
    {
        enet_host_destroy(client);
    }

    return EXIT_SUCCESS;
}