#include "GameStreamClient.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace moonlight::gamestream {
namespace {

constexpr int kTimeoutMs = 8000;
constexpr int kPairingTimeoutMs = 5 * 60 * 1000;
constexpr std::size_t kMaximumResponseBytes = 4 * 1024 * 1024;

std::string mbedError(const std::string& operation, int code) {
  std::array<char, 256> text{};
  mbedtls_strerror(code, text.data(), text.size());
  return operation + ": " + text.data();
}

class CryptoContext {
 public:
  CryptoContext() {
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&rng_);
    static constexpr char kPersonalization[] = "moonlight-vega";
    const int result = mbedtls_ctr_drbg_seed(
        &rng_, mbedtls_entropy_func, &entropy_,
        reinterpret_cast<const unsigned char*>(kPersonalization),
        sizeof(kPersonalization) - 1);
    if (result != 0) {
      throw std::runtime_error(mbedError("Could not initialize secure randomness", result));
    }
  }
  ~CryptoContext() {
    mbedtls_ctr_drbg_free(&rng_);
    mbedtls_entropy_free(&entropy_);
  }
  void random(unsigned char* output, std::size_t size) {
    const int result = mbedtls_ctr_drbg_random(&rng_, output, size);
    if (result != 0) {
      throw std::runtime_error(mbedError("Could not generate secure random data", result));
    }
  }
  mbedtls_ctr_drbg_context* rng() { return &rng_; }

 private:
  mbedtls_entropy_context entropy_;
  mbedtls_ctr_drbg_context rng_;
};

CryptoContext& crypto() {
  static CryptoContext context;
  return context;
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  }).base();
  return first < last ? std::string(first, last) : std::string();
}

void replaceAll(std::string& value, std::string_view from, std::string_view to) {
  std::size_t position = 0;
  while ((position = value.find(from, position)) != std::string::npos) {
    value.replace(position, from.size(), to);
    position += to.size();
  }
}

std::string decodeXml(std::string value) {
  replaceAll(value, "&lt;", "<");
  replaceAll(value, "&gt;", ">");
  replaceAll(value, "&quot;", "\"");
  replaceAll(value, "&apos;", "'");
  replaceAll(value, "&amp;", "&");
  return value;
}

std::string tag(const std::string& xml, const std::string& name) {
  const std::string xmlLower = lower(xml);
  const std::string nameLower = lower(name);
  const auto opening = xmlLower.find("<" + nameLower);
  if (opening == std::string::npos) return {};
  const auto content = xmlLower.find('>', opening + nameLower.size() + 1);
  if (content == std::string::npos) return {};
  const auto closing = xmlLower.find("</" + nameLower + ">", content + 1);
  if (closing == std::string::npos) return {};
  return decodeXml(trim(xml.substr(content + 1, closing - content - 1)));
}

void requireGameStreamSuccess(const std::string& xml, const std::string& operation) {
  const auto header = lower(xml.substr(0, std::min<std::size_t>(xml.size(), 512)));
  if (header.find("status_code=\"200\"") == std::string::npos &&
      header.find("status_code='200'") == std::string::npos) {
    const auto message = tag(xml, "status_message");
    throw std::runtime_error(operation + " failed" + (message.empty() ? std::string() : ": " + message));
  }
}

std::string hex(const unsigned char* data, std::size_t size) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < size; ++i) {
    output << std::setw(2) << static_cast<unsigned>(data[i]);
  }
  return output.str();
}

std::vector<unsigned char> unhex(const std::string& value) {
  if ((value.size() & 1U) != 0) {
    throw std::runtime_error("Server returned malformed hexadecimal data");
  }
  std::vector<unsigned char> output(value.size() / 2);
  for (std::size_t i = 0; i < output.size(); ++i) {
    try {
      output[i] = static_cast<unsigned char>(
          std::stoul(value.substr(i * 2, 2), nullptr, 16));
    } catch (const std::exception&) {
      throw std::runtime_error("Server returned malformed hexadecimal data");
    }
  }
  return output;
}

std::string makeUuid() {
  std::array<unsigned char, 16> bytes{};
  crypto().random(bytes.data(), bytes.size());
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
  std::ostringstream result;
  result << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    result << std::setw(2) << static_cast<unsigned>(bytes[i]);
    if (i == 3 || i == 5 || i == 7 || i == 9) result << '-';
  }
  return result.str();
}

std::string dataDirectory() {
  const char* xdg = std::getenv("XDG_DATA_HOME");
  const char* home = std::getenv("HOME");
  std::string base = xdg && *xdg
      ? xdg
      : (home && *home && std::string(home) != "/"
             ? std::string(home) + "/.local/share"
             : (access("/data", W_OK) == 0 ? "/data" : "/tmp"));
  const std::string path = base + "/moonlight-vega";
  std::size_t slash = 1;
  while (true) {
    slash = path.find('/', slash);
    const std::string part = path.substr(0, slash);
    if (!part.empty() && mkdir(part.c_str(), 0700) != 0 && errno != EEXIST) {
      throw std::runtime_error(
          "Could not create Moonlight credential directory: " +
          std::string(std::strerror(errno)));
    }
    if (slash == std::string::npos) break;
    ++slash;
  }
  return path;
}

std::string readFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  return std::string(
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>());
}

void writePrivateFile(const std::string& path, const std::string& value) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output || !(output << value)) {
    throw std::runtime_error("Could not persist Moonlight credentials");
  }
  chmod(path.c_str(), 0600);
}

struct Identity {
  std::string uniqueId;
  std::string certificatePem;
  std::string privateKeyPem;
  mbedtls_x509_crt certificate;
  mbedtls_pk_context privateKey;

  Identity() {
    mbedtls_x509_crt_init(&certificate);
    mbedtls_pk_init(&privateKey);
  }
  ~Identity() {
    mbedtls_pk_free(&privateKey);
    mbedtls_x509_crt_free(&certificate);
  }
  Identity(const Identity&) = delete;
  Identity& operator=(const Identity&) = delete;
};

void parseIdentity(Identity& identity) {
  int result = mbedtls_x509_crt_parse(
      &identity.certificate,
      reinterpret_cast<const unsigned char*>(identity.certificatePem.c_str()),
      identity.certificatePem.size() + 1);
  if (result != 0) {
    throw std::runtime_error(mbedError("Could not parse client certificate", result));
  }
  result = mbedtls_pk_parse_key(
      &identity.privateKey,
      reinterpret_cast<const unsigned char*>(identity.privateKeyPem.c_str()),
      identity.privateKeyPem.size() + 1,
      nullptr,
      0,
      mbedtls_ctr_drbg_random,
      crypto().rng());
  if (result != 0) {
    throw std::runtime_error(mbedError("Could not parse client private key", result));
  }
}

void generateIdentity(Identity& identity) {
  int result = mbedtls_pk_setup(
      &identity.privateKey, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  if (result != 0) {
    throw std::runtime_error(mbedError("Could not initialize RSA key", result));
  }
  result = mbedtls_rsa_gen_key(
      mbedtls_pk_rsa(identity.privateKey),
      mbedtls_ctr_drbg_random,
      crypto().rng(),
      2048,
      65537);
  if (result != 0) {
    throw std::runtime_error(mbedError("Could not generate RSA key", result));
  }

  mbedtls_x509write_cert writer;
  mbedtls_x509write_crt_init(&writer);
  mbedtls_x509write_crt_set_version(&writer, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&writer, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&writer, &identity.privateKey);
  mbedtls_x509write_crt_set_issuer_key(&writer, &identity.privateKey);
  const char* name = "CN=Moonlight Vega OS";
  result = mbedtls_x509write_crt_set_subject_name(&writer, name);
  if (result == 0) {
    result = mbedtls_x509write_crt_set_issuer_name(&writer, name);
  }
  std::array<unsigned char, 16> serial{};
  crypto().random(serial.data(), serial.size());
  serial[0] &= 0x7f;
  if (result == 0) {
    result = mbedtls_x509write_crt_set_serial_raw(
        &writer, serial.data(), serial.size());
  }
  if (result == 0) {
    result = mbedtls_x509write_crt_set_validity(
        &writer, "20240101000000", "20491231235959");
  }
  if (result == 0) {
    result = mbedtls_x509write_crt_set_basic_constraints(&writer, 0, -1);
  }
  if (result != 0) {
    mbedtls_x509write_crt_free(&writer);
    throw std::runtime_error(mbedError("Could not prepare client certificate", result));
  }

  std::vector<unsigned char> certificateBuffer(8192);
  result = mbedtls_x509write_crt_pem(
      &writer,
      certificateBuffer.data(),
      certificateBuffer.size(),
      mbedtls_ctr_drbg_random,
      crypto().rng());
  mbedtls_x509write_crt_free(&writer);
  if (result != 0) {
    throw std::runtime_error(mbedError("Could not write client certificate", result));
  }
  identity.certificatePem =
      reinterpret_cast<const char*>(certificateBuffer.data());

  std::vector<unsigned char> keyBuffer(8192);
  result = mbedtls_pk_write_key_pem(
      &identity.privateKey, keyBuffer.data(), keyBuffer.size());
  if (result != 0) {
    throw std::runtime_error(mbedError("Could not write client private key", result));
  }
  identity.privateKeyPem = reinterpret_cast<const char*>(keyBuffer.data());

  result = mbedtls_x509_crt_parse(
      &identity.certificate,
      reinterpret_cast<const unsigned char*>(identity.certificatePem.c_str()),
      identity.certificatePem.size() + 1);
  if (result != 0) {
    throw std::runtime_error(
        mbedError("Could not load generated client certificate", result));
  }
}

Identity& identity() {
  static std::unique_ptr<Identity> instance;
  static std::once_flag once;
  std::call_once(once, [] {
    instance = std::make_unique<Identity>();
    const std::string directory = dataDirectory();
    instance->uniqueId = trim(readFile(directory + "/uniqueid"));
    instance->certificatePem = readFile(directory + "/client.pem");
    instance->privateKeyPem = readFile(directory + "/client-key.pem");
    if (instance->uniqueId.size() != 16) {
      std::array<unsigned char, 8> id{};
      crypto().random(id.data(), id.size());
      instance->uniqueId = hex(id.data(), id.size());
      std::transform(
          instance->uniqueId.begin(),
          instance->uniqueId.end(),
          instance->uniqueId.begin(),
          [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
      writePrivateFile(directory + "/uniqueid", instance->uniqueId);
    }
    if (instance->certificatePem.empty() || instance->privateKeyPem.empty()) {
      generateIdentity(*instance);
      writePrivateFile(directory + "/client.pem", instance->certificatePem);
      writePrivateFile(directory + "/client-key.pem", instance->privateKeyPem);
    } else {
      parseIdentity(*instance);
    }
  });
  return *instance;
}

std::string pinnedCertificatePath(const std::string& host) {
  std::string safe = host;
  std::replace_if(
      safe.begin(), safe.end(),
      [](unsigned char ch) {
        return !std::isalnum(ch) && ch != '.' && ch != '-';
      },
      '_');
  return dataDirectory() + "/server-" + safe + ".pem";
}

class Socket {
 public:
  explicit Socket(int descriptor = -1) : descriptor_(descriptor) {}
  ~Socket() {
    if (descriptor_ >= 0) close(descriptor_);
  }
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  Socket(Socket&& other) noexcept
      : descriptor_(std::exchange(other.descriptor_, -1)) {}
  int get() const { return descriptor_; }

 private:
  int descriptor_;
};

Socket connectTcp(
    const std::string& host,
    std::uint16_t port,
    int timeoutMilliseconds = kTimeoutMs) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  addrinfo* raw = nullptr;
  const auto portText = std::to_string(port);
  const int resolved =
      getaddrinfo(host.c_str(), portText.c_str(), &hints, &raw);
  if (resolved != 0) {
    throw std::runtime_error(
        "Could not resolve " + host + ": " + gai_strerror(resolved));
  }
  std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(raw, freeaddrinfo);
  int lastError = ECONNREFUSED;
  for (auto* address = addresses.get(); address; address = address->ai_next) {
    const int descriptor = socket(
        address->ai_family, address->ai_socktype, address->ai_protocol);
    if (descriptor < 0) continue;
    const int flags = fcntl(descriptor, F_GETFL, 0);
    fcntl(descriptor, F_SETFL, flags | O_NONBLOCK);
    int result =
        connect(descriptor, address->ai_addr, address->ai_addrlen);
    if (result < 0 && errno == EINPROGRESS) {
      pollfd item{descriptor, POLLOUT, 0};
      result = poll(&item, 1, timeoutMilliseconds);
      if (result > 0) {
        socklen_t length = sizeof(lastError);
        getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &lastError, &length);
        result = lastError == 0 ? 0 : -1;
      } else {
        lastError = result == 0 ? ETIMEDOUT : errno;
        result = -1;
      }
    }
    if (result == 0) {
      fcntl(descriptor, F_SETFL, flags);
      timeval timeout{
          timeoutMilliseconds / 1000,
          (timeoutMilliseconds % 1000) * 1000};
      setsockopt(
          descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      setsockopt(
          descriptor, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      return Socket(descriptor);
    }
    lastError = errno;
    close(descriptor);
  }
  throw std::runtime_error(
      "Could not connect to " + host + ":" + portText + ": " +
      std::strerror(lastError));
}

std::string requestText(
    const std::string& host,
    std::uint16_t port,
    const std::string& path) {
  const bool ipv6 = host.find(':') != std::string::npos;
  return "GET " + path + " HTTP/1.1\r\nHost: " +
      (ipv6 ? "[" + host + "]" : host) + ":" + std::to_string(port) +
      "\r\nAccept: application/xml\r\nUser-Agent: Moonlight-Vega/0.2\r\n"
      "Connection: close\r\n\r\n";
}

std::string decodeChunked(const std::string& body) {
  std::string output;
  std::size_t position = 0;
  while (position < body.size()) {
    const auto lineEnd = body.find("\r\n", position);
    if (lineEnd == std::string::npos) {
      throw std::runtime_error("Malformed chunked HTTP response");
    }
    std::size_t size = 0;
    try {
      size = std::stoul(body.substr(position, lineEnd - position), nullptr, 16);
    } catch (...) {
      throw std::runtime_error("Malformed HTTP chunk size");
    }
    if (size == 0) break;
    position = lineEnd + 2;
    if (position + size + 2 > body.size()) {
      throw std::runtime_error("Truncated HTTP response");
    }
    output.append(body, position, size);
    position += size + 2;
  }
  return output;
}

std::string httpBody(const std::string& response) {
  const auto lineEnd = response.find("\r\n");
  const auto headerEnd = response.find("\r\n\r\n");
  if (lineEnd == std::string::npos || headerEnd == std::string::npos) {
    throw std::runtime_error("Malformed HTTP response");
  }
  const auto status = response.substr(0, lineEnd);
  if (status.find(" 200 ") == std::string::npos) {
    throw std::runtime_error("GameStream HTTP request failed: " + status);
  }
  std::string body = response.substr(headerEnd + 4);
  if (lower(response.substr(0, headerEnd)).find(
          "transfer-encoding: chunked") != std::string::npos) {
    body = decodeChunked(body);
  }
  return body;
}

std::string plainGet(
    const std::string& host,
    std::uint16_t port,
    const std::string& path,
    int timeoutMilliseconds = kTimeoutMs) {
  Socket socket = connectTcp(host, port, timeoutMilliseconds);
  const auto request = requestText(host, port, path);
  std::size_t sent = 0;
  while (sent < request.size()) {
    const auto result = send(
        socket.get(), request.data() + sent, request.size() - sent, 0);
    if (result < 0 && errno == EINTR) continue;
    if (result <= 0) {
      throw std::runtime_error("Could not send GameStream HTTP request");
    }
    sent += static_cast<std::size_t>(result);
  }
  std::string response;
  std::array<char, 8192> buffer{};
  while (response.size() < kMaximumResponseBytes) {
    const auto count = recv(socket.get(), buffer.data(), buffer.size(), 0);
    if (count == 0) break;
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) {
      throw std::runtime_error("Could not read GameStream HTTP response");
    }
    response.append(buffer.data(), static_cast<std::size_t>(count));
  }
  if (response.size() >= kMaximumResponseBytes) {
    throw std::runtime_error("GameStream HTTP response was too large");
  }
  return httpBody(response);
}

std::string tlsGet(
    const std::string& host,
    std::uint16_t port,
    const std::string& path,
    const std::string& pinnedPem) {
  Identity& client = identity();
  mbedtls_net_context net;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config config;
  mbedtls_net_init(&net);
  mbedtls_ssl_init(&ssl);
  mbedtls_ssl_config_init(&config);
  auto cleanup = [&] {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_net_free(&net);
  };
  try {
    int result = mbedtls_net_connect(
        &net,
        host.c_str(),
        std::to_string(port).c_str(),
        MBEDTLS_NET_PROTO_TCP);
    if (result != 0) {
      throw std::runtime_error(mbedError("Could not connect with TLS", result));
    }
    result = mbedtls_ssl_config_defaults(
        &config,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    if (result != 0) {
      throw std::runtime_error(mbedError("Could not initialize TLS", result));
    }
    mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(
        &config, mbedtls_ctr_drbg_random, crypto().rng());
    result = mbedtls_ssl_conf_own_cert(
        &config, &client.certificate, &client.privateKey);
    if (result != 0) {
      throw std::runtime_error(
          mbedError("Could not configure client identity", result));
    }
    result = mbedtls_ssl_setup(&ssl, &config);
    if (result != 0) {
      throw std::runtime_error(mbedError("Could not prepare TLS session", result));
    }
    mbedtls_ssl_set_bio(
        &ssl, &net, mbedtls_net_send, mbedtls_net_recv, nullptr);
    while ((result = mbedtls_ssl_handshake(&ssl)) != 0) {
      if (result != MBEDTLS_ERR_SSL_WANT_READ &&
          result != MBEDTLS_ERR_SSL_WANT_WRITE) {
        throw std::runtime_error(mbedError("TLS handshake failed", result));
      }
    }
    if (!pinnedPem.empty()) {
      mbedtls_x509_crt pinned;
      mbedtls_x509_crt_init(&pinned);
      result = mbedtls_x509_crt_parse(
          &pinned,
          reinterpret_cast<const unsigned char*>(pinnedPem.c_str()),
          pinnedPem.size() + 1);
      const mbedtls_x509_crt* peer = mbedtls_ssl_get_peer_cert(&ssl);
      const bool matches =
          result == 0 && peer && peer->raw.len == pinned.raw.len &&
          std::memcmp(peer->raw.p, pinned.raw.p, peer->raw.len) == 0;
      mbedtls_x509_crt_free(&pinned);
      if (!matches) {
        throw std::runtime_error(
            "Server certificate changed; remove and pair this host again");
      }
    }
    const auto request = requestText(host, port, path);
    std::size_t sent = 0;
    while (sent < request.size()) {
      result = mbedtls_ssl_write(
          &ssl,
          reinterpret_cast<const unsigned char*>(request.data() + sent),
          request.size() - sent);
      if (result == MBEDTLS_ERR_SSL_WANT_READ ||
          result == MBEDTLS_ERR_SSL_WANT_WRITE) {
        continue;
      }
      if (result <= 0) {
        throw std::runtime_error(
            mbedError("Could not send HTTPS request", result));
      }
      sent += static_cast<std::size_t>(result);
    }
    std::string response;
    std::array<unsigned char, 8192> buffer{};
    while (response.size() < kMaximumResponseBytes) {
      result = mbedtls_ssl_read(&ssl, buffer.data(), buffer.size());
      if (result == 0 || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) break;
      if (result == MBEDTLS_ERR_SSL_WANT_READ ||
          result == MBEDTLS_ERR_SSL_WANT_WRITE) {
        continue;
      }
      if (result < 0) {
        throw std::runtime_error(
            mbedError("Could not read HTTPS response", result));
      }
      response.append(
          reinterpret_cast<const char*>(buffer.data()),
          static_cast<std::size_t>(result));
    }
    cleanup();
    return httpBody(response);
  } catch (...) {
    cleanup();
    throw;
  }
}

std::string commonQuery() {
  return "uniqueid=" + identity().uniqueId + "&uuid=" + makeUuid();
}

std::vector<unsigned char> digest(
    const unsigned char* data,
    std::size_t size,
    bool sha256) {
  std::vector<unsigned char> output(sha256 ? 32 : 20);
  const int result = sha256
      ? mbedtls_sha256(data, size, output.data(), 0)
      : mbedtls_sha1(data, size, output.data());
  if (result != 0) {
    throw std::runtime_error(mbedError("Could not hash pairing data", result));
  }
  return output;
}

std::vector<unsigned char> aesEcb(
    const std::vector<unsigned char>& input,
    const std::vector<unsigned char>& key,
    bool encrypt) {
  if ((input.size() % 16) != 0 || key.size() < 16) {
    throw std::runtime_error("Invalid pairing cipher data");
  }
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  int result = encrypt
      ? mbedtls_aes_setkey_enc(&aes, key.data(), 128)
      : mbedtls_aes_setkey_dec(&aes, key.data(), 128);
  std::vector<unsigned char> output(input.size());
  for (std::size_t i = 0; result == 0 && i < input.size(); i += 16) {
    result = mbedtls_aes_crypt_ecb(
        &aes,
        encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
        input.data() + i,
        output.data() + i);
  }
  mbedtls_aes_free(&aes);
  if (result != 0) {
    throw std::runtime_error(mbedError("Pairing cipher failed", result));
  }
  return output;
}

std::vector<unsigned char> sign(
    const unsigned char* data,
    std::size_t size) {
  auto hash = digest(data, size, true);
  std::vector<unsigned char> signature(
      mbedtls_pk_get_len(&identity().privateKey));
  std::size_t signatureLength = 0;
  const int result = mbedtls_pk_sign(
      &identity().privateKey,
      MBEDTLS_MD_SHA256,
      hash.data(),
      hash.size(),
      signature.data(),
      signature.size(),
      &signatureLength,
      mbedtls_ctr_drbg_random,
      crypto().rng());
  if (result != 0) {
    throw std::runtime_error(mbedError("Could not sign pairing secret", result));
  }
  signature.resize(signatureLength);
  return signature;
}

void verifySignature(
    const std::vector<unsigned char>& secret,
    const unsigned char* signature,
    std::size_t signatureSize,
    const std::string& certificatePem) {
  mbedtls_x509_crt certificate;
  mbedtls_x509_crt_init(&certificate);
  int result = mbedtls_x509_crt_parse(
      &certificate,
      reinterpret_cast<const unsigned char*>(certificatePem.c_str()),
      certificatePem.size() + 1);
  if (result == 0) {
    auto hash = digest(secret.data(), secret.size(), true);
    result = mbedtls_pk_verify(
        &certificate.pk,
        MBEDTLS_MD_SHA256,
        hash.data(),
        hash.size(),
        signature,
        signatureSize);
  }
  mbedtls_x509_crt_free(&certificate);
  if (result != 0) {
    throw std::runtime_error(
        "Pairing signature verification failed; possible man-in-the-middle attack");
  }
}

std::uint16_t httpsPort(const network::ServerInfo& info) {
  return info.httpsPort == 0 ? 47984 : info.httpsPort;
}

std::string pinnedCertificate(const std::string& host) {
  const auto value = readFile(pinnedCertificatePath(host));
  if (value.empty()) {
    throw std::runtime_error("This host is not paired with Moonlight Vega");
  }
  return value;
}

}  // namespace

GameStreamClient::GameStreamClient(std::string host, std::uint16_t httpPort)
    : host_(std::move(host)), httpPort_(httpPort) {
  if (host_.empty()) throw std::invalid_argument("Host must not be empty");
}

network::ServerInfo GameStreamClient::serverInfo() const {
  return network::ServerInfoClient::fetch(
      host_, httpPort_, kTimeoutMs, identity().uniqueId);
}

void GameStreamClient::pair(const std::string& pin) {
  if (pin.size() != 4 ||
      !std::all_of(pin.begin(), pin.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
      })) {
    throw std::invalid_argument("Pairing PIN must contain exactly four digits");
  }
  const auto info = serverInfo();
  if (info.paired && !readFile(pinnedCertificatePath(host_)).empty()) return;
  int major = 7;
  try {
    major = std::stoi(info.appVersion);
  } catch (...) {
  }
  const bool useSha256 = major >= 7;

  std::array<unsigned char, 16> salt{};
  crypto().random(salt.data(), salt.size());
  const auto clientCertHex = hex(
      reinterpret_cast<const unsigned char*>(identity().certificatePem.data()),
      identity().certificatePem.size());
  std::string response = plainGet(
      host_,
      httpPort_,
      "/pair?" + commonQuery() +
          "&devicename=roth&updateState=1&phrase=getservercert&salt=" +
          hex(salt.data(), salt.size()) + "&clientcert=" + clientCertHex,
      kPairingTimeoutMs);
  requireGameStreamSuccess(response, "Pairing");
  if (tag(response, "paired") != "1") {
    throw std::runtime_error("Wolf rejected the pairing PIN");
  }
  const auto serverCertBytes = unhex(tag(response, "plaincert"));
  const std::string serverCertificate(
      serverCertBytes.begin(), serverCertBytes.end());

  std::vector<unsigned char> saltPin(salt.begin(), salt.end());
  saltPin.insert(saltPin.end(), pin.begin(), pin.end());
  const auto aesKey = digest(saltPin.data(), saltPin.size(), useSha256);
  std::array<unsigned char, 16> challenge{};
  crypto().random(challenge.data(), challenge.size());
  const auto encryptedChallenge = aesEcb(
      std::vector<unsigned char>(challenge.begin(), challenge.end()),
      aesKey,
      true);
  response = plainGet(
      host_,
      httpPort_,
      "/pair?" + commonQuery() +
          "&devicename=roth&updateState=1&clientchallenge=" +
          hex(encryptedChallenge.data(), encryptedChallenge.size()));
  requireGameStreamSuccess(response, "Pairing challenge");
  auto encryptedServerResponse = unhex(tag(response, "challengeresponse"));
  auto serverResponse = aesEcb(encryptedServerResponse, aesKey, false);
  const std::size_t hashSize = useSha256 ? 32 : 20;
  if (serverResponse.size() < hashSize + 16) {
    throw std::runtime_error("Wolf returned an invalid pairing challenge");
  }

  std::array<unsigned char, 16> clientSecret{};
  crypto().random(clientSecret.data(), clientSecret.size());
  std::vector<unsigned char> challengeAnswer(
      serverResponse.begin() + hashSize,
      serverResponse.begin() + hashSize + 16);
  const auto& certSignature = identity().certificate.MBEDTLS_PRIVATE(sig);
  challengeAnswer.insert(
      challengeAnswer.end(),
      certSignature.p,
      certSignature.p + certSignature.len);
  challengeAnswer.insert(
      challengeAnswer.end(), clientSecret.begin(), clientSecret.end());
  auto answerHash = digest(
      challengeAnswer.data(), challengeAnswer.size(), useSha256);
  answerHash.resize(32, 0);
  auto encryptedAnswer = aesEcb(answerHash, aesKey, true);
  response = plainGet(
      host_,
      httpPort_,
      "/pair?" + commonQuery() +
          "&devicename=roth&updateState=1&serverchallengeresp=" +
          hex(encryptedAnswer.data(), encryptedAnswer.size()));
  requireGameStreamSuccess(response, "Pairing response");
  const auto pairingSecret = unhex(tag(response, "pairingsecret"));
  if (pairingSecret.size() <= 16) {
    throw std::runtime_error("Wolf returned an invalid pairing secret");
  }
  std::vector<unsigned char> serverSecret(
      pairingSecret.begin(), pairingSecret.begin() + 16);
  verifySignature(
      serverSecret,
      pairingSecret.data() + 16,
      pairingSecret.size() - 16,
      serverCertificate);

  std::vector<unsigned char> expectedMaterial(
      challenge.begin(), challenge.end());
  mbedtls_x509_crt serverCert;
  mbedtls_x509_crt_init(&serverCert);
  const int parseResult = mbedtls_x509_crt_parse(
      &serverCert,
      reinterpret_cast<const unsigned char*>(serverCertificate.c_str()),
      serverCertificate.size() + 1);
  if (parseResult != 0) {
    mbedtls_x509_crt_free(&serverCert);
    throw std::runtime_error("Wolf returned an invalid certificate");
  }
  expectedMaterial.insert(
      expectedMaterial.end(),
      serverCert.MBEDTLS_PRIVATE(sig).p,
      serverCert.MBEDTLS_PRIVATE(sig).p +
          serverCert.MBEDTLS_PRIVATE(sig).len);
  mbedtls_x509_crt_free(&serverCert);
  expectedMaterial.insert(
      expectedMaterial.end(), serverSecret.begin(), serverSecret.end());
  const auto expectedHash = digest(
      expectedMaterial.data(), expectedMaterial.size(), useSha256);
  if (!std::equal(
          expectedHash.begin(), expectedHash.end(), serverResponse.begin())) {
    throw std::runtime_error(
        "Wolf pairing challenge did not authenticate correctly");
  }

  auto clientSignature = sign(clientSecret.data(), clientSecret.size());
  std::vector<unsigned char> clientPairingSecret(
      clientSecret.begin(), clientSecret.end());
  clientPairingSecret.insert(
      clientPairingSecret.end(),
      clientSignature.begin(),
      clientSignature.end());
  response = plainGet(
      host_,
      httpPort_,
      "/pair?" + commonQuery() +
          "&devicename=roth&updateState=1&clientpairingsecret=" +
          hex(clientPairingSecret.data(), clientPairingSecret.size()));
  requireGameStreamSuccess(response, "Pairing finalization");
  if (tag(response, "paired") != "1") {
    throw std::runtime_error("Wolf did not accept the client certificate");
  }

  response = tlsGet(
      host_,
      httpsPort(info),
      "/pair?" + commonQuery() +
          "&devicename=roth&updateState=1&phrase=pairchallenge",
      serverCertificate);
  requireGameStreamSuccess(response, "Secure pairing verification");
  if (tag(response, "paired") != "1") {
    throw std::runtime_error("Secure pairing verification failed");
  }
  writePrivateFile(pinnedCertificatePath(host_), serverCertificate);
}

std::vector<App> GameStreamClient::apps() const {
  const auto info = serverInfo();
  const auto xml = tlsGet(
      host_,
      httpsPort(info),
      "/applist?" + commonQuery(),
      pinnedCertificate(host_));
  requireGameStreamSuccess(xml, "Application list");
  std::vector<App> result;
  const auto xmlLower = lower(xml);
  std::size_t position = 0;
  while ((position = xmlLower.find("<app", position)) != std::string::npos) {
    const auto openEnd = xmlLower.find('>', position);
    const auto end = xmlLower.find("</app>", openEnd);
    if (openEnd == std::string::npos || end == std::string::npos) break;
    const auto block = xml.substr(openEnd + 1, end - openEnd - 1);
    const auto idText = tag(block, "id");
    const auto name = tag(block, "apptitle");
    if (!idText.empty() && !name.empty()) {
      try {
        result.push_back(
            App{static_cast<std::int32_t>(std::stol(idText)), name});
      } catch (...) {
      }
    }
    position = end + 6;
  }
  return result;
}

LaunchResult GameStreamClient::launch(
    std::int32_t appId,
    std::int32_t width,
    std::int32_t height,
    std::int32_t fps,
    std::int32_t bitrateKbps,
    const std::string& codec) const {
  if (appId <= 0 || width <= 0 || height <= 0 || fps <= 0 ||
      bitrateKbps <= 0) {
    throw std::invalid_argument("Invalid stream configuration");
  }
  STREAM_CONFIGURATION config;
  LiInitializeStreamConfiguration(&config);
  config.width = width;
  config.height = height;
  config.fps = fps;
  config.bitrate = bitrateKbps;
  config.packetSize = 1024;
  config.streamingRemotely = STREAM_CFG_LOCAL;
  config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
  config.supportedVideoFormats = codec == "av1"
      ? VIDEO_FORMAT_AV1_MAIN8
      : (codec == "hevc" ? VIDEO_FORMAT_H265 : VIDEO_FORMAT_H264);
  crypto().random(
      reinterpret_cast<unsigned char*>(config.remoteInputAesKey),
      sizeof(config.remoteInputAesKey));
  std::memset(
      config.remoteInputAesIv, 0, sizeof(config.remoteInputAesIv));
  std::uint32_t keyId = 0;
  crypto().random(
      reinterpret_cast<unsigned char*>(config.remoteInputAesIv),
      sizeof(keyId));
  std::memcpy(&keyId, config.remoteInputAesIv, sizeof(keyId));
  keyId = htonl(keyId);
  const auto info = serverInfo();
  const int surround = SURROUNDAUDIOINFO_FROM_AUDIO_CONFIGURATION(
      config.audioConfiguration);
  const std::string path =
      "/" + std::string(info.currentGame ? "resume" : "launch") + "?" +
      commonQuery() + "&appid=" + std::to_string(appId) + "&mode=" +
      std::to_string(width) + "x" + std::to_string(height) + "x" +
      std::to_string(fps) +
      "&additionalStates=1&sops=1&rikey=" +
      hex(
          reinterpret_cast<const unsigned char*>(config.remoteInputAesKey),
          sizeof(config.remoteInputAesKey)) +
      "&rikeyid=" + std::to_string(keyId) +
      "&localAudioPlayMode=0&surroundAudioInfo=" +
      std::to_string(surround) +
      "&remoteControllersBitmap=1&gcmap=1&gcpersist=1" +
      LiGetLaunchUrlQueryParameters();
  const auto xml = tlsGet(
      host_,
      httpsPort(info),
      path,
      pinnedCertificate(host_));
  requireGameStreamSuccess(xml, "Launching application");
  if (tag(xml, "gamesession") == "0" || tag(xml, "resume") == "0") {
    throw std::runtime_error(
        "Wolf refused to launch the selected application");
  }
  const auto rtsp = tag(xml, "sessionurl0");
  if (rtsp.empty()) {
    throw std::runtime_error("Wolf did not return an RTSP session URL");
  }
  return LaunchResult{rtsp, config};
}

void GameStreamClient::quit() const {
  const auto info = serverInfo();
  const auto xml = tlsGet(
      host_,
      httpsPort(info),
      "/cancel?" + commonQuery(),
      pinnedCertificate(host_));
  requireGameStreamSuccess(xml, "Stopping application");
  if (tag(xml, "cancel") == "0") {
    throw std::runtime_error("Wolf refused to stop the application");
  }
}

}  // namespace moonlight::gamestream
