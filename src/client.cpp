#include "ls_proxy.h"


Client::Client(int fd, const struct sockaddr_in& _addr, Connection* _conn):
    addr{get_host_and_port(_addr)}, conn{_conn}, queued_output{NULL},
    request_buf{NULL}, request_tmp_buf{NULL}, response_buf{NULL}
{
    read_evt = new_read_event(fd, Client::on_readable, this);
    write_evt = new_write_event(fd, Client::on_writable, this);
    request_history = free_hybridbuf.front();
    free_hybridbuf.pop();
}

Client::~Client() {
    LOG1("Connection closed: [%s]\n", addr.c_str());
    close_socket_gracefully(get_fd());
    del_event(read_evt);
    del_event(write_evt);
    free_event(read_evt);
    free_event(write_evt);
    free_hybridbuf.push(request_history);
    if (queued_output) delete queued_output;
    if (request_buf) delete request_buf;
    if (request_tmp_buf) delete request_tmp_buf;
    if (response_buf) delete response_buf;
}

void Client::recv_to_buffer_slowly(int fd) {
    auto stat = read_all(fd, global_buffer, sizeof(global_buffer));
    try {
        conn->parser->do_parse(global_buffer, stat.nbytes);
    } catch (ParserError& err) {
        // close connection
        assert(!conn->server);  // server only exists when client paused
        conn->free_parser();
        if (response_buf->data_size > 0) {
            set_reply_only_mode();
        } else {
            delete conn;
        }
        return;
    }
    auto end_ptr = conn->parser->get_first_end_of_msg();
    if (end_ptr) {  // at least one completed
        // store first msg to request_buf, the remaining to request_tmp_buf
        size_t size = end_ptr - global_buffer;
        client->request_buf->store(global_buffer, size);
        client->request_tmp_buf->store(end_ptr, stat.nbytes - size);
        // pause client, rewind buffers and interact with server
        client->pause_rw();
        client->request_buf->rewind();  // for further reads
        // TODO: response_buf
        conn->parser->switch_to_response_mode();
        conn->server = new Server(conn);
        add_event(conn->server->write_evt);
        add_event(conn->server->read_evt);
    } else {
        client->request_buf->store(global_buffer, stat.nbytes);
    }
}

void Client::keep_track_request_history(const char* data, size_t size) {
    // ignore previous complete-requests (if any)
    conn->parser->do_parse(data, size);
    auto end_ptr = conn->parser->get_last_end_of_msg();
    if (end_ptr) {
        // start storing from here
        request_history->clear();
        request_history->store(end_ptr, size - (end_ptr - data));
        LOG2("History: Starts new record from '%s'\n",
             end_ptr < data + size ? end_ptr : "EOL");
    } else {
        request_history->store(data, size);
    }
}

void Client::on_readable(int fd, short/*flag*/, void* arg) {
    auto client = (Client*)arg;
    auto conn = client->conn;
    if (conn->is_fast_mode()) {
        if (!conn->parser) {  // uninitialized
            conn->parser = new HttpParser();
            try {
                conn->server = new Server(conn);
            } catch (ConnectionError& err) {
                reply_with_503_unavailable(fd);
                delete conn;
                return;
            }
            conn->server->queued_output = new Circularbuf();
            client->queued_output = new Circularbuf();
            add_event(conn->server->read_evt);
        }
        conn->fast_forward(client, conn->server);
    } else {
        /* Slow mode */
        client->recv_to_buffer_slowly(fd);
    }
}

void Client::on_writable(int fd, short/*flag*/, void* arg) {
    auto client = (Client*)arg;
    auto conn = client->conn;
    if (conn->is_fast_mode()) {
        if (!conn->server) {  // server already closed (reply-only mode)
            client->queued_output->write_all_to(fd);
            if (client->queued_output->data_size() == 0) {
                delete conn;
            }
        } else {
            // add back server's read_evt because we removed it before
            del_event(client->write_evt);
            add_event(conn->server->read_evt);
            LOG2("[%s] Client writable again.\n", client->addr.c_str());
        }
    } else {
        /* Slow mode */
        // reply with response
        client->response_buf
    }
}
