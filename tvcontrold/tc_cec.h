#ifndef TC_CEC_H_INCLUDED
#define TC_CEC_H_INCLUDED

/**
 *  Initialize the CEC control module of TV control. 
 *
 *  \retval 0 on success.
 *  \retval -1 on error.
 */
int tc_cec_init(void);

/**
 *  Release the CEC control module.
 */
void tc_cec_release(void);

/**
 *  Power on every detected device.
 */
void tc_cec_poweron_all(void);

/**
 *  Standby every detected device.
 */
void tc_cec_standby_all(void);

/**
 *  Set this device as active source.
 */
void tc_cec_setactive(void);

/**
 *  Send the mute signal.
 */
void tc_cec_mute(void);

/**
 *  Send the volume up signal.
 */
void tc_cec_volumeup(void);

/**
 *  Send the volume down signal.
 */
void tc_cec_volumedown(void);

#endif // TC_CEC_H_INCLUDED
