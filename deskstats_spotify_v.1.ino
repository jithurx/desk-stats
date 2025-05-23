#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // Or Adafruit_SH1106.h if you have that display

// --- Spotify API Configuration ---
const char* ssid = "";
const char* password = "";

// Get these from Spotify Developer Dashboard
String spotifyClientId = "";
String spotifyClientSecret = ""; // Not directly used by ESP if you have the base64 string
// Base64 encoded string of "YOUR_CLIENT_ID:YOUR_CLIENT_SECRET"
String spotifyClientCredsB64 = "";
// Get this using an external tool/script (see prerequisites)
String spotifyRefreshToken = "";

String spotifyAccessToken = "";
unsigned long tokenExpiryTime = 0;

const char* spotifyTokenHost = "accounts.spotify.com";
const char* spotifyApiHost = "api.spotify.com";
const int httpsPort = 443;

// --- OLED Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Button Configuration ---
const int PREV_BUTTON_PIN = D5; // GPIO14
const int PLAYPAUSE_BUTTON_PIN = D6; // GPIO12
const int NEXT_BUTTON_PIN = D7; // GPIO13

unsigned long lastDebounceTime[3] = {0, 0, 0};
bool lastButtonState[3] = {HIGH, HIGH, HIGH};
bool currentButtonState[3] = {HIGH, HIGH, HIGH};
const unsigned long debounceDelay = 50;

// --- Global Variables ---
String currentTrack = "No Track";
String currentArtist = "Spotify";
bool isPlaying = false;
unsigned long lastApiCallTime = 0;
const unsigned long apiCallInterval = 7000; // Fetch currently playing every 7 seconds

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial to connect

  Serial.println("\nSpotify OLED Display");
  Serial.print("ESP8266 Chip ID: "); Serial.println(ESP.getChipId());
  Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());


  pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PLAYPAUSE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Spotify Display");
  display.println("Connecting WiFi...");
  display.display();

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    displayMessage("Getting Token...", "", false);
    if (!refreshAccessToken()) {
      displayMessage("Token Failed!", "Check Serial", true);
      while (1) delay(1000);
    }
  } else {
    displayMessage("WiFi Failed!", "Check Credentials", true);
    while (1) delay(1000);
  }
  Serial.print("Setup Free Heap: "); Serial.println(ESP.getFreeHeap());
}

void loop() {
  handleButtons();

  if (millis() - lastApiCallTime > apiCallInterval || lastApiCallTime == 0) {
    if (WiFi.status() == WL_CONNECTED) {
      if (millis() >= tokenExpiryTime - 60000) { // Refresh 1 minute before expiry
        Serial.println("Access Token expired or nearing expiry. Refreshing...");
        displayMessage("Refreshing Token", "", false);
        if (!refreshAccessToken()) {
          Serial.println("Failed to refresh token!");
          displayMessage("Token Refresh Err", "Check Serial", true);
          delay(5000);
          return;
        }
      }
      getCurrentlyPlaying();
    } else {
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      displayMessage("WiFi Lost", "Reconnecting...", false);
      connectWiFi();
    }
    lastApiCallTime = millis();
  }
}

void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { // Increased attempts
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    display.setCursor(0, 20); // Adjust line based on previous messages
    display.print("WiFi Connected!");
    display.display();
    delay(1000);
  } else {
    Serial.println("\nWiFi connection failed.");
    display.setCursor(0, 20);
    display.print("WiFi Failed!");
    display.display();
  }
}

bool refreshAccessToken() {
  Serial.print("Refresh Token - Free Heap: "); Serial.println(ESP.getFreeHeap());
  client.setInsecure(); 

  if (!client.connect(spotifyTokenHost, httpsPort)) {
    Serial.println("Connection to token host failed!");
    return false;
  }

  String postData = "grant_type=refresh_token&refresh_token=" + spotifyRefreshToken;
  String request = "POST /api/token HTTP/1.1\r\n";
  request += "Host: " + String(spotifyTokenHost) + "\r\n";
  request += "Authorization: Basic " + spotifyClientCredsB64 + "\r\n";
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: " + String(postData.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += postData;

  client.print(request);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) { // Increased timeout
      Serial.println(">>> Token Client Timeout !");
      client.stop();
      return false;
    }
  }

  String responseBody = "";
  bool headersEnded = false;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      headersEnded = true;
    }
    if (headersEnded) {
      responseBody += line;
    }
  }
  client.stop();
  responseBody.trim(); // Trim whitespace, especially leading/trailing newlines from chunked responses

  // Serial.println("Token Response Body:"); Serial.println(responseBody);

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, responseBody);

  if (error) {
    Serial.print(F("deserializeJson() for token failed: "));
    Serial.println(error.f_str());
    Serial.println("Token raw response: " + responseBody);
    return false;
  }

  if (doc.containsKey("access_token")) {
    spotifyAccessToken = doc["access_token"].as<String>();
    long expiresIn = doc["expires_in"].as<long>();
    tokenExpiryTime = millis() + (expiresIn * 1000);
    Serial.println("New Access Token obtained.");
    return true;
  } else {
    Serial.println("Could not get access token from response.");
    Serial.println("Response: " + responseBody);
    return false;
  }
}


void getCurrentlyPlaying() {
  Serial.print("Get Currently Playing - Free Heap: "); Serial.println(ESP.getFreeHeap());
  if (spotifyAccessToken == "") {
    Serial.println("No access token. Cannot fetch current track.");
    return;
  }

  client.setInsecure();

  if (!client.connect(spotifyApiHost, httpsPort)) {
    Serial.println("Connection to API host failed!");
    displayMessage("API Conn Err", "", true);
    return;
  }

  String request = "GET /v1/me/player/currently-playing HTTP/1.1\r\n";
  request += "Host: " + String(spotifyApiHost) + "\r\n";
  request += "Authorization: Bearer " + spotifyAccessToken + "\r\n";
  request += "Connection: close\r\n\r\n";

  client.print(request);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println(">>> API Client Timeout !");
      client.stop();
      displayMessage("API Timeout", "", true);
      return;
    }
  }

  String httpStatusLine = client.readStringUntil('\n');
  Serial.println("Status: " + httpStatusLine);
  if (!httpStatusLine.startsWith("HTTP/1.1 200 OK") && !httpStatusLine.startsWith("HTTP/1.1 204 No Content")) {
    Serial.println("Spotify API Error: " + httpStatusLine);
    if (httpStatusLine.startsWith("HTTP/1.1 401")) {
      Serial.println("Token likely expired, will attempt refresh on next cycle.");
      tokenExpiryTime = millis(); // Force refresh
    }
    while (client.available()) client.read(); // Clear buffer
    client.stop();
    if (httpStatusLine.startsWith("HTTP/1.1 204")) {
      currentTrack = "Nothing Playing";
      currentArtist = "-";
      isPlaying = false;
      updateDisplay();
    } else {
      displayMessage("API Error", httpStatusLine.substring(9, 12), true);
    }
    return;
  }

  String headerLine;
  unsigned long contentLength = 0;
  while (client.available()) {
    headerLine = client.readStringUntil('\n');
    headerLine.trim(); // Remove \r
    // Serial.println(headerLine); // Debug headers
    if (headerLine.startsWith("Content-Length:")) {
        contentLength = headerLine.substring(headerLine.indexOf(':') + 1).toInt();
    }
    if (headerLine == "") { // Empty line signifies end of headers
      break;
    }
  }

  String responseBody = "";
  if (contentLength > 0) {
    responseBody.reserve(contentLength + 1); // Pre-allocate string
    char c;
    unsigned long readCount = 0;
    unsigned long bodyTimeout = millis();
    while(readCount < contentLength && (millis() - bodyTimeout < 5000) ) { // Timeout for reading body
        if (client.available()) {
            c = client.read();
            responseBody += c;
            readCount++;
        }
    }
    if(readCount < contentLength) {
        Serial.println("Error: Read less than Content-Length for body.");
    }
  } else { // Fallback if Content-Length is not present or zero (e.g. chunked, though Spotify usually sends CL)
      unsigned long bodyTimeout = millis();
      while(client.connected() && client.available() && (millis() - bodyTimeout < 5000)) {
          responseBody += client.readStringUntil('\n'); // Less efficient, but a fallback
      }
  }
  client.stop();
  responseBody.trim(); // Trim whitespace

  Serial.println("--- Currently Playing Full Response (Raw) ---"); // DEBUG
  Serial.println(responseBody);                                  // DEBUG
  Serial.println("--- End of Raw Response ---");                 // DEBUG
  Serial.print("Response Body Length: "); Serial.println(responseBody.length()); // DEBUG

  if (responseBody.length() == 0 || httpStatusLine.startsWith("HTTP/1.1 204")) {
    Serial.println("No content playing or empty response.");
    currentTrack = "Nothing Playing";
    currentArtist = "-";
    isPlaying = false;
    updateDisplay();
    return;
  }

  // --- JSON Filtering ---
  StaticJsonDocument<256> filter; // Increased size slightly for filter definition
  filter["is_playing"] = true;
  JsonObject item_filter = filter.createNestedObject("item");
  item_filter["name"] = true; // Track name
  JsonArray artists_filter = item_filter.createNestedArray("artists");
  artists_filter[0]["name"] = true; // First artist's name
  // item_filter["type"] = true; // To check if it's a track or episode
  // item_filter["duration_ms"] = true; // If you want duration
  // JsonObject album_filter = item_filter.createNestedObject("album");
  // album_filter["name"] = true; // Album name

  // Adjust JSON buffer size - should be much smaller with filtering
  DynamicJsonDocument doc(1536); // Try with 1536, can go up to 2048 if needed for very long names

  DeserializationError error = deserializeJson(doc, responseBody, DeserializationOption::Filter(filter));

  if (error) {
    Serial.print(F("deserializeJson() failed for currently playing: "));
    Serial.println(error.f_str());
    Serial.println("Response length (again): " + String(responseBody.length()));
    // Serial.println("First 200 chars of response: " + responseBody.substring(0,200)); // Already printed full
    displayMessage("JSON Parse Err", error.c_str(), true);
    return;
  }

  // Serial.println("Filtered JSON content:"); // DEBUG
  // serializeJsonPretty(doc, Serial); // DEBUG
  // Serial.println(); // DEBUG

  if (doc.isNull() || !doc.containsKey("item") || doc["item"].isNull()) {
    Serial.println("No item playing or item missing after filtering.");
    currentTrack = "Nothing Playing";
    currentArtist = "-";
    isPlaying = doc.containsKey("is_playing") ? doc["is_playing"].as<bool>() : false; // Still check is_playing for context
  } else {
    JsonObject item = doc["item"];
    currentTrack = item.containsKey("name") ? item["name"].as<String>() : "N/A (Track)";
    
    if (item.containsKey("artists") && item["artists"].is<JsonArray>() && !item["artists"].as<JsonArray>().isNull() && item["artists"][0].containsKey("name")) {
      currentArtist = item["artists"][0]["name"].as<String>();
    } else {
      currentArtist = "N/A (Artist)";
    }
    isPlaying = doc.containsKey("is_playing") ? doc["is_playing"].as<bool>() : false;
  }

  updateDisplay();
  Serial.print("End of Get Currently Playing - Free Heap: "); Serial.println(ESP.getFreeHeap());
}

void sendPlaybackCommand(String command) {
  Serial.print("Playback Command - Free Heap: "); Serial.println(ESP.getFreeHeap());
  if (spotifyAccessToken == "") {
    Serial.println("No access token. Cannot send command.");
    return;
  }

  client.setInsecure();

  if (!client.connect(spotifyApiHost, httpsPort)) {
    Serial.println("Connection to API host failed for command!");
    displayMessage("Cmd Conn Err", "", true);
    return;
  }

  String httpMethod = "POST";
  String path = "/v1/me/player/";
  if (command == "play") {
    path += "play";
    httpMethod = "PUT";
  } else if (command == "pause") {
    path += "pause";
    httpMethod = "PUT";
  } else if (command == "next") {
    path += "next";
  } else if (command == "previous") {
    path += "previous";
  } else {
    Serial.println("Unknown command: " + command);
    client.stop();
    return;
  }

  String request = httpMethod + " " + path + " HTTP/1.1\r\n";
  request += "Host: " + String(spotifyApiHost) + "\r\n";
  request += "Authorization: Bearer " + spotifyAccessToken + "\r\n";
  request += "Content-Length: 0\r\n";
  request += "Connection: close\r\n\r\n";

  client.print(request);

  unsigned long timeout = millis();
  String responseStatus;
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> API Command Timeout !");
      client.stop();
      displayMessage("Cmd Timeout", "", true);
      return;
    }
  }
  // Read only status line for commands, body is not usually important
  if(client.available()){
    responseStatus = client.readStringUntil('\n');
    Serial.println("Command Response Status: " + responseStatus);
  }
  
  while(client.available()) client.read(); // Clear rest of buffer
  client.stop();

  delay(1000);
  getCurrentlyPlaying();
}

void handleButtons() {
  int buttonPins[] = {PREV_BUTTON_PIN, PLAYPAUSE_BUTTON_PIN, NEXT_BUTTON_PIN};
  for (int i = 0; i < 3; i++) {
    bool reading = digitalRead(buttonPins[i]);
    if (reading != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != currentButtonState[i]) {
        currentButtonState[i] = reading;
        if (currentButtonState[i] == LOW) {
          Serial.print("Button "); Serial.print(i); Serial.println(" pressed.");
          if (i == 0) {
            displayMessage("Previous...", "", false);
            sendPlaybackCommand("previous");
          } else if (i == 1) {
            if (isPlaying) {
              displayMessage("Pausing...", "", false);
              sendPlaybackCommand("pause");
            } else {
              displayMessage("Playing...", "", false);
              sendPlaybackCommand("play");
            }
          } else if (i == 2) {
            displayMessage("Next...", "", false);
            sendPlaybackCommand("next");
          }
        }
      }
    }
    lastButtonState[i] = reading;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (isPlaying) {
    display.println(">> Playing <<");
  } else {
    display.println("-- Paused --");
  }
  display.drawLine(0, 9, SCREEN_WIDTH - 1, 9, SSD1306_WHITE);

  display.setCursor(0, 13);
  if (currentTrack.length() > 21) {
    display.println(currentTrack.substring(0, 21));
    display.setCursor(0, 23);
    if (currentTrack.length() > 42) {
      display.println(currentTrack.substring(21, 40) + "..");
    } else {
      display.println(currentTrack.substring(21));
    }
  } else {
    display.println(currentTrack);
    display.setCursor(0, 23); // Ensure it moves to next line area
    display.println("");      // for consistent spacing
  }

  display.drawLine(0, 33, SCREEN_WIDTH - 1, 33, SSD1306_WHITE);

  display.setCursor(0, 37);
  if (currentArtist.length() > 21) {
    display.println(currentArtist.substring(0, 19) + "..");
  } else {
    display.println(currentArtist);
  }

  display.setCursor(0, 55);
  display.print("IP:");
  if(WiFi.status() == WL_CONNECTED) {
    String ipStr = WiFi.localIP().toString();
    int lastDot = ipStr.lastIndexOf('.');
    if (lastDot != -1) {
        display.print(ipStr.substring(lastDot + 1));
    } else {
        display.print("N/A");
    }
  } else {
    display.print("---");
  }
  
  display.display();
}

void displayMessage(String line1, String line2, bool hold) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(line1);
  display.setCursor(0, 25);
  display.println(line2);
  display.display();
}
