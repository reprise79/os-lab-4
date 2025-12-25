#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>

    typedef HANDLE MyPort;
    #define BAD_PORT INVALID_HANDLE_VALUE
    #define PAUSE(ms) Sleep(ms)

    MyPort connect_port(const char* name) {
        HANDLE h = CreateFileA(name,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if(h == BAD_PORT) {
            return BAD_PORT;
        }
        
        DCB cfg = {0};
        cfg.DCBlength = sizeof(cfg);
        GetCommState(h, &cfg);
        cfg.BaudRate = CBR_9600;
        cfg.ByteSize = 8;
        cfg.StopBits = ONESTOPBIT;
        cfg.Parity = NOPARITY;
        SetCommState(h, &cfg);
        return h;
    }

    int send_data(MyPort p, const char* text) {
        unsigned long written;
        
        if(!WriteFile(p, text, strlen(text), &written, NULL)) {
            return 0;
        }
        return 1;
    }

    void disconnect(MyPort p) {
        CloseHandle(p);
    }

#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <termios.h>

    typedef int MyPort;
    #define BAD_PORT -1
    #define PAUSE(ms) usleep((ms) * 1000)

    MyPort connect_port(const char* name) {
        int id = open(name, O_RDWR | O_NOCTTY | O_NDELAY);

        if(id == -1) {
            return BAD_PORT;
        }

        struct termios cfg;
        
        tcgetattr(id, &cfg);
        cfsetispeed(&cfg, B9600);
        cfsetospeed(&cfg, B9600);

        cfg.c_cflag &= ~PARENB;
        cfg.c_cflag &= ~CSTOPB;
        cfg.c_cflag &= ~CSIZE;
        cfg.c_cflag |= CS8;
        cfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        cfg.c_oflag &= ~OPOST;

        tcsetattr(id, TCSANOW, &cfg);
        return id;
    }

    int send_data(MyPort p, const char* text) {
        int len = strlen(text);
        int real_sent = write(p, text, len);
        return (real_sent == len); 
    }

    void disconnect(MyPort p) {
        close(p);
    }

#endif

int main(int argc, char* argv[]) {
    char* name = argv[1];
    printf("connecting to %s\n", name);

    MyPort p = connect_port(name);
    if(p == BAD_PORT) {
        printf("can't open port\n");
        return 1;
    }

    printf("started\n");

    srand(time(NULL));

    while(1) {
        float t = 10.0 + (rand() % 200) / 10.0;

        char msg[50];
        sprintf(msg, "%.1f\n", t);

        if (send_data(p, msg)) {
            printf("sent: %s", msg);
        }

        else {
            printf("error sending\n");
            break;
        }

        PAUSE(1000);
    }
    disconnect(p);
    return 0;
}