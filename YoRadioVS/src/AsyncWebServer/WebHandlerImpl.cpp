#include "WebHandlerImpl.h"

// Keep the same semantics as before but out-of-line to avoid large inline CFGs
void AsyncCallbackWebHandler::handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if((_username != "" && _password != "") && !request->authenticate(_username.c_str(), _password.c_str()))
    return request->requestAuthentication();
  if(_onUpload)
    _onUpload(request, filename, index, data, len, final);
}

void AsyncCallbackWebHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if((_username != "" && _password != "") && !request->authenticate(_username.c_str(), _password.c_str()))
    return request->requestAuthentication();
  if(_onBody)
    _onBody(request, data, len, index, total);
}