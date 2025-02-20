#include <bits/stdc++.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <vector>
#include <poll.h>
#include <map>
#include <string>
#include <fcntl.h>
#include "hashtable.h"
#include <assert.h>

using namespace std;

#define MAX_CONNECTIONS 128
#define container_of(ptr, T, member) \
    ((T*)((char * )ptr - offsetof(T, member)))

typedef std::vector<uint8_t> Buffer;

enum {
    ERR_UNKNOWN = 0,
    ERR_TOO_BIG = 1,    // error
    //RES_NX = 2,     // key not found
};

const size_t MAX_LEN = 64*4096;
struct Conn{
    int fd = -1;

    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    std::vector<uint8_t> incoming;
    
    std::vector<uint8_t> outgoing;
};

struct Response {
    uint32_t status = 0;
    std::vector<uint8_t> data;
};


// global states
static struct {
    HMap db;    // top-level hashtable
} g_data;

struct Entry{
    struct HNode node;
    std::string key;
    std::string value;
};
enum {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len)
{
    buf.insert(buf.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buf, size_t len)
{
    buf.erase(buf.begin(), buf.begin() + len);
}

static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        cout << "fcntl error \n";
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        cout<< "fcntl error \n";
        return;
    }
}

Conn *handle_accept(int fd)
{
    struct sockaddr_in client_addr = {};
    socklen_t client_len = sizeof(client_addr);
    int cd = accept(fd, (struct sockaddr *)&client_addr, &client_len);

    if(cd < 0)
        return NULL;

     uint32_t ip = client_addr.sin_addr.s_addr;
    // printf("new client from %u.%u.%u.%u:%u\n",
    //     ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
    //     ntohs(client_addr.sin_port)
    // );

    fd_set_nb(cd);

    Conn *new_conn = new Conn();
    new_conn->fd = cd;
    new_conn->want_read = true;
    return new_conn;
}
bool entry_eq(HNode *node1, HNode *node2)
{
    struct Entry *entry1 = container_of(node1, struct Entry, node);
    struct Entry *entry2 = container_of(node2, struct Entry, node);
   // cout << entry1->key << entry2->key << "\n";
    return entry1->key == entry2->key;
}
static uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

static void buf_append_u8(Buffer &buf, uint8_t data)
{
    buf.push_back(data);
}

static void buf_append_u32(Buffer &buf, uint32_t data)
{
    buf_append(buf, (const uint8_t *)&data, 4);
}


static void buf_append_i64(Buffer &buf, uint64_t data)
{
    buf_append(buf, (const uint8_t *)&data, 8);
}


static void buf_append_double(Buffer &buf, double data)
{
    buf_append(buf, (const uint8_t *)&data, 8);
}

static void out_str(Buffer &out, const char *s, size_t size)
{
    buf_append_u8(out, TAG_STR);
    buf_append_u32(out, (uint32_t)size);
    buf_append(out, (const uint8_t *)s, size);

}

static void out_nil(Buffer &out)
{
    buf_append_u8(out, TAG_NIL);
}

static void out_int(Buffer &out, int64_t val)
{
    buf_append_u8(out, TAG_INT);
    buf_append_i64(out, val);
}

static void out_double(Buffer &out, double val)
{
    buf_append_u8(out, TAG_DBL);
    buf_append_double(out, val);
}


static void out_err(Buffer &out, uint32_t code, const std::string &msg)
{
    buf_append_u8(out, TAG_ERR);
    buf_append_u32(out, code);
    buf_append_u32(out, (uint32_t)msg.size());
    buf_append(out, (const uint8_t *)msg.data() ,msg.size());
}

// static void out_arr(Buffer &out, uint32_t n)
// {
//     buf_append_u8(out, TAG_ARR);
//     buf_append_u32(out, n);
// }


static void do_get(std::vector<std::string> &cmd, Buffer &out) {
    Entry key = {};
    key.key = cmd[1];

    //get hash to search the hashtable
    key.node.hashcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    
    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if(!node)
    {
        out_nil(out);
        return;
    }
    //point to data, given the node pointer. 
    std::string &val = container_of(node, Entry, node)->value;
    assert(val.size() < MAX_LEN);
    out_str(out, val.data(), val.size());

}



static void do_set(std::vector<std::string> &cmd, Buffer &out) {
    Entry key = {};
    key.key.swap(cmd[1]);
    //key.value.swap(cmd[2]);

    //get hash to search the hashtable
    key.node.hashcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (node)
        // found, update the value
        container_of(node, Entry, node)->value.swap(cmd[2]);
    else
    {
        Entry *ent = new Entry();
        ent->key.swap(key.key);
        ent->value.swap(cmd[2]);
        ent->node.hashcode = key.node.hashcode;
        hm_insert(&g_data.db, &ent->node);

    }
    return out_nil(out);
}



static void do_delete(std::vector<std::string> &cmd, Buffer &out) {
    Entry key = {};
    key.key = cmd[1];

    //get hash to search the hashtable
    key.node.hashcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_delete(&g_data.db, &key.node, &entry_eq);
    //find the start of the entry poitner and delete the entry
    if(node)
        delete container_of(node, Entry, node);

    return out_int(out, node ? 1 : 0);

}
static void handle_request(std::vector<std::string> &cmd, Buffer &out)
{
    if(cmd.size() == 2 && cmd[0] == "get")
    {
        do_get(cmd, out);

    }
    else if(cmd.size() == 3 && cmd[0] == "set")
    {
        do_set(cmd,out);   
    }
    else if(cmd.size() == 2 && cmd[0] == "del")
    {
        do_delete(cmd, out);
    }
    else
    {
        return out_err(out, ERR_UNKNOWN, "unknown command.");
    }

}
//read the length 
static bool read_u32(const uint8_t *&data, const uint8_t *end, uint32_t &out)
{
    if(4 + data > end)
        return false;

    memcpy(&out, data, 4);
    data+=4;
    return true;
}

static bool read_str(const uint8_t *&data, const uint8_t *end, uint32_t len, std::string &out)
{
    if(len + data > end)
        return false;

    out.assign(data, data + len);
    data+=len;
    return true;
}

//size = nstr + sum(all_command + their lenght)
static int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out)
{
    const uint8_t *end = data + size;
    uint32_t nstr = 0;
    if(!read_u32(data, end, nstr))
    {
        return -1;
    }

    while(out.size() < nstr)
    {
        uint32_t len = 0;
        if(!read_u32(data, end, len))
        {
            return -1;
        }
        out.push_back(std::string());
        if(!read_str(data, end, len, out.back()))
            return -1;
    }
    return 0;    
}

static size_t response_size(Buffer &out, size_t header) {
    return out.size() - header - 4;
}

static void write_response(Buffer &out, size_t header)
{
    size_t msg_size = response_size(out, header);
     if (msg_size > MAX_LEN) {
        out.resize(header + 4);
        out_err(out, ERR_TOO_BIG, "response is too big.");
        msg_size = response_size(out, header);
    }
     // message header
    uint32_t len = (uint32_t)msg_size;
    memcpy(&out[header], &len, 4);

}

//understand this
static void response_begin(Buffer &out, size_t *header) {
    *header = out.size();       // messege header position
    buf_append_u32(out, 0);     // reserve space
}

static bool one_request(Conn *conn)
{
    if (conn->incoming.size() < 4) {
        return false;   // want read
    }
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);

      // message body
    if (4 + len > conn->incoming.size()) {
        return false;   // want read
    }

    if(len > MAX_LEN)
    {
        cout << "Msg too long" << len << " "<<conn->incoming.data() << " \n";
        conn->want_close = true;
        return false;
    }
    const uint8_t *request = &conn->incoming[4];

    std::vector<std::string> cmd;
    if(parse_req(request, len, cmd) < 0)
    {
        cout << "Error while parsing the command, closing the connection \n";
        conn->want_close = true;
        return false;
    }
    size_t header_pos = 0;
    //First 4 bytes - contains the total length.
    
    response_begin(conn->outgoing, &header_pos);
    //This needs to be searched in database
    handle_request(cmd, conn->outgoing);

    write_response(conn->outgoing, header_pos);

    buf_consume(conn->incoming, 4 + len);

    return true;

}

static void handle_write(Conn *conn)
{
    assert(conn->outgoing.size() > 0);
    ssize_t ret = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());

    if(ret < 0 && errno == EAGAIN)
    {
        return; //server not ready to send the data
    }
    //close the connnection 
    if(ret < 0)
    {
        cout << "Error Sending data \n";
        conn->want_close = true;
        return;
    }

    buf_consume(conn->outgoing, (size_t)ret);

    if(conn->outgoing.size() == 0)
    {
        conn->want_read = true;
        conn->want_write = false;
    }
    return;


} 

static void handle_read(Conn *conn)
{
    uint8_t rbuf[4 + MAX_LEN + 1];
    ssize_t ret = read(conn->fd, rbuf, sizeof(rbuf));

    if(ret < 0 && errno == EAGAIN)
    {
        cout << "Error inb read \n";
        return;
    }
    // handle IO error
    if (ret < 0) {
        cout <<"read() error \n";
        conn->want_close = true;
        return; // want close
    }
    //close the connnection 
    if(ret == 0)
    {
        if(conn->incoming.size() == 0)
            cout << "Client closed " << "\n";
        else
            cout << "Unexpected EOF \n";

        conn->want_close = true;
        return;
    }
    buf_append(conn->incoming, rbuf, (size_t)ret);

    while(one_request(conn)){}

    if(conn->outgoing.size() > 0)
    {
        conn->want_read = false;
        conn->want_write = true;
        return handle_write(conn);
    }
    return;

}




int main() {
	// your code goes here
	struct sockaddr_in server_addr;
	
	//Create the socket
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
	{
	    cout << "Error creating socket";
	    return 0;
	}
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(1535);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//why bind??
    if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr))<0)
	{
        cout << "Error in binding";
		return 0;
	}
    fd_set_nb(fd);

    int rv = listen(fd, MAX_CONNECTIONS);
    if (rv)
    {
        cout << "Listening ... \n" ;
    }
    std::vector<Conn *> fd2conn;

    std::vector<struct pollfd> poll_args;
    //Listent to incoming requests from client
    while(true)
    {
        poll_args.clear();

        struct pollfd pfd = {fd, POLLIN, 0};

        poll_args.push_back(pfd);

        //add all the requests to poll_args array
        for(Conn *conn : fd2conn)
        {
            if(!conn)
                continue;

                
            struct pollfd pfd = {conn->fd, POLLERR, 0};
            if(conn->want_read)
                pfd.events|=POLLIN;
            if(conn->want_write)
                pfd.events|=POLLOUT;

            poll_args.push_back(pfd);
        }

        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);

        if(rv < 0 && errno == EINTR){
            cout << "Continue: not an error";
            continue;
        }
        if (rv < 0){
            //cout << errno <<"Error \n";
            return 0;
        }   
        //handle the listening socket
        if (poll_args[0].revents)
        {
            Conn *conn = handle_accept(fd);
            if(conn)
            {
                if (fd2conn.size() <= (size_t)conn->fd) 
                    fd2conn.resize(conn->fd + 1);
                assert(!fd2conn[conn->fd]);
                fd2conn[conn->fd] = conn;
            }
        }
        //0 is revserved for server/lsitening port
        for(size_t i = 1; i < poll_args.size(); ++i)
        {
            uint32_t ready = poll_args[i].revents;
            if(ready == 0)
                continue;
            Conn *conn = fd2conn[poll_args[i].fd];
            if(!conn)
                continue;
            if(ready & POLLIN)
            {
                assert(conn->want_read);
                handle_read(conn);
            }
            if(ready & POLLOUT)
            {
                assert(conn->want_write);
                handle_write(conn);
            }

            //close connections, why through poll_args and not fd2conn?
            if((ready & POLLERR) || conn->want_close)
            {
                close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }
    }
    	
    return 0;
}

