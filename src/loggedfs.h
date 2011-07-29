/*****************************************************************************
 * Author: Victor Itkin <victor.itkin@gmail.com>
 *****************************************************************************
 * Copyright (c) 2011, Victor Itkin
 *
 * This library is free software; you can distribute it and/or modify it under
 * the terms of the GNU General Public License (GPL), as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GPL in the file COPYING for more
 * details.
 *
 */

#ifndef LOGGEDFS_H
#define	LOGGEDFS_H

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include <rlog/rlog.h>
#include <rlog/Error.h>
#include <rlog/RLogChannel.h>
#include <rlog/SyslogNode.h>
#include <rlog/StdioNode.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#include <iostream>
#include <string>
#include <sstream>
#include "Config.h"

using namespace std;
using namespace rlog;

static RLogChannel* outChannel = DEF_CHANNEL("out", Log_Info);
static RLogChannel* errChannel = DEF_CHANNEL("err", Log_Info);

static Config config;
static int savefd;
static int fileLog = 0;
static StdioNode* fileLogNode = NULL;

const int MaxFuseArgs = 32;

/* I've added all the following constants for the formatting of the rows in 
 * vision of later externalizing them in the XML configuration.
 * There may be a better way but I don't have time for now.
 */

const string DelimStr = "\t";
const char* Delim = DelimStr.data();

const string RowSuffixStr = DelimStr + "%s" + DelimStr + "%d" + DelimStr +
        "%d" + DelimStr + "%s" + DelimStr + "%s";

const char* RowSuffix = RowSuffixStr.data();

const string HeaderStr = "Time" + DelimStr + "Action" + DelimStr + "Path" +
        DelimStr + "Mode" + DelimStr + "From" + DelimStr + "To" + DelimStr +
        "To UID" + DelimStr + "To GID" + DelimStr + "To User Name" +
        DelimStr + "To Group Name" + DelimStr + "Size" + DelimStr + "Offset" +
        DelimStr + "Result" + DelimStr + "UID" + DelimStr + "PID" + DelimStr +
        "PPID" + DelimStr + "Command Line";

const char* Header = HeaderStr.data();

const string RowPattern1 = DelimStr + "%s" + DelimStr + DelimStr + DelimStr +
        DelimStr + DelimStr + DelimStr + DelimStr + DelimStr + DelimStr;

const string RowPattern2 = DelimStr + "%s" + DelimStr + "%o" + DelimStr +
        DelimStr + DelimStr + DelimStr + DelimStr + DelimStr + DelimStr +
        DelimStr;

const string RowPattern3 = DelimStr + "%s" + DelimStr + DelimStr + "%s" +
        DelimStr + DelimStr + DelimStr + DelimStr + DelimStr + DelimStr +
        DelimStr;

const string RowPattern4 = DelimStr + "%s" + DelimStr + DelimStr + DelimStr +
        DelimStr + DelimStr + DelimStr + DelimStr + DelimStr + "%d" + DelimStr +
        "%d";

const string GetAttrRowStr = DelimStr + "getattr" + RowPattern1;
const char* GetAttrRow = GetAttrRowStr.data();

const string AccessRowStr = DelimStr + "access" + RowPattern1;
const char* AccessRow = AccessRowStr.data();

const string ReadLinkRowStr = DelimStr + "readlink" + RowPattern1;
const char* ReadLinkRow = ReadLinkRowStr.data();

const string ReadDirRowStr = DelimStr + "readdir" + RowPattern1;
const char* ReadDirRow = ReadDirRowStr.data();

const string MkFileRowStr = DelimStr + "mkfile" + RowPattern2;
const char* MkFileRow = MkFileRowStr.data();

const string MkFifoRowStr = DelimStr + "mkfifo" + RowPattern2;
const char* MkFifoRow = MkFifoRowStr.data();

const string MkCharRowStr = DelimStr + "mkchar" + RowPattern2;
const char* MkCharRow = MkCharRowStr.data();

const string MkNodeRowStr = DelimStr + "mknod" + RowPattern2;
const char* MkNodeRow = MkNodeRowStr.data();

const string MkDirRowStr = DelimStr + "mkdir" + RowPattern2;
const char* MkDirRow = MkDirRowStr.data();

const string UnlinkRowStr = DelimStr + "unlink" + RowPattern1;
const char* UnlinkRow = UnlinkRowStr.data();

const string RmDirRowStr = DelimStr + "rmdir" + RowPattern1;
const char* RmDirRow = RmDirRowStr.data();

const string SymLinkRowStr = DelimStr + "symlink" + RowPattern3;
const char* SymLinkRow = SymLinkRowStr.data();

const string RenameRowStr = DelimStr + "rename" + DelimStr + "%s" + DelimStr +
        DelimStr + DelimStr + "%s" + DelimStr + DelimStr + DelimStr + DelimStr +
        DelimStr + DelimStr;

const char* RenameRow = RenameRowStr.data();

const string LinkRowStr = DelimStr + "link" + DelimStr + DelimStr + "%s" +
        DelimStr + DelimStr + DelimStr + "%s" + DelimStr + DelimStr + DelimStr +
        DelimStr + DelimStr;

const char* LinkRow = LinkRowStr.data();

const string ChModRowStr = DelimStr + "chmod" + RowPattern2;
const char* ChModRow = ChModRowStr.data();

const string ChOwnWithNamesRowStr = DelimStr + "chown" + DelimStr + "%s" +
        DelimStr + DelimStr + DelimStr + DelimStr + "%d" + DelimStr + "%d" +
        DelimStr + "%s" + DelimStr + "%s" + DelimStr + DelimStr;

const char* ChOwnWithNamesRow = ChOwnWithNamesRowStr.data();

const string ChOwnRowStr = DelimStr + "chown" + DelimStr + "%s" + DelimStr +
        DelimStr + DelimStr + DelimStr + "%d" + DelimStr + "%d" + DelimStr +
        DelimStr + DelimStr + DelimStr;

const char* ChOwnRow = ChOwnRowStr.data();

const string TruncateRowStr = DelimStr + "truncate" + DelimStr + "%s" +
        DelimStr + DelimStr + DelimStr + DelimStr + DelimStr + DelimStr +
        DelimStr + DelimStr + "%d" + DelimStr;

const char* TruncateRow = TruncateRowStr.data();

const string UTimeRowStr = DelimStr + "utime" + RowPattern1;
const char* UTimeRow = UTimeRowStr.data();

const string UTimeNsRowStr = DelimStr + "utimens" + RowPattern1;
const char* UTimeNsRow = UTimeNsRowStr.data();

const string OpenReadOnlyRowStr = DelimStr + "open-readonly" + RowPattern1;
const char* OpenReadOnlyRow = OpenReadOnlyRowStr.data();

const string OpenWriteOnlyRowStr = DelimStr + "open-writeonly" + RowPattern1;
const char* OpenWriteOnlyRow = OpenWriteOnlyRowStr.data();

const string OpenReadWriteRowStr = DelimStr + "open-readwrite" + RowPattern1;
const char* OpenReadWriteRow = OpenReadWriteRowStr.data();

const string OpenRowStr = DelimStr + "open" + RowPattern1;
const char* OpenRow = OpenRowStr.data();

const string ReadRequestRowStr = DelimStr + "read-request" + RowPattern4;
const char* ReadRequestRow = ReadRequestRowStr.data();

const string ReadRowStr = DelimStr + "read" + RowPattern4;
const char* ReadRow = ReadRowStr.data();

const string WriteRequestRowStr = DelimStr + "write-request" + RowPattern4;
const char* WriteRequestRow = WriteRequestRowStr.data();

const string WriteRowStr = DelimStr + "write" + RowPattern4;
const char* WriteRow = WriteRowStr.data();

const string StatFsRowStr = DelimStr + "statfs" + RowPattern1;
const char* StatFsRow = StatFsRowStr.data();

/* Added the struct as a comment in case if needed for later
 
struct Fields
{
    char* action;
    char* path;
    mode_t mode;
    char* from;
    char* to;
    uid_t toUid;
    gid_t toGid;
    char* toUserName;
    char* toGroupName;
    size_t size;
    off_t offset;
    char* result;
    pid_t pid;
    char* ppid;
    char* commandLine;
    uid_t uid;
};*/

struct LoggedFS_Args {
    char* mountPoint; // where the users read files
    char* configFilename;
    bool isDaemon; // true == spawn in background, log to syslog
    const char* fuseArgv[MaxFuseArgs];
    int fuseArgc;
};

static LoggedFS_Args* loggedfsArgs = new LoggedFS_Args;

// Declare "C" function types
extern "C" {
    typedef void* loggedFS_init_t(struct fuse_conn_info* info);
    typedef int loggedFS_getattr_t(const char* path, struct stat* stbuf);
    typedef int loggedFS_access_t(const char *path, int mask);
    typedef int loggedFS_readlink_t(const char* path, char* buf, size_t size);

    typedef int loggedFS_readdir_t(const char* path, void* buf,
            fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info* fi);

    typedef int loggedFS_mknod_t(const char* path, mode_t mode, dev_t rdev);
    typedef int loggedFS_mkdir_t(const char* path, mode_t mode);
    typedef int loggedFS_symlink_t(const char* from, const char* to);
    typedef int loggedFS_unlink_t(const char* path);
    typedef int loggedFS_rmdir_t(const char* path);
    typedef int loggedFS_rename_t(const char* from, const char* to);
    typedef int loggedFS_link_t(const char* from, const char* to);
    typedef int loggedFS_chmod_t(const char* path, mode_t mode);
    typedef int loggedFS_chown_t(const char* path, uid_t uid, gid_t gid);
    typedef int loggedFS_truncate_t(const char* path, off_t size);

#if (FUSE_USE_VERSION==25)
    typedef int loggedFS_utime_t(const char *path, struct utimbuf *buf);
#else
    typedef int loggedFS_utimens_t(const char* path, const struct timespec ts[2]);
#endif

    typedef int loggedFS_open_t(const char* path, struct fuse_file_info* fi);

    typedef int loggedFS_read_t(const char* path, char* buf, size_t size,
            off_t offset, struct fuse_file_info* fi);

    typedef int loggedFS_write_t(const char* path, const char* buf, size_t size,
            off_t offset, struct fuse_file_info* fi);

    typedef int loggedFS_statfs_t(const char* path, struct statvfs* stbuf);
    typedef int loggedFS_release_t(const char* path, struct fuse_file_info* fi);

    typedef int loggedFS_fsync_t(const char* path, int isdatasync,
            struct fuse_file_info* fi);

#ifdef HAVE_SETXATTR
    typedef int loggedFS_setxattr_t(const char* path, const char* name, const
            char* value, size_t size, int flags);

    typedef int loggedFS_getxattr_t(const char* path, const char* name,
            char* value, size_t size);

    typedef int loggedFS_listxattr_t(const char* path, char* list, size_t size);

    typedef int loggedFS_removexattr_t(const char* path, const char* name);
#endif
}

static loggedFS_init_t loggedFS_init;
static loggedFS_getattr_t loggedFS_getattr;
static loggedFS_access_t loggedFS_access;
static loggedFS_readlink_t loggedFS_readlink;
static loggedFS_readdir_t loggedFS_readdir;
static loggedFS_mknod_t loggedFS_mknod;
static loggedFS_mkdir_t loggedFS_mkdir;
static loggedFS_symlink_t loggedFS_symlink;
static loggedFS_unlink_t loggedFS_unlink;
static loggedFS_rmdir_t loggedFS_rmdir;
static loggedFS_rename_t loggedFS_rename;
static loggedFS_link_t loggedFS_link;
static loggedFS_chmod_t loggedFS_chmod;
static loggedFS_chown_t loggedFS_chown;
static loggedFS_truncate_t loggedFS_truncate;

#if (FUSE_USE_VERSION==25)
static loggedFS_utime_t loggedFS_utime;
#else
static loggedFS_utimens_t loggedFS_utimens;
#endif

static loggedFS_open_t loggedFS_open;
static loggedFS_read_t loggedFS_read;
static loggedFS_write_t loggedFS_write;
static loggedFS_statfs_t loggedFS_statfs;
static loggedFS_release_t loggedFS_release;
static loggedFS_fsync_t loggedFS_fsync;

#ifdef HAVE_SETXATTR
static loggedFS_setxattr_t loggedFS_setxattr;
static loggedFS_getxattr_t loggedFS_getxattr;
static loggedFS_listxattr_t loggedFS_listxattr;
static loggedFS_removexattr_t loggedFS_removexattr;
#endif

#endif	/* LOGGEDFS_H */
