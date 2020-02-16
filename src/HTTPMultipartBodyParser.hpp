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
  std::string readLine();
  void fillBufferUptoCR(size_t maxLen);
  void consumedBuffer(size_t consumed);
  bool skipCRLF();
  bool peekBoundary();
  char *buffer;
  size_t bufferSize;
  bool didReadCR;

  std::string boundary;
  std::string lastBoundary;
  std::string fieldName;
  std::string fieldMimeType;
  std::string fieldFilename;
};

} // namespace httpserver

#endif