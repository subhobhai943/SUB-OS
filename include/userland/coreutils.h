#ifndef _USERLAND_COREUTILS_H
#define _USERLAND_COREUTILS_H

// GNU-style text-processing applets for LazyBox. Each entry point matches
// applet_fn_t so it can be placed directly in the LazyBox dispatch table.

int coreutils_sort(int argc, char** argv);
int coreutils_uniq(int argc, char** argv);
int coreutils_cut(int argc, char** argv);
int coreutils_tr(int argc, char** argv);
int coreutils_rev(int argc, char** argv);
int coreutils_tac(int argc, char** argv);
int coreutils_nl(int argc, char** argv);
int coreutils_seq(int argc, char** argv);
int coreutils_diff(int argc, char** argv);
int coreutils_xxd(int argc, char** argv);
int coreutils_du(int argc, char** argv);
int coreutils_factor(int argc, char** argv);
int coreutils_sum(int argc, char** argv);
int coreutils_truncate(int argc, char** argv);

#endif // _USERLAND_COREUTILS_H
