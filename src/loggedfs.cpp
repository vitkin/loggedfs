/*******************************************************************************
 * Author:   Remi Flament <rflament at laposte dot net>
 *******************************************************************************
 * Copyright (c) 2005 - 2008, Remi Flament
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

/*******************************************************************************
 * Modifications: Victor Itkin <victor.itkin@gmail.com>
 *******************************************************************************
 */

/* Almost the whole code has been recopied from encfs source code and from 
 * fusexmp.c
 */

#ifdef linux
/* For pread()/pwrite() */
#define _X_SOURCE 500
#endif

#include "loggedfs.h"

#define PUSHARG(ARG) \
rAssert(out->fuseArgc < MaxFuseArgs); \
out->fuseArgv[out->fuseArgc++] = ARG

/*******************************************************************************
 */
static void openLogFile(const char* filename)
{
    FILE* pFileLog = fopen(filename, "w");
    fprintf(pFileLog, "%s\n", Header);
    fclose(pFileLog);

    fileLog = open(filename, O_WRONLY | O_CREAT | O_APPEND);
    fileLogNode = new StdioNode(fileLog, 16);
    fileLogNode->subscribeTo(RLOG_CHANNEL("out"));
    rLog(errChannel, "LoggedFS log file: %s", filename);
}

/*******************************************************************************
 */
static bool isAbsolutePath(const char* fileName)
{
    if (fileName && fileName[0] != '\0' && fileName[0] == '/')
        return true;
    else
        return false;
}

/*******************************************************************************
 * Return a dynamically allocated char* for the path.
 * Up to the caller to delete it once used.
 */
static char* getAbsolutePath(const char* path)
{
    char* realPath =
            new char[strlen(path) + strlen(loggedfsArgs->mountPoint) + 1];

    strcpy(realPath, loggedfsArgs->mountPoint);

    if (realPath[strlen(realPath) - 1] == '/')
        realPath[strlen(realPath) - 1] = '\0';

    return strcat(realPath, path);
}

/*******************************************************************************
 * Return a dynamically allocated char* for the path.
 * Up to the caller to delete it once used.
 */
static char* getRelativePath(const char* path)
{
    char* rPath = new char[strlen(path) + 2];

    strcpy(rPath, ".");

    return strcat(rPath, path);
}

/*******************************************************************************
 * Returns the command line of the process which accessed the file system.
 */
static string* getCallerCmdLine()
{
    stringstream ss;
    ss << "/proc/" << fuse_get_context()->pid << "/cmdline";

    string filename = ss.str();

    FILE* cmdline = fopen(filename.data(), "rb");

    char* arg = NULL;
    size_t size = 0;
    string* commandLine = new string();

    while (getdelim(&arg, &size, 0, cmdline) != -1)
    {
        char* token, * str1, * saveptr1;
        int j;

        for (j = 1, str1 = arg;; j++, str1 = NULL)
        {
            char* subtoken, * str2, * saveptr2;

            token = strtok_r(str1, "\n", &saveptr1);

            if (token == NULL)
                break;

            if (str1 == NULL)
                (*commandLine).append("~N~");

            for (str2 = token;; str2 = NULL)
            {
                subtoken = strtok_r(str2, Delim, &saveptr2);

                if (subtoken == NULL)
                    break;

                if (str2 == NULL)
                    (*commandLine).append("~T~");

                (*commandLine).append(subtoken);
            }
        }

        (*commandLine).append(" ");
    }

    free(arg);
    fclose(cmdline);

    return commandLine;
}

/*******************************************************************************
 * Returns the PPID of the process which accessed the file system.
 */
static string* getCallerPpid()
{
    stringstream ss;
    ss << "/proc/" << fuse_get_context()->pid << "/stat";

    string filename = ss.str();

    FILE* stat = fopen(filename.data(), "rt");

    char buf[256] = "";
    fread(buf, sizeof (buf), 1, stat);

    string buffer = buf;
    //free(buf);

    int pos = 0;

    for (int i = 0; i != 3; i++)
    {
        pos = buffer.find(" ", pos) + 1;
    }

    string subStr = buffer.substr(pos, buffer.find(" ", pos) - pos);

    string* callerPpid = new string(subStr);

    fclose(stat);

    return callerPpid;
}

/*******************************************************************************
 *
 */
static void loggedfs_log(const char* path, const char* action,
                         const int returncode, const char* format, ...)
{
    const char* retname;

    if (returncode >= 0)
        retname = "SUCCESS";
    else
        retname = "FAILURE";

    if (config.shouldLog(path, fuse_get_context()->uid, action, retname))
    {
        va_list args;
        char buf[1024];
        va_start(args, format);
        memset(buf, 0, 1024);
        vsprintf(buf, format, args);
        strcat(buf, RowSuffixStr.data());
        bool pNameEnabled = config.isPrintProcessNameEnabled();

        string* callerPpid;
        string* callerCmdline;

        if (pNameEnabled)
        {
            callerPpid = getCallerPpid();
            callerCmdline = getCallerCmdLine();
        }
        else
        {
            callerPpid = new string();
            callerCmdline = new string();
        }

        rLog(outChannel, buf, retname,
             fuse_get_context()->uid,
             fuse_get_context()->pid,
             (*callerPpid).data(),
             (*callerCmdline).data());

        delete callerPpid;
        delete callerCmdline;

        va_end(args);
    }
}

/*******************************************************************************
 *
 */
static void* loggedFS_init(struct fuse_conn_info* info)
{
    fchdir(savefd);
    close(savefd);

    return NULL;
}

/*******************************************************************************
 *
 */
static int loggedFS_getattr(const char* path, struct stat* stbuf)
{
    int res;

    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    res = lstat(rPath, stbuf);
    loggedfs_log(aPath, "getattr", res, GetAttrRow, aPath);
    free(aPath);
    free(rPath);

    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_access(const char *path, int mask)
{
    int res;

    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);

    res = access(rPath, mask);
    free(rPath);

    loggedfs_log(aPath, "access", res, AccessRow, aPath);
    free(aPath);


    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_readlink(const char* path, char* buf, size_t size)
{
    int res;

    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);

    res = readlink(rPath, buf, size - 1);
    free(rPath);

    loggedfs_log(aPath, "readlink", res, ReadLinkRow, aPath);
    free(aPath);

    if (res == -1)
        return -errno;

    buf[res] = '\0';

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info* fi)
{
    DIR* dp;
    struct dirent* de;
    int res;

    (void) offset;
    (void) fi;

    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);

    dp = opendir(rPath);

    free(rPath);

    if (dp == NULL)
    {
        res = -errno;
        loggedfs_log(aPath, "readdir", -1, ReadDirRow, aPath);
        free(aPath);

        return res;
    }

    while ((de = readdir(dp)) != NULL)
    {
        struct stat st;
        memset(&st, 0, sizeof (st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    loggedfs_log(aPath, "readdir", 0, ReadDirRow, aPath);
    free(aPath);

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_mknod(const char* path, mode_t mode, dev_t rdev)
{
    int res;
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);

    if (S_ISREG(mode))
    {
        res = open(rPath, O_CREAT | O_EXCL | O_WRONLY, mode);
        loggedfs_log(aPath, "mkfile", res, MkFileRow, aPath, mode);

        if (res >= 0)
            res = close(res);
    }
    else if (S_ISFIFO(mode))
    {
        res = mkfifo(rPath, mode);
        loggedfs_log(aPath, "mkfifo", res, MkFifoRow, aPath, mode);
    }
    else
    {
        res = mknod(rPath, mode, rdev);

        if (S_ISCHR(mode))
        {
            loggedfs_log(aPath, "mkchar", res, MkCharRow, aPath, mode);
        }
            /*else if (S_IFBLK(mode))
            {
                loggedfs_log(aPath, "mkblk", res,
                             "mkblk %s %o (block device creation)", aPath, mode);
            }*/
        else
            loggedfs_log(aPath, "mknod", res, MkNodeRow, aPath, mode);
    }

    free(aPath);

    if (res == -1)
    {
        free(rPath);

        return -errno;
    }
    else
    {
        lchown(rPath, fuse_get_context()->uid, fuse_get_context()->gid);
    }

    free(rPath);

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_mkdir(const char* path, mode_t mode)
{
    int res;
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    res = mkdir(rPath, mode);
    char* relPath = getRelativePath(aPath);
    loggedfs_log(relPath, "mkdir", res, MkDirRow, aPath, mode);
    free(relPath);
    free(aPath);

    if (res == -1)
    {
        free(rPath);

        return -errno;
    }
    else
        lchown(rPath, fuse_get_context()->uid, fuse_get_context()->gid);

    free(rPath);

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_unlink(const char* path)
{
    int res;
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    res = unlink(rPath);
    loggedfs_log(aPath, "unlink", res, UnlinkRow, aPath);

    free(aPath);
    free(rPath);

    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_rmdir(const char* path)
{
    int res;
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    res = rmdir(rPath);
    loggedfs_log(aPath, "rmdir", res, RmDirRow, aPath);

    free(aPath);
    free(rPath);

    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_symlink(const char* from, const char* to)
{
    int res;

    char* aTo = getAbsolutePath(to);
    char* rTo = getRelativePath(to);

    res = symlink(from, rTo);

    loggedfs_log(aTo, "symlink", res, SymLinkRow, aTo, from);

    free(aTo);

    if (res == -1)
    {
        free(rTo);

        return -errno;
    }
    else
        lchown(rTo, fuse_get_context()->uid, fuse_get_context()->gid);

    free(rTo);

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_rename(const char* from, const char* to)
{
    int res;
    char* aFrom = getAbsolutePath(from);
    char* aTo = getAbsolutePath(to);
    char* rFrom = getRelativePath(from);
    char* rTo = getRelativePath(to);

    res = rename(rFrom, rTo);

    free(rFrom);
    free(rTo);

    loggedfs_log(aFrom, "rename", res, RenameRow, aFrom, aTo);

    free(aFrom);
    free(aTo);

    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_link(const char* from, const char* to)
{
    int res;

    char* aFrom = getAbsolutePath(from);
    char* aTo = getAbsolutePath(to);
    char* rFrom = getRelativePath(from);
    char* rTo = getRelativePath(to);

    res = link(rFrom, rTo);

    free(rFrom);

    loggedfs_log(aTo, "link", res, LinkRow, aTo, aFrom);

    free(aFrom);

    if (res == -1)
    {
        free(aTo);
        free(rTo);

        return -errno;
    }
    else
        lchown(rTo, fuse_get_context()->uid, fuse_get_context()->gid);

    free(aTo);
    free(rTo);

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_chmod(const char* path, mode_t mode)
{
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    int res = chmod(rPath, mode);
    loggedfs_log(aPath, "chmod", res, ChModRow, aPath, mode);

    free(aPath);
    free(rPath);

    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_chown(const char* path, uid_t uid, gid_t gid)
{
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    int res = lchown(rPath, uid, gid);

    free(rPath);

    // User record from the password database
    struct passwd* p = getpwuid(uid);

    // Group record from the group database
    struct group* g = getgrgid(gid);

    if (p != NULL && g != NULL)
    {
        loggedfs_log(aPath, "chown", res, ChOwnWithNamesRow, aPath, uid, gid,
                     p->pw_name, g->gr_name);
    }
    else
        loggedfs_log(aPath, "chown", res, ChOwnRow, aPath, uid, gid);

    free(aPath);

    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_truncate(const char* path, off_t size)
{
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    int res = truncate(rPath, size);

    free(rPath);

    loggedfs_log(aPath, "truncate", res, TruncateRow, aPath, size);

    free(aPath);

    if (res == -1)
        return -errno;

    return 0;
}

#if (FUSE_USE_VERSION==25)

/*******************************************************************************
 *
 */
static int loggedFS_utime(const char *path, struct utimbuf *buf)
{
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    int res = utime(rPath, buf);

    free(rPath);

    loggedfs_log(aPath, "utime", res, UTimeRow, aPath);

    free(aPath);

    if (res == -1)
        return -errno;

    return 0;
}

#else

/*******************************************************************************
 *
 */
static int loggedFS_utimens(const char* path, const struct timespec ts[2])
{
    int res;
    struct timeval tv[2];

    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    res = utimes(rPath, tv);

    free(rPath);

    loggedfs_log(aPath, "utimens", res, UTimeNsRow, aPath);

    free(aPath);

    if (res == -1)
        return -errno;

    return 0;
}

#endif

/*******************************************************************************
 *
 */
static int loggedFS_open(const char* path, struct fuse_file_info* fi)
{
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    int res = open(rPath, fi->flags);

    free(rPath);

    // what type of open ? read, write, or read-write ?
    if (fi->flags & O_RDONLY)
    {
        loggedfs_log(aPath, "open-readonly", res, OpenReadOnlyRow, aPath);
    }
    else if (fi->flags & O_WRONLY)
    {
        loggedfs_log(aPath, "open-writeonly", res, OpenWriteOnlyRow, aPath);
    }
    else if (fi->flags & O_RDWR)
    {
        loggedfs_log(aPath, "open-readwrite", res, OpenReadWriteRow, aPath);
    }
    else
        loggedfs_log(aPath, "open", res, OpenRow, aPath);

    free(aPath);

    if (res == -1)
        return -errno;

    close(res);

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_read(const char* path, char* buf, size_t size, off_t offset,
                         struct fuse_file_info* fi)
{

    int res;
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    (void) fi;

    int fd = open(rPath, O_RDONLY);

    free(rPath);

    if (fd == -1)
    {
        res = -errno;

        loggedfs_log(aPath, "read-request", -1, ReadRequestRow, aPath, size,
                     offset);

        free(aPath);

        return res;
    }
    else
    {
        loggedfs_log(aPath, "read-request", 0, ReadRequestRow, aPath, size,
                     offset);
    }

    res = pread(fd, buf, size, offset);

    if (res == -1)
        res = -errno;
    else
        loggedfs_log(aPath, "read", 0, ReadRow, aPath, res, offset);

    free(aPath);

    close(fd);

    return res;
}

/*******************************************************************************
 *
 */
static int loggedFS_write(const char* path, const char* buf, size_t size,
                          off_t offset, struct fuse_file_info* fi)
{
    int res;
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);
    (void) fi;

    int fd = open(rPath, O_WRONLY);

    free(rPath);

    if (fd == -1)
    {
        res = -errno;

        loggedfs_log(aPath, "write-request", -1, WriteRequestRow, aPath, size,
                     offset);

        free(aPath);

        return res;
    }
    else
    {
        loggedfs_log(aPath, "write-request", 0, WriteRequestRow, aPath, size,
                     offset);
    }

    res = pwrite(fd, buf, size, offset);

    if (res == -1)
        res = -errno;
    else
        loggedfs_log(aPath, "write", 0, WriteRow, aPath, res, offset);

    free(aPath);

    close(fd);

    return res;
}

/*******************************************************************************
 *
 */
static int loggedFS_statfs(const char* path, struct statvfs* stbuf)
{
    char* aPath = getAbsolutePath(path);
    char* rPath = getRelativePath(path);

    int res = statvfs(rPath, stbuf);

    free(rPath);

    loggedfs_log(aPath, "statfs", res, StatFsRow, aPath);

    free(aPath);

    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 * Just a stub. This method is optional and can safely be left unimplemented
 */
static int loggedFS_release(const char* path, struct fuse_file_info* fi)
{
    (void) path;
    (void) fi;

    return 0;
}

/*******************************************************************************
 * Just a stub. This method is optional and can safely be left unimplemented
 */
static int loggedFS_fsync(const char* path, int isdatasync,
                          struct fuse_file_info* fi)
{
    (void) path;
    (void) isdatasync;
    (void) fi;

    return 0;
}

#ifdef HAVE_SETXATTR

/* xattr operations are optional and can safely be left unimplemented */

/*******************************************************************************
 *
 */
static int loggedFS_setxattr(const char* path, const char* name, const
                             char* value, size_t size, int flags)
{
    int res = lsetxattr(path, name, value, size, flags);

    if (res == -1)
        return -errno;

    return 0;
}

/*******************************************************************************
 *
 */
static int loggedFS_getxattr(const char* path, const char* name, char* value,
                             size_t size)
{
    int res = lgetxattr(path, name, value, size);

    if (res == -1)
        return -errno;

    return res;
}

/*******************************************************************************
 *
 */
static int loggedFS_listxattr(const char* path, char* list, size_t size)
{
    int res = llistxattr(path, list, size);

    if (res == -1)
        return -errno;

    return res;
}

/*******************************************************************************
 *
 */
static int loggedFS_removexattr(const char* path, const char* name)
{
    int res = lremovexattr(path, name);

    if (res == -1)
        return -errno;

    return 0;
}
#endif /* HAVE_SETXATTR */

/*******************************************************************************
 *
 */
static void usage(char* name)
{
    fprintf(stderr, "Usage:\n");

    fprintf(stderr,
            "%s [-h] | [-l log-file] [-c config-file] [-f] [-p] [-e] "
            "/directory-mountpoint\n",
            name);

    fprintf(stderr, "Type 'man loggedfs' for more details\n");

    return;
}

/*******************************************************************************
 *
 */
static bool processArgs(int argc, char* argv[], LoggedFS_Args* out)
{
    // set defaults
    out->isDaemon = true;

    out->fuseArgc = 0;
    out->configFilename = NULL;

    // pass executable name through
    out->fuseArgv[0] = argv[0];
    ++out->fuseArgc;

    // leave a space for mount point, as FUSE expects the mount point before
    // any flags
    out->fuseArgv[1] = NULL;
    ++out->fuseArgc;
    opterr = 0;

    int res;

    bool got_p = false;

    // We need the "nonempty" option to mount the directory in recent FUSE's
    // because it's non empty and contains the files that will be logged.
    //
    // We need "use_ino" so the files will use their original inode numbers,
    // instead of all getting 0xFFFFFFFF . For example, this is required for
    // logging the ~/.kde/share/config directory, in which hard links for lock
    // files are verified by their inode equivalency.

#define COMMON_OPTS "nonempty,use_ino"

    string options = "";

    char* str1, * option, * value;
    char* saveptr1, * saveptr2;
    unsigned char count = 0;

    while ((res = getopt(argc, argv, "hpfec:l:o:")) != -1)
    {
        switch (res)
        {
        case 'h':
            usage(argv[0]);

            return false;

        case 'f':
            out->isDaemon = false;
            // this option was added in fuse 2.x
            PUSHARG("-f");
            rLog(errChannel, "LoggedFS not running as a daemon");

            break;

        case 'p':
            PUSHARG("-o");
            PUSHARG("allow_other,default_permissions," COMMON_OPTS);
            got_p = true;
            rLog(errChannel, "LoggedFS running as a public filesystem");

            break;

        case 'e':
            PUSHARG("-o");
            PUSHARG("nonempty");
            rLog(errChannel, "Using existing directory");

            break;

        case 'c':
            out->configFilename = optarg;
            rLog(errChannel, "Configuration file: %s", optarg);

            break;

        case 'l':
            openLogFile(optarg);

            break;

        case 'o':
            // Option added for when used in "/etc/fstab".
            // In that case the default FUSE options COMMON_OPTS are not enabled
            // if they are not explicitly specified - i.e. up to the caller 
            // to add those options.
            // e.g.:
            // loggedfs some_absolute_path fuse 
            // c=xml_configuration_file[,l=csv_file],nonempty,use_ino,
            // allow_other,default_permissions 0 0

            rLog(errChannel, "Using mount options: %s", optarg);

            char* token;

            for (str1 = optarg;; str1 = NULL)
            {
                token = strtok_r(str1, ",", &saveptr1);

                if (token == NULL)
                    break;

                option = strtok_r(token, "=", &saveptr2);

                if (strcmp(option, "c") == 0)
                {
                    value = strtok_r(NULL, "=", &saveptr2);

                    if (value != NULL)
                    {
                        out->configFilename = value;
                        rLog(errChannel, "Configuration file: %s", value);
                    }
                }
                else if (strcmp(option, "l") == 0)
                {
                    value = strtok_r(NULL, "=", &saveptr2);

                    if (value != NULL)
                    {
                        openLogFile(value);
                    }
                }
                else
                {
                    if (count++ > 0)
                        options.append(",");

                    options.append(token);
                }
            }

            PUSHARG("-o");
            PUSHARG(strdup(options.data()));
            got_p = true;

            rLog(errChannel, "Setting FUSE options: %s",
                 options.data());

            break;

        default:
            break;
        }
    }

    if (!got_p)
    {
        PUSHARG("-o");
        PUSHARG(COMMON_OPTS);
    }
#undef COMMON_OPTS

    if (optind + 1 <= argc)
    {
        out->mountPoint = argv[optind++];
        out->fuseArgv[1] = out->mountPoint;
    }
    else
    {
        fprintf(stderr, "Missing mountpoint\n");
        usage(argv[0]);

        return false;
    }

    // If there are still extra unparsed arguments, pass them onto FUSE..
    if (optind < argc)
    {
        rAssert(out->fuseArgc < MaxFuseArgs);

        while (optind < argc)
        {
            rAssert(out->fuseArgc < MaxFuseArgs);
            out->fuseArgv[out->fuseArgc++] = argv[optind];
            ++optind;
        }
    }

    if (!isAbsolutePath(out->mountPoint))
    {
        fprintf(stderr, "You must use absolute paths "
                "(beginning with '/') for %s\n", out->mountPoint);

        return false;
    }

    if (fileLog == 0)
    {
        printf("%s\n", Header);

        // Need to force the flush in case of redirection to 
        // force it to be the first line because of the RLog subscriber
        fflush(stdout);
    }

    return true;
}

/*******************************************************************************
 *
 */
int main(int argc, char* argv[])
{
    RLogInit(argc, argv);

    StdioNode* stdOut = new StdioNode(STDOUT_FILENO, 17);
    stdOut->subscribeTo(RLOG_CHANNEL("out"));

    StdioNode* stdErr = new StdioNode(STDERR_FILENO, 17);
    stdErr->subscribeTo(RLOG_CHANNEL("err"));

    // Display the args passed to LoggedFS
    string loggedfsArgsStr = "";

    for (int i = 0; i < argc; i++)
    {
        const char* arg = argv[i];

        loggedfsArgsStr.append(arg);
        loggedfsArgsStr.append(" ");
    }

    rLog(errChannel, "LoggedFS args (%d): %s", argc, loggedfsArgsStr.data());

    SyslogNode* logNode = NULL;

    umask(0);
    fuse_operations loggedFS_oper;

    // in case this code is compiled against a newer FUSE library and new
    // members have been added to fuse_operations, make sure they get set to
    // 0..
    memset(&loggedFS_oper, 0, sizeof (fuse_operations));

    loggedFS_oper.init = loggedFS_init;
    loggedFS_oper.getattr = loggedFS_getattr;
    loggedFS_oper.access = loggedFS_access;
    loggedFS_oper.readlink = loggedFS_readlink;
    loggedFS_oper.readdir = loggedFS_readdir;
    loggedFS_oper.mknod = loggedFS_mknod;
    loggedFS_oper.mkdir = loggedFS_mkdir;
    loggedFS_oper.symlink = loggedFS_symlink;
    loggedFS_oper.unlink = loggedFS_unlink;
    loggedFS_oper.rmdir = loggedFS_rmdir;
    loggedFS_oper.rename = loggedFS_rename;
    loggedFS_oper.link = loggedFS_link;
    loggedFS_oper.chmod = loggedFS_chmod;
    loggedFS_oper.chown = loggedFS_chown;
    loggedFS_oper.truncate = loggedFS_truncate;

#if (FUSE_USE_VERSION==25)
    loggedFS_oper.utime = loggedFS_utime;
#else
    loggedFS_oper.utimens = loggedFS_utimens;
#endif

    loggedFS_oper.open = loggedFS_open;
    loggedFS_oper.read = loggedFS_read;
    loggedFS_oper.write = loggedFS_write;
    loggedFS_oper.statfs = loggedFS_statfs;
    loggedFS_oper.release = loggedFS_release;
    loggedFS_oper.fsync = loggedFS_fsync;

#ifdef HAVE_SETXATTR
    loggedFS_oper.setxattr = loggedFS_setxattr;
    loggedFS_oper.getxattr = loggedFS_getxattr;
    loggedFS_oper.listxattr = loggedFS_listxattr;
    loggedFS_oper.removexattr = loggedFS_removexattr;
#endif

    for (int i = 0; i < MaxFuseArgs; ++i)
        loggedfsArgs->fuseArgv[i] = NULL; // libfuse expects null args..

    if (processArgs(argc, argv, loggedfsArgs))
    {
        if (fileLog != 0)
        {
            delete stdOut;
            stdOut = NULL;
        }

        if (loggedfsArgs->isDaemon)
        {
            logNode = new SyslogNode("loggedfs");

            if (fileLog != 0)
            {
                logNode->subscribeTo(RLOG_CHANNEL("err"));
            }
            else
            {
                logNode->subscribeTo(RLOG_CHANNEL(""));

                // disable stdout reporting...
                delete stdOut;
                stdOut = NULL;
            }

            // disable stderr reporting...
            delete stdErr;
            stdErr = NULL;
        }

        rLog(errChannel, "LoggedFS starting at %s.", loggedfsArgs->mountPoint);

        if (loggedfsArgs->configFilename != NULL)
        {
            if (strcmp(loggedfsArgs->configFilename, "-") == 0)
            {
                rLog(errChannel, "Using stdin configuration");
                char* input = new char[2048]; // 2kB MAX input for configuration
                memset(input, 0, 2048);
                char* ptr = input;

                int size = 0;

                do
                {
                    size = fread(ptr, 1, 1, stdin);
                    ptr++;
                }
                while (!feof(stdin) && size > 0);

                config.loadFromXmlBuffer(input);
                free(input);
            }
            else
            {
                rLog(errChannel, "Using configuration file %s.",
                     loggedfsArgs->configFilename);

                config.loadFromXmlFile(loggedfsArgs->configFilename);
            }
        }

        rLog(errChannel, "chdir to %s", loggedfsArgs->mountPoint);

        chdir(loggedfsArgs->mountPoint);
        savefd = open(".", 0);

        // Display the args passed to FUSE
        string fuseArgs = "";

        for (int i = 0; i < loggedfsArgs->fuseArgc; i++)
        {
            const char* arg = loggedfsArgs->fuseArgv[i];

            fuseArgs.append(arg);
            fuseArgs.append(" ");
        }

        rLog(errChannel, "FUSE args (%d): %s", loggedfsArgs->fuseArgc, fuseArgs.data());

#if (FUSE_USE_VERSION==25)
        fuse_main(loggedfsArgs->fuseArgc,
                  const_cast<char**> (loggedfsArgs->fuseArgv), &loggedFS_oper);
#else
        fuse_main(loggedfsArgs->fuseArgc,
                  const_cast<char**> (loggedfsArgs->fuseArgv), &loggedFS_oper,
                  NULL);
#endif

        delete stdErr;
        stdErr = NULL;

        if (fileLog != 0)
        {
            delete fileLogNode;
            fileLogNode = NULL;
            close(fileLog);
        }
        else
        {
            delete stdOut;
            stdOut = NULL;
        }

        if (loggedfsArgs->isDaemon)
        {
            delete logNode;
            logNode = NULL;
        }

        rLog(errChannel, "LoggedFS closing.");

        delete loggedfsArgs;
    }
}
