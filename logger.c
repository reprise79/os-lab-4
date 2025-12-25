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

        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        SetCommTimeouts(h, &timeouts);

        return h;
    }

    int read_data(MyPort p, char* buffer, int size) {
        unsigned long read;
        
        if(!ReadFile(p, buffer, size, &read, NULL)) {
            return 0;
        }
        return (int)read;
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

        tcsetattr(id, TCSANOW, &cfg);
        return id;
    }

    int read_data(MyPort p, char* buffer, int size) {
        return read(p, buffer, size); 
    }

    void disconnect(MyPort p) {
        close(p);
    }

#endif

#define SEC_IN_DAY (24*3600)
#define SEC_IN_MONTH (24 * 30 * 3600)
#define SEC_IN_YEAR (365 * 24 * 3600)

void get_time(char* buf) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", 
        t->tm_year + 1900,
        t->tm_mon + 1,
        t ->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec);
}

void append_log(const char* filename, const char* time_str, float value) {
    FILE* f = fopen(filename, "a");
    if(f) {
        fprintf(f, "%s | %.2f\n", time_str, value);
        fclose(f);
    }
}

void clean_logs(const char* filename, long max_age_seconds) {
    char temp_filename[100];
    sprintf(temp_filename, "%s.tmp", filename);

    FILE* src = fopen(filename, "r");
    FILE* dst = fopen(temp_filename, "w");

    time_t now = time(NULL);
    char line[128];

    while(fgets(line, sizeof(line), src)) {
        int Y, M, D, h, m, s;
        if(sscanf(line, "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &s) == 6) {
            struct tm t = {0};
            t.tm_year = Y - 1900;
            t.tm_mon  = M - 1;
            t.tm_mday = D;
            t.tm_hour = h;
            t.tm_min  = m;
            t.tm_sec  = s;
            t.tm_isdst = -1;

            time_t record_time = mktime(&t);
            double diff = difftime(now, record_time);

            if(diff <= max_age_seconds) {
                fputs(line, dst);
            }
        }
    }

    fclose(src);
    fclose(dst);

    remove(filename);
    rename(temp_filename, filename);

    printf("removed records older than %ld sec\n", max_age_seconds);
}

typedef struct {
    double sum;
    int count;
    int last_time_unit;
} stats;

int get_current_hour() {
    time_t now = time(NULL);
    return localtime(&now)->tm_hour;
}

int get_current_day() {
    time_t now = time(NULL);
    return localtime(&now)->tm_yday;
}

int main(int argc, char* argv[]) {
    char* name = argv[1];
    printf("connecting to %s\n", name);

    MyPort p = connect_port(name);
    if(p == BAD_PORT) {
        printf("can't open port\n");
        return 1;
    }

    printf("started\n");

    stats hour_stats = {0, 0, get_current_hour()};
    stats day_stats = {0, 0, get_current_day()};

    char str_buf[64];
    int pos = 0;
    time_t last_cleanup = 0;

    while(1) {
        char chunk[1];
        if(read_data(p, chunk, 1) > 0) {
            if (chunk[0] != '\n' && pos < 63) {
                str_buf[pos++] = chunk[0];
                continue;
            }

            str_buf[pos] = '\0';
            pos = 0;

            float temp = (float)atof(str_buf);
            char time_buf[64];
            get_time(time_buf);

            printf("%s raw: %.1f\n", time_buf, temp);

            append_log("log_raw.txt", time_buf, temp);

            hour_stats.sum += temp;
            hour_stats.count++;
            day_stats.sum += temp;
            day_stats.count++;

            int curr_hour = get_current_hour();
            if(curr_hour != hour_stats.last_time_unit) {
                float hour_avg = (float)(hour_stats.sum/hour_stats.count);
                append_log("log_hour.txt", time_buf, hour_avg);
                printf("hour ended, avg: %.2f saved.\n", hour_avg);

                hour_stats.sum = 0;
                hour_stats.count = 0;
                hour_stats.last_time_unit = curr_hour;
            }

            int curr_day = get_current_day();
            if(curr_day != day_stats.last_time_unit) {
                float day_avg = (float)(day_stats.sum/day_stats.count);
                append_log("log_day.txt", time_buf, day_avg);
                printf("day ended, avg: %.2f saved.\n", day_avg);

                day_stats.sum = 0;
                day_stats.count = 0;
                day_stats.last_time_unit = curr_day;
            }

            time_t now = time(NULL);

            if(now - last_cleanup > 3600) {
                printf("cleanup logs\n");
                clean_logs("log_raw.txt", SEC_IN_DAY);
                clean_logs("log_hour.txt", SEC_IN_MONTH);
                clean_logs("log_day.txt", SEC_IN_YEAR);
                last_cleanup = now;
            }
        }

        else {
            PAUSE(10);
        }

    }
    disconnect(p);
    return 0;
}