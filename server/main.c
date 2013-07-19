#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

static const char MESSAGE[] = "This is John Corner speaking\n";
static const int PORT = 8964;

void listener_cb(struct evconnlistener *, evutil_socket_t,
		struct sockaddr *, int socklen, void *);
void conn_writecb(struct bufferevent *, void *);
void conn_readcb(struct bufferevent *, void *);
void conn_eventcb(struct bufferevent *, short, void *);
void signal_cb(evutil_socket_t, short, void *);

int main() {
	struct event_base *base;
	struct evconnlistener *listener;
	struct event *signal_event;
	struct sockaddr_in sin;
	
	base = event_base_new();
	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);

	listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
			LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
			(struct sockaddr*)&sin,
			sizeof(sin));

	if (!listener) {
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}

	signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);

	if (!signal_event || event_add(signal_event, NULL) < 0) {
		fprintf(stderr, "Could not create/add a signal event!\n");
		return 1;
	}

	event_base_dispatch(base);

	evconnlistener_free(listener);
	event_free(signal_event);
	event_base_free(base);

	printf("done\n");
	return 0;
}

void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
		struct sockaddr *sa, int socklen, void *user_data)
{
	struct event_base *base = user_data;
	struct bufferevent *bev;
	
	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}
	bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, NULL);
	bufferevent_enable(bev, EV_WRITE|EV_READ);

	bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

void read_cb(struct bufferevent *bev, void *ctx)
{
}

void conn_writecb(struct bufferevent *bev, void *user_data)
{
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		printf("flushed answer\n");
		bufferevent_free(bev);
	}
}

void conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
	if (events & BEV_EVENT_EOF) {
		printf("Connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
				strerror(errno));
	}
	bufferevent_free(bev);
}

void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	struct event_base *base = user_data;
	struct timeval delay = {0, 0};
	printf("Caught an interrupt signal; exiting cleanly.\n");
	event_base_loopexit(base, &delay);
}

void conn_readcb(struct bufferevent *bev, void *user_data)
{
	#define MAX_LINE 4096
	char line[MAX_LINE + 1];
	int size;
	evutil_socket_t fd = bufferevent_getfd(bev);
	while (size = bufferevent_read(bev, line, MAX_LINE), size > 0) {
		line[size] = '\0';
		printf("fd=%u, read line:%s\n", fd, line);
		bufferevent_write(bev, line, size);
	}
}
