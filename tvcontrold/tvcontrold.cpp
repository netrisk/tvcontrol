#include <tc_log.h>
#include <tc_cec.h>
#include <tc_server.h>
#include <tc_cmd.h>
#include <tc_osd.h>
#include <tc_mouse.h>
#include <stdlib.h>
#include <unistd.h>
#include <config.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <signal.h>

void sig_handler(int signo)
{
	if (signo == SIGINT) {
		tc_log(TC_LOG_INFO, "Received SIGINT");
		tc_server_exit();
	} else if (signo == SIGTERM) {
		tc_log(TC_LOG_INFO, "Received SIGTERM");
		tc_server_exit();
	}
}

int main(void)
{
	/* Initialize the log */
	tc_log_init();
	tc_log(TC_LOG_INFO, "Starting tvcontrold");
	tc_log(TC_LOG_INFO, "main thread pid:%u", (unsigned)syscall(SYS_gettid));

	/* Capture the signals */
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		tc_log(TC_LOG_WARN, "Cannot capture SIGINT");
	if (signal(SIGTERM, sig_handler) == SIG_ERR)
		tc_log(TC_LOG_WARN, "Cannot capture SIGTERM");

	/* Go to the home path for loading data */
	const char *home = getenv("HOME");
	bool readhome = false;
	if (home) {
		if (!chdir(home))
			readhome = true;
	}
	if (!readhome)
		tc_log(TC_LOG_WARN, "Error detecting home path");

	/* Initialize every submodule */
	if (tc_cmd_init(readhome))
		return EXIT_FAILURE;
	#ifdef ENABLE_CEC
	if (tc_cec_init())
		return EXIT_FAILURE;
	#endif /* ENABLE_CEC */
	#ifdef ENABLE_OSD
	if (tc_osd_init())
		return EXIT_FAILURE;
	#endif /* ENABLE_OSD */
	if (tc_server_init())
		return -1;
	if (tc_mouse_init())
		return -1;

	/* Call the startup command */
	tc_log(TC_LOG_INFO, "Executing startup commands");
	tc_cmd("startup", 7);

	/* We have started */
	tc_log(TC_LOG_INFO, "Started tvcontrold");

	/* Serve commands */
	tc_server_exec();

	/* Release everything */
	tc_log(TC_LOG_INFO, "Closing tvcontrold");
	#ifdef ENABLE_CEC
	tc_cec_release();
	#endif /* ENABLE_CEC */
	tc_server_release();
	#ifdef ENABLE_OSD
	tc_osd_release();
	#endif /* ENABLE_OSD */
	tc_mouse_release();
	tc_cmd_release();
	tc_log(TC_LOG_INFO, "Closed tvcontrold");
	return EXIT_SUCCESS;
}
