#include "ls_proxy.h"


Server::Server(Connection* _conn):
    conn{_conn}, queued_output_f{new Circularbuf()}, response_buf_s{NULL}
{
    int sock = connect_TCP(Server::address, Server::port);
    if (evutil_make_socket_nonblocking(sock) < 0) {
        ERROR_EXIT("Cannot make socket nonblocking");
    }
    LOG2("Connected to [SERVER] (active: %d)\n", ++Server::connection_count);
    read_evt = new_read_event(sock, Server::recv_msg, this);
    write_evt = new_write_event(sock, Server::send_msg, this);
}

Server::~Server() {
    LOG2("Connection closed: [SERVER] (active: %d)\n", --Server::connection_count);
    close(get_fd());
    del_event(read_evt);
    del_event(write_evt);
    free_event(read_evt);
    free_event(write_evt);
    if (queued_output_f) delete queued_output_f;
    if (response_buf_s) delete response_buf_s;
}

void Server::recv_all_to_buffer(int fd) {

    // int count = read(fd, read_buffer, sizeof(read_buffer));
    // server->filebuf->store(read_buffer, count);
    // LOG2("%11d bytes << [SERVER]\n", count);

    // if (count == 0 || server->check_response_completed(count)) {
    //     // start replying to client
    //     del_event(server->read_evt);
    //     server->filebuf->rewind();
    //     delete server;  // destroy server
    //     client->server = NULL;
    //     add_event(client->write_evt);
    // }
}

bool Server::check_response_completed(int last_read_count) {
    // TODO: HEAD method can has content-length, but no body
    // TODO: 304 Not Modified can has transfer-encoding, but no body

    // constexpr int digits = log10(MAX_BODY_SIZE) + 3;
    // char res[digits];

    // TODO: detect transfer encoding first

    // if (last_read_count == 5) return true;
    // return false;

    // if (content_len == 0) {  // not yet parsed
    //     filebuf->search_header_membuf(CRLF "Content-Length:", res);
    //     content_len = atoi(res);
    // }
    // if (content_len > 0 && filebuf->data_size /*???*/ == content_len) {
    //     // TODO: new header_len member?
    //     return true;
    // }
    // return false;
}

void Server::recv_msg(int fd, short/*flag*/, void* arg) {
    auto server = (Server*)arg;
    auto conn = server->conn;
    if (conn->is_fast_mode()) {
        assert(conn->client);  // client always exists whenever server has msg
        conn->fast_forward(server, conn->client);
    } else {  // slow mode
        server->recv_all_to_buffer(fd);
    }
}

void Server::send_msg(int/*fd*/, short/*flag*/, void* arg) {
    auto server = (Server*)arg;
    auto conn = server->conn;
    if (conn->is_fast_mode()) {
        // add back client->read_evt because we removed it before
        add_event(conn->client->read_evt);
        del_event(server->write_evt);
        LOG2("[%s] Added back client->read_evt\n", conn->client->addr.c_str());
    } else {

    }

    // int count = server->filebuf->fetch(read_buffer, sizeof(read_buffer));
    // if (count <= 0) {
    //     // done forwarding
    //     del_event(server->write_evt);
    //     server->filebuf->clear();
    //     add_event(server->read_evt);
    // }
    // write(fd, read_buffer, count);
    // LOG2("%11d bytes >> [SERVER]\n", count);
}
