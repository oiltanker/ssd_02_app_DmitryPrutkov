#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

struct Image {
    std::string
        title,
        description,
        bestQualityLink;
    size_t timestamp;
    std::vector<std::string> tags;

    std::string toString() const;
};
std::ostream& operator<<(std::ostream& o, Image& img);

std::vector<Image> getInterestings();
std::string prepareTag(const std::string& tag);

#endif // CLIENT_H