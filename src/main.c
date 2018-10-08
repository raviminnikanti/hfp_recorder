/*
 * main.c
 */

#include "main.h"

/* TODO: implement commandline arg parser */
int main(int argc, char *argv[])
{

	l_log_set_syslog();

	l_log_set_stderr();

	if (!l_main_init()) {
		l_error("Unable to create main_loop");
		exit(EXIT_FAILURE);
	}

	dbus_init();

	l_main_run();

	/* cleanup after mainloop complete. */
	l_main_exit();

	return 0;
}
