// net.h - a TCP connection between two copies of Stack Trace, for Versus mode.
// One copy hosts (listens on a port), the other joins (connects to the host's
// address). They then swap short lines of text.
//
// Everything is non-blocking: call net_update() once a frame and it never
// waits, so the window doesn't freeze while connecting or waiting for messages.
//
// This file must not include raylib.h, and net.cpp must not either: on Windows
// the sockets need windows.h, whose names (Rectangle, CloseWindow, DrawText...)
// clash with raylib's. Keeping the two apart is what makes it build.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

const int VERSUS_PORT = 5050;
const int WATCH_PORT = 5051;    // spectators connect here, so they can never take the player's place

enum class NetState {
    Idle,        // nothing open
    Listening,   // hosting, waiting for someone to join
    Connecting,  // joining, waiting for the host to answer
    Connected,   // talking
    Failed       // couldn't host or connect, or the connection was lost: see NetConn::error
};

struct NetConn {
    NetState state = NetState::Idle;
    std::string error;          // why it failed, in plain words
    std::string peerAddress;    // the other computer's address, once connected
    intptr_t listenSock = -1;   // the socket handles (SOCKET on Windows, int elsewhere)
    intptr_t sock = -1;
    double connectStarted = 0;  // for the join time limit
    std::string inBuffer;       // bytes received that don't make a whole line yet
    std::string outBuffer;      // bytes waiting to be sent (if the network was busy)
    std::vector<std::string> inbox;  // whole lines received, oldest first
};

// Call once at start-up and once at the end (WSAStartup on Windows).
bool net_init();
void net_shutdown();

// Start hosting on port, or start joining address:port. Both return straight
// away; net_update() then moves the connection along.
bool net_host(NetConn &c, int port);
bool net_join(NetConn &c, const std::string &address, int port, double now);

// Call every frame: accepts the joiner, finishes connecting, reads lines into
// the inbox, and notices if the other side has gone. now is the time in seconds.
void net_update(NetConn &c, double now);

// Sends one line (a '\n' is added). Returns false if the connection has gone.
bool net_send(NetConn &c, const std::string &line);

// Takes the oldest received line. Returns false if there's none.
bool net_receive(NetConn &c, std::string &line);

void net_close(NetConn &c);

// Hosting for spectators: a listening socket that keeps accepting, so any
// number of people can connect (screen_versus.cpp decides how many to keep).
struct NetServer {
    intptr_t sock = -1;
    std::string error;
};
bool net_server_open(NetServer &s, int port);
// A new connection, if one has arrived: out becomes Connected. Never waits.
bool net_server_accept(NetServer &s, NetConn &out);
void net_server_close(NetServer &s);
bool net_server_open_now(const NetServer &s);   // listening?

// This computer's address on the network it would use to reach the internet,
// e.g. "10.170.59.202". "" if it can't tell (no network).
std::string net_local_address();
