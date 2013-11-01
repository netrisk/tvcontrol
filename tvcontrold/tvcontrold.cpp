#include <tc_log.h>
#include <tc_cec.h>
#include <tc_server.h>
#include <tc_cmd.h>
#include <tc_osd.h>
#include <stdlib.h>
#include <unistd.h>
#include <config.h>

int main(void)
{
	/* Initialize the log */
	tc_log_init();
	tc_log(TC_LOG_INFO, "Starting tvcontrold");

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
	if (tc_osd_init())
		return EXIT_FAILURE;
	if (tc_server_init())
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
	tc_log(TC_LOG_INFO, "Closed tvcontrold");
	return EXIT_SUCCESS;
}
