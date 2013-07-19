#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

void read_callback(struct bufferevent *bev, void *ctx);
void event_callback(struct bufferevent *bev, short events, void *ctx);

int main() {
	struct event_base *base;
	struct sockaddr_in sin;
	struct bufferevent *bev;
	
	base = event_base_new();
	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(8964);
	inet_aton("127.0.0.1", &sin.sin_addr);

	memset(sin.sin_zero, 0x00, 0);

	bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, read_callback, NULL, event_callback, NULL);
	if (bufferevent_socket_connect(bev, 
				(struct sockaddr *)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "Could not connect!\n");
		bufferevent_free(bev);	
		return -1;
	}
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	event_base_dispatch(base);

	return 0;
}

void read_callback(struct bufferevent *bev, void *ctx) {
	struct evbuffer *input = bufferevent_get_input(bev);
	int size = evbuffer_get_length(input);
	evbuffer_write(input, 1);
	bufferevent_write(bev, "hello world", 12);
}

void event_callback(struct bufferevent *bev, short events, void *ctx) {
	if (events & BEV_EVENT_CONNECTED) {
		printf("connected");
	} else if (events & BEV_EVENT_EOF) {
		printf("Connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
				strerror(errno));
	}
}
