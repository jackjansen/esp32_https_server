#include "HTTPMultipartBodyParser.hpp"
#include <sstream>

const size_t MAXLINESIZE = 256;

namespace httpsserver {

HTTPMultipartBodyParser::HTTPMultipartBodyParser(HTTPRequest * req)
: HTTPBodyParser(req),
  peekBuffer(NULL),
  peekBufferSize(0),
  boundary(""),
  lastBoundary(""),
  fieldName(""),
  fieldMimeType(""),
  fieldFilename("")
{
  auto contentType = _request->getHeader("Content-Type");
#ifdef DEBUG_MULTIPART_PARSER      
  Serial.print("Content type: ");
  Serial.println(contentType.c_str());
#endif
  auto boundaryIndex = contentType.find("boundary=");
  if(boundaryIndex == std::string::npos) {
    // TODO: error, I guess
    HTTPS_LOGE("Multipart: missing boundary=");
    return;
  }
  //TODO: remove all magic constants
  boundary = contentType.substr(boundaryIndex + 9);
  auto commaIndex = boundary.find(';');
  boundary = "--" + boundary.substr(0, commaIndex);
  if(boundary.size() > 72) {
    HTTPS_LOGE("Multipart: boundary string too long");
    return; //TODO: error 500, RFC violation
  }
  lastBoundary = boundary + "--";
}

HTTPMultipartBodyParser::~HTTPMultipartBodyParser() {
  if (peekBuffer) {
    free(peekBuffer);
    peekBuffer = NULL;
  }
}

void HTTPMultipartBodyParser::fillBufferUptoCR(size_t maxLen) {
  char *bufPtr;
  if (peekBuffer == NULL) {
    // Nothing in the buffer. Allocate one of the wanted size
    peekBuffer = (char *)malloc(maxLen);
    bufPtr = peekBuffer;
    peekBufferSize = 0;
  } else if (peekBufferSize > 1 && peekBuffer[peekBufferSize-1] == '\r') {
    // We already have a CR in the buffer.
    didReadCR = true;
    return;
  } else if (peekBufferSize < maxLen) {
    // Something in the buffer, but not enough
    peekBuffer = (char *)realloc(peekBuffer, maxLen);
    bufPtr = peekBuffer + peekBufferSize;
  } else {
    // We already have enough data in the buffer.
    return;
  }
  didReadCR = false;
  while(bufPtr < peekBuffer+maxLen) {
    char c;
    size_t didRead = _request->readChars(&c, 1);
    if (didRead != 1) {
      HTTPS_LOGE("Multipart incomplete");
      break;
    }
    *bufPtr++ = c;
    if (c == '\r') {
      didReadCR = true;
      break;
    }
  }
  peekBufferSize = bufPtr - peekBuffer;
}

void HTTPMultipartBodyParser::consumedBuffer(size_t consumed) {
  if (consumed == 0) consumed = peekBufferSize;
  if (consumed == peekBufferSize) {
    free(peekBuffer);
    peekBuffer = NULL;
    peekBufferSize = 0;
  } else {
    memmove(peekBuffer, peekBuffer+consumed, peekBufferSize-consumed);
    peekBufferSize -= consumed;
  }
}

bool HTTPMultipartBodyParser::skipCRLF() {
  // If we have read a CR discard the accompanying LF
  char c;
  if (!didReadCR) return false;
  if (peekBuffer && peekBufferSize > 1) {
    if (*peekBuffer == '\n') {
      consumedBuffer(1);
      didReadCR = false;
      return true;
    }
    return false;
  }
  if (_request->readChars(&c, 1) != 1 || c != '\n') {
    HTTPS_LOGE("Multipart incorrect line terminator");
    _request->discardRequestBody();
    return false;
  }
  didReadCR = false;
  return true;
}

std::string HTTPMultipartBodyParser::readLine() {
  fillBufferUptoCR(MAXLINESIZE);
  if (!didReadCR) {
    HTTPS_LOGE("Multipart line too long");
  }
  std::string rv(peekBuffer, peekBufferSize-1);
  consumedBuffer(0);
  skipCRLF();
  return rv;
}

// Returns true if the buffer contains a boundary (or possibly lastBoundary)
bool HTTPMultipartBodyParser::peekBoundary() {
  if (peekBuffer == NULL || peekBufferSize < boundary.size()) return false;
  char *ptr = peekBuffer;
  if (*ptr == '\n') ptr++;
  return memcmp(ptr, boundary.c_str(), boundary.size()) == 0;
}

bool HTTPMultipartBodyParser::nextField() {
  fillBufferUptoCR(MAXLINESIZE);
  skipCRLF();
  while(!peekBoundary()) {
    std::string dummy = readLine();
    if (_request->requestComplete()) return false;
    fillBufferUptoCR(MAXLINESIZE);
  }
  std::string line = readLine();
  if (line == lastBoundary) {
    _request->discardRequestBody();
    return false;
  }
  if (line != boundary) {
    HTTPS_LOGE("Multipart missing boundary");
    return false;
  }
  // Read header lines up to and including blank line
  fieldName = "";
  fieldMimeType = "text/plain";
  fieldFilename = "";
  while (true) {
    line = readLine();
    if (line == "") break;
    if (line.substr(0, 14) == "Content-Type: ") {
      fieldMimeType = line.substr(14);
    }
    if (line.substr(0, 31) == "Content-Disposition: form-data;") {
      // Parse name=value; or name="value"; fields.
      std::string field;
      line = line.substr(31);
      while(true) {
        size_t pos = line.find_first_not_of(' ');
        if (pos != std::string::npos) {
          line = line.substr(pos);
        }
        if (line == "") break;
        pos = line.find(';');
        if (pos == std::string::npos) {
          field = line;
          line = "";
        } else {
          field = line.substr(0, pos);
          line = line.substr(pos+1);
        }
        pos = field.find('=');
        if (pos == std::string::npos) {
          HTTPS_LOGE("Multipart ill-formed form-data header");
          continue;
        }
        std::string headerName = field.substr(0, pos);
        std::string headerValue = field.substr(pos+1);
        if (headerValue.substr(0,1) == "\"") headerValue = headerValue.substr(1, headerValue.size()-2);
        if (headerName == "name") {
          fieldName = headerValue;
        }
        if (headerName == "filename") {
          fieldFilename = headerValue;
        }
      }
    }
  }
  if (fieldName == "") {
    HTTPS_LOGE("Multipart missing name");
    return false;
  }
  return true;
}

std::string HTTPMultipartBodyParser::getFieldName() {
  return fieldName;
}

std::string HTTPMultipartBodyParser::getFieldFilename() {
  return fieldFilename;
}

std::string HTTPMultipartBodyParser::getFieldMimeType() {
  return fieldMimeType;
}
bool HTTPMultipartBodyParser::endOfField() {
  return peekBoundary();
}

size_t HTTPMultipartBodyParser::read(byte* buffer, size_t bufferSize) {
  if (peekBoundary()) return 0;
  size_t readSize = std::max(bufferSize, MAXLINESIZE);
  fillBufferUptoCR(readSize);
  if (peekBoundary()) return 0;
  size_t copySize = std::min(bufferSize, peekBufferSize);
  memcpy(buffer, peekBuffer, copySize);
  consumedBuffer(copySize);
  // Note that we may have copied the CR that is really part of the CR LF boundary CR LF sequence.
  // Test for that.
  fillBufferUptoCR(MAXLINESIZE);
  if (peekBoundary()) {
    didReadCR = true;
    copySize--;
  }
  return copySize;
}


}