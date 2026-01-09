#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include "pb_encode.h"
#include "pb_decode.h"
#include "pb_common.h"
#include "uart.pb.h"
#include <stdint.h>
#include <arpa/inet.h>


#define PORT "/dev/ttyUSB0"
#define BAUD 115200

typedef struct {
    char* port;
    int baud;
} uart_cfg_t;

uart_cfg_t config;
struct termios uart_options;

void print_usage(const char *prog_name) 
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help            Show this help message and exit\n");
    printf("  -p, --port PORT       Specify the UART port (default: /dev/ttyUSB0)\n");
    printf("  -b, --baudrate BAUD   Specify the baud rate (default: 115200)\n");
}

int parse_args(int argc, char *argv[])
{
    config = (uart_cfg_t){ .port = PORT, .baud = BAUD };


    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"baudrate", required_argument, 0, 'b'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hp:b:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'p':
                config.port = optarg;
                break;
            case 'b':
                config.baud = atoi(optarg);
                break;
            default:
                print_usage(argv[0]);
                return 1;
        
        }
    }

    printf("Using UART port: %s\n", config.port);
    printf("Using baud rate: %d\n", config.baud);

    
    return 0;
}

int init_uart() {
    int fd = open("/dev/ttyUSB1", O_RDWR | O_NOCTTY | O_NONBLOCK);
    
    if (fd < 0) {
        perror("open");
        return -1;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }
    
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | CSTOPB);
    
    tty.c_cflag &= ~CRTSCTS;
    
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }
    
    return fd;
}

int main(int argc, char *argv[]) {
    int parse_result = parse_args(argc, argv);
    int fd = init_uart();
    
    if (fd < 0) {
        fprintf(stderr, "Failed to initialize UART\n");
        return 1;
    }
    uint8_t buffer[256];
    int buffer_pos = 0;
    uint32_t expected_len = 0;
    bool reading_header = true;

    /*
        This thing sucked to figure out, basically I go byte by byte storing the header, then once we got the header
        (which is the packet length) we can decode the rest of the message using that.  
    
    */
    while(1) {
        uint8_t byte;
        ssize_t n = read(fd, &byte, 1);
        
        if (n == 1) {
            // Store byte, increment
            buffer[buffer_pos++] = byte;
            
            // State 1, do we hath header
            if (reading_header && buffer_pos == 4) {
                expected_len = *(uint32_t*)buffer;
                printf("Got header: [%02x %02x %02x %02x] = %u bytes expected\n", 
                    buffer[0], buffer[1], buffer[2], buffer[3], expected_len);
                reading_header = false;
                buffer_pos = 0;
            }
            // State 2, decode `
            else if (!reading_header) {
                if (buffer_pos == expected_len) {
                    // Got full message, decode it
                    pb_istream_t stream = pb_istream_from_buffer(buffer, expected_len);
                    uartMessage message = uartMessage_init_zero;
                    
                    if (pb_decode(&stream, uartMessage_fields, &message)) {
                        printf("Decoded!\n");
                        printf("MAC: ");
                        for (size_t i = 0; i < message.mac.size; i++) {
                            printf("%02x", message.mac.bytes[i]);
                        }
                        printf("\nSSID: %.*s\n", message.ssid.size, (char *)message.ssid.bytes);
                        printf("Channel: %u\n", message.channel);
                        printf("RSSI: %d\n", message.rssi);
                        printf("Auth Mode: %d\n", message.authmode);
                        printf("Pairwise Cipher: %d\n", message.pairwise_cipher);
                        printf("Groupwise Cipher: %d\n", message.groupwise_cipher);
                        printf("Country: %.*s\n\n", message.country.size, (char *)message.country.bytes);
                    } else {
                        printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
                    }
                    
                    reading_header = true;
                    buffer_pos = 0;
                }
            }
        }
    }
}
