#ifndef __HTTPD_HTTPD_H
#define __HTTPD_HTTPD_H

#include <string>
#include <vector>
#include <map>

#include "clientserver/clientserver.h"

#include "files/serialization.h"



/*
  GET /hello?args=1&args2=2 HTTP/1.1
  Host: 192.168.0.6:9099
  User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; en-GB; rv:1.8.1.6) Gecko/20070725 Firefox/2.0.0.6
  Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,* / *;q=0.5
  Accept-Language: en,ru;q=0.5
  Accept-Encoding: gzip,deflate
  Accept-Charset: windows-1251,utf-8;q=0.7,*;q=0.7
  Keep-Alive: 300
  Connection: keep-alive
  Cookie: vid=3294389247


*/


#include "request.h"
#include "unparse.h"
#include "parse.h"
#include "response.h"



#endif

