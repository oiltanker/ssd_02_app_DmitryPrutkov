#include "client.h"

#include <iostream>
#include <locale>
#include <stdexcept>

using namespace std;

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams


const string_t flickrUrl = U("https://api.flickr.com/");
const string_t restApiUrl = U("/services/rest/");
const string_t apiKey = U("1eafb85749233507d5dbf83c9d14bb0a");
const string_t resultFormat = U("json");

string Image::toString() const {
    string imgaeStr = "Image: { '"
        + conversions::to_utf8string(title) + "', "//"', '"
        + conversions::to_utf8string(description).substr(0, 50) + "', "
        + conversions::to_utf8string(bestQualityLink) + ", "
        + to_string(timestamp) + ", { ";
    for (const string_t& tag: tags)
        imgaeStr += conversions::to_utf8string(tag) + ", ";
    imgaeStr = imgaeStr.substr(0, imgaeStr.length() - 2) + " } }";
    return imgaeStr;
}

std::ostream& operator<<(std::ostream& o, Image& img) {
    return o << img.toString();
}

string prepareTag(const string& tag) {
    try {
        locale loc("en_US.utf8");
        u16string uTag = conversions::utf8_to_utf16(tag);
        wstring wTag(uTag.begin(), uTag.end()), wTagCut;

        for(wchar_t wc: wTag) if (isalnum(wc, loc)) wTagCut += tolower(wc, loc);
        uTag = u16string(wTagCut.begin(), wTagCut.end());

        return conversions::utf16_to_utf8(uTag);
    } catch (const exception& e) {
        return "null";
    }
}

size_t parseULL(string_t str_t) {
    string str = conversions::to_utf8string(str_t);
    return stoull(str);
}

string_t getMethodUrl(string_t method, int argCount, string_t argNames[], string_t argValues[]) {
    uri_builder uBld(restApiUrl);
    uBld.append_query(U("method"), method);
    uBld.append_query(U("api_key"), apiKey);
    for(int i = 0; i < argCount; i++) uBld.append_query(argNames[i], argValues[i]);
    uBld.append_query(U("format"), resultFormat);
    uBld.append_query(U("nojsoncallback"), 1);
    return uBld.to_string();
}

string parseSizesJson(json::value& szJson) {
    try {
        json::array& szArr = szJson[U("sizes")][U("size")].as_array();

        string_t result = U("null");
        size_t curMaxSize = 0;
        for(size_t i = 0; i < szArr.size(); i++) {
            json::value& sz = szArr[i];

            size_t w, h;
            if (sz[U("width")].is_integer()) w = sz[U("width")].as_integer();
            else w = parseULL(sz[U("width")].as_string());
            if (sz[U("height")].is_integer()) h = sz[U("height")].as_integer();
            else h = parseULL(sz[U("height")].as_string());
            size_t size = w * h;
            
            if (sz[U("label")].as_string() == U("Original")) {
                result = sz[U("source")].as_string();
                break;
            } else if (size > curMaxSize) {
                result = sz[U("source")].as_string();
                curMaxSize = size;
            }
        }

        return result;
    } catch (const exception &e) {
        cout << e.what() << "\n";
        return U("null");
    }
}

pplx::task<int> getBestQuality(
    http_client& client, string_t imageId,
    vector<Image>& images, size_t imgIndx
) {
    string_t args[] = {U("photo_id")};
    string_t vals[] = {imageId};
    string_t reqUrl = getMethodUrl(U("flickr.photos.getSizes"), 1, args, vals);

    pplx::task<int> requestTask =
        client.request(methods::GET, reqUrl)
        .then([=](http_response response) {
            return response.content_ready();
        }).then([=, &images](http_response response) { // Handle response headers arriving
            // Print status code and response body
            if (response.status_code() != 200) {
                images[imgIndx].bestQualityLink = "null";
                return 1;
            } else {
                response.extract_json()
                .then([=, &images](json::value imageSizesJson) {
                    images[imgIndx].bestQualityLink = parseSizesJson(imageSizesJson);
                });
                return 0;
            }
        });
    return requestTask;
}

int parseImterestingJson(json::value& intJson, vector<Image>& images) {
    if (
        !intJson.has_field(U("photos")) ||
        (intJson.has_field(U("code")) && (
            intJson[U("code")].is_integer() && intJson[U("code")].as_integer() != 200 ||
            intJson[U("code")].as_string() != "200"
        ))
    ) {
        return 1;
    }
    http_client client(flickrUrl);

    json::array& imgArr = intJson[U("photos")][U("photo")].as_array();
    vector<pplx::task<int>> tasks;
    for (size_t i = 0; i < imgArr.size(); i++) {
        json::value& img = imgArr[i];

        string_t id = img[U("id")].is_integer() ?
            to_string(img[U("id")].as_integer()) :
            img[U("id")].as_string();

        string_t title = img[U("title")].as_string();
        string_t description = img[U("description")][U("_content")].as_string();
        size_t timestamp = img[U("dateupload")].is_integer() ?
            img[U("dateupload")].as_integer() :
            parseULL(img[U("dateupload")].as_string());
        vector<string_t> tags = [&img]() -> vector<string_t> {
            string_t str_t = img[U("tags")].as_string();

            vector<string_t> result;
            for(
                size_t pos = str_t.find(U(" "));
                pos != string::npos;
                pos = str_t.find(U(" "))
            ) {
                result.push_back(prepareTag(str_t.substr(0, pos)));
                str_t = str_t.substr(pos + 1);
            }
            result.push_back(str_t);

            return result;
        }();

        images.push_back({
            title,
            description,
            U("null"),
            timestamp,
            tags
        });
        tasks.push_back(getBestQuality(client, id, images, images.size() - 1));
    }
    int res = 0;
    for (pplx::task<int>& task: tasks) {
        res += task.get();
    }
    if (res > 0) return 2;
    else return 0;
}

vector<Image> getInterestings() {
    // Create http_client to send the request
    http_client client(flickrUrl);
    string_t args[] = {U("page"), U("per_page"), U("extras")};
    string_t vals[] = {U("1"), U("500"), U("date_upload,tags,description")};
    string_t reqUrl = getMethodUrl(U("flickr.interestingness.getList"), 3, args, vals);

    json::value interestingJson;
    pplx::task<void> requestTask =
        client.request(methods::GET, reqUrl)
        .then([=](http_response response) {
            return response.content_ready();
        }).then([=, &interestingJson](http_response response) { // Handle response headers arriving
            if (response.status_code() == 200)
                interestingJson = response.extract_json().get();
            else
                interestingJson = json::value();
        });

    vector<Image> images;
    // Wait for all the outstanding I/O to complete and handle any exceptions
    int res;
    try {
        requestTask.get();
        res = parseImterestingJson(interestingJson, images);
    } catch (const std::exception &e) {
        cout << e.what() << "\n";
        throw e;
    }

    if (res != 0) {
        throw runtime_error("Could not get interesting images.");
    }
    return images;
}