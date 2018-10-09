/*
 * socket.h
 */

#ifndef SOCKET_H_
#define SOCKET_H_

#define MAX_DATA_BUF_SIZE	256
void new_rfcomm_connection(int sock);
bool write_data(const char *data, int len);
#endif
