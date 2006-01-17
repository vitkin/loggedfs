/*****************************************************************************
 * Author:   Rémi Flament <remipouak@yahoo.fr>
 *****************************************************************************
 * Copyright (c) 2005, Rémi Flament
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
#include <sys/time.h>

#include "Config.h"

#define PUSHARG(ARG) \
rAssert(out->fuseArgc < MaxFuseArgs); \
out->fuseArgv[out->fuseArgc++] = ARG

using namespace std;
using namespace rlog;


static RLogChannel *Info = DEF_CHANNEL("info", Log_Info);
static Config config;
static int savefd;
static int fileLog=0;
static StdioNode* fileLogNode=NULL;

const int MaxFuseArgs = 32;
struct LoggedFS_Args
{
    char* mountPoint; // where the users read files
    char* configFilename;
    bool isDaemon; // true == spawn in background, log to syslog
    const char *fuseArgv[MaxFuseArgs];
    int fuseArgc;
};


static LoggedFS_Args *loggedfsArgs = new LoggedFS_Args;


static bool isAbsolutePath( const char *fileName )
{
    if(fileName && fileName[0] != '\0' && fileName[0] == '/')
        return true;
    else
        return false;
}


static char* getAbsolutePath(const char *path)
{
    char *realPath=new char[strlen(path)+strlen(loggedfsArgs->mountPoint)+1];
    strcpy(realPath,loggedfsArgs->mountPoint);
    if (realPath[strlen(realPath)-1]=='/')
        realPath[strlen(realPath)-1]='\0';
    strcat(realPath,path);
    return realPath;

}

static char* getRelativePath(const char* path)
{
    char* rPath=new char[strlen(path)+2];

    fchdir(savefd); //TODO remove this, we need to do it only once
    strcpy(rPath,".");
    strcat(rPath,path);

    return rPath;
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

static void loggedfs_log(const char* path,const char* action,const int returncode,const char *format,...)
{
    char *retname;
    if (returncode >= 0)
        retname = "SUCCESS";
    else
        retname = "FAILURE";
    if (config.shouldLog(path,fuse_get_context()->uid,action,retname))
    {
        va_list args;
        char buf[1024];
        va_start(args,format);
        memset(buf,0,1024);
        vsprintf(buf,format,args);
        strcat(buf," {%s} [ pid = %d %s uid = %d ]");
        rLog(Info, buf,retname, fuse_get_context()->pid,config.isPrintProcessNameEnabled()?getcallername():"", fuse_get_context()->uid);
        va_end(args);
    }
}

static int loggedFS_getattr(const char *path, struct stat *stbuf)
{
    int res;

    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = lstat(path, stbuf);
    loggedfs_log(aPath,"getattr",res,"getattr %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_readlink(const char *path, char *buf, size_t size)
{
    int res;

    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = readlink(path, buf, size - 1);
    loggedfs_log(aPath,"readlink",res,"readlink %s",aPath);
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
    int res;

    (void) offset;
    (void) fi;

    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);


    dp = opendir(path);
    if(dp == NULL) {
        res = -errno;
        loggedfs_log(aPath,"readdir",-1,"readdir %s",aPath);
        return res;
    }

    while((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    loggedfs_log(aPath,"readdir",0,"readdir %s",aPath);
    return 0;
}

static int loggedFS_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = mknod(path, mode, rdev);
    loggedfs_log(aPath,"mknod",res,"mknod %s",aPath);
    if(res == -1)
        return -errno;
    else
        res = lchown(path, fuse_get_context()->uid, fuse_get_context()->gid);
    return 0;
}

static int loggedFS_mkdir(const char *path, mode_t mode)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = mkdir(path, mode);
    loggedfs_log(getRelativePath(aPath),"mkdir",res,"mkdir %s",aPath);
    if(res == -1)
        return -errno;
    else
        res = lchown(path, fuse_get_context()->uid, fuse_get_context()->gid);

    return 0;
}

static int loggedFS_unlink(const char *path)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = unlink(path);
    loggedfs_log(aPath,"unlink",res,"unlink %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_rmdir(const char *path)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = rmdir(path);
    loggedfs_log(aPath,"rmdir",res,"rmdir %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_symlink(const char *from, const char *to)
{
    int res;
    char *aFrom=getAbsolutePath(from);
    char *aTo=getAbsolutePath(to);
    from=getRelativePath(from);
    to=getRelativePath(to);
    res = symlink(from, to);
    loggedfs_log( aFrom,"symlink",res,"symlink from %s to %s",aFrom,aTo);
    if(res == -1)
        return -errno;
    else
        res = lchown(from, fuse_get_context()->uid, fuse_get_context()->gid);


    return 0;
}

static int loggedFS_rename(const char *from, const char *to)
{
    int res;
    char *aFrom=getAbsolutePath(from);
    char *aTo=getAbsolutePath(to);
    from=getRelativePath(from);
    to=getRelativePath(to);
    res = rename(from, to);
    loggedfs_log( aFrom,"rename",res,"rename from %s to %s",aFrom,aTo);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_link(const char *from, const char *to)
{
    int res;

    char *aFrom=getAbsolutePath(from);
    char *aTo=getAbsolutePath(to);
    from=getRelativePath(from);
    to=getRelativePath(to);
    res = link(from, to);
    loggedfs_log( aFrom,"link",res,"link from %s to %s",aFrom,aTo);
    if(res == -1)
        return -errno;
    else
        res = lchown(from, fuse_get_context()->uid, fuse_get_context()->gid);

    return 0;
}

static int loggedFS_chmod(const char *path, mode_t mode)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = chmod(path, mode);
    loggedfs_log(aPath,"chmod",res,"chmod %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = lchown(path, uid, gid);
    loggedfs_log(aPath,"chown",res,"chown %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_truncate(const char *path, off_t size)
{
    int res;

    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = truncate(path, size);
    loggedfs_log(aPath,"truncate",res,"truncate %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_utime(const char *path, struct utimbuf *buf)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = utime(path, buf);
    loggedfs_log(aPath,"utime",res,"utime %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_open(const char *path, struct fuse_file_info *fi)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = open(path, fi->flags);
    loggedfs_log(aPath,"open",res,"open %s",aPath);
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

    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    (void) fi;

    timeval begin,end;

    gettimeofday(&begin , NULL );
    fd = open(path, O_RDONLY);
    if(fd == -1) {
        res = -errno;
        loggedfs_log(aPath,"read",-1,"read %d bytes from %s",size,aPath);
        return res;
    } else {
        loggedfs_log(aPath,"read",0,"read %d bytes from %s",size,aPath);
    }

    res = pread(fd, buf, size, offset);
    gettimeofday(&end , NULL );

    if(res == -1)
        res = -errno;
    else loggedfs_log(aPath,"read",0, "%d bytes read from %s in %d microseconds",aPath,end.tv_usec-begin.tv_usec);

    close(fd);
    return res;
}

static int loggedFS_write(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi)
{
    int fd;
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    (void) fi;

    timeval begin,end;

    gettimeofday(&begin , NULL );

    fd = open(path, O_WRONLY);
    if(fd == -1) {
        res = -errno;
        loggedfs_log(aPath,"write",-1,"write %d bytes to %s",size,aPath);
        return res;
    } else {
        loggedfs_log(aPath,"write",0,"write %d bytes to %s",size,aPath);
    }


    res = pwrite(fd, buf, size, offset);
    gettimeofday(&end , NULL );

    if(res == -1)
        res = -errno;
    else loggedfs_log(aPath,"write",0, "%d bytes written to %s in %d microseconds.",res,aPath,end.tv_usec-begin.tv_usec);

    close(fd);
    return res;
}

static int loggedFS_statfs(const char *path, struct statfs *stbuf)
{
    int res;
    char *aPath=getAbsolutePath(path);
    path=getRelativePath(path);
    res = statfs(path, stbuf);
    loggedfs_log(aPath,"statfs",res,"statfs %s",aPath);
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

    while ((res = getopt (argc, argv, "pfc:l:")) != -1)
    {
        switch (res)
        {

        case 'f':
            out->isDaemon = false;
            // this option was added in fuse 2.x
            PUSHARG("-f");
	    rLog(Info,"LoggedFS not running as a daemon");
            break;
        case 'p':
            PUSHARG("-o");
            PUSHARG("allow_other,default_permissions");
            rLog(Info,"LoggedFS running as a public filesystem");
            break;
        case 'c':
            out->configFilename=optarg;
	    rLog(Info,"Configuration file : %s",optarg);
            break;
        case 'l':
            fileLog=open(optarg,O_WRONLY|O_CREAT|O_APPEND );
            fileLogNode=new StdioNode(fileLog);
            fileLogNode->subscribeTo( RLOG_CHANNEL("") );
	    rLog(Info,"LoggedFS log file : %s",optarg);
            break;
        default:

            break;
        }
    }

    if(optind+1 <= argc)
    {
        out->mountPoint = argv[optind++];
        out->fuseArgv[1] = out->mountPoint;
    }
    else
    {
        fprintf(stderr,"Missing mountpoint\n");
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

    if(!isAbsolutePath( out->mountPoint ))
    {
        fprintf(stderr,"You must use absolute paths "
                "(beginning with '/') for %s\n",out->mountPoint);
        return false;
    }
    return true;
}




int main(int argc, char *argv[])
{
    RLogInit( argc, argv );

    StdioNode* stdLog=new StdioNode(STDOUT_FILENO);
    stdLog->subscribeTo( RLOG_CHANNEL("") );
    SyslogNode *logNode = NULL;
    char* input=new char[2048]; // 2ko MAX input for configuration




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

            if (strcmp(loggedfsArgs->configFilename,"-")==0)
            {
                rLog(Info, "Using stdin configuration");
                memset(input,0,2048);
                char *ptr=input;

                int size=0;
                do {
                    size=fread(ptr,1,1,stdin);
                    ptr++;
                }while (!feof(stdin) && size>0);
                config.loadFromXmlBuffer(input);
            }
            else
            {
                rLog(Info, "Using configuration file %s.",loggedfsArgs->configFilename);
                config.loadFromXmlFile(loggedfsArgs->configFilename);
            }
        }

        rLog(Info,"chdir to %s",loggedfsArgs->mountPoint);
        chdir(loggedfsArgs->mountPoint);
        savefd = open(".", 0);

        fuse_main(loggedfsArgs->fuseArgc,
                  const_cast<char**>(loggedfsArgs->fuseArgv), &loggedFS_oper);
        delete stdLog;
        stdLog = NULL;
        if (fileLog!=0)
        {
            delete fileLogNode;
            fileLogNode=NULL;
            close(fileLog);
        }
        if (loggedfsArgs->isDaemon)
        {
            delete logNode;
            logNode=NULL;
        }
        rLog(Info,"LoggedFS closing.");

    }
    else rLog(Info,"LoggedFS not starting");
}
