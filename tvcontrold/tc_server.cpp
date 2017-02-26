#include <tc_server.h>
#include <tc_log.h>
#include <tc_cmd.h>
#include <tc_msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/poll.h>
#include <string.h>

static int tc_server_udp_fd = -1;
static int tc_server_tcp_fd = -1;
static int tc_server_tcp_con = -1;
static tc_msg_queue_t tc_server_queue = TC_MSG_QUEUE_INIT;
static uint8_t tc_server_tcp_data[1000];
static uint32_t tc_server_tcp_len = 0;
static bool tc_server_tcp_response_todo = false;
static const char *tc_server_tcp_response_data = (const char *)NULL;
static const char *tc_server_tcp_response[3] = { NULL, NULL, NULL };
static uint32_t tc_server_tcp_response_index = 0;
static uint32_t tc_server_tcp_response_offset = 0;

/* Enable this to debug */
/* #define TC_SERVER_DEBUG */

/**
 *  Close the TCP connection and free the data.
 */
static void tc_server_tcp_close(void)
{
	tc_server_tcp_len = 0;
	if (tc_server_tcp_response_data) {
		free((void *)tc_server_tcp_response_data);
		tc_server_tcp_response_data = NULL;
	}
	memset(tc_server_tcp_response, 0, sizeof(tc_server_tcp_response));
	tc_server_tcp_response_index = 0;
	tc_server_tcp_response_offset = 0;
	tc_server_tcp_response_todo = false;
	close(tc_server_tcp_con);
	tc_server_tcp_con = -1;
}

/**
 *  Analize the HTTP header.
 *
 *  \param data  Data to analyze.
 *  \param len   Length of the data.
 *  \return -1 if the data has errors.
 *  \return 0 if the data has been processed with success.
 *  \return 1 if we need more data.
 */
static int tc_server_tcp_analyze(const uint8_t *data, uint32_t len)
{
	/* Check HTTP header */
	if (len < 4)
		return 1;
	if (memcmp(data, "GET ", 4))
		return -1;
	/* Get the command */
	char buf[257];
	uint32_t buf_len = 0;
	uint32_t index = 4;
	uint32_t scape_index = 0;
	uint8_t  scape_char = 0;
	while (true) {
		if (index == len)
			return 1;
		char c = data[index++];
		if (c == ' ' || c == '\t' || c == '\r') {
			if (buf_len)
				break;
			else
				continue;
		}
		if (buf_len + 1 == sizeof(buf))
			return -1;
		if (scape_index) {
			uint8_t scape_digit = 0;
			if (c >= '0' || c <= '9')
				scape_digit = c - '0';
			else if (c >= 'a' || c <= 'f')
				scape_digit = c - ('a' - 10);
			else if (c >= 'A' || c <= 'F')
				scape_digit = c - ('A' - 10);
			else
				return -1;
			scape_char <<= 4;
			scape_char |= scape_digit;
			if (scape_index == 2)
				buf[buf_len++] = scape_char;
			continue;
		} else if (c == '%') {
			scape_index = 1;
			scape_char = 0;
			continue;
		}
		buf[buf_len++] = c;
	}
	/* Execute the command */
	buf[buf_len] = 0;
	/* Check if it is a command */
	if (buf_len >= 5 && !memcmp(buf, "/cmd/", 5)) {
		char *cmd = buf + 5;
		uint32_t cmd_len = buf_len - 5;
		tc_log(TC_LOG_INFO, "Command: \"%s\"", cmd);
		int ret = tc_cmd(cmd, cmd_len);
		if (ret == 0) {
			#ifdef TC_SERVER_DEBUG
			tc_log(TC_LOG_DEBUG, "server: environment preparing");
			#endif /* TC_SERVER_DEBUG */
			tc_server_tcp_response_data = tc_cmd_env_csv();
			#ifdef TC_SERVER_DEBUG
			tc_log(TC_LOG_DEBUG, "server: environment prepared");
			#endif /* TC_SERVER_DEBUG */
			return 0;
		}
		if (ret < 0) {
			tc_log(TC_LOG_ERR, "Error in command: \"%s\"", cmd);
			return -1;
		}
		return 1;
	} else if (buf_len == 5 && !memcmp(buf, "/ping", 5)) {
		#ifdef TC_SERVER_DEBUG
		tc_log(TC_LOG_DEBUG, "server: ping");
		#endif /* TC_SERVER_DEBUG */
		tc_server_tcp_response_data = tc_cmd_env_csv();
		return 0;
	}
	/* Return success */
	return 0;
}

void tc_server_release(void)
{
	if (tc_server_udp_fd != -1) {
		close(tc_server_udp_fd);
		tc_server_udp_fd = -1;
	}
	if (tc_server_tcp_fd != -1) {
		close(tc_server_tcp_fd);
		tc_server_tcp_fd = -1;
	}
	if (tc_server_tcp_con != -1)
		tc_server_tcp_close();
	tc_msg_queue_close(&tc_server_queue);
}

int tc_server_init(void)
{
	/* Open the socket */
	tc_server_udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (tc_server_udp_fd == -1) {
		tc_log(TC_LOG_ERR, "Error creating UDP server socket");
		return -1;
	}
	tc_server_tcp_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (tc_server_udp_fd == -1) {
		tc_log(TC_LOG_ERR, "Error creating TCP server socket");
		return -1;
	}

	/* Bind for the address */
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1423);
	addr.sin_addr.s_addr = INADDR_ANY;
	int r = bind(tc_server_udp_fd, (struct sockaddr *)&addr, sizeof(addr));
	if (r) {
		tc_log(TC_LOG_ERR, "Error binding UDP server socket");
		tc_server_release();
		return -1;
	}
	r = bind(tc_server_tcp_fd, (struct sockaddr *)&addr, sizeof(addr));
	if (r) {
		tc_log(TC_LOG_ERR, "Error binding TCP server socket");
		tc_server_release();
		return -1;
	}
	r = listen(tc_server_tcp_fd, 10);
	if (r) {
		tc_log(TC_LOG_ERR, "Error listening TCP server socket");
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
		struct pollfd fds[4];
		uint32_t fdn = 0;
		struct pollfd *fd_udp = NULL;
		struct pollfd *fd_tcp = NULL;
		struct pollfd *fd_queue = NULL;
		struct pollfd *fd_tcp_con = NULL;
		fds[fdn].fd = tc_server_udp_fd;
		fds[fdn].events = POLLIN;
		fd_udp = &fds[fdn];
		fdn++;
		if (tc_server_tcp_con == -1) {
			fds[fdn].fd = tc_server_tcp_fd;
			fds[fdn].events = POLLIN;
			fd_tcp = &fds[fdn];
			fdn++;
		}
		fds[fdn].fd = TC_MSG_QUEUE_POLLFD(&tc_server_queue);
		fds[fdn].events = POLLIN;
		fd_queue = &fds[fdn];
		fdn++;
		if (tc_server_tcp_con >= 0) {
			fds[fdn].fd = tc_server_tcp_con;
			fds[fdn].events = tc_server_tcp_response_todo ? POLLOUT : POLLIN;
			fd_tcp_con = &fds[fdn];
			fdn++;
		}
		int r = poll(fds, fdn, -1);
		if (fd_udp && fd_udp->revents & POLLIN) {
			/* We have received a message from UDP */
			struct sockaddr_in src;
			socklen_t src_len = sizeof(src);
			char buf[257];
			ssize_t r = recvfrom(tc_server_udp_fd, buf, sizeof(buf)-1,
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
		if (fd_tcp && fd_tcp->revents & POLLIN) {
			/* We have received a TCP connection */
			struct sockaddr_in src;
			socklen_t src_len = sizeof(src);
			int r = accept(tc_server_tcp_fd, (struct sockaddr *)&src, &src_len);
			if (r >= 0) {
				tc_server_tcp_con = r;
				tc_server_tcp_response_todo = false;
				tc_log(TC_LOG_INFO, "TCP connection established");
			}
		}
		if (fd_tcp_con && !tc_server_tcp_response_todo &&
		    (fd_tcp_con->revents & POLLIN)) {
			/* We have received TCP data */
			int r = read(tc_server_tcp_con, 
			             tc_server_tcp_data + tc_server_tcp_len,
			             sizeof(tc_server_tcp_data) - tc_server_tcp_len - 1);
			if (r > 0) {
				tc_server_tcp_len += r;
				int r = tc_server_tcp_analyze(tc_server_tcp_data, tc_server_tcp_len);
				if (r < 0) {
					#ifdef TC_SERVER_DEBUG
					tc_log(TC_LOG_DEBUG, "server: tcp: parsing error");
					#endif /* TC_SERVER_DEBUG */
					tc_server_tcp_response_todo = true;
					tc_server_tcp_response[0] = "HTTP/1.0 400 Bad Request\r\n\r\n";
					tc_server_tcp_response[1] = tc_server_tcp_response_data;
					tc_server_tcp_response[2] = NULL;
					tc_server_tcp_response_index = 0;
					tc_server_tcp_response_offset = 0;
				} else if (r == 0) {
					#ifdef TC_SERVER_DEBUG
					tc_log(TC_LOG_DEBUG, "server: tcp: OK");
					#endif /* TC_SERVER_DEBUG */
					tc_server_tcp_response_todo = true;
					tc_server_tcp_response[0] = "HTTP/1.0 200 OK\r\n\r\n";
					tc_server_tcp_response[1] = tc_server_tcp_response_data;
					tc_server_tcp_response[2] = NULL;
					tc_server_tcp_response_index = 0;
					tc_server_tcp_response_offset = 0;
				}
			} else if (r < 0 || tc_server_tcp_len == sizeof(tc_server_tcp_data)-1) {
				tc_log(TC_LOG_INFO, "Error in TCP communication");
				tc_server_tcp_close();
			} else if (r == 0) {
				#ifdef TC_SERVER_DEBUG
				tc_log(TC_LOG_DEBUG, "server: tcp: premature close");
				#endif /* TC_SERVER_DEBUG */
				/* Closed remotely */
				tc_server_tcp_response_todo = true;
				tc_server_tcp_response[0] = "HTTP/1.0 400 Bad Request\r\n\r\n";
				tc_server_tcp_response[1] = NULL;
				tc_server_tcp_response_index = 0;
				tc_server_tcp_response_offset = 0;
			}
		}
		if (fd_tcp_con && tc_server_tcp_response_todo &&
		    (fd_tcp_con->revents & POLLOUT)) {
			/* We have to send TCP data */
			const char *ptr = 
			             tc_server_tcp_response[tc_server_tcp_response_index]
			                + tc_server_tcp_response_offset;
			int r = write(tc_server_tcp_con, ptr, strlen(ptr));
			if (r > 0) {
				if (!ptr[r]) {
					tc_server_tcp_response_index++;
					tc_server_tcp_response_offset = 0;
					if (!tc_server_tcp_response[tc_server_tcp_response_index])
						tc_server_tcp_close();
				} else
					tc_server_tcp_response_offset += r;
			} else {
				tc_log(TC_LOG_ERR, "server: Error sending TCP");
				tc_server_tcp_close();
			}
		}
		if (fd_queue && fd_queue->revents & POLLIN) {
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
