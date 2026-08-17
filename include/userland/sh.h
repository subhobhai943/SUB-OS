#ifndef _USERLAND_SH_H
#define _USERLAND_SH_H

int sh_main(int argc, char** argv);
int sh_execute_script(const char* filepath);
const char* sh_get_env(const char* key);
int sh_set_env(const char* key, const char* value);

#endif // _USERLAND_SH_H
