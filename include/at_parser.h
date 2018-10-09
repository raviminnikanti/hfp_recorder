/*
 * at_parser.h
 */

#ifndef AT_PARSER_H_
#define AT_PARSER_H_

void handle_recv_data(char *data, int bytes_read);
bool send_command(const char *cmd);

#endif /* AT_PARSER_H_ */
