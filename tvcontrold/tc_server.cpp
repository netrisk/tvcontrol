#include <tc_server.h>
#include <tc_log.h>
#include <tc_cmd.h>
#include <tc_msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/poll.h>

static int tc_server_fd = -1;
static tc_msg_queue_t tc_server_queue = TC_MSG_QUEUE_INIT;

void tc_server_release(void)
{
	if (tc_server_fd != -1) {
		close(tc_server_fd);
		tc_server_fd = -1;
	}
	tc_msg_queue_close(&tc_server_queue);
}

int tc_server_init(void)
{
	/* Open the socket */
	tc_server_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (tc_server_fd == -1) {
		tc_log(TC_LOG_ERR, "Error creating server socket");
		return -1;
	}

	/* Bind for the address */
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1423);
	addr.sin_addr.s_addr = INADDR_ANY;
	int r = bind(tc_server_fd, (struct sockaddr *)&addr, sizeof(addr));
	if (r) {
		tc_log(TC_LOG_ERR, "Error binding server socket");
		tc_server_release();
		return -1;
	}

	/* Create the event pipe */
	if (tc_msg_queue_create(&tc_server_queue)) {
		tc_server_release();
		return -1;
	}

	/* Return success */
	return 0;
}

void tc_server_exec(void)
{
	/* Wait for anything to be received */
	while (true) {
		/* Check if there is any reception event */
		struct pollfd fds[2];	
		fds[0].fd = tc_server_fd;
		fds[0].events = POLLIN;
		fds[1].fd = TC_MSG_QUEUE_POLLFD(&tc_server_queue);
		fds[1].events = POLLIN;
		int r = poll(fds, 2, -1);
		if (fds[0].revents & POLLIN) {
			/* We have received a message */
			struct sockaddr_in src;
			socklen_t src_len = sizeof(src);
			char buf[257];
			ssize_t r = recvfrom(tc_server_fd, buf, sizeof(buf)-1,
			                     0, (struct sockaddr *)&src, &src_len);
			if (r > 0) {
				buf[r] = 0;
				tc_log(TC_LOG_INFO, "Command: \"%s\"", buf);
				int ret = tc_cmd(buf, r);
				if (ret > 0)
					break;
				if (ret < 0)
					tc_log(TC_LOG_ERR, "Error in command: \"%s\"", buf);
			}
		}
		if (fds[1].revents & POLLIN) {
			/* We have received an event */
			tc_msg_t msg;
			if (tc_msg_recv(&tc_server_queue, &msg)) {
				tc_log(TC_LOG_ERR, "server: Error reading from event pipe");
				break;
			}
			/* Execute the event */
			tc_log(TC_LOG_INFO, "Event: \"%s\"", msg.buf);
			int ret = tc_cmd((const char *)msg.buf, msg.len);
			if (ret > 0)
				break;
			if (ret < 0)
				tc_log(TC_LOG_ERR, "Error in event: \"%s\"", msg.buf);
		}
	}
}

int tc_server_event(const char *buffer, uint8_t len)
{
	if (tc_msg_send(&tc_server_queue, buffer, len)) {
		tc_log(TC_LOG_ERR, "Error enqueuing event");
		return -1;
	}
	return 0;
}
