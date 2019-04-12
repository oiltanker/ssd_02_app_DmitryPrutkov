#include "service.h"

#include <iostream>
#include <thread>
#include <algorithm>

using namespace std;

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams
using namespace web::http::experimental::listener;

enum class ResponseType {
    Link, Faliure, NotFound, NoTag
};
enum class ReturnType {
    Json, Html
};
http_response createResponse(ReturnType returnType, ResponseType responseType, const Image* img = nullptr);

int runService() {
    http_listener listener("http://0.0.0.0:80/interestingPhoto");
    listener.support(web::http::methods::GET, [=](http_request request) {
        request.content_ready().then([=](http_request request) {
            auto vars = uri::split_query(uri::decode(request.relative_uri().query()));
            ReturnType retType;
            if (vars.find("returnjson") != vars.end() && vars["returnjson"] == "1")
                retType = ReturnType::Json;
            else
                retType = ReturnType::Html;
            if (vars.find("tag") != vars.end()) { // tag received
                string tag = conversions::to_utf8string(vars["tag"]);
                string processedTag = prepareTag(tag);
                cout << "Searching for tag '" << tag << "' (" << processedTag << ")\n";

                vector<Image> images;
                try {
                    images = getInterestings();
                } catch (const exception& e) {
                    cout << "Unable to obtain interesting image list.\n";
                    request.reply(createResponse(retType, ResponseType::Faliure));
                    return;
                }

                for (Image& image: images) {
                    if (find(image.tags.begin(), image.tags.end(), tag) != image.tags.end()) {
                        // Image found
                        request.reply(createResponse(retType, ResponseType::Link, &image));
                        return;
                    }
                }
                // No image found
                request.reply(createResponse(retType, ResponseType::NotFound));
            } else { // No tag provided
                cout << "No tag provided.\n";
                request.reply(createResponse(retType, ResponseType::NoTag));
            }
        });
    });
    try {
        listener
            .open()
            .then([&listener]() {
                cout << "listening to " << listener.uri().to_string() << "\n";
            })
            .wait();
        this_thread::sleep_until(chrono::time_point<chrono::system_clock>::max());
        return 0;
    } catch (exception const &e) {
        cout << e.what() << "\n";
        return 1;
    }
}

http_response createResponse(ReturnType returnType, ResponseType responseType, const Image* img) {
    http_response response;
    response.headers().add("Cache-Control", "no-cache");

    switch (returnType) {
        case ReturnType::Html:
            switch (responseType) {
                case ResponseType::Link:
                    response.set_status_code(301);
                    response.headers().add("Location", (*img).bestQualityLink);
                    break;
                case ResponseType::Faliure:
                    response.set_status_code(599);
                    response.headers().add("Content-type", "text/html");
                    response.set_body(R"(<!DOCTYPE html>
<html>
    <head>
        <title>Failure</title>
    </head>
    <body>
        <h1>Unable to obtain interesting image list.</h1>
        <p>Failure obtaining interesting images from 'www.flickr.com'.</p>
    </body>
</html>)"
                    );
                    break;
                case ResponseType::NotFound:
                    response.set_status_code(404);
                    response.headers().add("Content-type", "text/html");
                    response.set_body(R"(<!DOCTYPE html>
<html>
    <head>
        <title>Image not found</title>
    </head>
    <body>
        <h1>No fitting image found</h1>
        <p>Tag specified did not fit any image in the recent interesting images.</p>
    </body>
</html>)"
                    );
                    break;
                case ResponseType::NoTag:
                    response.set_status_code(400);
                    response.headers().add("Content-type", "text/html");
                    response.set_body(R"(<!DOCTYPE html>
<html>
    <head>
        <title>Error</title>
    </head>
    <body>
        <h1>No tag specified</h1>
        <p>This method rquires 'tag' argument.</p>
    </body>
</html>)"
                    );
                    break;
            }
            break;
        case ReturnType::Json:
            switch (responseType) {
                case ResponseType::Link:
                    response.set_status_code(200);
                    response.set_body(
                        "{\"code\":0,\"url\":\"" + (*img).bestQualityLink + "\"}"
                    );
                    break;
                case ResponseType::Faliure:
                    response.set_status_code(500);
                    response.set_body(
                        "{\"code\":100,\"message\":\"Could not obtain interesting image list\"}"
                    );
                    break;
                case ResponseType::NotFound:
                    response.set_status_code(500);
                    response.set_body(
                        "{\"code\":101,\"message\":\"No fitting image found\"}"
                    );
                    break;
                case ResponseType::NoTag:
                    response.set_status_code(500);
                    response.set_body(
                        "{\"code\":102,\"message\":\"No tag specified\"}"
                    );
                    break;
            }
            break;
    }

    return response;
}