#ifndef GDTFNET_H
#define GDTFNET_H

#include <string>

bool GdtfLogin(const std::string& user,
               const std::string& password,
               const std::string& cookieFile,
               long& httpCode);

bool GdtfGetList(const std::string& cookieFile,
                 std::string& listData);

bool GdtfDownload(const std::string& rid,
                  const std::string& destFile,
                  const std::string& cookieFile,
                  long& httpCode);

#endif // GDTFNET_H
