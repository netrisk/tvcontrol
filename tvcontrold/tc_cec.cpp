#include <config.h>

#ifdef ENABLE_CEC

#include <tc_log.h>
#include <tc_server.h>
#include <stdlib.h>
#include <libcec/cectypes.h>
#include <libcec/cec.h>
#include <stdio.h>
#include <ctype.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Define the following macro to enable debugging
/* #define TC_CEC_DEBUG */

// Dynamic link library default name different on each system
#if defined(_WIN32)
#define TC_CEC_LIB "libcec.dll"
#elif defined(_WIN64)
#define TC_CEC_LIB "libcec.x64.dll"
#elif defined(__APPLE__)
#define TC_CEC_LIB "libcec.dylib"
#elif defined(CEC_LIB_VERSION_MAJOR_STR)
#define TC_CEC_LIB "libcec.so." CEC_LIB_VERSION_MAJOR_STR
#elif CEC_LIB_VERSION_MAJOR == 1
#define TC_CEC_LIB "libcec.so.1"
#else
#error "Default libcec dynamic library name not supported"
#endif

static CEC::libcec_configuration tc_cec_config;
static CEC::ICECCallbacks        tc_cec_callbacks;
static char                     *tc_cec_port;
static uint32_t                  tc_cec_log_level = 
	CEC::CEC_LOG_ERROR | CEC::CEC_LOG_WARNING |
	CEC::CEC_LOG_NOTICE
#ifdef TC_CEC_DEBUG
	| CEC::CEC_LOG_TRAFFIC | CEC::CEC_LOG_DEBUG
#endif /* TC_CEC_DEBUG */
	;
#if defined(_WIN32) || defined(_WIN64)
static HINSTANCE tc_cec_lib = NULL;
#else
static void     *tc_cec_lib = NULL;
#endif
static CEC::ICECAdapter *tc_cec_adapter = NULL;


/* --- List of devices ---------------------------------------------------- */

/**
 *  Type that represents a device
 */
typedef struct tc_cec_device_t
{
	bool detected;             /**< True if it is detected  */
	CEC::cec_vendor_id vendor; /**< Vendor id of the device */
} tc_cec_device_t;

/** Maximum number of devices */
#define TC_CEC_DEVICES_COUNT 15

/** Array of devices with CEC info */
static tc_cec_device_t tc_cec_devices[TC_CEC_DEVICES_COUNT];

#define TC_CEC_DEVICE_FMT "%s (%s)"
#define TC_CEC_DEVICE_PRM(i) \
	tc_cec_adapter->ToString((CEC::cec_logical_address)i), \
	tc_cec_adapter->ToString((CEC::cec_vendor_id)(tc_cec_devices[i].vendor))


/* --- CEC event management ----------------------------------------------- */

static int tc_cec_logmessage(void *cbparam, CEC::cec_log_message message)
{
	if ((message.level & tc_cec_log_level) != message.level)
		return 0;
	uint8_t severity = TC_LOG_ERR;
	const char *prefix = "";
	switch (message.level) {
	default:
	case CEC::CEC_LOG_ERROR:   severity = TC_LOG_ERR;   break;
	case CEC::CEC_LOG_WARNING: severity = TC_LOG_WARN;  break;
	case CEC::CEC_LOG_NOTICE:  severity = TC_LOG_INFO;  break;
	case CEC::CEC_LOG_TRAFFIC: severity = TC_LOG_DEBUG; prefix = "TRAFFIC: "; break;
	case CEC::CEC_LOG_DEBUG:   severity = TC_LOG_DEBUG; break;
	}
	tc_log(severity, "%s%s", prefix, message.message);
	return 0;
}

static int tc_cec_keypress(void *cbparam, CEC::cec_keypress key)
{
	tc_log(TC_LOG_WARN, "Unknown key pressed: code=%u duration=%u",
	       (unsigned)key.keycode, (unsigned)key.duration);
	return 0;
}

/**
 *  Execute a given event in lower case.
 *
 *  \param event  Event to be executed
 */
static void tc_cec_event(char *event)
{
	int l = strlen(event);
	int i;
	for (i = 0; i < l; i++)
		event[i] = tolower(event[i]);
	#ifdef TC_CEC_DEBUG
	tc_log(TC_LOG_DEBUG, "cec: event: %s", event);
	#endif /* TC_CEC_DEBUG */
	tc_server_event(event, l);
}

static int tc_cec_command(void *cbparam, CEC::cec_command command)
{
	// Try to find the initiator device
	tc_cec_device_t *initiator = NULL;
	if (command.initiator >= 0 && 
	    command.initiator < TC_CEC_DEVICES_COUNT)
		initiator = &tc_cec_devices[command.initiator];

	// Check if it is a vendor id command
	if (command.opcode_set &&
	    command.opcode == CEC::CEC_OPCODE_DEVICE_VENDOR_ID &&
	    initiator &&
	    command.parameters.size == 3) {
		initiator->detected = true;
		initiator->vendor = (CEC::cec_vendor_id)
			((((uint32_t)command.parameters.data[0]) << 16) |
			 (((uint32_t)command.parameters.data[1]) << 8) |
			  ((uint32_t)command.parameters.data[2]));
		tc_log(TC_LOG_INFO, "Detected %s(%u) by %s(0x%06x)",
		       tc_cec_adapter->ToString(command.initiator),
		       (unsigned)command.initiator,
		       tc_cec_adapter->ToString(initiator->vendor),
		       (unsigned)initiator->vendor);
	} else if (command.opcode_set &&
	           command.opcode == CEC::CEC_OPCODE_ACTIVE_SOURCE) {
		#ifdef TC_CEC_DEBUG
		tc_log(TC_LOG_DEBUG, "cec: activesource: %s",
		       tc_cec_adapter->ToString(command.initiator));
		#endif /* TC_CEC_DEBUG */
		char event[256];
		snprintf(event, sizeof(event), "on_cec_activesource_%s",
		         tc_cec_adapter->ToString(command.initiator));
		tc_cec_event(event);
	} else if (command.opcode_set &&
	           command.opcode == CEC::CEC_OPCODE_ROUTING_CHANGE &&
	           command.parameters.size == 4) {
		#ifdef TC_CEC_DEBUG
		tc_log(TC_LOG_DEBUG, "cec: routingchange: %s from %02x%02x to %02x%02x",
		       tc_cec_adapter->ToString(command.initiator),
		       command.parameters.data[0],
		       command.parameters.data[1],
		       command.parameters.data[2],
		       command.parameters.data[3]);
		#endif /* TC_CEC_DEBUG */
		char event[256];
		snprintf(event, sizeof(event), "on_cec_routingchange_%s_%02x%02x",
		         tc_cec_adapter->ToString(command.initiator),
		         command.parameters.data[2],
		         command.parameters.data[3]);
		tc_cec_event(event);
	} else {
		char parameters[512];
		uint32_t parameters_length = 0;
		parameters[0] = 0;
		uint32_t i;
		for (i = 0; i < command.parameters.size; i++)
			parameters_length += snprintf(parameters + parameters_length,
			                              sizeof(parameters) - parameters_length,
			                              "%02x", command.parameters.data[i]);
		tc_log(TC_LOG_WARN, "Unknown command executed");
		tc_log(TC_LOG_WARN, "  initiator=%s(%u)",
		       tc_cec_adapter->ToString(command.initiator),
		       (unsigned)command.initiator);
		tc_log(TC_LOG_WARN, "  destination=%s(%u)",
		       tc_cec_adapter->ToString(command.destination),
		       (unsigned)command.destination);
		tc_log(TC_LOG_WARN, "  flags:%s%s",
		       command.ack ? " ack" : "",
		       command.eom ? " eom" : "");
		if (command.opcode_set)
			tc_log(TC_LOG_WARN, "  opcode=%s(0x%02x)",
			       tc_cec_adapter->ToString(command.opcode),
			       (unsigned)command.opcode);
		tc_log(TC_LOG_WARN, "  parameters=%s",  parameters);
		tc_log(TC_LOG_WARN, "  timeout=%u ms",  (unsigned)command.transmit_timeout);
	}
	return 0;
}

static int tc_cec_alert(void *cbparam, const CEC::libcec_alert type,
                        CEC::libcec_parameter param)
{
	switch (type) {
	case CEC::CEC_ALERT_CONNECTION_LOST:
		// TODO: Manage it properly
		tc_log(TC_LOG_ERR, "Connection lost - exiting");
		abort();
		break;
	default:
		tc_log(TC_LOG_WARN, "Unknown alert: type %u", type);
		break;
	}
	return 0;
}


/* --- CEC API implementation --------------------------------------------- */

int tc_cec_init(void)
{
	// Initialize the devices table
	memset(tc_cec_devices, 0, sizeof(tc_cec_devices));

	// Initialize the CEC objects generic way
	tc_cec_config.Clear();
	tc_cec_callbacks.Clear();
	tc_cec_config.clientVersion = 0;
	tc_cec_config.bActivateSource = 0;
	tc_cec_callbacks.CBCecLogMessage = &tc_cec_logmessage;
	tc_cec_callbacks.CBCecKeyPress   = &tc_cec_keypress;
	tc_cec_callbacks.CBCecCommand    = &tc_cec_command;
	tc_cec_callbacks.CBCecAlert      = &tc_cec_alert;
	tc_cec_config.callbacks          = &tc_cec_callbacks;

	// Application specific configuration of CEC objects
	// TODO: Make the types of devices configurable
	//tc_cec_config.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_PLAYBACK_DEVICE);
	tc_cec_config.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_RECORDING_DEVICE);
	//tc_cec_config.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_TUNER);
	//tc_cec_config.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_AUDIO_SYSTEM);
	
	// Configure more parts
	// TODO: tc_cec_config.baseDevice (integer)
	// TODO: tc_cec_config.iHDMIPort (integer from 1 to 15)
	// TODO: tc_cec_config.bGetSettingsFromROM (boolean 0 or 1)
	// TODO: tc_cec_config.bMonitorOnly (boolean 0 or 1)
	// TODO: snprintf(tc_cec_config.strDeviceName, 13, "%s", argv[iArgPtr + 1]);
	// TODO: Configure the port if required
	
	// Load the library if still not done
	if (!tc_cec_lib) {
		#if defined(_WIN32) || defined(_WIN64)
		tc_cec_lib = LoadLibrary(TC_CEC_LIB);
		#else
    	tc_cec_lib = dlopen(TC_CEC_LIB, RTLD_LAZY);
		#endif
    	if (!tc_cec_lib) {
    		tc_log(TC_LOG_ERR, "%s", dlerror());
    		return -1;
    	}
	}

	// Get the initialize function
	#if defined(_WIN32) || defined(_WIN64)
	typedef void* (__cdecl*_LibCecInitialise)(CEC::libcec_configuration *);
	_LibCecInitialise LibCecInitialise;
	LibCecInitialise =
		(_LibCecInitialise)(GetProcAddress(tc_cec_lib, "CECInitialise"));
	#else
	typedef void* _LibCecInitialise(CEC::libcec_configuration *);
	_LibCecInitialise* LibCecInitialise =
		(_LibCecInitialise*)dlsym(tc_cec_lib, "CECInitialise");
	#endif
	if (!LibCecInitialise) {
    		tc_log(TC_LOG_ERR, "LibCecInitialise symbol not found libcec");
    		return -1;
	}
	
	// Initialize the library
	tc_cec_adapter = static_cast< CEC::ICECAdapter* >(LibCecInitialise(&tc_cec_config));
	if (!tc_cec_adapter) {
		/* Show an error that we cannot initialize libcec */
		return -1;
	}

	// Show the version in the log (more information for 1.7.2 and on)
	#if CEC_LIB_VERSION_MINOR >= 7
	tc_log(TC_LOG_INFO, "libCEC version: %s%s%s",
		tc_cec_adapter->ToString((CEC::cec_server_version)tc_cec_config.serverVersion),
		(tc_cec_config.serverVersion >= 0x1702) ? ", " : "",
		(tc_cec_config.serverVersion >= 0x1702) ?
			tc_cec_adapter->GetLibInfo() : ""
	);
	#else
	tc_log(TC_LOG_INFO, "libCEC version: %s",
	       tc_cec_adapter->ToString((CEC::cec_server_version)tc_cec_config.serverVersion));
	#endif

	// Try to autodetect the port if not provided
	int result = 0;
	if (!tc_cec_port) {
		tc_log(TC_LOG_INFO, "Autodetecting CEC adapters");
		CEC::cec_adapter devices[10];
		uint8_t found = tc_cec_adapter->FindAdapters(devices, 10, NULL);
		if (found <= 0) {
			tc_log(TC_LOG_ERR, "No CEC devices found");
 			result = -1;
		}
		if (!result) {
			uint32_t i;
			for (i = 0; i < found; i++)
				tc_log(TC_LOG_INFO, "  CEC found: %s%s", devices[i].comm,
			                i ? " (selected)" : "");
			tc_cec_port = strdup(devices[0].comm);
		}
	}

	// Opening the connection
	if (!result) {
		tc_log(TC_LOG_INFO, "Opening CEC adapter: %s", tc_cec_port);
  		if (!tc_cec_adapter->Open(tc_cec_port)) {
			tc_log(TC_LOG_ERR, "Unable to open the device on port %s",
			       tc_cec_port);
			result = -1;
			// TODO: Show the port
		}
	}

	// Return success
	return result;
}

void tc_cec_release(void)
{
	// Free the resources
	if (tc_cec_port)
		free(tc_cec_port);
	if (tc_cec_adapter)
		tc_cec_adapter->Close();

	// Unload the library
	if (tc_cec_lib) {
		#if defined(_WIN32) || defined(_WIN64)
		typedef void (__cdecl*_DestroyLibCec)(void * device);
		_DestroyLibCec DestroyLibCec;
		DestroyLibCec = (_DestroyLibCec) (GetProcAddress(tc_cec_lib, "CECDestroy"));
		#else
		typedef void* _DestroyLibCec(CEC::ICECAdapter *);
		_DestroyLibCec *DestroyLibCec =
			(_DestroyLibCec*)dlsym(tc_cec_lib, "CECDestroy");
		#endif
		if (DestroyLibCec)
			DestroyLibCec(tc_cec_adapter);
		#if defined(_WIN32) || defined(_WIN64)
		FreeLibrary(tc_cec_lib);
		#else
		dlclose(tc_cec_lib);
		#endif
		tc_cec_lib = NULL;
	}
}

void tc_cec_poweron_all(void)
{
	uint32_t i;
	for (i = 0; i < TC_CEC_DEVICES_COUNT; i++) {
		tc_cec_device_t *device = &tc_cec_devices[i];
		if (!device->detected)
			continue;
		if (!tc_cec_adapter->PowerOnDevices((CEC::cec_logical_address)i))
			tc_log(TC_LOG_ERR, "Error powering on " TC_CEC_DEVICE_FMT, 
			       TC_CEC_DEVICE_PRM(i));
	}
}

void tc_cec_standby_all(void)
{
	uint32_t i;
	for (i = 0; i < TC_CEC_DEVICES_COUNT; i++) {
		tc_cec_device_t *device = &tc_cec_devices[i];
		if (!device->detected)
			continue;
    	if (!tc_cec_adapter->StandbyDevices((CEC::cec_logical_address)i))
    		tc_log(TC_LOG_ERR, "Error standing by " TC_CEC_DEVICE_FMT,
    		       TC_CEC_DEVICE_PRM(i));
	}
}

void tc_cec_setactive(void)
{
    	if (!tc_cec_adapter->SetActiveSource())
    		tc_log(TC_LOG_ERR, "Error setting active source");
}

void tc_cec_volumeup(void)
{
    	if (!tc_cec_adapter->VolumeUp())
    		tc_log(TC_LOG_ERR, "Error sending Volume Up");
}

void tc_cec_volumedown(void)
{
    	if (!tc_cec_adapter->VolumeDown())
    		tc_log(TC_LOG_ERR, "Error sending Volume Down");
}

void tc_cec_mute(void)
{
	tc_log(TC_LOG_INFO, "MUTE");
    	if (!tc_cec_adapter->SendKeypress(CEC::CECDEVICE_AUDIOSYSTEM, CEC::CEC_USER_CONTROL_CODE_MUTE, true))
    		tc_log(TC_LOG_ERR, "Error sending Mute");
    	if (!tc_cec_adapter->SendKeyRelease(CEC::CECDEVICE_AUDIOSYSTEM, true))
    		tc_log(TC_LOG_ERR, "Error sending Mute");
}

#endif /* ENABLE_CEC */
