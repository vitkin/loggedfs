#include "Filter.h"
#include <pcre.h>

#define OVECCOUNT 30


Filter::Filter()
{
}

Filter::~Filter()
{
}

bool Filter::matches( const char* str,const char* pattern)
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




bool Filter::matches(const char* path,int uid, const char *action, const char* retname)
{
bool a= (matches(path,this->extension) && (uid==this->uid || this->uid==-1) && matches(action,this->action) && matches(retname,this->retname));

return a;
}
