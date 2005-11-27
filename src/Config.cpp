/*****************************************************************************
 * Author:   Rémi Flament <remipouak@yahoo.fr>
 *
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

#include "Config.h"
#include <fstream>
#include <iostream>
#include <pcre.h>

#define OVECCOUNT 30

Config::Config()
{
    enabled=true; // default
}

Config::~Config()
{
    includes.clear();
    excludes.clear();
}

bool Config::load(const char* filename)
{
    char line[512];
    char buffer[20];
    FILE* fd = fopen( filename, "r" );
    if(fd < 0)
        return false;

    while (!feof(fd)) {
        fscanf(fd,"%[^\n]\n", line);

        if (line[0] == '#')
            continue;

        if (line[0] == ' ')
            continue;

        if (sscanf(line, "logEnabled %s",buffer) == 1) {
            if (!strcmp(buffer,"true"))
            {
                enabled=true;
            }
            else enabled=false;
        }

        if (sscanf(line, "include %s",buffer) == 1) {
            includes.push_back(buffer);
        }

        if (sscanf(line, "exclude %s",buffer) == 1) {
            excludes.push_back(buffer);
        }
    }
    fclose(fd);
    return true;
}

bool Config::matches( const char* str,const char* pattern)
{
    pcre *re;
    const char *error;
    int ovector[OVECCOUNT];
    int erroffset;


    re = pcre_compile(
             pattern,
             0,
             &error,
             &erroffset,
             NULL);


    if (re == NULL)
    {
        printf("PCRE compilation failed at offset %d: %s\n", erroffset, error);
        return false;
    }

    int rc = pcre_exec(
                 re,                   /* the compiled pattern */
                 NULL,                 /* no extra data - we didn't study the pattern */
                 str,              /* the subject string */
                 strlen(str),       /* the length of the subject */
                 0,                    /* start at offset 0 in the subject */
                 0,                    /* default options */
                 ovector,              /* output vector for substring information */
                 OVECCOUNT);           /* number of elements in the output vector */

    return (rc >= 0);
}

bool Config::shouldLog(const char* filename)
{
    bool should=false;
    if (enabled)
    {
    	if (includes.size()>0)
	{
		for (unsigned int i=0;i<includes.size() && !should;i++)
		{
		if (matches(filename,includes[i].c_str()))
			should=true;
		}
		for (unsigned int i=0;i<excludes.size() && should;i++)
		{
		if (matches(filename,excludes[i].c_str()))
			should=false;
		}
	}
	else
	{
		should=true;
	}

    }
    

    return should;
}

char* Config::toString()
{

    string buf;

    buf="logEnabled : ";

    if (enabled)
        buf+="true\n";
    else
        buf="false\n";


    for (unsigned int i=0;i<includes.size();i++)
    {
        buf+="include "+includes[i]+"\n";
    }

    for (unsigned int i=0;i<excludes.size();i++)
    {
        buf+="exclude "+excludes[i]+"\n";
    }


    return strdup(buf.c_str());
}

