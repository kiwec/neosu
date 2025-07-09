#include "NetworkHandler.h"
#include "curl_blob.h"

#include <curl/curl.h>

#include <sstream>

#include "ConVar.h"
#include "Engine.h"

NetworkHandler::NetworkHandler() {}

NetworkHandler::~NetworkHandler() {}

UString NetworkHandler::httpGet(UString url, long timeout, long connectTimeout) {
    CURL *curl = curl_easy_init();

    if(curl != NULL) {
        curl_easy_setopt(curl, CURLOPT_URL, url.toUtf8());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlStringWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlReadBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, true);

        curl_easy_setopt_CAINFO_BLOB_embedded(curl);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            debugLog("Error while fetching %s: %s\n", url.toUtf8(), curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);

        return UString(this->curlReadBuffer.c_str());
    } else {
        debugLog("NetworkHandler::httpGet() error, curl == NULL!\n");
        return "";
    }
}

std::string NetworkHandler::httpDownload(UString url, long timeout, long connectTimeout) {
    CURL *curl = curl_easy_init();

    if(curl != NULL) {
        std::stringstream curlWriteBuffer(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
        curl_easy_setopt(curl, CURLOPT_URL, url.toUtf8());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlStringStreamWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlWriteBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, true);

        curl_easy_setopt_CAINFO_BLOB_embedded(curl);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            curlWriteBuffer = std::stringstream();
            debugLog("NetworkHandler::httpDownload() error, code %u!\n", static_cast<unsigned int>(res));
        }

        curl_easy_cleanup(curl);

        return curlWriteBuffer.str();
    } else {
        debugLog("NetworkHandler::httpDownload() error, curl == NULL!\n");
        return std::string("");
    }
}

size_t NetworkHandler::curlStringWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

size_t NetworkHandler::curlStringStreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::stringstream *)userp)->write((const char *)contents, (size_t)size * nmemb);
    return size * nmemb;
}
