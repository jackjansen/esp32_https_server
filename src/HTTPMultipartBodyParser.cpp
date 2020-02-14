#include "HTTPMultipartBodyParser.hpp"
#include <sstream>

namespace httpsserver {

// #define DEBUG_MULTIPART_PARSER
void HTTPMultipartBodyParser::temp() {
  // Stream the incoming request body to the response body
  // Theoretically, this should work for every request size.
  std::string buffer;
  // What we're searching for
  enum {BOUNDARY, CONTENT_DISPOSITION, DATA_START, DATA_END} searching = BOUNDARY;
  std::string fieldName;
  std::string fieldValue;
  bool isFile;
  // HTTPReqeust::requestComplete can be used to check whether the
  // body has been parsed completely.
  while(!(_request->requestComplete())) {
    const auto oldBufferSize = buffer.size();
#ifdef DEBUG_MULTIPART_PARSER      
    Serial.print("Old buffer size: ");
    Serial.println(oldBufferSize);
#endif
    buffer.resize(256);
    // HTTPRequest::readBytes provides access to the request body.
    // It requires a buffer, the max buffer length and it will return
    // the amount of bytes that have been written to the buffer.
    auto s = _request->readChars(&buffer[oldBufferSize], buffer.size() - oldBufferSize);
    if(!s)
      break;
#ifdef DEBUG_MULTIPART_PARSER
    Serial.print("Reading buffer (");
    Serial.print(s);
    Serial.print(" bytes): ");
    Serial.println(buffer.c_str());
#endif
    buffer.resize(s + oldBufferSize);      
    // reading line by line
    std::istringstream is(buffer);
    for(std::string line; is.good(); ) {
      std::getline(is, line);
#ifdef DEBUG_MULTIPART_PARSER      
      Serial.print("Next line is ");
      Serial.print(line.size());
      Serial.print(" bytes: ");
      Serial.println(line.c_str());
#endif
      // remove preceding \r
      bool crRemoved = false;
      if(!line.empty() && (line.back() == '\r')) {
        crRemoved = true;
        line.pop_back();
      }
      if(is.good()) {
        switch(searching) {
          case BOUNDARY:
            if((line == boundary) || (line == lastBoundary)) {
#ifdef DEBUG_MULTIPART_PARSER      
              Serial.println("Boundary found");
#endif
              searching = CONTENT_DISPOSITION;
              // save the previous field value
              if(!fieldValue.empty()) {
#ifdef DEBUG_MULTIPART_PARSER      
                Serial.print("Field: ");
                Serial.println(fieldName.c_str());
                Serial.print("Value: ");
                Serial.println(fieldValue.c_str());
#endif
                handleField(fieldName, fieldValue);
                fieldValue.clear();
              }
            }
            break;
          case CONTENT_DISPOSITION:
            if(line.substr(0, 21) == "Content-Disposition: ") {
              auto i = line.find(" name=");
              if(i != std::string::npos) {
#ifdef DEBUG_MULTIPART_PARSER      
                Serial.println("Correct content disposition found");
#endif
                isFile = line.find(" filename=") != std::string::npos;
                fieldName = line.substr(i + 7);
                fieldName.erase(std::find(fieldName.begin(), fieldName.end(), ';'), fieldName.end());
                fieldName.pop_back();
                searching = DATA_START;
              } else
                searching = BOUNDARY;
            }
            break;
          case DATA_START:
            if(line.empty())
              searching = DATA_END;
            break;
          case DATA_END:
            if((line == boundary) || (line == lastBoundary)) {
              searching = CONTENT_DISPOSITION;
              // remove the last CRLF pair before the boundary
              if(isFile && (fieldValue.size() > 1)) {
                fieldValue.pop_back();
                fieldValue.pop_back();
              }
              // save the previous field value
#ifdef DEBUG_MULTIPART_PARSER      
              Serial.print("Field: ");
              Serial.println(fieldName.c_str());
              Serial.print("Value (");
              Serial.print(fieldValue.size());
              Serial.print(" bytes): ");
              Serial.println(fieldValue.c_str());
#endif
              handleField(fieldName, fieldValue);
              fieldValue.clear();
              break;
            }
            // else not boundary
            if(isFile) {
              // if binary file contents, add CRLF ignored by getline
              if(crRemoved)
                line.push_back('\r');
              line.push_back('\n');
            } else {
              // else text only, removing preceding \r (ordinary values are single-line)
              //if(!line.empty())
              //  line.pop_back();
              searching = BOUNDARY;
            }
#ifdef DEBUG_MULTIPART_PARSER      
            Serial.print("Adding (");
            Serial.print(line.size());
            Serial.print(" bytes): ");
            Serial.print(line.c_str());
#endif
            fieldValue += line;
            break;
        }
      } else {
        // no LF found, store the remaining chars in the buffer and read further
#ifdef DEBUG_MULTIPART_PARSER      
        Serial.println("No more LF found in buffer");
        Serial.print("Remaining (");
        Serial.print(line.size() + crRemoved);
        Serial.print(" bytes): ");
        Serial.print(line.c_str());
#endif
        if(crRemoved)
          Serial.print('\r');
        // if last char is \r, it's possible the next one will be \n, but we haven't read it to buffer yet
        // if the buffer contains the boundary, it must be in its beginning, because boundary starts right after CRLF pair.
        // but then, the remainder cannot be longer than 72 bytes ("--" + boundary max 70 chars)
        if(line.size() && (line.size() < 73) && (boundary.find(line) == 0)) {
          // boundary found, at least partially
          // do nothing, the remainder will be read on the next loop iteration
#ifdef DEBUG_MULTIPART_PARSER      
          Serial.print("Boundary found without LF: ");
          Serial.println(line.c_str());
#endif
        } else {
          // no boundary
          // to avoid the case when the buffer is full (256 bytes), we add it to field value here
          //TODO: it is possible to cause hangups by passing long strings during (searching != DATA_END),
          // but for now we assume the client is well-behaving
          if(searching == DATA_END) {
            if(isFile && crRemoved)
              line.push_back('\r');
#ifdef DEBUG_MULTIPART_PARSER      
            Serial.println("Adding it.");
#endif
            fieldValue += line;
            line.clear();
          }
        }
        buffer = line;
      }
    }
  }
}

HTTPMultipartBodyParser::HTTPMultipartBodyParser(HTTPRequest * req)
: HTTPBodyParser(req),
  boundary(""),
  lastBoundary("")
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
#if 1
  auto bodyLength = _request->getContentLength();
  Serial.printf("xxxjack body len=%d\n", int(bodyLength));
  if (bodyLength) {
    auto bodyBuffer = new char[bodyLength+1];
    _request->readChars(bodyBuffer, bodyLength);
    bodyBuffer[bodyLength] = '\0';
    Serial.printf(" xxxjack body: \"%s\".\n", bodyBuffer);
  }
#endif
}

HTTPMultipartBodyParser::~HTTPMultipartBodyParser() {

}

bool HTTPMultipartBodyParser::nextField() {
  return false;
}

std::string HTTPMultipartBodyParser::getFieldName() {
  return std::string("foo");
}

std::string HTTPMultipartBodyParser::getFieldMimeType() {
  return std::string("text/plain");
}

size_t HTTPMultipartBodyParser::getLength() {
  return 0;
}

size_t HTTPMultipartBodyParser::getRemainingLength() {
  return 0;
}

size_t HTTPMultipartBodyParser::read(byte* buffer, size_t bufferSize) {
  return 0;
}


}