#ifndef SRC_HTTPMULTIPARTBODYPARSER_HPP_
#define SRC_HTTPMULTIPARTBODYPARSER_HPP_

#include <Arduino.h>
#include "HTTPBodyParser.hpp"

namespace httpsserver {

class HTTPMultipartBodyParser : public HTTPBodyParser {
public:
  // From HTTPBodyParser
  using HTTPBodyParser::HTTPBodyParser;
  virtual bool nextField();
  virtual std::string getFieldName();
  virtual std::string getFieldMimeType();
  virtual size_t getLength();
  virtual size_t getRemainingLength();
  virtual size_t read(byte* buffer, size_t bufferSize);
};

} // namespace httpserver

#endif