#include "ls_proxy.h"


void Connection::set_slow_mode() {
    fast_mode = false;
    in_transition = true;
    client->request_buf = new Filebuf();
    client->request_tmp_buf = new Filebuf();
    client->response_buf = new Filebuf();

    // pause client, wait until server finished its msg then close server
    client->pause_rw();
    del_and_reAdd_event(server->read_evt, 10);   // attach a 10s timeout
    if (server->queued_output->data_size() > 0) {
        add_event(server->write_evt);
    }

    // retrieve current request
    client->copy_history_to(client->request_buf);
    // decommission fast-mode queue
    while (client->queued_output->data_size() > 0) {
        auto st = client->queued_output->write_all_to(client->response_buf);
        if (st.has_error && errno != EAGAIN && errno != EINTR) {
            ERROR("Write failed (#%d)", client->response_buf->get_fd());
            break;
        }
    }
    delete client->queued_output;
    client->queued_output = NULL;
    LOG2("[%s] Connection been set to slow mode.\n", client->addr.c_str());
}

void Connection::fast_forward(Client*/*client*/, Server*/*server*/) {
    // first store to global_buffer
    Circularbuf* queue = server->queued_output;
    auto stat_c = read_all(client->get_fd(), global_buffer,
                           queue->remaining_space());
    if (stat_c.has_error && errno != EAGAIN && errno != EINTR) {
        ERROR("Read failed");
    }
    try {
        client->keep_track_request_history(global_buffer, stat_c.nbytes);
    } catch (ParserError& err) {
        // close connection, since we can't reliably parse requests
        free_server();
        free_parser();
        if (client->queued_output->data_size() > 0) {
            client->set_reply_only_mode();
        } else {
            delete this;
        }
        return;
    }
    // then append to server's queued output, and write them out
    assert(queue->copy_from(global_buffer, stat_c.nbytes) == stat_c.nbytes);
    auto stat_s = queue->write_all_to(server->get_fd());
    LOG2("[%s] %6lu >>>> queue(%lu) >>>> %-6lu [SERVER]\n", client->addr.c_str(),
         stat_c.nbytes, queue->data_size(), stat_s.nbytes);
    if (stat_c.has_eof) {  // client closed
        delete this;
        return;
    }
    if (stat_s.nbytes == 0) {  // server unwritable
        // disable client's read_evt temporarily
        del_event(client->read_evt);
        add_event(server->write_evt);
        LOG2("[%s] Server temporarily unwritable.\n", client->addr.c_str());
    }
}

void Connection::fast_forward(Server*/*server*/, Client*/*client*/) {
    // append to client's queued output, and write them out
    auto stat_s = client->queued_output->read_all_from(server->get_fd());
    auto stat_c = client->queued_output->write_all_to(client->get_fd());
    LOG2("[%s] %6lu <<<< queue(%lu) <<<< %-6lu [SERVER]\n", client->addr.c_str(),
         stat_c.nbytes, client->queued_output->data_size(), stat_s.nbytes);
    if (stat_s.has_eof) {  // server closed
        free_server();
        free_parser();
        if (client->queued_output->data_size() > 0) {
            client->set_reply_only_mode();
        } else {
            delete this;
        }
        return;
    }
    if (stat_c.nbytes == 0) {  // client unwritable
        // disable server's read_evt temporily
        del_event(server->read_evt);
        add_event(client->write_evt);
        LOG2("[%s] Client temporarily unwritable.\n", client->addr.c_str());
    }
}

void Connection::accept_new(int master_sock, short/*flag*/, void*/*arg*/) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int sock = accept(master_sock, (struct sockaddr*)&addr, &addr_len);
    if (evutil_make_socket_nonblocking(sock) < 0) {
        ERROR_EXIT("Cannot make socket nonblocking");
    }
    if (free_hybridbuf.empty()) {
        WARNING("Max connection <%d> reached", MAX_CONNECTION);
        reply_with_503_unavailable(sock);
        LOG1("Connection closed: [%s]\n", get_host_and_port(addr).c_str());
        close_socket_gracefully(sock);
        return;
    }
    Connection* conn = new Connection();
    conn->client = new Client(sock, addr, conn);
    add_event(conn->client->read_evt);
    LOG1("Connected by [%s] (#%d)\n", conn->client->addr.c_str(), sock);
}
