#pragma once
#include <string>
class Preferences {
public:
 bool begin(const char *,bool readOnly){return !readOnly;}
 std::string getString(const char *,const char *fallback){return fallback;}
 unsigned getUInt(const char *,unsigned fallback){return fallback;}
 unsigned putString(const char *,const char *){return 1;}
 unsigned putUInt(const char *,unsigned){return 1;}
 void end(){}
};
