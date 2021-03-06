#include <tc_cmd.h>
#include <tc_cec.h>
#include <tc_log.h>
#include <tc_pioneer.h>
#include <tc_osd.h>
#include <tc_mouse.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <config.h>

/* Define the following macro to debug */
/* #define TC_CMD_DEBUG */


/* --- Generic objects and functions por processing ----------------------- */

/**
 *  Object that represents a command.
 */
typedef struct tc_cmd_t {
	const char *name;
	int (*exec)(tc_cmd_t *cmd, const char *buf, uint32_t len);
	int (*extend)(tc_cmd_t *cmd, const char *buf, uint32_t len);
	void (*free)(tc_cmd_t *cmd);
	tc_cmd_t *next;
} tc_cmd_t;

/** Command to be extended if any */
static tc_cmd_t *tc_cmd_extend = NULL;

/**
 *  Get the length of the next word.
 *
 *  \param buf  Buffer to get the first word.
 *  \param len  Length of the buffer.
 *  \return The length of the first word.
 */
static uint32_t tc_cmd_wordlen(const char *buf, uint32_t len)
{
	uint32_t r;
	for (r = 0; r < len; r++)
		if (buf[r] == ' ')
			return r;
	return len;
}

/**
 *  Remove the first word.
 *
 *  \param buf  Buffer to remove the first word.
 *  \param len  Length of the first word.
 */
static void tc_cmd_wordrm(const char **buf, uint32_t *len)
{
	uint32_t r;
	for (r = 0; r + 1 < *len; r++)
		if ((*buf)[r] == ' ') {
			*buf += r + 1;
			*len -= r + 1;
			return;
		}
	*buf = NULL;
	*len = 0;
}

/**
 *  Check if a given buffer starts with a string and space or nothing.
 *
 *  \param buf    Input and output parameer with the initial and final buffer.
 *  \param len    Input and output parameter with the initial and final buffer.
 *  \param start  Starting to check if it matches and to remove if so.
 *  \retval true if it mathes, false otherwise.
 */
static bool tc_cmd_starts(const char **buf, uint32_t *len, const char *start)
{
	int start_len = strlen(start);
	if (*len < start_len)
		return false;
	if (memcmp(*buf, start, start_len))
		return false;
	if (*len == start_len) {
		*buf = NULL;
		*len = 0;
	} else if ((*buf)[start_len] != ' ')
		return false;
	else {
		*buf += start_len + 1;
		*len -= start_len + 1;
	}
	return true;
}

/**
 *  Check if a buffer is just the content of the string.
 *
 *  \param buf   Buffer to check.
 *  \param len   Length of the buffer to check.
 *  \param str   String to check.
 *  \return true if it matches.
 *  \return false if it doesn't match.
 */
static bool tc_cmd_is(const char *buf, uint32_t len, const char *str)
{
	int str_len = strlen(str);
	if (len != str_len)
		return false;
	return !memcmp(buf, str, len);
}

/* --- Environment management functions ---------------------------------- */

/** Environment entry */
typedef struct tc_cmd_env_t {
	const char *name;
	const char *value;
	tc_cmd_env_t *next;
} tc_cmd_env_t;

/** Environment for command execution */
static tc_cmd_env_t *tc_cmd_env = NULL;

/**
 *  Find the environment variable structure.
 *
 *  \param name     Name of the environment variable.
 *  \param namelen  Length of the name of the variable.
 *  \retval NULL if not found.
 *  \retval A pointer to the environment structure if found.
 */
static tc_cmd_env_t *tc_cmd_env_find(const char *name, uint32_t namelen)
{
	tc_cmd_env_t *e = tc_cmd_env;
	while (e) {
		if (strlen(e->name) == namelen &&
		    !strncmp(e->name, name, namelen))
			return e;
		e = e->next;
	}
	return NULL;
}

/**
 *  Check if a character is valid for an environment name
 *
 *  \param ch  Character
 *  \retval true if it is valid
 *  \retval false if it is not valid
 */
static bool tc_cmd_env_ischar(char ch)
{
	return isalnum(ch) || ch == '_';
}

int tc_cmd_env_set(const char *name,  uint32_t namelen,
                   const char *value, uint32_t valuelen)
{
	/* Validate the name */
	uint32_t i = 0;
	for (i = 0; i < namelen; i++) {
		if (!tc_cmd_env_ischar(name[i]))
			break;
	}
	if (i < namelen || namelen == 0) {
		tc_log(TC_LOG_ERR, "Invalid name for variable \"%s\"",
		       strndupa(name, namelen));
		return -1;
	}

	/* Replace an existing value if found */
	tc_cmd_env_t *e = tc_cmd_env_find(name, namelen);
	if (e) {
		free((void *)e->value);
		e->value = strndup(value, valuelen);
		tc_log(TC_LOG_INFO, "Variable %s = \"%s\" (replaced value)",
		       strndupa(name, namelen), strndupa(value, valuelen));
		return 0;
	}

	/* Add the new value if not found */
	e = (tc_cmd_env_t *)malloc(sizeof(tc_cmd_env_t));
	e->name = strndup(name, namelen);
	e->value = strndup(value, valuelen);
	e->next = tc_cmd_env;
	tc_cmd_env = e;
	tc_log(TC_LOG_INFO, "Variable %s = \"%s\" (new value)",
	       strndupa(name, namelen), strndupa(value, valuelen));
	return 0;
}

/**
 *  Check if the CSV scaping is required.
 *
 *  \param text  Text to check if scaping is required.
 *  \return True if scape required.
 */
static bool tc_cmd_env_csv_scape_required(const char *text)
{
	while (true) {
		char c = *text++;
		if (!c)
			return false;
		if (c == ' ' || c == '\t' || c == ',' || c == '\n' || c == '\"')
			return true;
	}
}

/**
 *  Get the length of a CSV value after being scaped.
 *
 *  \param text  Text to be scaped.
 *  \return The number of characters after being scaped.
 */
static uint32_t tc_cmd_env_csv_scape_len(const char *text)
{
	uint32_t len = 0;
	bool scape = tc_cmd_env_csv_scape_required(text);
	if (scape)
		len++;
	uint32_t i = 0;
	while (true) {
		char c = *text++;
		if (!c)
			break;
		if (c == '\"')
			len++;
		len++;
	}
	if (scape)
		len++;
	return len;
}

/**
 *  Write an scaped text into the output.
 *
 *  \param output  Output buffer (that should have space).
 *  \param text    Text to scape as CSV.
 *  \return New output pointer advanced.
 */
static char *tc_cmd_env_csv_scape(char *output, const char *text)
{
	bool scape = tc_cmd_env_csv_scape_required(text);
	if (scape)
		*output++ = '\"';
	uint32_t i = 0;
	while (true) {
		char c = *text++;
		if (!c)
			break;
		if (c == '\"')
			*output++ = '\"';
		*output++ = c;
	}
	if (scape)
		*output++ = '\"';
	return output;
}

const char *tc_cmd_env_csv(void)
{
	/* Debug */
	#ifdef TC_CMD_DEBUG
	tc_log(TC_LOG_DEBUG, "tc_cmd: env_csv");
	#endif /* TC_CMD_DEBUG */

	/* Calculate tht length */
	uint32_t len = 1;
	tc_cmd_env_t *e = tc_cmd_env;
	while (e) {
		len += tc_cmd_env_csv_scape_len(e->name);
		len++;
		len += tc_cmd_env_csv_scape_len(e->value);
		len++;
		e = e->next;
	}

	/* Allocate the output */
	#ifdef TC_CMD_DEBUG
	tc_log(TC_LOG_DEBUG, "tc_cmd: env_csv: alloc:%u", (unsigned)len);
	#endif /* TC_CMD_DEBUG */
	char *output = (char *)malloc(len);

	/* Write each entry of the output */
	char *o = output;
	e = tc_cmd_env;
	while (e) {
		o = tc_cmd_env_csv_scape(o, e->name);
		*o++ = ',';
		o = tc_cmd_env_csv_scape(o, e->value);
		*o++ = '\n';
		e = e->next;
	}

	/* Write the end of text */
	*o++ = 0;

	/* Debug */
	#ifdef TC_CMD_DEBUG
	tc_log(TC_LOG_DEBUG, "tc_cmd: env_csv: finished");
	#endif /* TC_CMD_DEBUG */

	/* Return the output */
	return output;
}

/**
 *  Create a new buffer with the replaced environment values.
 * 
 *  \param buf  Buffer with values to be replaced.
 *  \param len  Length of original buffer and output of final.
 *  \return The allocated buffer that should be freed with free.
 */
static char *tc_cmd_env_subs(const char *buf, uint32_t *len)
{
	uint8_t state = 0;
	uint32_t l = 0;
	uint32_t alloc = 0;
	char *result = NULL;
	uint32_t i = 0;
	bool finished = false;
	while (true) {
		if (l == alloc) {
			alloc = alloc ? (alloc << 1) : 16;
			result = (char *)realloc(result, alloc);
		}
		if (i == *len) {
			finished = true;
			break;
		}
		if (buf[i] == '$') {
			i++;
			if (i == *len)  {
				tc_log(TC_LOG_ERR, "Syntax error: $ at end of command");
				break;
			}
			if (buf[i] != '$') {
				uint32_t endi;
				for (endi = i; endi < *len; endi++)
					if (!tc_cmd_env_ischar(buf[endi]))
						break;
				if (endi == i) { 
					tc_log(TC_LOG_ERR, "Syntax error: $ not followed by variable name");
					break;
				}
				tc_cmd_env_t *env = tc_cmd_env_find(buf + i, endi - i);
				if (!env) {
					tc_log(TC_LOG_ERR, "Syntax error: Variable \"%s\" not found",
					       strndupa(buf + i, endi - i));
					break;
				}
				uint32_t newl = l + strlen(env->value);
				while (alloc < newl) {
					alloc = alloc << 1;
					result = (char *)realloc(result, alloc);
				}
				memcpy(result + l, env->value, strlen(env->value));
				l = newl;
				i = endi;
				continue;
			}
		}
		result[l++] = buf[i++];
	}
	*len = l;
	if (!finished) {
		if (result) {
			free(result);
			result = NULL;
		}
	}
	return result;
}


/* --- List of commands registered --------------------------------------- */

/**
 *  List of commands to be able to process.
 */
static tc_cmd_t *tc_cmd_first = NULL;

/**
 *  Add a new command to the list of available commands.
 *
 *  \param cmd  Command to be added.
 */
static void tc_cmd_add(tc_cmd_t *cmd)
{
	cmd->next = tc_cmd_first;
	tc_cmd_first = cmd;
	#ifdef TC_CMD_DEBUG
	tc_log(TC_LOG_DEBUG, "cmd: add: \"%s\"", cmd->name);
	#endif /* TC_CMD_DEBUG */
}


/* --- Native command list ----------------------------------------------- */

/**
 *  Execute the exit command.
 *
 *  \param cmd   Pointer to the command to execute
 *  \param buf   Buffer with the name of the command to execute
 *  \param len   Length of the command to execute.
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_exit_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	return len ? -1 : 1;
}

/** Exit command object */
static tc_cmd_t tc_cmd_exit = {
	.name = "exit",
	.exec = tc_cmd_exit_exec
};

/**
 *  Execute the set command.
 *
 *  \param cmd   Pointer to the command to execute
 *  \param buf   Buffer with the name of the command to execute
 *  \param len   Length of the command to execute.
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_set_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	const char *w = buf;
	uint32_t wl = tc_cmd_wordlen(buf, len);
	tc_cmd_wordrm(&buf, &len);
	return tc_cmd_env_set(w, wl, buf, len);
}

/** Exit command object */
static tc_cmd_t tc_cmd_set = {
	.name = "set",
	.exec = tc_cmd_set_exec
};

#ifdef ENABLE_OSD
/**
 *  Execute a OSD command.
 *
 *  \param cmd   Pointer to the command to execute
 *  \param buf   Buffer with the name of the command to execute
 *  \param len   Length of the command to execute.
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_osd_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	/* svg ... */
	if (tc_cmd_starts(&buf, &len, "svg")) {
		tc_osd_svg(buf, len);
		return 0;
	} else if (tc_cmd_starts(&buf, &len, "png")) {
		tc_osd_png(buf, len);
		return 0;
	}
	return -1;
}

/** OSD command object */
static tc_cmd_t tc_cmd_osd = {
	.name = "osd",
	.exec = tc_cmd_osd_exec
};
#endif /* ENABLE_OSD */

#ifdef ENABLE_CEC
/**
 *  Execute a CEC command.
 *
 *  \param cmd   Pointer to the command to execute
 *  \param buf   Buffer with the name of the command to execute
 *  \param len   Length of the command to execute.
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_cec_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	/* poweron ... */
	if (tc_cmd_starts(&buf, &len, "poweron")) {
		/* poweron all */
		if (tc_cmd_is(buf, len, "all")) {
			tc_cec_poweron_all();
			return 0;
		}
	/* standby ... */
	} else if (tc_cmd_starts(&buf, &len, "standby")) {
		/* poweroff all */
		if (tc_cmd_is(buf, len, "all")) {
			tc_cec_standby_all();
			return 0;
		}
	/* setactive */
	} else if (tc_cmd_is(buf, len, "setactive")) {
		tc_cec_setactive();
		return 0;
	/* volumeup */
	} else if (tc_cmd_is(buf, len, "volumeup")) {
		tc_cec_volumeup();
		return 0;
	/* volumedown */
	} else if (tc_cmd_is(buf, len, "volumedown")) {
		tc_cec_volumedown();
		return 0;
	/* mute */
	} else if (tc_cmd_is(buf, len, "mute")) {
		tc_cec_mute();
		return 0;
	}
	return -1;
}

/** CEC command object */
static tc_cmd_t tc_cmd_cec = {
	.name = "cec",
	.exec = tc_cmd_cec_exec
};
#endif /* ENABLE_CEC */

/** Pionner object type */
typedef struct tc_cmd_pioneer_t {
	tc_cmd_t cmd;
	tc_pioneer_t pioneer;
} tc_cmd_pioneer_t;

/**
 *  Execute a pioneer command.
 *
 *  \param cmd   Pointer to the command to execute
 *  \param buf   Buffer with the name of the command to execute
 *  \param len   Length of the command to execute.
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_pioneer_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	tc_cmd_pioneer_t *p = tc_containerof(cmd, tc_cmd_pioneer_t, cmd);
	uint8_t c = TC_PIONEER_CMD_NONE;
	if (tc_cmd_is(buf, len, "poweron"))
		c = TC_PIONEER_CMD_POWERON;
	else if (tc_cmd_is(buf, len, "standby"))
		c = TC_PIONEER_CMD_STANDBY;
	else if (tc_cmd_is(buf, len, "volumeup"))
		c = TC_PIONEER_CMD_VOLUMEUP;
	else if (tc_cmd_is(buf, len, "volumedown"))
		c = TC_PIONEER_CMD_VOLUMEDOWN;
	else if (tc_cmd_is(buf, len, "muteon"))
		c = TC_PIONEER_CMD_MUTEON;
	else if (tc_cmd_is(buf, len, "muteoff"))
		c = TC_PIONEER_CMD_MUTEOFF;
	else if (tc_cmd_is(buf, len, "mute"))
		c = TC_PIONEER_CMD_MUTE;
	else if (tc_cmd_starts(&buf, &len, "mcacc")) {
		if (tc_cmd_is(buf, len, "1"))
			c = TC_PIONEER_CMD_MCACC1;
		else if (tc_cmd_is(buf, len, "2"))
			c = TC_PIONEER_CMD_MCACC2;
		else if (tc_cmd_is(buf, len, "3"))
			c = TC_PIONEER_CMD_MCACC3;
		else if (tc_cmd_is(buf, len, "4"))
			c = TC_PIONEER_CMD_MCACC4;
		else if (tc_cmd_is(buf, len, "5"))
			c = TC_PIONEER_CMD_MCACC5;
		else if (tc_cmd_is(buf, len, "6"))
			c = TC_PIONEER_CMD_MCACC6;
	} else if (tc_cmd_starts(&buf, &len, "listenmode")) {
		if (tc_cmd_is(buf, len, "stereo"))
			c = TC_PIONEER_CMD_LISTENMODE_STEREO;
		else if (tc_cmd_is(buf, len, "extstereo"))
			c = TC_PIONEER_CMD_LISTENMODE_EXTSTEREO;
		else if (tc_cmd_is(buf, len, "direct"))
			c = TC_PIONEER_CMD_LISTENMODE_DIRECT;
		else if (tc_cmd_is(buf, len, "alc"))
			c = TC_PIONEER_CMD_LISTENMODE_ALC;
		else if (tc_cmd_is(buf, len, "expanded"))
			c = TC_PIONEER_CMD_LISTENMODE_EXPANDED;
	} else if (tc_cmd_starts(&buf, &len, "input")) {
		if (tc_cmd_is(buf, len, "tuner"))
			c = TC_PIONEER_CMD_INPUT_TUNER;
		else if (tc_cmd_is(buf, len, "dvd"))
			c = TC_PIONEER_CMD_INPUT_DVD;
		else if (tc_cmd_is(buf, len, "tv"))
			c = TC_PIONEER_CMD_INPUT_TV;
		else if (tc_cmd_is(buf, len, "sat"))
			c = TC_PIONEER_CMD_INPUT_SAT;
	}
	if (c == TC_PIONEER_CMD_NONE)
		return -1;
	return tc_pioneer_send(&p->pioneer, c);
}

/**
 *  Free the pioneer object.
 *
 *  \param cmd  Command
 */
static void tc_cmd_pioneer_free(tc_cmd_t *cmd)
{
	tc_cmd_pioneer_t *p = tc_containerof(cmd, tc_cmd_pioneer_t, cmd);
	tc_pioneer_release(&p->pioneer);
	free((void *)p->cmd.name);
	free(p);
}

/**
 *  Function to initialize a new pioneer object.
 * 
 *  \param buf  Buffer with the initialization data.
 *  \param len  Length of the initialization data.
 */
static int tc_cmd_pioneer_init(const char *buf, uint32_t len)
{
	/* Parse the input */
	const char *name = buf;
	uint32_t wl = tc_cmd_wordlen(buf, len);
	tc_cmd_wordrm(&buf, &len);
	if (wl < 1 || len < 1)
		return -1;
	/* Create the new objecct */
	tc_cmd_pioneer_t *p = (tc_cmd_pioneer_t *)malloc(sizeof(tc_cmd_pioneer_t));
	memset(p, 0, sizeof(p));
	if (tc_pioneer_init(&p->pioneer, buf, len, name, wl)) {
		free(p);
		return -1;
	}
	p->cmd.name = strndup(name, wl);
	p->cmd.exec = tc_cmd_pioneer_exec;
	p->cmd.free = tc_cmd_pioneer_free;
	tc_cmd_add(&p->cmd);
	return 0;
}

/**
 *  Execute a Mouse command.
 *
 *  \param cmd   Pointer to the command to execute
 *  \param buf   Buffer with the name of the command to execute
 *  \param len   Length of the command to execute.
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_mouse_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	/* svg ... */
	if (tc_cmd_starts(&buf, &len, "move")) {
		tc_mouse_move();
		return 0;
	}
	return -1;
}

/** OSD command object */
static tc_cmd_t tc_cmd_mouse = {
	.name = "mouse",
	.exec = tc_cmd_mouse_exec
};


/* --- Scripting commands ------------------------------------------------- */

/**
 *  Entry for the scripting of the command.
 */
typedef struct tc_cmd_script_entry_t {
	const char *cmd;
	tc_cmd_script_entry_t *next;
} tc_cmd_script_entry_t;

/**
 *  Scripting command obect.
 */
typedef struct tc_cmd_script_t {
	tc_cmd_t cmd;
	tc_cmd_script_entry_t *entry;
} tc_cmd_script_t;

/**
 *  Internal function to execute an scripting command.
 *
 *  \param cmd  Pointer to the command to execute.
 *  \param buf  Buffer with the command to execute.
 *  \param len  Length of the command to execute
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_script_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	tc_cmd_script_t *s = tc_containerof(cmd, tc_cmd_script_t, cmd);
	tc_cmd_script_entry_t *e = s->entry;
	while (e) {
		tc_log(TC_LOG_INFO, "Subcommand \"%s\"", e->cmd);
		int r = tc_cmd(e->cmd, strlen(e->cmd));
		if (r) {
			if (r < 0)
				tc_log(TC_LOG_ERR, "Error in subcommand \"%s\"",
				       e->cmd);
			return r;
		}
		e = e->next;
	}
	return 0;
}

/**
 *  Called to extend the given command.
 *
 *  \param cmd  Command to be extended.
 *  \param buf  Buffer with the data.
 *  \param len  Length of the data to extend.
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_script_extend(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	tc_cmd_script_t *s = tc_containerof(cmd, tc_cmd_script_t, cmd);
	tc_cmd_script_entry_t **e = &s->entry;
	while (*e)
		e = &(*e)->next;
	tc_cmd_script_entry_t *ne;
	ne = (tc_cmd_script_entry_t *)malloc(sizeof(tc_cmd_script_entry_t));
	memset(ne, 0, sizeof(tc_cmd_script_entry_t));
	ne->cmd = strndup(buf, len);
	*e = ne;
	return 0;
}

/**
 *  Called to free the script command.
 *
 *  \param cmd  Command to be freed.
 */
static void tc_cmd_script_free(tc_cmd_t *cmd)
{
	tc_cmd_script_t *s = tc_containerof(cmd, tc_cmd_script_t, cmd);
	while (s->entry) {
		tc_cmd_script_entry_t *e = s->entry;
		s->entry = e->next;
		free((void *)e->cmd);
		free(e);
	}
	free((void *)s->cmd.name);
	free(s);
}

/**
 *  Initialize the script command.
 *
 *  \param buf   Buffer to initialize it with.
 *  \param len   Length of the command to execute.
 */
static int tc_cmd_script_init(const char *buf, uint32_t len)
{
	tc_cmd_script_t *cmd = (tc_cmd_script_t *)malloc(sizeof(tc_cmd_script_t));
	memset(cmd, 0, sizeof(tc_cmd_script_t));
	cmd->cmd.name = strndup(buf, len);
	cmd->cmd.exec = &tc_cmd_script_exec;
	cmd->cmd.extend = &tc_cmd_script_extend;
	cmd->cmd.free = &tc_cmd_script_free;
	tc_cmd_add(&cmd->cmd);
	tc_cmd_extend = &cmd->cmd;
	return 0;
}


/* --- Process execution commands ---------------------------------------- */

/**
 *  Execute an process command
 *
 *  \param cmd  Pointer to the command to execute.
 *  \param buf  Buffer with the command to execute.
 *  \param len  Length of the command to execute
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_exec_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	/* exec <process> <arguments> */
	char *command = (char *)malloc(len + 1);
	memcpy(command, buf, len);
	command[len] = 0;
	tc_log(TC_LOG_INFO, "Executing command: \"%s\"", command);
	int r = system(command);
	return r ? -1 : 0;
}

/** Init command object */
static tc_cmd_t tc_cmd_exec_cmd = {
	.name = "exec",
	.exec = tc_cmd_exec_exec
};


/* --- Initialization commands -------------------------------------------- */

/**
 *  Execute an initialization command
 *
 *  \param cmd  Pointer to the command to execute.
 *  \param buf  Buffer with the command to execute.
 *  \param len  Length of the command to execute
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
static int tc_cmd_init_exec(tc_cmd_t *cmd, const char *buf, uint32_t len)
{
	/* init script <name> */
	if (tc_cmd_starts(&buf, &len, "script"))
		return tc_cmd_script_init(buf, len);
	/* init pioneer <name> <host> */
	else if (tc_cmd_starts(&buf, &len, "pioneer"))
		return tc_cmd_pioneer_init(buf, len);
	return -1;
}

/** Init command object */
static tc_cmd_t tc_cmd_init_cmd = {
	.name = "init",
	.exec = tc_cmd_init_exec
};


/* --- Main API for commands ---------------------------------------------- */

/**
 *  Load a file to add new commands 
 *
 *  \param path   Path of the file with the new commands.
 *  \retval -1 on error (with a log entry).
 *  \retval 1 on file not possible to be read.
 *  \retval 0 on success.
 */
static int tc_cmd_load(const char *path)
{
	/* Try to open the file */
	FILE *f = fopen(path, "rt");
	if (!f) {
		tc_log(TC_LOG_WARN, "Script file \"%s\" not present",
		       path);
		return 1;
	}
	tc_log(TC_LOG_INFO, "Executing script file \"%s\"", path);

	/* Try to read a line until it finishes */
	int result = 0;
	while (true) {
		/* Get a new line without carry return */
		char buf[256];
		char *r = fgets(buf, sizeof(buf), f);
		if (!r)
			break;
		int buf_len = strlen(buf);
		if (!buf_len)
			continue;
		if (buf[buf_len-1] == '\n') {
			buf[--buf_len] = 0;
			if (!buf_len)
				continue;
		}

		/* Execute the commands */
		tc_log(TC_LOG_INFO, "Command \"%s\"", strndupa(buf, buf_len));
		int nr = tc_cmd(buf, buf_len);
		if (nr) {
			if (nr < 0)
				result = -1;
			break;
		}
	}

	/* Close the file */
	fclose(f);

	/* Return the result */
	return result;
}


int tc_cmd_init(bool readhome)
{
	tc_cmd_add(&tc_cmd_exit);
	tc_cmd_add(&tc_cmd_set);
	#ifdef ENABLE_OSD
	tc_cmd_add(&tc_cmd_osd);
	#endif /* ENABLE_OSD */
	#ifdef ENABLE_CEC
	tc_cmd_add(&tc_cmd_cec);
	#endif /* ENABLE_CEC */
	tc_cmd_add(&tc_cmd_mouse);
	tc_cmd_add(&tc_cmd_exec_cmd);
	tc_cmd_add(&tc_cmd_init_cmd);
	if (tc_cmd_load("/etc/tvcontrold/cmd.conf") < 0)
		return -1;
	if (readhome) {
		if (tc_cmd_load(".tvcontrold/cmd.conf") < 0)
			return -1;
	}
	return 0;
}

int tc_cmd(const char *buf, uint32_t len)
{
	char *newb = tc_cmd_env_subs(buf, &len);
	if (!newb)
		return -1;
	if (len == 0)
		return 0;
	buf = newb;
	if (buf[0] == '#') {
		free(newb);
		return 0;
	}
	bool extend = (buf[0] == '\t');
	if (!extend)
		tc_cmd_extend = NULL;
	if (extend) {
		if (!tc_cmd_extend || !tc_cmd_extend->extend) {
			tc_log(TC_LOG_ERR, "Error extending command not supported");
			free(newb);
			return -1;
		}
		int r = tc_cmd_extend->extend(tc_cmd_extend, buf+1, len-1);
		free(newb);
		return r;
	}
	tc_cmd_t *cmd = tc_cmd_first;
	while (cmd) {
		if (tc_cmd_starts(&buf, &len, cmd->name)) {
			int r = cmd->exec(cmd, buf, len);
			free(newb);
			return r;
		}
		cmd = cmd->next;
	}
	tc_log(TC_LOG_ERR, "Unknown command \"%s\"", strndupa(buf, len));
	free(newb);
	return -1;
}

void tc_cmd_release(void)
{
	while (tc_cmd_first) {
		tc_cmd_t *c = tc_cmd_first;
		tc_cmd_first = c->next;
		if (c->free)
			c->free(c);
	}
	while (tc_cmd_env) {
		tc_cmd_env_t *e = tc_cmd_env;
		tc_cmd_env = e->next;
		free((void *)e->name);
		free((void *)e->value);
		free(e);
	}
}
