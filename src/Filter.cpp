/*******************************************************************************
 * Author:   Remi Flament <rflament at laposte dot net>
 *******************************************************************************
 * Copyright (c) 2005 - 2007, Remi Flament
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

#include "Filter.h"

#define OVECCOUNT 30

/*******************************************************************************
 *
 */
Filter::Filter()
{
}

/*******************************************************************************
 *
 */
Filter::~Filter()
{
    map<string, pcre*>::iterator it;

    for (it = regexMap.begin(); it != regexMap.end(); it++)
    {
        pcre_free((*it).second);
    }

    regexMap.clear();
}

/*******************************************************************************
 *
 */
bool Filter::matches(const char* str, const char* pattern)
{
    pcre* re; 
    const char* error;
    int erroffset;

    // Check if a regular expression has been compiled for the pattern   
    if (regexMap.count(pattern) == 0)
    {
        // Compile a regular expression for that pattern
        re = pcre_compile(pattern,
                          0,
                          &error,
                          &erroffset,
                          NULL);

        if (re == NULL)
        {
            fprintf(stderr, "PCRE compilation failed at offset %d: %s\n",
                    erroffset, error);
            
            return false;
        }

        // Add the compiled regex for the pattern to the map
        regexMap[pattern] = re;
    }
    else
    {
        // Retrieve the compiled regex for the pattern from the map
        re = regexMap[pattern];
    }
    
    int ovector[OVECCOUNT];

    int rc = pcre_exec(re, /* the compiled pattern */
                       NULL, /* no extra data - we didn't study the pattern */
                       str, /* the subject string */
                       strlen(str), /* the length of the subject */
                       0, /* start at offset 0 in the subject */
                       0, /* default options */
                       ovector, /* output vector for substring information */
                       OVECCOUNT); /* number of elements in the output vector */
    
    //free(ovector);

    return (rc >= 0);
}

/*******************************************************************************
 *
 */
bool Filter::matches(const char* path, int rUid, const char* rAction,
                     const char* rRetname)
{
    bool a = (matches(path, this->extension) &&
            (rUid == this->uid || this->uid == -1) &&
            matches(rAction, this->action) &&
            matches(rRetname, this->retname));

    return a;
}
