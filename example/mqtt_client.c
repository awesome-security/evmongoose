#include <mongoose.h>

static const char *s_address = "localhost:1883";
static const char *s_user_name = NULL;
static const char *s_password = NULL;
static const char *s_topic = "/stuff";
static struct mg_mqtt_topic_expression s_topic_expr = {NULL, 0};

static void ev_handler(struct mg_connection *nc, int ev, void *data)
{
	struct mg_mqtt_message *msg = (struct mg_mqtt_message *)data;

	switch (ev) {
		case MG_EV_CONNECT: {
			struct mg_send_mqtt_handshake_opts opts;
			memset(&opts, 0, sizeof(opts));
			opts.user_name = s_user_name;
			opts.password = s_password;

			mg_set_protocol_mqtt(nc);
			mg_send_mqtt_handshake_opt(nc, "dummy", opts);
			break;
		}

		case MG_EV_MQTT_CONNACK:
			if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
				printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
				exit(1);
			}
			s_topic_expr.topic = s_topic;
			printf("Subscribing to '%s'\n", s_topic);
			mg_mqtt_subscribe(nc, &s_topic_expr, 1, 42);
			break;

		case MG_EV_MQTT_PUBACK:
		  printf("Message publishing acknowledged (msg_id: %d)\n", msg->message_id);
	      break;
		
		case MG_EV_MQTT_SUBACK:
			printf("Subscription acknowledged, forwarding to '/test'\n");
			break;

		case MG_EV_MQTT_PUBLISH: {
#if 0
			char hex[1024] = {0};
			mg_hexdump(nc->recv_mbuf.buf, msg->payload.len, hex, sizeof(hex));
			printf("Got incoming message %.*s:\n%s", (int)msg->topic.len, msg->topic.p, hex);
#else
			printf("Got incoming message %.*s: %.*s\n", (int) msg->topic.len,
			msg->topic.p, (int) msg->payload.len, msg->payload.p);
#endif

			printf("Forwarding to /test\n");
			mg_mqtt_publish(nc, "/test", 65, MG_MQTT_QOS(0), msg->payload.p,
			msg->payload.len);
			break;
		}
		case MG_EV_CLOSE:
	      printf("Connection closed\n");
	      exit(1);
	}
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
	printf("Got signal: %d\n", w->signum);
	ev_break(loop, EVBREAK_ALL);
}

static void mqtt_ping_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
	mg_mqtt_ping((struct mg_connection *)w->data);
}

int main(int argc, char *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sig_watcher;
	ev_timer ping_timer;
	struct mg_mgr mgr;
	struct mg_connection *nc;
	int i;

	ev_signal_init(&sig_watcher, signal_cb, SIGINT);
	ev_signal_start(loop, &sig_watcher);
	
	mg_mgr_init(&mgr, NULL);

	/* Parse command line arguments */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0)
			s_address = argv[++i];
		else if (strcmp(argv[i], "-u") == 0)
			s_user_name = argv[++i];
		else if (strcmp(argv[i], "-t") == 0)
			s_topic = argv[++i];
		else if (strcmp(argv[i], "-p") == 0)
			s_password = argv[++i];
	}

	nc = mg_connect(&mgr, s_address, ev_handler);
	if (!nc) {
		fprintf(stderr, "mg_connect(%s) failed\n", s_address);
		goto err;
	}

	ev_timer_init(&ping_timer, mqtt_ping_cb, 10, 10);
	ping_timer.data = nc;
	ev_timer_start(loop, &ping_timer);
	
	ev_run(loop, 0);

err:
	printf("exit...\n");
	
	mg_mgr_free(&mgr);

	return 0;
}


