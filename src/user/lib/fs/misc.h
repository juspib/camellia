#pragma once
#include <stdbool.h>
#include <stdlib.h>

bool fork2_n_mount(const char *path);

void forward_open(handle_t reqh, const char *path, long len, int flags);

void fs_passthru(const char *prefix);
void fs_whitelist(const char **list);
void fs_union(const char **list);

void fs_dir_inject(const char *path);

bool mount_at_pred(const char *path);

// TODO separate fs drivers and wrappers around syscalls

/** like _syscall_fs_wait, but ensures *buf is a null terminated string on VFSOP_OPEN */
handle_t ufs_wait(char *buf, size_t len, struct ufs_request *req);

/** Mounts something and injects its path into the fs */
#define MOUNT_AT(path) for (; mount_at_pred(path); exit(1))
