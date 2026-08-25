#ifndef _SECURITY_AUTH_H
#define _SECURITY_AUTH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_USERS 16

typedef struct user_account {
    uint32_t uid;
    uint32_t gid;
    char username[32];
    char full_name[64];
    char home_dir[64];
    char shell[32];
    char password_hash[65]; // 64-char SHA256 hex string + null
    bool in_use;
} user_account_t;

void auth_init(void);
int  auth_add_user(const char* username, const char* password, uint32_t uid, uint32_t gid, const char* home, const char* shell);
bool auth_verify_password(const char* username, const char* password);
int  auth_change_password(const char* username, const char* new_password);
const user_account_t* auth_get_user_by_name(const char* username);
const user_account_t* auth_get_user_by_uid(uint32_t uid);

const user_account_t* auth_get_current_user(void);
int auth_set_current_user(const char* username);

size_t auth_get_user_count(void);
const user_account_t* auth_get_user_by_index(size_t index);

/* Interactive boot-time login: prompts for a username and password on the
 * console and blocks until a valid pair is entered, then sets the current
 * user. Default account is SUB / SUB. */
void console_login(void);

#endif // _SECURITY_AUTH_H
