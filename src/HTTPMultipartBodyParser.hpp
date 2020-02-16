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
  virtual std::string getFieldFilename();
  virtual std::string getFieldMimeType();
  virtual bool endOfField();
  virtual size_t read(byte* buffer, size_t bufferSize);
private:
  std::string readLine();
  void fillBufferUptoCR(size_t maxLen);
  void consumedBuffer(size_t consumed);
  bool skipCRLF();
  bool peekBoundary();
  char *peekBuffer;
  size_t peekBufferSize;
  bool didReadCR;

  std::string boundary;
  std::string lastBoundary;
  std::string fieldName;
  std::string fieldMimeType;
  std::string fieldFilename;
};

} // namespace httpserver

#endif