/*
 * at_parser.c
 */


#include "main.h"

typedef void (*cmd_handler)(const char *cmd, int index);

#define INRANGE(X, Y, Z)	(X >= Y && X <= Z)

#if 0
/* HFP v1.5
 * bit map of HF BRSF supported features
 */

/* Don't un-comment these #define's. These are just for reference.
 * They overlap with +BRSF #define's.
 */
#define EC_NR_FUNCTION			(1<<0)
#define THREE_WAY_CALLING		(1<<1)
#define CLI_CAPABILITY			(1<<2)
#define VOICE_RECOGNITION		(1<<3)
#define REMOTE_VOLUME_CONTROL 	(1<<4)
#define ENHANCED_CALL_STATUS	(1<<5)
#define ENHANCED_CALL_CONTROL	(1<<6)

#endif

#define SUPPORTED_FEATURES		(1<<2)

#define IS_FEATURES_SUPPORTED(X, Y)		(X & Y)


/*
 * Read command ex: AT+CIND?
 * Query command ex: AT+CIND=?
 */

enum at_cmds {
	OK = 0,
	ERROR,
	AT_BRSF,
	BRSF,
	AT_CIND_Q,
	CIND,
	AT_CIND_R,
	AT_CMER,
	CIEV,
	AT_BIND,
	AT_BIND_Q,
	AT_BIND_R,
	BIND,
	AT_BIEV,
	ATA,
	RING,
	AT_CHUP,
	AT_CLIP,
	CLIP,
};

const char *str_cmds[] = {
		"OK",
		"ERROR",
		"AT+BRSF=",
		"+BRSF:",
		"AT+CIND=?"	// retrieve info about supported AG indicators and their ordering.
		"+CIND"
		"AT+CIND?"	// get the current status of the AG supported indicators.
		"AT+CMER="
		"+CIEV"
		"AT+BAC"	// notify AG of the available codecs in HF. in-case both HF and AG support codec negotiation feature.
		"AT+BIND="
		"AT+BIND=?"	// request from HF to get the supported indicators supported by AG, in-case HF and AG support HF indicators.
		"AT+BIND?"	// request from HF to get the current enabled HF indicators on AG.
		"+BIND"		// AG response for AT+BIND? command.
		"AT+BIEV="	// HF command to AG to indicate change in HF indicators.

		"ATA"	// standard call answer AT command.
		"RING"
		"AT+CHUP="
		"AT+CLIP="
		"+CLIP:"	// +CLIP: <number>, 128-143 or +CLIP: <number>, 144-159 or +CLIP: <number>, 160-175
};

struct cmd_struct {
	cmd_handler handler_callback;
};

struct _connection {
	enum at_cmds last_cmd;
	/* Indicator indexes */
	int service_index;
	int call_index;
	int callsetup_index;
	int signal_index;
	int ring_count;
	char *incoming_callid;
};

struct _connection connection;

/* HFP 1.7 - 4 Hands-Free Control Interoperability Requirements
 * documents the complete HF connection establishment procedure.
 */

/* arg passed to this function should be string. */
bool send_command(const char *cmd)
{
	char data[MAX_DATA_BUF_SIZE];
	int i;

	for (i = 0; *cmd != '\0'; i++, cmd++)
		data[i] = *cmd;

	data[i] = '\r';
	data[++i] = '\n';

	write_data(data, i);

	return true;
}

static int get_cmd_index(const char *cmd)
{
	int i, j;
	int len = strlen(cmd);

	for (i = 0; i < len; i++) {
		if (cmd[i] == '?' || cmd[i] == '=' || cmd[i] == ':' )
			break;
	}

	int cmd_count = sizeof(str_cmds)/sizeof(str_cmds[0]);

	for (j = 0; j < cmd_count; j++) {
		if (strncmp(str_cmds[j], cmd, i) == 0)
			break;
	}

	/* we expect that the cmd will be always a known command. */
	return j;
}

static char *get_cmd_value(const char *cmd)
{
	char *tmp;
	/* Assert on a coding errors. */
	assert(!cmd || *cmd == '\0');

	tmp = strchr_multi_byte(cmd, ":?=");
	if (!tmp || strlen(tmp) <= 1)
		return NULL;

	return (tmp + 1);
}

void handle_clip_events(const char *cmd, int index)
{
	char *value = get_cmd_value(cmd);
	util_charstrip(value, '"');
	util_strstrip(value);
	connection.incoming_callid = strdup(value);
	l_info("Incoming caller id is: %s", connection.incoming_callid);
}

void handle_ring_events(const char *cmd, int index)
{
	++connection.ring_count;
	if (connection.ring_count >= 3) {
		l_info("Received more than 3 rings. Accepting call from caller id: %s", connection.incoming_callid);
		connection.ring_count = 0;
		send_command(str_cmds[ATA]);
		connection.last_cmd = ATA;
	}
}

void handle_ciev_events(const char *cmd, int index)
{
	char *indicator, *end = NULL;
	unsigned long ind_index = 1, ind_value;
	char *value = get_cmd_value(cmd);
	indicator = strtok(value, ",");
	ind_value = strtoul(indicator, &end, 10);
	if (end && *end) {
		l_error("%s Failed processing +CIEV event. Unknown command: %s", __func__, cmd);
		goto failed;
	}

	while(1) {
		indicator = strtok(NULL, ",");
		if (!indicator) { break; }
		ind_value = strtoul(indicator, &end, 10);
		if (ind_index == connection.service_index) {
			ind_value ? l_info("Service connection is available now") : l_info("Lost service connection");
			break;
		} else if (ind_index == connection.callsetup_index) {
			if (ind_value == 0) {
				l_info("Call set up is done");
			} else if (ind_value == 1) {
				l_info("An incoming call progress is ongoing");
			} else if (ind_value == 2) {
				l_info("An outgoing call set up is ongoing");
			} else if (ind_value == 3) {
				l_info("Remote party is being alerted in an outgoing call");
			}
		} else if(ind_index == connection.call_index) {
			ind_value ? l_info("Call is active now") : l_info("No active call is in progress");
		}
		++ind_index;
	}

	send_command(str_cmds[OK]);
	return;
failed:
	send_command(str_cmds[ERROR]);
}

/*
 * CIND query response format: +CIND: ("service",(0-1)),("callsetup",(0-3))
 */
static void cind_query_response(char *value)
{
	char *tmp;
	const char *service = "\"service\"", *callsetup = "\"callsetup\"";
	const char *call = "\"call\"", *signal = "\"signal\"";
	int index = 1;

	util_strstrip(value);
	tmp = value;

	while (*tmp != '\0') {
		tmp = strchr(tmp, '(');
		if (!tmp) {
			l_error("Invalid CIND query response %s\n", value);
			goto failed;
		}
		++tmp;
		if (!strncmp(tmp, service, 9)) {
			connection.service_index = index;
		} else if (!strncmp(tmp, callsetup, 11)) {
			connection.callsetup_index = index;
		} else if (!strncmp(tmp, call, 6)) {
			connection.call_index = index;
		} else if (!strncmp(tmp, signal, 8)) {
			connection.signal_index = index;
		}

		if ((tmp = strchr(tmp, ')')) && (tmp = strchr(tmp, ')'))) {
			++tmp;
			++index;
		} else {
			l_error("Misformed CIND query response %s\n", value);
			goto failed;
		}
	}

	l_info("service index: %d, callsetup index: %d, call index: %d\n",
			connection.service_index, connection.callsetup_index, connection.call_index);

	send_command(str_cmds[OK]);
	send_command(str_cmds[AT_CIND_R]);
	connection.last_cmd = AT_CIND_R;
	return;

failed:
	send_command(str_cmds[ERROR]);
}

/* service:
<value>=0 implies no service. No Home/Roam network available.
<value>=1 implies presence of service. Home/Roam network available.
call: Standard call status indicator
<value>=0 means no call active.
<value>=1 means a call is active.
callsetup: When supported, this indicator shall be used in conjunction with, and as an extension of the standard call indicator.
<value>=0 means not currently in call set up.
<value>=1 means an incoming call process ongoing.
<value>=2 means an outgoing call set up is ongoing.
<value>=3 means remote party being alerted in an outgoing call. */
static void cind_read_response(char *value)
{
	char *indicator, *end = NULL;
	unsigned long i, ind_value;
	char *cmd;
	if (!value || *value == '\0') {

		goto failed;
	}

	util_strstrip(value);
	indicator = strtok(value, ",");
	for (i = 1; ; i++) {
		ind_value = strtoul(indicator, &end, 10);
		if (end && *end)
			goto failed;

		if (i == connection.service_index) {
			if (ind_value)
				l_info("Home/Roam network service is available");
			else
				l_info("No Home/Roam network service is available");
		} else if (i == connection.call_index) {
			if (ind_value)
				l_info("Active call is already in-progress");
			else
				l_info("No active call is in progress");
		} else if (i == connection.callsetup_index) {
			if (ind_value == 0)
				l_info("No current call set up is in progress");
			else if (ind_value == 1)
				l_info("An incoming call progress is ongoing");
			else if (ind_value == 2)
				l_info("An outgoing call set up is ongoing");
			else if (ind_value == 3)
				l_info("Remote party is being alerted in an outgoing call");
		}

		indicator = strtok(NULL, ",");
		if (!indicator)
			break;
	}

	send_command(str_cmds[OK]);
	/* AT+CMER=3,0,0,1 - Command to enable "indicator events reporting".
	 * AT+CMER=3,0,0,0 - To disable "indicator event reporting".
	 */
	cmd = l_strdup_printf("%s%s", str_cmds[AT_CMER], "3,0,0,1");
	send_command(cmd);
	l_free(cmd);
	connection.last_cmd = AT_CMER;
failed:
	send_command(str_cmds[ERROR]);
}

void handle_cind_response(const char *cmd, int index)
{
	char *value;

	value = get_cmd_value(cmd);
	if (!value){
		l_error("Invalid CIND response %s", cmd);
		return;
	}

	if (strchr(value, '('))
		cind_query_response(value);
	else
		cind_read_response(value);
}

void handle_brsf_response(const char *cmd, int index)
{
	char *value, *end;
	int features;

// HFP 1.7 AG supported features.
#define THREE_WAY_CALLING		(0<<1)
#define EC_NR_FUNCTION			(1<<1)
#define VOICE_RECOGNITION		(1<<2)
#define INBAND_RINGING			(1<<3)
#define ATTACH_NUM_TO_VOICETAG	(1<<4)
#define ABILITY_TO_REJECT_CALL	(1<<5)
#define ENHANCED_CALL_STATUS	(1<<6)
#define ENHANCED_CALL_CONTROL	(1<<7)
#define EXTENDED_ERROR_RESULTS	(1<<8)
#define CODEC_NEGOTIATION		(1<<9)
#define HF_INDICATORS			(1<<10)

	value = get_cmd_value(cmd);
	if (!value) {
		l_error("Invalid BRSF response %s", cmd);
		return;
	}

	features = strtoul(value, &end, 10);
	if (end && *end) {
		l_error("Invalid BRSF response %s", cmd);
		return;
	}

	l_info("features supported by AG:");

	if (IS_FEATURES_SUPPORTED(features, THREE_WAY_CALLING))
		l_info("three way calling supported");
	if (IS_FEATURES_SUPPORTED(features, EC_NR_FUNCTION))
		l_info("echo cancellation and noise reduction supported");
	if (IS_FEATURES_SUPPORTED(features, VOICE_RECOGNITION))
		l_info("voice recognition supported");
	if (IS_FEATURES_SUPPORTED(features, INBAND_RINGING))
		l_info("in-band ringing supported");
	if (IS_FEATURES_SUPPORTED(features, ATTACH_NUM_TO_VOICETAG))
		l_info("Attach a number to a voice tag supported");
	if (IS_FEATURES_SUPPORTED(features, ABILITY_TO_REJECT_CALL))
		l_info("ability to reject call supported");
	if (IS_FEATURES_SUPPORTED(features, ENHANCED_CALL_STATUS))
		l_info("Enhanced call status supported");
	if (IS_FEATURES_SUPPORTED(features, ENHANCED_CALL_CONTROL))
		l_info("Enhanced call control supported");
	if (IS_FEATURES_SUPPORTED(features, EXTENDED_ERROR_RESULTS))
		l_info("Extended error results supported");
	if (IS_FEATURES_SUPPORTED(features, CODEC_NEGOTIATION))
		l_info("Codec negotiation supported");
	if (IS_FEATURES_SUPPORTED(features, HF_INDICATORS))
		l_info("HF indicators supported");

	send_command(str_cmds[OK]);
	send_command(str_cmds[AT_CIND_Q]);
	connection.last_cmd = AT_CIND_Q;
}

void handle_brsf_cmd(const char *cmd, int index)
{
	char *value;

	if (!cmd) {
		value = l_strdup_printf("%s%d", str_cmds[AT_BRSF], SUPPORTED_FEATURES);
		send_command(value);
		free(value);
		return;
	}

	/* HF device never receive AT+BRSF command. */
	value = get_cmd_value(cmd);
	if (!value) {
		l_error("Error in BRSF response %s", cmd);
		return;
	}

	util_strstrip(value);
	l_info("BRSF command supported features %s", value);
}

void handle_ok_response(const char *cmd, int index)
{
	char *str;
	if (connection.last_cmd == AT_CMER) {
		/* Enable Caller Line Identification. */
		str = l_strdup_printf("%s%d", str_cmds[AT_CLIP], 1);
		send_command(str);
		l_free(str);
		connection.last_cmd = AT_CLIP;
	}
}

void handle_error_response(const char *cmd, int index)
{
	if (connection.last_cmd == ATA) {
		l_error("Attending incoming call failed");
	} else {
		l_error("Command failed: %s", str_cmds[connection.last_cmd]);
	}
}

/* callback methods don't expect escape character in leading and trailing
 * characters.
 */
struct cmd_struct cmd_handle[] = {
		{ handle_ok_response },
		{ handle_error_response },
		{ handle_brsf_cmd },
		{ handle_brsf_response },
		{ NULL },
		{ handle_cind_response },
		{ NULL },
		{ NULL },
		{ handle_ciev_events },
		{ NULL },
		{ NULL },
		{ NULL },
		{ NULL },
		{ NULL },
		{ NULL },
		{ NULL },
		{ handle_ring_events },
		{ NULL },
		{ NULL },
		{ handle_clip_events },
};

void init_connection(void)
{
	char *value;
	value = l_strdup_printf("%s%d", str_cmds[AT_BRSF], SUPPORTED_FEATURES);
	send_command(value);
	l_free(value);
}

static void process_command(const char *data)
{
	char *cmd;
	int len, index;

	if (!data || strlen(data) < 2) {
		l_debug("Invalid AT command");
		return;
	}

	cmd = l_strdup(data);
	index = get_cmd_index(cmd);
	free(cmd);

	len = sizeof(cmd_handle)/sizeof(cmd_handle[0]);

	if (!INRANGE(index, 0, len - 1)) {
		l_debug("Unknown command %s", cmd);
		return;
	}

	cmd_handle[index].handler_callback(data, index);
}

void handle_recv_data(char *data, int bytes_read)
{
	char cmd[MAX_DATA_BUF_SIZE];
	int total_bytes = bytes_read;


	/* data field may contain multiple commands.
	 * Each command need to be processed separately.
	 */

	while (total_bytes > 0) {
		int i, j, k;

		memset(cmd, 0, sizeof(cmd));
		for (i = 0; i < bytes_read; i++) {
			if (data[i] == '\r' || data[i] == '\n')
				continue;

			break;
		}

		for (j = i, k = 0 ; j < bytes_read ; k++, j++) {
			if (data[j] != '\r' && data[j] != '\n')
				cmd[k] = data[j];

			break;
		}

		cmd[k] = '\0';

		process_command(cmd);

		/* Can we reduce this loop ? */
		for (i = j ; i < bytes_read; i++) {
			if (data[i] == '\r' || data[i] == '\n')
				continue;

			break;
		}
		total_bytes -= i;
	}
}

#ifdef UNIT_TEST

int main(void)
{
	char data1[] = {'\r', 'A', 'T', '\r', '\n', 'O', 'K', '\r', '\n'};
	handle_recv_data(data1, sizeof(data1)/sizeof(data1[0]));

	char *data2 = "\r\nAT+BRSF\rATA\nOK\r\n\r\n"

	handle_recv_data(, sizeof(data1)/sizeof(data1[0]));

}

#endif
