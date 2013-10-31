#include <tc_log.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

static timespec tc_log_time_base;
static bool     tc_log_time_valid;
static pthread_mutex_t tc_log_mutex = PTHREAD_MUTEX_INITIALIZER;

void tc_log_init(void)
{
	int r = clock_gettime(CLOCK_MONOTONIC, &tc_log_time_base);
	if (r) {
		tc_log(TC_LOG_WARN, "Error initializing log timming");
		tc_log_time_valid = false;
	} else
		tc_log_time_valid = true;
}

void tc_log(uint8_t severity, const char *fmt, ...)
{
	// Lock the mutex
	pthread_mutex_lock(&tc_log_mutex);

	// Show the time
	if (tc_log_time_valid) {
		timespec t;
		int r = clock_gettime(CLOCK_MONOTONIC, &t);
		if (!r) {
			double time;
			time = t.tv_sec;
			time += t.tv_nsec * 1e-9;
			time -= tc_log_time_base.tv_sec;
			time -= tc_log_time_base.tv_nsec * 1e-9;
			fprintf(stderr, "[%.2lf] ", time);
		}
	}

	// Show the severity
	const char *sev = "(invalid)";
	switch(severity) {
	case TC_LOG_ERR:   sev = "ERROR: "; break;
	case TC_LOG_WARN:  sev = "WARN:  "; break;
	case TC_LOG_INFO:  sev = "INFO:  "; break;
	case TC_LOG_DEBUG: sev = "DEBUG: "; break;
	}
	fprintf(stderr, "%s", sev);

	// Show the formatted part
	va_list list;
	va_start(list, fmt);
	vfprintf(stderr, fmt, list);
	va_end(list);

	// New line feed
	fprintf(stderr, "\n");
	
	// Unlock the mutex
	pthread_mutex_unlock(&tc_log_mutex);
}
