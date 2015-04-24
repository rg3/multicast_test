/*
 * This file is part of multicast_test.
 *
 * Copyright 2015 Ricardo Garcia <public@rg3.name>
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty. 
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>

#define MAX_INTERFACES   (20)
#define MAX_SOCKETS      (50)
#define READ_BUFFER_SIZE (256*1024)

struct MulticastEndpoint
{
    struct in_addr multicast;
    in_port_t port;

    int num_interfaces;
    struct in_addr interfaces[MAX_INTERFACES];
};

struct ProcessEndpoints
{
    int num_endpoints;
    struct MulticastEndpoint endpoints[MAX_SOCKETS];
};

struct Sockets
{
    int num_sockets;
    int sockets[MAX_SOCKETS];
};

/*
 * Expected string format:
 *
 * MULTICAST_IP:PORT:INTERFACE_IP[,INTERFACE_IP...]
 *
 */
int make_endpoint(const char *str, struct MulticastEndpoint *out)
{
    char *c;
    char *ip_str = NULL;
    char *port_str = NULL;
    struct in_addr ip;
    int retcode = 0;

    // Find first colon.
    c = strchr(str, ':');
    if (c == NULL)
    {
        retcode = 1;
        goto out;
    }

    // Duplicate and parse multicast IP string.
    ip_str = strndup(str, c - str);
    if (! ip_str)
    {
        retcode = 2;
        goto out;
    }

    if (! inet_pton(AF_INET, ip_str, &(out->multicast)))
    {
        retcode = 3;
        goto out;
    }

    // Find port
    str = c + 1;
    c = strchr(str, ':');
    if (c == NULL)
    {
        retcode = 4;
        goto out;
    }
    port_str = strndup(str, c - str);
    if (! port_str)
    {
        retcode = 5;
        goto out;
    }

    // Parse port.
    char *endptr;
    long port = strtol(port_str, &endptr, 10);
    if (endptr != port_str + strlen(port_str))
    {
        retcode = 6;
        goto out;
    }
    if (port > USHRT_MAX)
    {
        retcode = 7;
        goto out;
    }
    out->port = htons((uint16_t)port);

    // Parse interface addresses.
    str = c + 1;
    out->num_interfaces = 0;
    size_t len;
    int do_break = 0;
    for (;;)
    {
        c = strchr(str, ',');
        if (! c)
        {
            len = strlen(str);
            do_break = 1;
        }
        else
        {
            len = c - str;
        }

        // Dupe and parse.
        if (ip_str)
        {
            free(ip_str);
            ip_str = NULL;
        }

        ip_str = strndup(str, len);
        if (! ip_str)
        {
            retcode = 8;
            goto out;
        }

        if (! inet_pton(AF_INET, ip_str, &ip))
        {
            retcode = 9;
            goto out;
        }

        // Store if possible.
        if (out->num_interfaces >= MAX_INTERFACES)
        {
            retcode = 10;
            goto out;
        }
        memcpy(out->interfaces + out->num_interfaces, &ip, sizeof(ip));
        out->num_interfaces++;

        if (do_break)
            break;

        str = c + 1;
    }

out:
    free(ip_str);
    free(port_str);
    return retcode;
}

/*
 * Makes all endpoints. One per positional argument.
 */
int make_all_endpoints(int argc, char *argv[], struct ProcessEndpoints *out)
{
    int i;
    int ret;

    out->num_endpoints = 0;

    for (i = 1; i < argc; ++i)
    {
        if (out->num_endpoints >= MAX_SOCKETS)
        {
            fprintf(stderr, "Too many multicast addresses given\n");
            return 1;
        }
        ret = make_endpoint(argv[i], out->endpoints + out->num_endpoints++);
        if (ret != 0)
        {
            fprintf(stderr, "Error parsing argument %d: "
                    "internal error code %d\n", i, ret);
            return 2;
        }
    }

    return 0;
}

/*
 * Creates sockets from specified endpoints.
 */
int endpoints_to_sockets(const struct ProcessEndpoints *in,
                         struct Sockets *out)
{
    struct sockaddr_in bind_addr;
    struct ip_mreq mreq;
    int i;
    int j;
    int s;

    out->num_sockets = 0;
    for (i = 0; i < in->num_endpoints; ++i)
    {
        // Create socket.
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s == -1)
        {
            perror("Error creating socket");
            return 1;
        }

        // Save it once created.
        out->sockets[out->num_sockets++] = s;

        int one = 1;
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
        {
            perror("Error setting reuse flag");
            return 2;
        }

        // Bind to multicast address.
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = in->endpoints[i].port;
        memcpy(&(bind_addr.sin_addr),
               &(in->endpoints[i].multicast),
               sizeof(in->endpoints[i].multicast));
        if (bind(s, (const struct sockaddr *)(&bind_addr),
                 sizeof(bind_addr)) != 0)
        {
            perror("Error binding to multicast address");
            return 3;
        }

        // Subscribe to the multicast address from all interfaces.
        for (j = 0; j < in->endpoints[i].num_interfaces; ++j)
        {
            memcpy(&(mreq.imr_multiaddr),
                   &(in->endpoints[i].multicast),
                   sizeof(in->endpoints[i].multicast));
            memcpy(&(mreq.imr_interface),
                   &(in->endpoints[i].interfaces[j]),
                   sizeof(in->endpoints[i].interfaces[j]));
            if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           &mreq, sizeof(mreq)) != 0)
            {
                perror("Error adding multicast group membership");
                return 4;
            }
        }

        printf("Multicast address number %d: "
               "created file descriptor %d on %d interfaces\n",
               i + 1, s, in->endpoints[i].num_interfaces);
    }

    return 0;
}

/*
 * Close sockets.
 */
void close_sockets(const struct Sockets *in)
{
    // We don't check errors because we're going to exit anyway.
    int i;

    printf("\nClosing sockets\n");
    for (i = 0; i < in->num_sockets; ++i)
        close(in->sockets[i]);
}

/*
 * Poll sockets for data.
 */
int exit_poll = 0;

void sigint_handler(int unused)
{
    exit_poll = 1; // Set flag from handler.
}

void poll_sockets(const struct Sockets *in)
{
    // Signal handler to stop polling when SIGINT is received.
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&(sa.sa_mask));
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) != 0)
    {
        perror("Unable to set signal handler for SIGINT");
        return;
    }

    // Allocate read buffer.
    unsigned char *buffer = malloc(READ_BUFFER_SIZE);
    if (! buffer)
    {
        fprintf(stderr, "Unable to allocate read buffer\n");
        return;
    }

    struct pollfd fds[MAX_SOCKETS];
    struct timeval tv;
    ssize_t read_count;
    int i;
    int ret;

    // Infinite poll() loop.
    for (;;)
    {
        for (i = 0; i < in->num_sockets; ++i)
        {
            fds[i].fd = in->sockets[i];
            fds[i].events = POLLIN | POLLPRI;
            fds[i].revents = 0;
        }

        ret = poll(fds, in->num_sockets, -1);
        if (ret > 0)
        {
            for (i = 0; i < in->num_sockets; ++i)
            {
                if (fds[i].revents & POLLERR)
                    fprintf(stderr, "POLLERR on socket %d\n", fds[i].fd);
                if (fds[i].revents & POLLHUP)
                    fprintf(stderr, "POLLHUP on socket %d\n", fds[i].fd);
                if (fds[i].revents & POLLNVAL)
                    fprintf(stderr, "POLLNVAL on socket %d\n", fds[i].fd);

                if ((fds[i].revents & POLLIN) || (fds[i].revents & POLLPRI))
                {
                    read_count = read(fds[i].fd, buffer, READ_BUFFER_SIZE);
                    gettimeofday(&tv, NULL);
                    printf("%lld.%06lld read %lld bytes%s from socket %d\n",
                           (long long)(tv.tv_sec),
                           (long long)(tv.tv_usec),
                           (long long)(read_count),
                           (read_count >= READ_BUFFER_SIZE?" (or more)":""),
                           fds[i].fd);
                }
            }
        }
        else if (ret == 0)
        {
            fprintf(stderr, "Warning: timeout on poll() without timeout\n");
        }
        else
        {
            if (errno != EINTR)
            {
                perror("Error polling sockets");
                break;
            }
        }

        // Stop polling on SIGINT.
        if (exit_poll)
            break;
    }

    free(buffer);
    return;
}

/*
 * main()
 */
int main(int argc, char *argv[])
{
    struct ProcessEndpoints pe;
    struct Sockets socks;
    int ret;

    if (argc <= 1)
    {
        fprintf(stderr, "Usage: %s "
                "MULTICAST_IP:PORT:INTERFACE_IP[,INTERFACE_IP...] ...\n",
                argv[0]);
        return 1;
    }

    ret = make_all_endpoints(argc, argv, &pe);
    if (ret != 0)
    {
        return 2;
    }

    ret = endpoints_to_sockets(&pe, &socks);
    if (ret != 0)
    {
        return 3;
    }

    poll_sockets(&socks);
    close_sockets(&socks);

    return 0;
}
