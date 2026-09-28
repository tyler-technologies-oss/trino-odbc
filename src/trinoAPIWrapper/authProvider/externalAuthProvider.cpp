#include "externalAuthProvider.hpp"

#include <chrono>
#include <map>
#include <thread>

#include "nlohmann/json.hpp"

#include "../../util/browserInteraction.hpp"
#include "../../util/delimKvpHelper.hpp"
#include "../../util/writeLog.hpp"

#include "tokenCacheAuthProviderBase.hpp"
#include "tokens/tokenCache.hpp"

using json = nlohmann::json;

/*
How long the user has to finish logging in through the browser
before the driver gives up waiting for a token.
*/
std::chrono::minutes EXTERNAL_AUTH_LOGIN_TIMEOUT = std::chrono::minutes(5);
/*
Trino holds each token poll open for up to 10 seconds before it
answers that the login is still pending. The request timeout has to
be longer than that, or curl gives up before Trino answers.
*/
long TOKEN_POLL_REQUEST_TIMEOUT_MS = 30000;
// The request timeout every other request uses, see connectionConfig.cpp.
long DEFAULT_REQUEST_TIMEOUT_MS            = 10000;
std::string EXTERNAL_AUTH_TRIGGER_ENDPOINT = "v1/statement";
std::string EXTERNAL_AUTH_TRIGGER_QUERY    = "SELECT 'authenticating...'";


struct ExternalAuthParams {
    std::string hostname                                   = "";
    unsigned short port                                    = 0;
    CURL* curl                                             = nullptr;
    std::string* responseData                              = nullptr;
    std::map<std::string, std::string>* responseHeaderData = nullptr;
    std::map<std::string, std::string>* requestHeaders     = nullptr;
};


std::string refreshExternalAuth(ExternalAuthParams& params) {
  if (not params.responseData) {
    return "";
  }
  if (not params.responseHeaderData) {
    return "";
  }

  WriteLog(LL_TRACE, "  Attempting external authentication");

  std::string URL = params.hostname + ":" + std::to_string(params.port) + "/" +
                    EXTERNAL_AUTH_TRIGGER_ENDPOINT;
  // In order to make sure we reauthenticate, we need to NOT pass
  // our current auth credentials. This way we will be asked to
  // authenticate again fresh. One way to do that is just to clear out all
  // custom headers from the request.
  // In theory this could be modified to only remove the
  // "Authorization" header, but that's significantly more work.
  struct curl_slist* headers = nullptr;
  curl_easy_setopt(params.curl, CURLOPT_HTTPHEADER, headers);
  curl_slist_free_all(headers);

  // Clear out the buffers for curl callbacks so we
  // don't end up with data from the prior CURL request
  params.responseData->clear();
  params.responseHeaderData->clear();
  curl_easy_setopt(params.curl, CURLOPT_URL, URL.c_str());
  curl_easy_setopt(
      params.curl, CURLOPT_POSTFIELDS, EXTERNAL_AUTH_TRIGGER_QUERY.c_str());

  // Now hit the Trino API, which will return a 401.
  CURLcode res = curl_easy_perform(params.curl);

  // Get the HTTP status code so we can log it in case of an error.
  long http_code = 0;
  curl_easy_getinfo(params.curl, CURLINFO_RESPONSE_CODE, &http_code);

  WriteLog(LL_TRACE,
           "  Auth trigger CURLcode returned: " + std::to_string(res));

  if (not params.responseHeaderData->count("www-authenticate")) {
    WriteLog(LL_ERROR,
             "  ERROR: Unauthenticated request did not return www-authenticate "
             "header");
    WriteLog(LL_ERROR, "  URL was: " + URL);
    WriteLog(LL_ERROR, "  CURLcode was: " + std::to_string(res));
    WriteLog(LL_ERROR, "  HTTP Code was: " + std::to_string(http_code));
    return "";
  }

  // Trino may send multiple www-authenticate challenges (e.g., Basic and
  // Bearer) as separate HTTP headers. They are stored concatenated with
  // newlines. Find the Bearer challenge that contains the redirect params
  // needed for external authentication.
  std::string wwwAuthHeader = params.responseHeaderData->at("www-authenticate");
  std::string bearerPrefix  = "Bearer ";
  std::string headerKVPs;

  size_t bearerPos = wwwAuthHeader.find(bearerPrefix);
  if (bearerPos != std::string::npos) {
    size_t kvpStart = bearerPos + bearerPrefix.size();
    size_t kvpEnd   = wwwAuthHeader.find('\n', kvpStart);
    headerKVPs      = wwwAuthHeader.substr(
        kvpStart,
        kvpEnd == std::string::npos ? std::string::npos : kvpEnd - kvpStart);
  }

  if (headerKVPs.empty()) {
    WriteLog(LL_ERROR,
             "  ERROR: No Bearer challenge found in www-authenticate header");
    WriteLog(LL_ERROR, "  Header value was: " + wwwAuthHeader);
    return "";
  }

  // The www-authenticate header contains a comma delimited list
  // of key vaule pairs.
  auto authServerInfo = parseKVPsFromCommaDelimStr(headerKVPs);

  // One of the key-value pairs is the x_redirect_server, which is
  // where you go to start authentication. It's an endpoint on the
  // Trino coordinator.
  std::string redirectServer = authServerInfo.at("x_redirect_server");
  WriteLog(LL_INFO, "  Authenticating to: " + redirectServer);

  // This kicks off the auth process in a browser window.
  openURLInDefaultBrowser(redirectServer);

  // Once that's done, we need to hit the a token server to obtain
  // an auth token. This is another endpoint on the trino coordinator
  // The coordinator blocks this call for up to 10 seconds while the
  // user logs in. If the login isn't done by then, it answers with a
  // "nextUri" to poll again, so we keep polling until the user
  // finishes or the login timeout runs out.
  std::string tokenServer = authServerInfo.at("x_token_server");
  std::string token       = "";
  auto deadline =
      std::chrono::steady_clock::now() + EXTERNAL_AUTH_LOGIN_TIMEOUT;
  curl_easy_setopt(
      params.curl, CURLOPT_TIMEOUT_MS, TOKEN_POLL_REQUEST_TIMEOUT_MS);
  while (std::chrono::steady_clock::now() < deadline) {
    // Clear out the buffers for curl callbacks so we
    // don't end up with data from the prior CURL request
    params.responseData->clear();
    params.responseHeaderData->clear();

    // Set the URL to the token server endpoint.
    curl_easy_setopt(params.curl, CURLOPT_URL, tokenServer.c_str());

    // Change from a POST back to a GET request.
    curl_easy_setopt(params.curl, CURLOPT_POSTFIELDS, nullptr);
    curl_easy_setopt(params.curl, CURLOPT_HTTPGET, 1L);

    // Hit the token server
    CURLcode res = curl_easy_perform(params.curl);
    if (res != CURLE_OK) {
      WriteLog(LL_DEBUG,
               "  Token poll failed, retrying: " +
                   std::string(curl_easy_strerror(res)));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    // Parse the response as JSON. It holds a "token" once the user
    // has logged in, a "nextUri" while the login is still pending,
    // or an "error" if the login failed.
    json responseJson = json::parse(*params.responseData, nullptr, false);
    if (responseJson.is_discarded() or not responseJson.is_object()) {
      WriteLog(LL_DEBUG,
               "  Token poll returned an unexpected response, retrying: " +
                   *params.responseData);
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }
    if (responseJson.contains("token")) {
      // This is the success path.
      WriteLog(LL_INFO, "  Authentication completed successfully");
      token = responseJson["token"].get<std::string>();
      break;
    } else if (responseJson.contains("error")) {
      WriteLog(LL_ERROR,
               "  ERROR: External auth was rejected: " +
                   responseJson["error"].dump());
      break;
    } else if (responseJson.contains("nextUri")) {
      WriteLog(LL_TRACE, "  Waiting for the browser login to finish");
      tokenServer = responseJson["nextUri"].get<std::string>();
    }
  }
  // Put back the request timeout that every other request uses.
  curl_easy_setopt(params.curl, CURLOPT_TIMEOUT_MS, DEFAULT_REQUEST_TIMEOUT_MS);

  if (token.empty()) {
    // This is the failure path, we didn't get a token.
    WriteLog(LL_ERROR, "  External auth failed");
  }
  return token;
}


class ExternalAuthConfig : public TokenCacheAuthProviderBase {

  public:
    ExternalAuthConfig(std::string hostname,
                       unsigned short port,
                       std::string connectionName)
        : TokenCacheAuthProviderBase(hostname, port, connectionName) {}

    std::string
    obtainAccessToken(CURL* curl,
                      std::string* responseData,
                      std::map<std::string, std::string>* responseHeaderData) {
      ExternalAuthParams params;
      params.curl               = curl;
      params.hostname           = this->hostname;
      params.port               = this->port;
      params.responseData       = responseData;
      params.responseHeaderData = responseHeaderData;
      params.requestHeaders     = &this->headers;

      // Obtain a fresh auth token
      return refreshExternalAuth(params);
    }

    virtual ~ExternalAuthConfig() = default;
};


std::unique_ptr<AuthConfig> getExternalAuthConfigPtr(
    std::string hostname, unsigned short port, std::string connectionName) {
  return std::make_unique<ExternalAuthConfig>(hostname, port, connectionName);
}
