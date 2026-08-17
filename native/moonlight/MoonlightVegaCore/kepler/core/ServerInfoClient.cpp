#include "ServerInfoClient.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace moonlight::network {
namespace {

constexpr std::size_t kMaximumResponseBytes = 1024 * 1024;

class FileDescriptor {
 public:
  explicit FileDescriptor(int value = -1) : value_(value) {}
  ~FileDescriptor() {
    if (value_ >= 0) {
      close(value_);
    }
  }

  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;

  int get() const { return value_; }

 private:
  int value_;
};

std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  }).base();
  return first < last ? std::string(first, last) : std::string();
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

void replaceAll(std::string& value, std::string_view from, std::string_view to) {
  std::size_t position = 0;
  while ((position = value.find(from, position)) != std::string::npos) {
    value.replace(position, from.size(), to);
    position += to.size();
  }
}

std::string decodeXmlEntities(std::string value) {
  replaceAll(value, "&lt;", "<");
  replaceAll(value, "&gt;", ">");
  replaceAll(value, "&quot;", "\"");
  replaceAll(value, "&apos;", "'");
  replaceAll(value, "&amp;", "&");
  return value;
}

std::string extractTag(const std::string& xml, const std::string& tag) {
  const std::string xmlLower = lower(xml);
  const std::string tagLower = lower(tag);
  const std::string opening = "<" + tagLower;
  const std::string closing = "</" + tagLower + ">";

  const auto openingPosition = xmlLower.find(opening);
  if (openingPosition == std::string::npos) {
    return {};
  }
  const auto contentPosition = xmlLower.find('>', openingPosition + opening.size());
  if (contentPosition == std::string::npos) {
    return {};
  }
  const auto closingPosition = xmlLower.find(closing, contentPosition + 1);
  if (closingPosition == std::string::npos) {
    return {};
  }

  return decodeXmlEntities(trim(xml.substr(
      contentPosition + 1,
      closingPosition - contentPosition - 1)));
}

std::int32_t parseInt(const std::string& value, std::int32_t fallback = 0) {
  if (value.empty()) {
    return fallback;
  }
  try {
    return static_cast<std::int32_t>(std::stol(value));
  } catch (const std::exception&) {
    return fallback;
  }
}

double parseDouble(const std::string& value, double fallback = 0) {
  if (value.empty()) {
    return fallback;
  }
  try {
    return std::stod(value);
  } catch (const std::exception&) {
    return fallback;
  }
}

std::string makeUuid() {
  std::random_device source;
  std::uniform_int_distribution<unsigned int> distribution(0, 255);
  unsigned char bytes[16];
  for (auto& byte : bytes) {
    byte = static_cast<unsigned char>(distribution(source));
  }
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);

  std::ostringstream result;
  result << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < sizeof(bytes); ++index) {
    result << std::setw(2) << static_cast<unsigned int>(bytes[index]);
    if (index == 3 || index == 5 || index == 7 || index == 9) {
      result << '-';
    }
  }
  return result.str();
}

int connectWithTimeout(
    const addrinfo* address,
    int timeoutMilliseconds) {
  FileDescriptor socketFd(socket(
      address->ai_family,
      address->ai_socktype,
      address->ai_protocol));
  if (socketFd.get() < 0) {
    return -1;
  }

  const int originalFlags = fcntl(socketFd.get(), F_GETFL, 0);
  if (originalFlags < 0 ||
      fcntl(socketFd.get(), F_SETFL, originalFlags | O_NONBLOCK) < 0) {
    return -1;
  }

  const int result = connect(socketFd.get(), address->ai_addr, address->ai_addrlen);
  if (result < 0 && errno != EINPROGRESS) {
    return -1;
  }

  if (result < 0) {
    pollfd descriptor{socketFd.get(), POLLOUT, 0};
    const int pollResult = poll(&descriptor, 1, timeoutMilliseconds);
    if (pollResult <= 0) {
      errno = pollResult == 0 ? ETIMEDOUT : errno;
      return -1;
    }

    int socketError = 0;
    socklen_t socketErrorLength = sizeof(socketError);
    if (getsockopt(
            socketFd.get(),
            SOL_SOCKET,
            SO_ERROR,
            &socketError,
            &socketErrorLength) < 0 ||
        socketError != 0) {
      errno = socketError;
      return -1;
    }
  }

  if (fcntl(socketFd.get(), F_SETFL, originalFlags) < 0) {
    return -1;
  }

  const timeval timeout{
      timeoutMilliseconds / 1000,
      (timeoutMilliseconds % 1000) * 1000};
  setsockopt(socketFd.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(socketFd.get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  return dup(socketFd.get());
}

FileDescriptor openConnection(
    const std::string& host,
    std::uint16_t port,
    int timeoutMilliseconds) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* rawAddresses = nullptr;
  const std::string portText = std::to_string(port);
  const int resolutionResult = getaddrinfo(
      host.c_str(),
      portText.c_str(),
      &hints,
      &rawAddresses);
  if (resolutionResult != 0) {
    throw std::runtime_error(
        "Could not resolve " + host + ": " + gai_strerror(resolutionResult));
  }
  std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(
      rawAddresses,
      freeaddrinfo);

  int lastError = ECONNREFUSED;
  for (const addrinfo* address = addresses.get(); address != nullptr;
       address = address->ai_next) {
    const int socketFd = connectWithTimeout(address, timeoutMilliseconds);
    if (socketFd >= 0) {
      return FileDescriptor(socketFd);
    }
    lastError = errno;
  }

  throw std::runtime_error(
      "Could not connect to " + host + ":" + portText + ": " +
      std::strerror(lastError));
}

void sendAll(int socketFd, const std::string& request) {
  std::size_t sent = 0;
  while (sent < request.size()) {
    const ssize_t result = send(
        socketFd,
        request.data() + sent,
        request.size() - sent,
        0);
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result <= 0) {
      throw std::runtime_error(
          std::string("Failed to send /serverinfo request: ") +
          std::strerror(errno));
    }
    sent += static_cast<std::size_t>(result);
  }
}

std::string receiveAll(int socketFd) {
  std::string response;
  std::vector<char> buffer(8192);

  while (response.size() < kMaximumResponseBytes) {
    const ssize_t received = recv(socketFd, buffer.data(), buffer.size(), 0);
    if (received == 0) {
      break;
    }
    if (received < 0 && errno == EINTR) {
      continue;
    }
    if (received < 0) {
      throw std::runtime_error(
          std::string("Failed while reading /serverinfo: ") +
          std::strerror(errno));
    }
    response.append(buffer.data(), static_cast<std::size_t>(received));
  }

  if (response.size() >= kMaximumResponseBytes) {
    throw std::runtime_error("The /serverinfo response exceeded 1 MiB");
  }
  return response;
}

std::string decodeChunkedBody(const std::string& body) {
  std::string decoded;
  std::size_t position = 0;

  while (position < body.size()) {
    const auto lineEnd = body.find("\r\n", position);
    if (lineEnd == std::string::npos) {
      throw std::runtime_error("Malformed chunked /serverinfo response");
    }
    const std::string sizeText = body.substr(position, lineEnd - position);
    std::size_t chunkSize = 0;
    try {
      chunkSize = std::stoul(sizeText, nullptr, 16);
    } catch (const std::exception&) {
      throw std::runtime_error("Malformed chunk size in /serverinfo response");
    }
    if (chunkSize == 0) {
      break;
    }
    position = lineEnd + 2;
    if (position + chunkSize + 2 > body.size()) {
      throw std::runtime_error("Truncated chunked /serverinfo response");
    }
    decoded.append(body, position, chunkSize);
    position += chunkSize + 2;
  }
  return decoded;
}

std::string parseHttpBody(const std::string& response) {
  const auto firstLineEnd = response.find("\r\n");
  const auto headersEnd = response.find("\r\n\r\n");
  if (firstLineEnd == std::string::npos || headersEnd == std::string::npos) {
    throw std::runtime_error("Sunshine returned a malformed HTTP response");
  }

  const std::string statusLine = response.substr(0, firstLineEnd);
  if (statusLine.find(" 200 ") == std::string::npos) {
    throw std::runtime_error("Sunshine /serverinfo failed: " + statusLine);
  }

  const std::string headersLower = lower(response.substr(0, headersEnd));
  std::string body = response.substr(headersEnd + 4);
  if (headersLower.find("transfer-encoding: chunked") != std::string::npos) {
    body = decodeChunkedBody(body);
  }
  return body;
}

}  // namespace

ServerInfo ServerInfoClient::fetch(
    const std::string& host,
    std::uint16_t port,
    int timeoutMilliseconds,
    const std::string& clientUniqueId) {
  if (host.empty()) {
    throw std::invalid_argument("Host must not be empty");
  }
  if (port == 0) {
    throw std::invalid_argument("Port must be between 1 and 65535");
  }

  FileDescriptor connection = openConnection(host, port, timeoutMilliseconds);
  const bool isIpv6Literal = host.find(':') != std::string::npos;
  const std::string hostHeader = isIpv6Literal ? "[" + host + "]" : host;
  const std::string request =
      "GET /serverinfo?uniqueid=" + clientUniqueId + "&uuid=" + makeUuid() +
      " HTTP/1.1\r\nHost: " + hostHeader + ":" + std::to_string(port) +
      "\r\nAccept: application/xml\r\nConnection: close\r\n\r\n";

  sendAll(connection.get(), request);
  const std::string body = parseHttpBody(receiveAll(connection.get()));

  const std::string rootStatus = lower(body.substr(0, std::min<std::size_t>(body.size(), 256)));
  if (rootStatus.find("status_code=\"200\"") == std::string::npos &&
      rootStatus.find("status_code='200'") == std::string::npos) {
    throw std::runtime_error("Sunshine returned a non-success GameStream response");
  }

  const std::string gsVersion = extractTag(body, "gsversion");

  return ServerInfo{
      host,
      port,
      extractTag(body, "hostname"),
      extractTag(body, "appversion"),
      gsVersion.empty() ? extractTag(body, "gfeversion") : gsVersion,
      extractTag(body, "uniqueid"),
      extractTag(body, "state"),
      static_cast<std::uint16_t>(parseInt(extractTag(body, "httpsport"), 47984)),
      parseInt(extractTag(body, "pairstatus")) == 1,
      parseInt(extractTag(body, "currentgame")),
      parseDouble(extractTag(body, "servercodecmodesupport"))};
}

}  // namespace moonlight::network
