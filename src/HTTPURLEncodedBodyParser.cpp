#include "HTTPURLEncodedBodyParser.hpp"

namespace httpsserver {
HTTPURLEncodedBodyParser::HTTPURLEncodedBodyParser(HTTPRequest * req)
: HTTPBodyParser(req),
  bodyBuffer(NULL),
  bodyPtr(NULL),
  bodyLength(0),
  fieldBuffer(""),
  fieldPtr(NULL),
  fieldRemainingLength(0)
{
  bodyLength = _request->getContentLength();
  Serial.printf("xxxjack body len=%d\n", int(bodyLength));
  if (bodyLength) {
    bodyBuffer = new char[bodyLength+1];
    _request->readChars(bodyBuffer, bodyLength);
    bodyPtr = bodyBuffer;
    bodyBuffer[bodyLength] = '\0';
    Serial.printf(" xxxjack body: \"%s\".\n", bodyBuffer);
  }
}

HTTPURLEncodedBodyParser::~HTTPURLEncodedBodyParser() {
  if (bodyBuffer) delete[] bodyBuffer;
  bodyBuffer = NULL;
}

bool HTTPURLEncodedBodyParser::nextField() {
  fieldBuffer = "";
  fieldPtr = NULL;
  fieldRemainingLength = 0;

  char *equalPtr = index(bodyPtr, '=');
  if (equalPtr == NULL) return false;
  fieldName = std::string(bodyPtr, equalPtr-bodyPtr);
  
  char *valuePtr = equalPtr + 1;
  char *endPtr = index(valuePtr, '&');
  if (endPtr == NULL) {
    endPtr = equalPtr + strlen(equalPtr);
    bodyPtr = endPtr;
  } else {
    bodyPtr = endPtr+1;
  }
  fieldBuffer = std::string(valuePtr, endPtr - valuePtr);
  fieldBuffer = urlDecode(fieldBuffer);
  fieldRemainingLength = fieldBuffer.size();
  fieldPtr = fieldBuffer.c_str();
  return true;
}

std::string HTTPURLEncodedBodyParser::getFieldName() {
  return fieldName;
}

std::string HTTPURLEncodedBodyParser::getFieldMimeType() {
  return std::string("text/plain");
}

size_t HTTPURLEncodedBodyParser::getLength() {
  return fieldBuffer.size();
}

size_t HTTPURLEncodedBodyParser::getRemainingLength() {
  return fieldRemainingLength;
}

size_t HTTPURLEncodedBodyParser::read(byte* buffer, size_t bufferSize) {
  if (bufferSize > fieldRemainingLength) bufferSize = fieldRemainingLength;
  memcpy(buffer, fieldPtr, bufferSize);
  fieldRemainingLength -= bufferSize;
  fieldPtr += bufferSize;
  return bufferSize;
}


}