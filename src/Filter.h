#ifndef LOGGEDFS_FILTER_H
#define LOGGEDFS_FILTER_H

#include <stdio.h>
#include <string.h>


class Filter
{
public:
	Filter();
	~Filter();
	const char* getExtension() {return extension;};
	int getUID() {return uid;};
	const char* getAction() {return action;};
	const char* getRetname() {return retname;};
	
	void setExtension(const char* e) {this->extension=strdup(e);};
	void setUID(int u) {this->uid=u;};
	void setAction(const char* a) {this->action=strdup(a);};
	void setRetname(const char* r) {this->retname=strdup(r);};
	bool matches(const char* path, int uid, const char *action, const char* retname);
	
private:
	const char* extension;
	int uid;
	const char* action;
	const char* retname;
	bool matches( const char* str,const char* pattern);
};


#endif
