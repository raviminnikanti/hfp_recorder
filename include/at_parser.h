/*
 * at_parser.h
 * Copyright (C) 2015-2016 Ooma Incorporated. All rights reserved.
 */

#ifndef AT_PARSER_H_
#define AT_PARSER_H_

void handle_recv_data(char *data, int bytes_read);
bool is_valid_at_command(const char *cmd);
bool send_command(const char *cmd);

#endif /* AT_PARSER_H_ */
