/*
 * socket.c
 */

#include "main.h"

static void io_disconnect_callback(struct l_io *io, void *user_data)
{
	l_info("socket disconnected");
}

/* if returned false handler will be destroyed and
 * no more called.
 */
static bool io_read_callback(struct l_io *io, void *user_data)
{
	char buffer[MAX_DATA_BUF_SIZE];
	int fd = l_io_get_fd(io);
	ssize_t bytes_read;

	bytes_read = read(fd, buffer, MAX_DATA_BUF_SIZE);
	if (bytes_read < 0) {
		l_error("socket read error: %s", strerror(errno));
		return false;
	}

	return true;
}

void new_rfcomm_connection(int sock)
{

	struct l_io *io = l_io_new(sock);
	if (!io) {
		/* returns NULL in case failed to add watch.
		 * Any memory allocation failure causes the application
		 * to abort.
		 */
		l_error("failed to add io watch on RFCOMM connection");
	}
	l_io_set_close_on_destroy(io, true);
	l_io_set_read_handler(io, io_read_callback, NULL, NULL);
	l_io_set_disconnect_handler(io, io_disconnect_callback, NULL, NULL);
}

/* We play HF role. We only accept SCO connections */

/* Releasing a “Service Level Connection” shall also release
 * any existing “Audio Connection” related to it.
 */
