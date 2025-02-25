// #include <bits/stdc++.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include<vector>
#include <string>
#include <cassert>

using namespace std;

#define MAX_CONNECTIONS 128
#define MAX_LEN 4096 * 64

int32_t write_all(int fd, const char *buf, size_t n)
{    
    while(n > 0)
    {
        ssize_t ret = write(fd, buf, n);
        if ( ret <= 0)
        {
            cout << "Error in write \n";
            return -1;
        }

        assert((size_t)ret <= n);
        n-=(size_t)ret;
        buf+=(size_t)ret;

    }
    return 0;
}

int32_t read_full(int fd, char *buf, size_t n)
{
    while(n > 0)
    {   
        ssize_t ret = read(fd, buf, n);
        if ( ret <= 0)
        {
            return -1;
        }

        assert((size_t)ret <= n);
        n-=(size_t)ret;
        buf+=(size_t)ret;

    }
    return 0;
}

int32_t send_req(int fd, const std::vector<std::string> &cmd)
{
    uint32_t len = 4;
    for(const std::string &s : cmd)
    {
        len += 4 + s.size();
    }
    if(len > MAX_LEN)
    {
        cout << "Data Length exceeds max length \n";
        return -1; 
    }

    char wbuf[4 + MAX_LEN];
    memcpy(&wbuf[0], &len, 4);
    uint32_t nstr = cmd.size();
    //cout << nstr << " " << len << "\n";
    memcpy(&wbuf[4], &nstr, 4);
    size_t cur = 8;
    for(const std::string &s: cmd)
    {
        uint32_t p = (uint32_t)s.size();
        memcpy(&wbuf[cur], &p, 4);
        memcpy(&wbuf[cur + 4], s.data(), s.size());
        cur += s.size() + 4;
        // cout << "loop " << s.data() << " "  << s.size() <<"\n";

    }
    // for(uint32_t i = 0; i < (len +  4); i++)
    //     printf("%x ", wbuf[i]);

    // cout << len;
    // for(uint32_t i = 0; i < len; i++)
    //     cout << wbuf[i] << " ";
    return write_all(fd, wbuf, 4 + len);
}
enum {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

static int32_t print_response(const uint8_t *data, size_t size) {
    if(size < 1)
    {
        printf("Error \n");
    }
    switch (data[0])
    {
        case TAG_NIL:
            //printf("No response: NIL\n");
            return 1;
        case TAG_ERR:
            if (size < 1 + 8) {
                printf("Error in reading err tag\n");
                return -1;
            }{
            int32_t code = 0;
            uint32_t len = 0;
            memcpy(&code, &data[1], 4);
            memcpy(&len, &data[1 + 4], 4);
            if (size < 1 + 8 + len) {
                printf("Size mistach in error response\n");
                return -1;
            }
            printf("(err) %d %.*s\n", code, len, &data[1 + 8]);
            return 1 + 8 + len;
            }
        case TAG_STR:
            if( size < 1 + 4)
            {
                printf("Error reading string response\n");
                return -1;
            }

            {
            int32_t len = 0;
            memcpy(&len, &data[1], 4);
            if(size < 1 + 4 + len)
            {
                printf("Error in size of string response\n");

            }
            uint8_t buf[len];
            memcpy(buf, &data[5], len);
            for(int i = 0; i < len; i++)
            {
                cout << buf[i];
            }
            cout<<"\n";
            return len + 1 + 4;       
            } 
        case TAG_INT:
            if( size < 1 + 8)
            {
                printf("Error reading int response\n");
                return -1;
            }
            {
            int64_t res = 0;
            memcpy(&res, &data[1], 4);
            printf("(int) %lld\n", res);

            return 1 + 8;
            }
        case TAG_DBL:
            if( size < 1 + 8)
            {
                printf("Error reading int response\n");
                return -1;
            }
            {
            double res = 0;
            memcpy(&res, &data[1], 4);
            printf("(db) %g\n", res);
            return 1 + 8;
            }
        case TAG_ARR:
            if( size < 1 + 4)
            {
                printf("Error reading int response\n");
                return -1;
            }
            {
            uint32_t array_len = 0;
            memcpy(&array_len, &data[1], 4);
            printf("(int) %d\n", array_len);
            uint32_t pointer = 5; 
            for(uint32_t i = 0; i < array_len; i++)
            {
                int32_t rv = print_response(&data[pointer], size- pointer);
                if(rv< 0)
                    return rv;
                pointer+=rv;
            }
            printf("Read full array \n");
            return pointer;
            }
        default:
            printf("Bas response\n");
            return -1;

    }
}
int32_t read_res(int fd) {
    // 4 bytes header
    char rbuf[4 + MAX_LEN + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            cout << "EOF\n";
        } else {
            cout << "read() error \n";
        }
        return -1;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);  // assume little endian
    if (len > MAX_LEN) {
        cout << "too long \n";
        return -1;
    }


    err = read_full(fd, &rbuf[4], len);
    if (err) {
        cout << "read() error \n";
        return -1;
    }

    // // reply body
    // if(len < 4)
    // {
    //     cout << "BAD response \n";
    //     return -1;
    // }
    int32_t rv = print_response((uint8_t *)&rbuf[4], len);
    if (rv > 0 && (uint32_t)rv != len) {
        printf("bad response");
        rv = -1;
    }
    return rv;

//    return 0;
}


int read_client_data(int fd)
{
    char buf[128] = {};
    int recv = read(fd, buf, sizeof(buf) - 1);
    if (recv < 0)
    {
        cout <<"Error in reading the data from client";
        return 0;
    }
    //cout <<"Data received: " << buf;
    return 1;
}

int main(int argc, char **argv) {
	// your code goes here
	struct sockaddr_in client_addr = {};
	
	//Create the socket
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
	{
	    cout << "Error creating socket";
	    return 0;
	}
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(2000);
	client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//why bind??
    if (connect(fd, (struct sockaddr *)&client_addr, sizeof(client_addr))<0)
	{
        cout << "Error in connecting \n";
		return 0;
	}
    
    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
       // cout << cmd.back() <<  "\n";
    }
     
    int32_t err = send_req(fd, cmd);
   // cout << err << "\n";
    if (err) {
        cout << "Error sending data \n"; 
        close(fd);
        return err;
    }

    err = read_res(fd);
    if (err == -1) {
        cout << "Error from server: " << errno ;
        close(fd);
        return err;
    
    }
  //  close(fd);

    return 0;
}

