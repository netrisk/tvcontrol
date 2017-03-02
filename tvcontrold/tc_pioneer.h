#ifndef TC_PIONEER_H_INCLUDED
#define TC_PIONEER_H_INCLUDED

#include <pthread.h>
#include <tc_types.h>

/**
 *  Pioneer object to be initialized to work with the
 *  pioneer network protocol.
 */
typedef struct tc_pioneer_t {
	const char *name; /**< Name of the pioneer command.      */
	const char *host; /**< Name of the pioneer host.         */
	pthread_t thread; /**< Thread for this pioneer object    */
	int pipe[2];      /**< Communication pipe.               */
	int32_t vol_accel;/**< Volume acceleration.              */
	timespec prev;    /**< Last time for transmission        */
	uint8_t lastcmd;  /**< Last command executed.            */
	/* Current pioneer state */
	bool pwr;         /**< Power status of the pioneer       */
	uint32_t vol;     /**< Volume number.                    */
	uint8_t fl[16];   /**< Data of FL (screen info).         */
	uint32_t fn;      /**< Input                             */
	bool mute;        /**< Mute status of the receiver       */
	bool mute_known;  /**< Variable to know if mute is known */
	uint8_t mc;       /**< Current MCACC using               */
} tc_pioneer_t;

/**
 *  Initialize a Pioneer protocol object.
 *
 *  \param pioneer   Pioneer object to initialize.
 *  \param host      Host to connect to.
 *  \param hostlen   Length of the host name
 *  \param name      Name of the pioneer device for commands.
 *  \param namelen   Length of the name.
 *  \retval 0 on success.
 *  \retval -1 on error.
 */
int tc_pioneer_init(tc_pioneer_t *pioneer,
                    const char *host,
                    uint32_t hostlen,
                    const char *name,
                    uint32_t namelen);

/* Commands to be executed through pioneer */
#define TC_PIONEER_CMD_NONE       (0)
#define TC_PIONEER_CMD_QUERY      (1)
#define TC_PIONEER_CMD_POWERON    (2)
#define TC_PIONEER_CMD_STANDBY    (3)
#define TC_PIONEER_CMD_VOLUMEUP   (4)
#define TC_PIONEER_CMD_VOLUMEDOWN (5)
#define TC_PIONEER_CMD_MUTEON     (6)
#define TC_PIONEER_CMD_MUTEOFF    (7)
#define TC_PIONEER_CMD_MUTE       (8)
#define TC_PIONEER_CMD_MCACC1     (9)
#define TC_PIONEER_CMD_MCACC2     (10)
#define TC_PIONEER_CMD_MCACC3     (11)
#define TC_PIONEER_CMD_MCACC4     (12)
#define TC_PIONEER_CMD_MCACC5     (13)
#define TC_PIONEER_CMD_MCACC6     (14)
#define TC_PIONEER_CMD_LISTENMODE_STEREO    (15)
#define TC_PIONEER_CMD_LISTENMODE_EXTSTEREO (16)
#define TC_PIONEER_CMD_LISTENMODE_DIRECT    (17)
#define TC_PIONEER_CMD_LISTENMODE_ALC       (18)
#define TC_PIONEER_CMD_LISTENMODE_EXPANDED  (19)
#define TC_PIONEER_CMD_INPUT_TUNER      (20)
#define TC_PIONEER_CMD_INPUT_DVD        (21)
#define TC_PIONEER_CMD_INPUT_TV         (22)
#define TC_PIONEER_CMD_INPUT_SAT        (23)

/**
 *  Execute the commands through the pioneer.
 *
 *  \param pioneer   Pioneer object.
 *  \param cmd       Command to execute
 *  \retval 0 on success.
 *  \retval -1 on error.
 */
int tc_pioneer_send(tc_pioneer_t *pioneer, uint8_t cmd);

/**
 *  Release the pioneer object.
 *
 *  \param pioneer  Pioneer object
 */
void tc_pioneer_release(tc_pioneer_t *pioneer);

#endif /* TC_PIONEER_H_INCLUDED */
