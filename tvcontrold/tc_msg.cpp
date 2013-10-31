#include <tc_msg.h>
#include <tc_tools.h>
#include <tc_log.h>
#include <unistd.h>

int tc_msg_queue_create(tc_msg_queue_t *queue)
{
	if (pipe(queue->fd)) {
		tc_log(TC_LOG_ERR, "Error creating the queue");
		return -1;
	}
	return 0;
}

void tc_msg_queue_close(tc_msg_queue_t *queue)
{
	if (queue->fd[0] != -1) {
		close(queue->fd[0]);
		queue->fd[0] = -1;
	}
	if (queue->fd[1] != -1) {
		close(queue->fd[1]);
		queue->fd[1] = -1;
	}
}

int tc_msg_recv(tc_msg_queue_t *queue, tc_msg_t *msg)
{
	/* We have received an event */
	int r = read(queue->fd[0], &msg->len, 1);
	if (r < 1)
		return -1;
	if (tc_read_all(queue->fd[0], msg->buf, msg->len))
		return -1;
	msg->buf[msg->len] = 0;
	return 0;
}

int tc_msg_send(tc_msg_queue_t *queue, const void *buf, uint8_t len)
{
	if (tc_write_all(queue->fd[1], &len, 1))
		return -1;
	if (tc_write_all(queue->fd[1], buf, len))
		return -1;
	return 0;
}
