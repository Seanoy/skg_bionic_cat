#ifndef AGORA_SERVER_H
#define AGORA_SERVER_H

int send_join_request(const char *token, const char *channel,
                      char *out_agent_id, size_t buf_size);
int send_leave_request(char *agent_id);
int send_update_request(char *agent_id, char *token);
int send_get_status_request(char *agent_id);
int send_get_list_request(char *agent_id);
void agora_openssl_init(void);
void agora_openssl_deinit(void);

#endif // AGORA_SERVER_H
