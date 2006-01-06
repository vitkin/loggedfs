/*****************************************************************************
 * Author:   Rémi Flament <remipouak@yahoo.fr>
 *****************************************************************************
 * Copyright (c) 2005, R�i Flament
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

/* Almost the whole code has been recopied from encfs source code and from fusexmp.c
*/

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
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

#include "Config.h"

#define PUSHARG(ARG) \
rAssert(out->fuseArgc < MaxFuseArgs); \
out->fuseArgv[out->fuseArgc++] = ARG

using namespace std;
using namespace rlog;


static RLogChannel *Info = DEF_CHANNEL("info", Log_Info);
static Config config;

const int MaxFuseArgs = 32;
struct LoggedFS_Args
{
    char* backend; // where the files are stored
    char* mountPoint; // where the users read files
    char* configFilename;
    bool isDaemon; // true == spawn in background, log to syslog
    const char *fuseArgv[MaxFuseArgs];
    int fuseArgc;
};


static LoggedFS_Args *loggedfsArgs = new LoggedFS_Args;


static bool should_log(const char* path,int uid,const char* action)
{
    return config.shouldLog(path,uid,action);
}

static bool isAbsolutePath( const char *fileName )
{
    if(fileName && fileName[0] != '\0' && fileName[0] == '/')
        return true;
    else
        return false;
}

static char* getAbsolutePath(const char *path)
{
    char *realPath=new char[strlen(path)+strlen(loggedfsArgs->backend)+1];
    strcpy(realPath,loggedfsArgs->backend);
    if (realPath[strlen(realPath)-1]=='/')
        realPath[strlen(realPath)-1]='\0';
    strcat(realPath,path);
    return realPath;

}

/*
 * Returns the name of the process which accessed the file system.
 */
static char* getcallername()
{
    char filename[100];
    sprintf(filename,"/proc/%d/cmdline",fuse_get_context()->pid);
    FILE * proc=fopen(filename,"rt");
    char cmdline[256]="";
    fread(cmdline,sizeof(cmdline),1,proc);
    fclose(proc);
    return strdup(cmdline);
}

static void loggedfs_log(const char* path,const char* action,const char *format,...)
{
    if (should_log(path,fuse_get_context()->uid,action))
    {
        va_list args;
        char buf[1024];
        va_start(args,format);
        memset(buf,0,1024);
        vsprintf(buf,format,args);
        strcat(buf," [ Process id %d (%s) (uid %d) ]");
        rLog(Info, buf, fuse_get_context()->pid,getcallername(), fuse_get_context()->uid);
        va_end( args );

    }
}

static int loggedFS_getattr(const char *path, struct stat *stbuf)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"getattr","getattr %s",path);
    res = lstat(path, stbuf);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_readlink(const char *path, char *buf, size_t size)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"readlink","readlink %s",path);
    res = readlink(path, buf, size - 1);
    if(res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

static int loggedFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;

    (void) offset;
    (void) fi;



    path=getAbsolutePath(path);
    loggedfs_log( path,"readdir","List of directory %s",path);



    dp = opendir(path);
    if(dp == NULL)
        return -errno;

    while((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    return 0;
}

static int loggedFS_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"mknod","mknod %s",path);
    res = mknod(path, mode, rdev);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_mkdir(const char *path, mode_t mode)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"mkdir","mkdir %s",path);
    res = mkdir(path, mode);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_unlink(const char *path)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"unlink","unlink %s",path);
    res = unlink(path);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_rmdir(const char *path)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"rmdir","rmdir %s",path);
    res = rmdir(path);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_symlink(const char *from, const char *to)
{
    int res;
    //path=getAbsolutePath(path);
    loggedfs_log( from,"symlink","symlink from %s to %s",from,to);
    res = symlink(from, to);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_rename(const char *from, const char *to)
{
    int res;
    //path=getAbsolutePath(path);
    loggedfs_log( from,"rename","rename from %s tp %s",from,to);
    res = rename(from, to);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_link(const char *from, const char *to)
{
    int res;

    loggedfs_log( from,"link","link from %s to %s",from,to);
    res = link(from, to);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_chmod(const char *path, mode_t mode)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"chmod","chmod %s",path);
    res = chmod(path, mode);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"chown","chown %s",path);
    res = lchown(path, uid, gid);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_truncate(const char *path, off_t size)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"truncate","truncate %s",path);
    res = truncate(path, size);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_utime(const char *path, struct utimbuf *buf)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"utime","utime %s",path);
    res = utime(path, buf);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_open(const char *path, struct fuse_file_info *fi)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log(path,"open", "Opening of %s",path);
    res = open(path, fi->flags);
    if(res == -1)
        return -errno;

    close(res);
    return 0;
}

static int loggedFS_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi)
{
    int fd;
    int res;


    path=getAbsolutePath(path);
    loggedfs_log(path,"read", "Attempt to read %d bytes from %s ",size,path);
    (void) fi;
    fd = open(path, O_RDONLY);
    if(fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if(res == -1)
        res = -errno;
    else loggedfs_log(path,"read", "%d bytes read from %s",res,path);

    close(fd);
    return res;
}

static int loggedFS_write(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi)
{
    int fd;
    int res;
    path=getAbsolutePath(path);
    loggedfs_log(path,"write", "Attempt to write %d bytes to %s",size,path);
    (void) fi;
    fd = open(path, O_WRONLY);
    if(fd == -1)
        return -errno;

    res = pwrite(fd, buf, size, offset);
    if(res == -1)
        res = -errno;
    else loggedfs_log(path,"write", "%d bytes written to %s",res,path);

    close(fd);
    return res;
}

static int loggedFS_statfs(const char *path, struct statfs *stbuf)
{
    int res;
    path=getAbsolutePath(path);
    loggedfs_log( path,"statfs","statfs %s",path);
    res = statfs(path, stbuf);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_release(const char *path, struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */

    (void) path;
    (void) fi;
    return 0;
}

static int loggedFS_fsync(const char *path, int isdatasync,
                          struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */

    (void) path;
    (void) isdatasync;
    (void) fi;
    return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int loggedFS_setxattr(const char *path, const char *name, const char *value,
                             size_t size, int flags)
{
    int res = lsetxattr(path, name, value, size, flags);
    if(res == -1)
        return -errno;
    return 0;
}

static int loggedFS_getxattr(const char *path, const char *name, char *value,
                             size_t size)
{
    int res = lgetxattr(path, name, value, size);
    if(res == -1)
        return -errno;
    return res;
}

static int loggedFS_listxattr(const char *path, char *list, size_t size)
{
    int res = llistxattr(path, list, size);
    if(res == -1)
        return -errno;
    return res;
}

static int loggedFS_removexattr(const char *path, const char *name)
{
    int res = lremovexattr(path, name);
    if(res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static
bool processArgs(int argc, char *argv[], LoggedFS_Args *out)
{
    // set defaults
    out->isDaemon = true;

    out->fuseArgc = 0;
    out->configFilename=NULL;



    // pass executable name through
    out->fuseArgv[0] = argv[0];
    ++out->fuseArgc;

    // leave a space for mount point, as FUSE expects the mount point before
    // any flags
    out->fuseArgv[1] = NULL;
    ++out->fuseArgc;
    opterr = 0;

    int res;

    while ((res = getopt (argc, argv, "pfc:")) != -1)
    {
        switch (res)
        {

        case 'f':
            out->isDaemon = false;
            // this option was added in fuse 2.x
            PUSHARG("-f");
            break;
	case 'p':
	    PUSHARG("-o");
	    PUSHARG("allow_other");
	    break;
        case 'c':
            out->configFilename=optarg;
            break;
        default:

            break;
        }
    }

    if(optind+2 <= argc)
    {
        out->backend = argv[optind++];
        out->mountPoint = argv[optind++];
        out->fuseArgv[1] = out->mountPoint;
    }
    else
    {
        fprintf(stderr,"Missing options\n");
        return false;
    }

    // If there are still extra unparsed arguments, pass them onto FUSE..
    if(optind < argc)
    {
        rAssert(out->fuseArgc < MaxFuseArgs);

        while(optind < argc)
        {
            rAssert(out->fuseArgc < MaxFuseArgs);
            out->fuseArgv[out->fuseArgc++] = argv[optind];
            ++optind;
        }
    }

    if(!isAbsolutePath( out->backend ) || !isAbsolutePath( out->mountPoint ))

    {
        fprintf(stderr,"You must use absolute paths "
                "(beginning with '/')\n");
        return false;
    }



    return true;
}


int main(int argc, char *argv[])
{
    RLogInit( argc, argv );
    StdioNode* stdLog=new StdioNode( STDERR_FILENO );
    stdLog->subscribeTo( RLOG_CHANNEL("") );
    SyslogNode *logNode = NULL;
    


    umask(0);
    fuse_operations loggedFS_oper;
    // in case this code is compiled against a newer FUSE library and new
    // members have been added to fuse_operations, make sure they get set to
    // 0..
    memset(&loggedFS_oper, 0, sizeof(fuse_operations));
    loggedFS_oper.getattr	= loggedFS_getattr;
    loggedFS_oper.readlink	= loggedFS_readlink;
    loggedFS_oper.readdir	= loggedFS_readdir;
    loggedFS_oper.mknod	= loggedFS_mknod;
    loggedFS_oper.mkdir	= loggedFS_mkdir;
    loggedFS_oper.symlink	= loggedFS_symlink;
    loggedFS_oper.unlink	= loggedFS_unlink;
    loggedFS_oper.rmdir	= loggedFS_rmdir;
    loggedFS_oper.rename	= loggedFS_rename;
    loggedFS_oper.link	= loggedFS_link;
    loggedFS_oper.chmod	= loggedFS_chmod;
    loggedFS_oper.chown	= loggedFS_chown;
    loggedFS_oper.truncate	= loggedFS_truncate;
    loggedFS_oper.utime	= loggedFS_utime;
    loggedFS_oper.open	= loggedFS_open;
    loggedFS_oper.read	= loggedFS_read;
    loggedFS_oper.write	= loggedFS_write;
    loggedFS_oper.statfs	= loggedFS_statfs;
    loggedFS_oper.release	= loggedFS_release;
    loggedFS_oper.fsync	= loggedFS_fsync;
#ifdef HAVE_SETXATTR
    loggedFS_oper.setxattr	= loggedFS_setxattr;
    loggedFS_oper.getxattr	= loggedFS_getxattr;
    loggedFS_oper.listxattr	= loggedFS_listxattr;
    loggedFS_oper.removexattr= loggedFS_removexattr;
#endif


    for(int i=0; i<MaxFuseArgs; ++i)
        loggedfsArgs->fuseArgv[i] = NULL; // libfuse expects null args..

    if (processArgs(argc, argv, loggedfsArgs))
    {
	if (loggedfsArgs->isDaemon)
	{
		logNode = new SyslogNode( "loggedfs" );
		logNode->subscribeTo( RLOG_CHANNEL("") );
		// disable stderr reporting..
		delete stdLog;
		stdLog = NULL;
	}
	rLog(Info, "LoggedFS starting at %s.",loggedfsArgs->mountPoint);
        if (loggedfsArgs->configFilename!=NULL)
        {
            rLog(Info, "Using configuration file %s.",loggedfsArgs->configFilename);
            config.loadFromXml(loggedfsArgs->configFilename);
        }
        return fuse_main(loggedfsArgs->fuseArgc,
                         const_cast<char**>(loggedfsArgs->fuseArgv), &loggedFS_oper);
    }
}
