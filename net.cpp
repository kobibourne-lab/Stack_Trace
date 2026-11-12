// net.cpp - the Versus connection. See net.h.
//
// Mac and Linux use BSD sockets; Windows uses Winsock, which is nearly the
// same with a few different names. The small helpers below hide those
// differences so the rest of the file is written once.
//
// Never include raylib.h here (see net.h).

#include "net.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET Sock;
    static const Sock BAD_SOCK = INVALID_SOCKET;
    static int last_error() { return WSAGetLastError(); }
    static bool would_block(int e) { return e == WSAEWOULDBLOCK; }
    static bool connect_pending(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY; }
    static void close_sock(Sock s) { closesocket(s); }
    static bool set_nonblocking(Sock s) { u_long on = 1; return ioctlsocket(s, FIONBIO, &on) == 0; }
    typedef int socklen_type;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <cerrno>
    typedef int Sock;
    static const Sock BAD_SOCK = -1;
    static int last_error() { return errno; }
    static bool would_block(int e) { return e == EWOULDBLOCK || e == EAGAIN; }
    static bool connect_pending(int e) { return e == EINPROGRESS || e == EALREADY; }
    static void close_sock(Sock s) { close(s); }
    static bool set_nonblocking(Sock s) { int f = fcntl(s, F_GETFL, 0); return f >= 0 && fcntl(s, F_SETFL, f | O_NONBLOCK) == 0; }
    typedef socklen_t socklen_type;
#endif

// Sending on a connection the other side has closed would otherwise stop the
// whole program with SIGPIPE on Mac and Linux.
#ifdef MSG_NOSIGNAL
    static const int SEND_FLAGS = MSG_NOSIGNAL;
#else
    static const int SEND_FLAGS = 0;
#endif

static Sock to_sock(intptr_t s) { return (Sock)s; }
static intptr_t from_sock(Sock s) { return s == BAD_SOCK ? -1 : (intptr_t)s; }

const double JOIN_TIMEOUT = 6.0;       // seconds to wait for the host to answer
const size_t MAX_BUFFER = 1 << 20;     // a line longer than this means something's wrong

// ---------------------------------------------------------------------------
// IPv4 addresses as text, written out by hand (inet_pton and inet_ntop aren't
// in every Windows compiler's headers)
// ---------------------------------------------------------------------------

static bool parse_ipv4(const std::string &text, in_addr &out)
{
    unsigned parts[4];
    int count = 0;
    unsigned value = 0;
    bool digit = false;
    for (size_t i = 0; i <= text.size(); i++) {
        char c = i < text.size() ? text[i] : '.';
        if (c >= '0' && c <= '9') {
            value = value * 10 + (unsigned)(c - '0');
            if (value > 255) return false;
            digit = true;
        } else if (c == '.') {
            if (!digit || count == 4) return false;
            parts[count++] = value;
            value = 0;
            digit = false;
        } else if (c == ' ' && (i == 0 || i == text.size() - 1)) {
            continue;
        } else {
            return false;
        }
    }
    if (count != 4) return false;
    unsigned long host = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    out.s_addr = htonl(host);
    return true;
}

static std::string format_ipv4(const in_addr &a)
{
    unsigned long host = ntohl(a.s_addr);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lu.%lu.%lu.%lu", (host >> 24) & 255, (host >> 16) & 255, (host >> 8) & 255, host & 255);
    return buf;
}

// ---------------------------------------------------------------------------

bool net_init()
{
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void net_shutdown()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

// Settings every connected socket gets: no waiting, no delay on small messages,
// and no SIGPIPE on a Mac.
static void prepare(Sock s)
{
    set_nonblocking(s);
    int on = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof on);
#ifdef SO_NOSIGPIPE
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&on, sizeof on);
#endif
}

// The connection is over. Lines already received stay in the inbox, so the
// last thing the other side said can still be read.
static void fail(NetConn &c, const std::string &why)
{
    std::vector<std::string> received = c.inbox;
    net_close(c);
    c.state = NetState::Failed;
    c.error = why;
    c.inbox = received;
}

void net_close(NetConn &c)
{
    if (c.listenSock != -1) close_sock(to_sock(c.listenSock));
    if (c.sock != -1) close_sock(to_sock(c.sock));
    c.listenSock = c.sock = -1;
    c.state = NetState::Idle;
    c.inBuffer.clear();
    c.outBuffer.clear();
    c.inbox.clear();
    c.peerAddress.clear();
    c.error.clear();
}

bool net_host(NetConn &c, int port)
{
    net_close(c);
    Sock s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == BAD_SOCK) { fail(c, "Couldn't open a network socket."); return false; }
#ifndef _WIN32
    // Lets you host again straight after a match (Windows doesn't need this)
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof on);
#endif
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(s, (sockaddr *)&addr, sizeof addr) != 0) {
        close_sock(s);
        fail(c, "Port " + std::to_string(port) + " is already in use. Is another copy of Stack Trace hosting?");
        return false;
    }
    if (listen(s, 1) != 0 || !set_nonblocking(s)) {
        close_sock(s);
        fail(c, "Couldn't start hosting.");
        return false;
    }
    c.listenSock = from_sock(s);
    c.state = NetState::Listening;
    return true;
}

// ---------------------------------------------------------------------------
// Hosting for spectators
// ---------------------------------------------------------------------------

bool net_server_open(NetServer &srv, int port)
{
    net_server_close(srv);
    Sock s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == BAD_SOCK) { srv.error = "Couldn't open a network socket."; return false; }
#ifndef _WIN32
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof on);
#endif
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(s, (sockaddr *)&addr, sizeof addr) != 0 || listen(s, 8) != 0 || !set_nonblocking(s)) {
        close_sock(s);
        srv.error = "Port " + std::to_string(port) + " is already in use, so nobody can watch.";
        return false;
    }
    srv.sock = from_sock(s);
    srv.error.clear();
    return true;
}

bool net_server_accept(NetServer &srv, NetConn &out)
{
    if (srv.sock == -1) return false;
    sockaddr_in peer;
    socklen_type len = sizeof peer;
    Sock s = accept(to_sock(srv.sock), (sockaddr *)&peer, &len);
    if (s == BAD_SOCK) return false;
    prepare(s);
    net_close(out);
    out.sock = from_sock(s);
    out.peerAddress = format_ipv4(peer.sin_addr);
    out.state = NetState::Connected;
    return true;
}

void net_server_close(NetServer &srv)
{
    if (srv.sock != -1) close_sock(to_sock(srv.sock));
    srv.sock = -1;
}

bool net_server_open_now(const NetServer &srv) { return srv.sock != -1; }

bool net_join(NetConn &c, const std::string &address, int port, double now)
{
    net_close(c);
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (!parse_ipv4(address, addr.sin_addr)) {
        fail(c, "That isn't an address. It's four numbers with dots, like 10.170.59.202.");
        return false;
    }
    Sock s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == BAD_SOCK) { fail(c, "Couldn't open a network socket."); return false; }
    prepare(s);
    c.sock = from_sock(s);
    c.peerAddress = format_ipv4(addr.sin_addr);
    c.connectStarted = now;

    if (connect(s, (sockaddr *)&addr, sizeof addr) == 0) {
        c.state = NetState::Connected;
        return true;
    }
    if (!connect_pending(last_error())) {
        fail(c, "Couldn't reach " + c.peerAddress + ".");
        return false;
    }
    c.state = NetState::Connecting;
    return true;
}

// Moves every whole line in inBuffer into the inbox.
static void split_lines(NetConn &c)
{
    size_t nl;
    while ((nl = c.inBuffer.find('\n')) != std::string::npos) {
        std::string line = c.inBuffer.substr(0, nl);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        c.inbox.push_back(line);
        c.inBuffer.erase(0, nl + 1);
    }
}

// Sends what's waiting in outBuffer, as much as the network takes right now.
static void flush(NetConn &c)
{
    while (c.state == NetState::Connected && !c.outBuffer.empty()) {
        int n = send(to_sock(c.sock), c.outBuffer.data(), (int)c.outBuffer.size(), SEND_FLAGS);
        if (n > 0) {
            c.outBuffer.erase(0, (size_t)n);
        } else {
            if (n < 0 && would_block(last_error())) return;   // try again next frame
            fail(c, "The connection to the other player was lost.");
            return;
        }
    }
}

void net_update(NetConn &c, double now)
{
    if (c.state == NetState::Listening) {
        sockaddr_in peer;
        socklen_type len = sizeof peer;
        Sock s = accept(to_sock(c.listenSock), (sockaddr *)&peer, &len);
        if (s != BAD_SOCK) {
            prepare(s);
            close_sock(to_sock(c.listenSock));   // one opponent at a time
            c.listenSock = -1;
            c.sock = from_sock(s);
            c.peerAddress = format_ipv4(peer.sin_addr);
            c.state = NetState::Connected;
        }
        return;
    }

    if (c.state == NetState::Connecting) {
        Sock s = to_sock(c.sock);
        fd_set writable, failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(s, &writable);
        FD_SET(s, &failed);
        timeval none = { 0, 0 };
        int ready = select((int)(s + 1), nullptr, &writable, &failed, &none);
        if (ready > 0) {
            int err = 0;
            socklen_type len = sizeof err;
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
            if (err == 0 && !FD_ISSET(s, &failed)) {
                c.state = NetState::Connected;
            } else {
                std::string who = c.peerAddress;
                fail(c, "Nobody is hosting at " + who + ". Check the address, and that the other copy has pressed Host.");
            }
        } else if (now - c.connectStarted > JOIN_TIMEOUT) {
            std::string who = c.peerAddress;
            fail(c, "Couldn't reach " + who + ". Are you both on the same hotspot? College Wi-Fi blocks this.");
        }
        return;
    }

    if (c.state != NetState::Connected) return;
    flush(c);
    if (c.state != NetState::Connected) return;

    // Read everything that has arrived, then split it into lines
    char buf[4096];
    while (true) {
        int n = recv(to_sock(c.sock), buf, sizeof buf, 0);
        if (n > 0) {
            c.inBuffer.append(buf, (size_t)n);
            if (c.inBuffer.size() > MAX_BUFFER) { fail(c, "The other player sent something too big."); return; }
        } else if (n == 0) {
            // They closed the connection. Keep the lines that arrived before
            // they went (their last words, like LEFT or FULL), then fail.
            split_lines(c);
            fail(c, "The other player left.");
            return;
        } else {
            if (would_block(last_error())) break;
            fail(c, "The connection to the other player was lost.");
            return;
        }
    }
    split_lines(c);
}

bool net_send(NetConn &c, const std::string &line)
{
    if (c.state != NetState::Connected) return false;
    c.outBuffer += line;
    c.outBuffer += '\n';
    flush(c);
    return c.state == NetState::Connected;
}

bool net_receive(NetConn &c, std::string &line)
{
    if (c.inbox.empty()) return false;
    line = c.inbox.front();
    c.inbox.erase(c.inbox.begin());
    return true;
}

std::string net_local_address()
{
    // "Connecting" a UDP socket sends nothing; it just asks the computer which
    // of its addresses it would use to reach the internet. That's the Wi-Fi's.
    Sock s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == BAD_SOCK) return "";
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    parse_ipv4("8.8.8.8", addr.sin_addr);
    std::string result;
    if (connect(s, (sockaddr *)&addr, sizeof addr) == 0) {
        sockaddr_in mine;
        socklen_type len = sizeof mine;
        if (getsockname(s, (sockaddr *)&mine, &len) == 0) result = format_ipv4(mine.sin_addr);
    }
    close_sock(s);
    if (result == "0.0.0.0") result.clear();
    return result;
}
