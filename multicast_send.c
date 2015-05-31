/*
 * This file is part of multicast_test.
 *
 * Copyright 2015 Ricardo Garcia <r@rg3.name>
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty. 
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MIN_MSG_SIZE (1)
#define MAX_MSG_SIZE (32768)

void usage(char *progname)
{
    fprintf(stderr, "Usage: %s MULTICAST_IP PORT INTERFACE_IP MESSAGE_FILE\n",
            progname);
}

int main(int argc, char *argv[])
{
    FILE *stream = NULL;
    char *msg = NULL;
    int retval = 0;
    int fd = -1;

    /*
     * Argument processing part.
     */

    if (argc != 5)
    {
        usage(argv[0]);
        retval = 1;
        goto out;
    }

    // Parse multicast IP.
    struct in_addr mip;
    if (! inet_pton(AF_INET, argv[1], &mip))
    {
        fprintf(stderr, "Error: invalid multicast IP: %s\n", argv[1]);
        retval = 2;
        goto out;
    }

    // Parse port.
    in_port_t port;
    char *endptr;
    long lport = strtol(argv[2], &endptr, 10);
    if (endptr != argv[2] + strlen(argv[2]) || lport > USHRT_MAX)
    {
        fprintf(stderr, "Error: invalid port: %s\n", argv[2]);
        retval = 3;
        goto out;
    }
    port = (in_port_t)lport;

    // Parse interface IP.
    struct in_addr iip;
    if (! inet_pton(AF_INET, argv[3], &iip))
    {
        fprintf(stderr, "Error: invalid interface IP: %s\n", argv[3]);
        retval = 4;
        goto out;
    }

    // Read message file.
    stream = fopen(argv[4], "rb");
    if (! stream)
    {
        fprintf(stderr, "Error: unable to open message file\n");
        retval = 5;
        goto out;
    }

    if (fseeko(stream, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "Error: unable to get message size\n");
        retval = 6;
        goto out;
    }

    off_t size = ftello(stream);
    if (size < MIN_MSG_SIZE || size > MAX_MSG_SIZE)
    {
        fprintf(stderr, "Error: file size not in valid range (%d-%d bytes)\n",
                MIN_MSG_SIZE, MAX_MSG_SIZE);
        retval = 7;
        goto out;
    }

    msg = malloc(size);
    if (! msg)
    {
        fprintf(stderr, "Error: unable to allocate memory\n");
        retval = 8;
        goto out;
    }

    if (fseeko(stream, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error: unable to seek inside file\n");
        retval = 9;
        goto out;
    }

    if (fread(msg, (size_t)size, 1, stream) != 1)
    {
        fprintf(stderr, "Error: unable to read message file\n");
        retval = 9;
        goto out;
    }

    /*
     * Network part.
     */

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
    {
        perror("Error: unable to create socket");
        retval = 10;
        goto out;
    }
   
   struct sockaddr_in baddr = {
       .sin_family = AF_INET,
       .sin_port = 0,
       .sin_addr = {
           .s_addr = INADDR_ANY
       }
   };
   if (bind(fd, (const struct sockaddr *)(&baddr), sizeof(baddr)) == -1)
   {
       perror("Error: unable to bind socket");
       retval = 11;
       goto out;
   }

   if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF,
                  (const void *)(&iip), sizeof(iip)) == -1)
   {
       perror("Error: unable to set multicast interface");
       retval = 12;
       goto out;
   }

   struct sockaddr_in daddr = {
       .sin_family = AF_INET,
       .sin_port = htons(port),
       .sin_addr = {
           .s_addr = mip.s_addr
       }
   };
   if (sendto(fd, msg, size, 0,
              (const struct sockaddr *)(&daddr), sizeof(daddr)) == -1)
   {
       perror("Error: unable to send message");
       retval = 13;
       goto out;
   }

out:
    if (fd != -1)
        close(fd);
    if (stream)
        fclose(stream);
    free(msg);
    return retval;
}
