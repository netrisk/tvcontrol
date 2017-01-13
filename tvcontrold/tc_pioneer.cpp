#include <tc_pioneer.h>
#include <tc_log.h>
#include <tc_server.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>

/* Define the following macro for debug */
/* #define TC_PIONEER_DEBUG */

/**
 *  Parse a feature to be on or off.
 *
 *  \param buf   Buffer with a 1 for off or 0 for on
 *  \param data  Data to be stored.
 *  \retval 0 on success, -1 on error.
 */
static int tc_pioneer_parse_bool(const char *buf, bool *data)
{
	char ch = *buf;
	if (ch == '0') {
		*data = true;
		return 0;
	} else if (ch == '1') {
		*data = false;
		return 0;
	} else
		return -1;
}

/**
 *  Parse a decimal character.
 *
 *  \param buf  Bufer to parse.
 *  \param data Data returned.
 *  \retval 0 on success, -1 on error.
 */
static int tc_pioneer_parse_dec1(const char *buf, uint8_t *data)
{
	char ch = buf[0];
	if (ch >= '0' && ch <= '9') {
		*data = ch - '0';
		return 0;
	}
	return -1;
}

/**
 *  Parse an hexadecimal character.
 *
 *  \param buf  Bufer to parse.
 *  \param data Data returned.
 *  \retval 0 on success, -1 on error.
 */
static int tc_pioneer_parse_hex1(const char *buf, uint8_t *data)
{
	char ch = buf[0];
	if (ch >= 'a' && ch <= 'f') {
		*data = ch - 'a' + 10;
		return 0;
	}
	if (ch >= 'A' && ch <= 'F') {
		*data = ch - 'A' + 10;
		return 0;
	}
	return tc_pioneer_parse_dec1(buf, data);
}

/**
 *  Parse an hexadecimal set of characters.
 *
 *  \param buf  Bufer to parse.
 *  \param data Data returned.
 *  \param n    Number of hexadecimal characters
 *  \retval 0 on success, -1 on error.
 */
static int tc_pioneer_parse_hexn(const char *buf, uint8_t *data, uint32_t n)
{
	uint32_t i;
	if (n & 1) {
		if (tc_pioneer_parse_hex1(buf, data))
			return -1;
		n--;
	}
	for (i = 0; i < n; i+=2, buf+=2) {
		uint8_t data1, data2;
		if (tc_pioneer_parse_hex1(buf, &data1) ||
		    tc_pioneer_parse_hex1(buf+1, &data2))
			return -1;
		*data++ = (data1 << 4) | data2;
	}
	return 0;
}

/**
 *  Parse a set of decimal characters
 *
 *  \param buf  Bufer to parse.
 *  \param data Data returned.
 *  \param n    Number of decimal characters
 *  \retval 0 on success, -1 on error.
 */
static int tc_pioneer_parse_decn(const char *buf, uint32_t *data, uint32_t n)
{
	uint32_t i;
	uint32_t result = 0;
	for (i = 0; i < n; i++, buf++) {
		result *= 10;
		uint8_t data1;
		if (tc_pioneer_parse_dec1(buf, &data1))
			return -1;
		result += data1;
	}
	*data = result;
	return 0;
}

/**
 *  Function called when a pioneer message has been received.
 *
 *  \param p    Pioneer object.
 *  \param buf  Reception buffer.
 *  \param len  Length of the received data.
 */
static void tc_pioneer_rx(tc_pioneer_t *p, const char *buf,
                          uint32_t len) 
{
	/* Check for the mute information */
	if (len == 4 && !strncmp(buf, "PWR", 3)) {
		if (!tc_pioneer_parse_bool(buf + 3, &p->pwr)) {
			#ifdef TC_PIONEER_DEBUG
			tc_log(TC_LOG_DEBUG, "pioneer: pwr: %s", p->pwr ? "on" : "off");
			#endif /* TC_PIONEER_DEBUG */
			if (p->lastcmd != TC_PIONEER_CMD_QUERY)
				tc_pioneer_send(p, TC_PIONEER_CMD_QUERY);
			return;
		}
	/* Check for the volume */
	} else if (len == 6 && !strncmp(buf, "VOL", 3)) {
		if (!tc_pioneer_parse_decn(buf + 3, &p->vol, 3)) {
			#ifdef TC_PIONEER_DEBUG
			tc_log(TC_LOG_DEBUG, "pioneer: volume: \"%u\"", p->vol);
			#endif /* TC_PIONEER_DEBUG */
			return;
		}
	/* Check for the on screen information */
	} else if (len == 32 && !strncmp(buf, "FL", 2)) {
		if (!tc_pioneer_parse_hexn(buf + 2, p->fl, 30)) {
			p->fl[15] = 0;
			#ifdef TC_PIONEER_DEBUG
			tc_log(TC_LOG_DEBUG, "pioneer: screen: \"%s\"", p->fl + 1);
			#endif /* TC_PIONEER_DEBUG */
			return;
		}
	/* Check for the on screen information */
	} else if (len == 4 && !strncmp(buf, "FN", 2)) {
		if (!tc_pioneer_parse_decn(buf + 2, &p->fn, 2)) {
			#ifdef TC_PIONEER_DEBUG
			tc_log(TC_LOG_DEBUG, "pioneer: input: \"%u\"", p->fn);
			#endif /* TC_PIONEER_DEBUG */
			const char *event = "on_pioneer_input_unknown";
			switch (p->fn) {
			case 2: event = "on_pioneer_input_tuner"; break;
			case 4: event = "on_pioneer_input_dvd"; break;
			case 5: event = "on_pioneer_input_tv"; break;
			case 6: event = "on_pioneer_input_sat"; break;
			}
			tc_server_event(event, strlen(event));
			return;
		}
	/* Check for the mute information */
	} else if (len == 4 && !strncmp(buf, "MUT", 3)) {
		bool newmute;
		if (!tc_pioneer_parse_bool(buf + 3, &newmute)) {
			#ifdef TC_PIONEER_DEBUG
			tc_log(TC_LOG_DEBUG, "pioneer: mute: %s", newmute ? "on" : "off");
			#endif /* TC_PIONEER_DEBUG */
			if (newmute != p->mute) {
				p->mute = newmute;
				if (p->mute_known) {
					char event[256];
					int n = snprintf(event, sizeof(event),
					                 "on_%s_%s", p->name, newmute ? "mute" : "unmute");
					tc_server_event(event, n);
				}
			}
			p->mute_known = true;
			return;
		}
	/* Check for MCACC information */
	} else if (len == 3 && !strncmp(buf, "MC", 2)) {
		if (!tc_pioneer_parse_dec1(buf + 2, &p->mc)) {
			#ifdef TC_PIONEER_DEBUG
			tc_log(TC_LOG_DEBUG, "pioneer: mcacc: %u", p->mc);
			#endif /* TC_PIONEER_DEBUG */
			return;
		}
	}
	/* If it is unknown or error */
	tc_log(TC_LOG_WARN, "pioneer: unknown rx: \"%s\", len:%u",
	       strndupa(buf, len), len);
}

/**
 *  Generate a transmission packet.
 * 
 *  \param p    Pioneer object.
 *  \param buf  Buffer to transmit with.
 *  \param len  Maximum length of the packet to transmit.
 *  \param cmd  Command to be executed
 *  \return The generated message length
 */
static uint32_t tc_pioneer_tx(tc_pioneer_t *p, char *buf, uint32_t len,
                              uint8_t cmd)
{
	/* Check the time of previous transmission */
	double inctime = 1.0;
	timespec newt;
	int r = clock_gettime(CLOCK_MONOTONIC, &newt);
	if (!r) {
		if (p->prev.tv_sec || p->prev.tv_nsec) {
			inctime = newt.tv_sec - p->prev.tv_sec;
			inctime += (newt.tv_nsec - p->prev.tv_nsec)*1e-9;
			#ifdef TC_PIONEER_DEBUG
			tc_log(TC_LOG_DEBUG, "pioneer: tx: time betwen %lf",
			       inctime);
			#endif /* TC_PIONEER_DEBUG */
		}
		p->prev = newt;
	}

	/* No acceleration if operation is not for volume */
	if (cmd != TC_PIONEER_CMD_VOLUMEDOWN &&
	    cmd != TC_PIONEER_CMD_VOLUMEUP)
		p->vol_accel = 0;

	/* Process it */
	const char *str = NULL;
	switch (cmd) {
	case TC_PIONEER_CMD_QUERY:
		str = "?P\r\n?V\r\n?M\r\n?MC\r\n";
		p->mute_known = false;
		break;
	case TC_PIONEER_CMD_POWERON: str = "PO\r\n"; break;
	case TC_PIONEER_CMD_STANDBY: str = "PF\r\n"; break;
	case TC_PIONEER_CMD_VOLUMEUP:   
	case TC_PIONEER_CMD_VOLUMEDOWN:  {
		if (inctime > 0.8)
			p->vol_accel = 0;
		if (inctime < 0.3) {
			if (cmd == TC_PIONEER_CMD_VOLUMEUP)
				p->vol_accel++;
			else
				p->vol_accel--;
		}
		if (cmd == TC_PIONEER_CMD_VOLUMEUP)
			p->vol_accel = p->vol_accel <= 0 ? 1 : p->vol_accel;
		else
			p->vol_accel = p->vol_accel >= 0 ? -1 : p->vol_accel;
		if (p->vol_accel > 8)
			p->vol_accel = 8;
		if (p->vol_accel < -8)
			p->vol_accel = -8;
		int32_t vol = p->vol + p->vol_accel;
		if (vol < 0)
			vol = 0;
		if (vol > 185)
			vol = 185;
		p->vol = vol;
		return snprintf(buf, len, "%03uVL\r\n", vol);
	}
	case TC_PIONEER_CMD_MUTEON:     str = "MO\r\n"; break;
	case TC_PIONEER_CMD_MUTEOFF:    str = "MF\r\n"; break;
	case TC_PIONEER_CMD_MUTE:       str = p->mute ? "MF\r\n" : "MO\r\n"; break;
	case TC_PIONEER_CMD_MCACC1:     str = "1MC\r\n"; break;
	case TC_PIONEER_CMD_MCACC2:     str = "2MC\r\n"; break;
	case TC_PIONEER_CMD_MCACC3:     str = "3MC\r\n"; break;
	case TC_PIONEER_CMD_MCACC4:     str = "4MC\r\n"; break;
	case TC_PIONEER_CMD_MCACC5:     str = "5MC\r\n"; break;
	case TC_PIONEER_CMD_MCACC6:     str = "6MC\r\n"; break;
	case TC_PIONEER_CMD_LISTENMODE_STEREO:    str = "0001SR\r\n"; break;
	case TC_PIONEER_CMD_LISTENMODE_EXTSTEREO: str = "0112SR\r\n"; break;
	case TC_PIONEER_CMD_LISTENMODE_DIRECT:    str = "0007SR\r\n"; break;
	case TC_PIONEER_CMD_LISTENMODE_ALC:       str = "0151SR\r\n"; break;
	case TC_PIONEER_CMD_LISTENMODE_EXPANDED:  str = "0106SR\r\n"; break;
	case TC_PIONEER_CMD_INPUT_TUNER:          str = "02FN\r\n"; break;
	case TC_PIONEER_CMD_INPUT_DVD:            str = "04FN\r\n"; break;
	case TC_PIONEER_CMD_INPUT_TV:             str = "05FN\r\n"; break;
	case TC_PIONEER_CMD_INPUT_SAT:            str = "06FN\r\n"; break;
	}
	if (str) {
		memcpy(buf, str, strlen(str));
		return strlen(str);
	}
	return 0;
}

/**
 *  Execute the pioneer thread.
 *
 *  \param arg  Argument for the pioneer thread.
 *  \return NULL
 */
static void *tc_pioneer_exec(void *arg)
{
	tc_log(TC_LOG_INFO, "pioneer: thread pid:%u", (unsigned)syscall(SYS_gettid));

	/* Forevery try to connect and process */
	uint8_t cmd = TC_PIONEER_CMD_NONE;
	char rx_buf[256];
	uint32_t rx_len = 0;
	char tx_buf[256];
	uint32_t tx_len = 0;
	int fd = -1;
	bool connected;
	tc_pioneer_t *p = (tc_pioneer_t *)arg;
	while (true) {
		/* Try to create the socket */
		if (fd == -1) {
			fd = socket(PF_INET, SOCK_STREAM, 0);
			if (fd == -1) {
				tc_log(TC_LOG_ERR, "pioneer: Error creating the socket");
				return NULL;
			}
			connected = false;
		}
		/* Try to connect the socket */
		if (!connected) {
			tc_log(TC_LOG_INFO, "pioneer: connecting to \"%s\"",
			       p->host);
			struct addrinfo *res = NULL;
			int r = getaddrinfo(p->host, "telnet", NULL, &res);
			if (!r && res) {
				r = connect(fd, res->ai_addr, res->ai_addrlen);
				if (!r) {
					connected = true;
					tc_log(TC_LOG_INFO, "pioneer: connected to \"%s\"",
					       p->host);
				} else {
					tc_log(TC_LOG_ERR, "pioneer: connection error");
					sleep(3);
				}
			} else {
				tc_log(TC_LOG_ERR, "pioneer: unknown host");
				sleep(3);
			}
			freeaddrinfo(res);
		}
		if (!connected)
			continue;
		/* Try to process all the received data */
		while (true) {
			uint32_t i;
			for (i = 0; i < rx_len; i++)
				if (rx_buf[i] == '\r' || rx_buf[i] == '\n')
					break;
			if (i == rx_len)
				break;
			if (i) {
				#ifdef TC_PIONEER_DEBUG
				tc_log(TC_LOG_DEBUG, "pioneer: rx: \"%s\", len:%u",
        			       strndupa(rx_buf, i), i);
				#endif /* TC_PIONEER_DEBUG */
				tc_pioneer_rx(p, rx_buf, i);
			}
			i++;
			rx_len -= i;
			memmove(rx_buf, rx_buf + i, rx_len);
			i = 0;
		}
		if (rx_len == sizeof(rx_buf))
			rx_len = 0;
		/* Try to generate new output */
		if (!tx_len && cmd != TC_PIONEER_CMD_NONE) {
			tx_len = tc_pioneer_tx(p, tx_buf, sizeof(tx_buf), cmd);
			#ifdef TC_PIONEER_DEBUG
			if (tx_len)
				tc_log(TC_LOG_DEBUG, "pioneer: tx: \"%s\", len:%u",
        			       strndupa(tx_buf, tx_len), tx_len);
			#endif /* TC_PIONEER_DEBUG */
			p->lastcmd = cmd;
			cmd = TC_PIONEER_CMD_NONE;
		}
		/* Process the connection in the poll */
		struct pollfd fds[2];
		fds[0].fd = fd;
		fds[0].events = (rx_len < sizeof(rx_buf) ? POLLIN : 0) |
		                (tx_len ? POLLOUT : 0) |
		                POLLHUP;
		fds[1].fd = p->pipe[0];
		fds[1].events = (cmd == TC_PIONEER_CMD_NONE) ? POLLIN : 0;
		int r = poll(fds, 2, -1);
		if (rx_len < sizeof(rx_buf) && (fds[0].revents & (POLLIN|POLLHUP))) {
			/* Read data from the socket */
			int r = read(fd, rx_buf + rx_len,
			             sizeof(rx_buf) - rx_len);
			if (r <= 0) {
				if (r < 0)
					tc_log(TC_LOG_ERR, "pioneer: Reception error");
				else
					tc_log(TC_LOG_ERR, "pioneer: Socket closed");
				close(fd);
				fd = -1;
				continue;
			}
			rx_len += r;
		}
		if (tx_len && (fds[0].revents & POLLOUT)) {
			int r = write(fd, tx_buf, tx_len);
			if (r <= 0) {
				tc_log(TC_LOG_ERR, "pioneer: Transmission error");
				close(fd);
				fd = -1;
				continue;
			}
			tx_len -= r;
			memmove(tx_buf, tx_buf + r, tx_len);
		}
		if (cmd == TC_PIONEER_CMD_NONE && (fds[1].revents & POLLIN)) {
			int r = read(p->pipe[0], &cmd, 1);
			if (r <= 0) {
				tc_log(TC_LOG_ERR, "pioneer: Pipe error");
				continue;
			}
		}
	}
	return NULL;
}

int tc_pioneer_init(tc_pioneer_t *pioneer,
                    const char *host,
                    uint32_t hostlen,
                    const char *name,
                    uint32_t namelen)
{
	memset(pioneer, 0, sizeof(tc_pioneer_t));
	pioneer->name = strndup(name, namelen);
	pioneer->host = strndup(host, hostlen);
	if (pipe(pioneer->pipe)) {
		tc_log(TC_LOG_ERR, "pioneer: Error creating the pipe");
		return -1;
	}
	int r = pthread_create(&pioneer->thread, NULL,
	                       tc_pioneer_exec, pioneer);
	if (r < 0) {
		tc_log(TC_LOG_ERR, "pioneer: Error creating thread");
		return -1;
	}
	tc_pioneer_send(pioneer, TC_PIONEER_CMD_QUERY);
	return 0;
}

int tc_pioneer_send(tc_pioneer_t *pioneer, uint8_t cmd)
{
	#ifdef TC_PIONEER_DEBUG
	tc_log(TC_LOG_DEBUG, "pioneer: send: %u", cmd);
	#endif /* TC_PIONEER_DEBUG */
	int r = write(pioneer->pipe[1], &cmd, 1);
	if (r < 1) {
		tc_log(TC_LOG_ERR, "pioneer: Error communicating through pipe");
		return -1;
	}
	return 0;
}
