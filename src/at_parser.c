/*
 * at_parser.c
 */


#include "main.h"

typedef void (*cmd_handler)(const char *cmd);


#define INRANGE(X, Y, Z)	(X >= Y && X <= Z)

#if 0
/* HFP v1.5
 * bit map of HF BRSF supported features
 */

// Don't un-comment these #define's. They overlap with +BRSF #define's.
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


/* Read command ex: AT+CIND?
 * Query command ex: AT+CIND=?
 *
 */

enum at_cmds {
	OK,
	AT_BRSF,
	BRSF,
	AT_BAC
};

const char *str_cmds[] = {
		"OK",
		"AT+BRSF",
		"+BRSF",
		"AT+BAC"	// notify AG of the available codecs in HF. in-case both HF and AG support codec negotiation feature.
		"AT+CIND=?"	// retrieve info about supported AG indicators and their ordering.
		"AT+CIND?"	// get the current status of the AG supported indicators.
		"AT+CMER"
		"+CIEV"
		"AT+BIND="
		"AT+BIND=?"	// request from HF to get the supported indicators supported by AG, in-case HF and AG support HF indicators.
		"AT+BIND?"	// request from HF to get the current enabled HF indicators on AG.
		"+BIND"		// AG response for AT+BIND? command.
		"AT+BIEV="	// HF command to AG to indicate change in HF indicators.

		"ATA"	// standard call answer AT command.
		"RING"
		"AT+CHUP"
		"+CLIP:"	// +CLIP: <number>, 128-143 or +CLIP: <number>, 144-159 or +CLIP: <number>, 160-175
};

struct cmd_struct {
	enum at_cmds cmd_t;
	cmd_handler handler_callback;
};

/* HFP 1.7 - 4 Hands-Free Control Interoperability Requirements
 * documents the complete HF connection establishment procedure.
 */

static inline bool is_escape_character(char c)
{
	if (c == 10 || c == 13 || c == 32)
		return true;

	return false;
}

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

enum at_cmds str_to_enum_type(const char *cmd)
{
	int i;

	int cmd_count = sizeof(str_cmds)/sizeof(str_cmds[0]);

	for (i = 0; i < cmd_count; i++) {
		if (strcmp(str_cmds[i], cmd) == 0)
			break;
	}

	/* we expect that the cmd will be always a known command. */
	return i;
}

void handle_brsf_response(const char *cmd)
{

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

	char *tmp = strchr(cmd, ':');
	if (!tmp) {
		l_error("Invalid BRSF response");
		return;
	}

	int features = atoi(tmp + 1);

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
}

void handle_brsf_cmd(const char *cmd)
{
	;
}

void handle_ok_response(const char *cmd)
{
	;
}

/* callback methods don't expect escape character in leading and trailing
 * characters.
 */
struct cmd_struct cmd_handle[] = {
		{ OK,			handle_ok_response },
		{ AT_BRSF,		handle_brsf_cmd },
		{ BRSF,			handle_brsf_response },
};

void init_connection(void)
{
	struct l_string *str = l_string_new(16);
	l_string_append_printf(str, "%s=%d", str_cmds[AT_BRSF], SUPPORTED_FEATURES);
	char *cmd = l_string_unwrap(str);
	send_command(cmd);
	l_free(cmd);
}

static void process_command(const char *data)
{
	char *cmd;
	int len;

	if (!data || strlen(data) < 2) {
		l_debug("Invalid AT command");
		return;
	}

	cmd = l_strdup(data);
	enum at_cmds cmd_type = str_to_enum_type(cmd);
	free(cmd);

	len = sizeof(cmd_handle)/sizeof(cmd_handle[0]);

	if (!INRANGE(cmd_type, 0, len - 1)) {
		l_debug("Unknown command %s", cmd);
		return;
	}

	cmd_handle[cmd_type].handler_callback(data);
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
