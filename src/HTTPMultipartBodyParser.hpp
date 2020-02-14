#ifndef SRC_HTTPMULTIPARTBODYPARSER_HPP_
#define SRC_HTTPMULTIPARTBODYPARSER_HPP_

#include <Arduino.h>
#include "HTTPBodyParser.hpp"

namespace httpsserver {

class HTTPMultipartBodyParser : public HTTPBodyParser {
public:
  HTTPMultipartBodyParser(HTTPRequest * req);
  ~HTTPMultipartBodyParser();
  virtual bool nextField();
  virtual std::string getFieldName();
  virtual std::string getFieldMimeType();
  virtual size_t getLength();
  virtual size_t getRemainingLength();
  virtual size_t read(byte* buffer, size_t bufferSize);
private:
  void temp();
  void handleField(std::string, std::string);
  std::string boundary;
  std::string lastBoundary;
};

} // namespace httpserver

#endif