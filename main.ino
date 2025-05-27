#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "PixelifySans_10pt.h" 

// --- config ---
const char* ssid = "";
const char* password = "";

// Get these from Spotify Developer Dashboard
String spotifyClientId = "";

// Base64 encoded string of "YOUR_CLIENT_ID:YOUR_CLIENT_SECRET"
String spotifyClientCredsB64 = "";

// Get this using an external tool/script (see prerequisites)
String spotifyRefreshToken = "";

// --- OLED Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Button Configuration ---
const int PREV_BUTTON_PIN = D5; 
const int PLAYPAUSE_BUTTON_PIN = D6; 
const int NEXT_BUTTON_PIN = D7; 

// --- Timing Constants ---
const unsigned long API_CALL_INTERVAL = 7000;          
const unsigned long DISPLAY_ANIMATION_INTERVAL = 33;  
const unsigned long TOKEN_REFRESH_LEAD_TIME = 60000; 
const unsigned long BUTTON_DEBOUNCE_DELAY = 50;
const unsigned long BOOT_ANIM_FRAME_DELAY = 250; 
const int SCROLL_DELAY_TRACK = 180; 
const int SCROLL_DELAY_ARTIST = 220; 

// --- Global Spotify State ---
String spotifyAccessToken = "";
unsigned long tokenExpiryTime = 0;
const char* spotifyTokenHost = "accounts.spotify.com";
const char* spotifyApiHost = "api.spotify.com";
const int httpsPort = 443;
WiFiClientSecure client; 

// --- Global Display State ---
String currentTrack = "No Track";
String currentArtist = "Spotify";
bool isPlaying = false;
unsigned long apiProgressMs = 0;    
unsigned long apiDurationMs = 0;    
unsigned long lastApiUpdateTime = 0; 
bool songDataValid = false; 
String prevTrackName = ""; 
String prevArtistName = ""; 
int trackScrollOffset = 0;
unsigned long lastTrackScrollTime = 0;
int artistScrollOffset = 0;
unsigned long lastArtistScrollTime = 0;

// Font & Layout Constants
const int SYS_FONT_TEXT_HEIGHT = 8; 
const int SYS_FONT_LINE_SPACING = 2;
const int TEXT_AREA_WIDTH_PIXELS = SCREEN_WIDTH - 4; 
const int CUSTOM_FONT_Y_ADVANCE = 24; 

// Boot Animation State
enum BootStatus { BS_LOGO_INIT, BS_WIFI_CONNECTING, BS_WIFI_DONE, BS_TOKEN_FETCHING, BS_TOKEN_DONE, BS_ERROR, BS_NONE };
BootStatus currentBootStatus = BS_LOGO_INIT; 
bool bootSequenceActive = true; 
unsigned long lastBootAnimFrameTime = 0;
int bootAnimFrame = 0;


// Wavy Progress Bar Settings
int wave_amplitude = 2;       
float wave_frequency = 0.22;  
int wave_baseline_y = SCREEN_HEIGHT - 5; 
int wave_thickness = 3;       
static float continuousWavePhase = 0.0f; 
const float wavePhaseIncrement = 0.10f; 

// Button State
unsigned long lastButtonDebounceTime[3] = {0, 0, 0};
bool lastButtonPhysicalState[3] = {HIGH, HIGH, HIGH}; 
bool currentButtonLogicalState[3] = {HIGH, HIGH, HIGH}; 

// Timing for main loop operations
unsigned long lastApiCallAttemptTime = 0;
unsigned long lastDisplayUpdateTime = 0;


// Function Declarations
void updateDisplay();
void drawBootScreen(); 
void displayMessage(String line1, String line2, bool holdBriefly = false, bool alsoToSerial = true);
void initialConnectAndAuth();
bool connectWiFiRuntime();
bool refreshAccessToken();
void getCurrentlyPlaying();
void sendPlaybackCommand(String command);
void handleButtons();
int getTextWidth(const String& str); 


// ==========================================================================
// SETUP
// ==========================================================================
void setup() {
  Serial.begin(115200);
  unsigned long setupStartTime = millis();
  while (!Serial && (millis() - setupStartTime < 2000)) { delay(10); yield(); }

  Serial.println("\nSpotify OLED Display v1.4.6");
  Serial.print("Initial Free Heap: "); Serial.println(ESP.getFreeHeap());

  pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PLAYPAUSE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); 
  }
  
  initialConnectAndAuth(); 

  bootSequenceActive = false; 
  Serial.print("Setup Complete - Free Heap: "); Serial.println(ESP.getFreeHeap());
  lastApiCallAttemptTime = millis() - API_CALL_INTERVAL - 1000; 
  lastDisplayUpdateTime = millis(); 
  getCurrentlyPlaying(); 
}

// ==========================================================================
// INITIAL CONNECTION & AUTH (During Boot)
// ==========================================================================
void initialConnectAndAuth() {
  bootSequenceActive = true;
  currentBootStatus = BS_LOGO_INIT; 
  drawBootScreen(); 
  delay(1500); 

  currentBootStatus = BS_WIFI_CONNECTING;
  Serial.println("Boot: Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  unsigned long stageStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    drawBootScreen(); 
    if (millis() - stageStartTime > 250) { 
        attempts++;
        Serial.print(".");
        stageStartTime = millis();
    }
    delay(50); 
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nBoot: WiFi connected!");
    Serial.print("Boot: IP address: "); Serial.println(WiFi.localIP());
    currentBootStatus = BS_WIFI_DONE;
    drawBootScreen(); 
    delay(1000); 
  } else {
    Serial.println("\nBoot: WiFi connection FAILED.");
    currentBootStatus = BS_ERROR;
    drawBootScreen(); 
    while (1) { delay(1000); yield(); }
  }

  currentBootStatus = BS_TOKEN_FETCHING;
  Serial.println("Boot: Getting Spotify Token...");
  drawBootScreen(); 
  delay(500); 
  
  if (refreshAccessToken()) { 
    Serial.println("Boot: Spotify Token obtained!");
    currentBootStatus = BS_TOKEN_DONE;
    drawBootScreen();
    delay(1000); 
  } else {
    Serial.println("Boot: Spotify Token FAILED.");
    currentBootStatus = BS_ERROR;
    drawBootScreen(); 
    while (1) { delay(1000); yield(); }
  }
}


// ==========================================================================
// MAIN LOOP
// ==========================================================================
void loop() {
  if (bootSequenceActive && currentBootStatus != BS_ERROR) { 
      drawBootScreen(); 
      delay(50);
      yield();
      return; 
  }

  handleButtons(); 

  if (millis() - lastApiCallAttemptTime >= API_CALL_INTERVAL) { 
    if (WiFi.status() == WL_CONNECTED) {
      bool needsTokenRefresh = (spotifyAccessToken == "" || millis() >= tokenExpiryTime - TOKEN_REFRESH_LEAD_TIME);
      
      if (needsTokenRefresh) {
        Serial.println("Runtime: Token needs refresh or is missing.");
        if (!refreshAccessToken()) {
          Serial.println("Runtime: Failed to refresh token! Will retry later.");
        } else {
           getCurrentlyPlaying(); 
        }
      } else {
         getCurrentlyPlaying(); 
      }
    } else {
      Serial.println("Runtime: WiFi disconnected. Attempting to reconnect...");
      if (connectWiFiRuntime()) { 
          if (spotifyAccessToken != "" && millis() < tokenExpiryTime) {
              getCurrentlyPlaying();
          }
      }
    }
    lastApiCallAttemptTime = millis(); 
  }

  if (millis() - lastDisplayUpdateTime >= DISPLAY_ANIMATION_INTERVAL) { 
    updateDisplay(); 
    lastDisplayUpdateTime = millis(); 
  }
  yield(); 
}

// ==========================================================================
// DISPLAY FUNCTIONS
// ==========================================================================
void displayMessage(String line1, String line2, bool holdBriefly, bool alsoToSerial) { 
  if (bootSequenceActive && !alsoToSerial && !holdBriefly) { 
      if (alsoToSerial) Serial.println("OLED_MSG_SKIPPED_BOOT_NON_HOLD: L1(" + line1 + ") L2(" + line2 + ")");
      return;
  }
  if (alsoToSerial) {
    Serial.println("OLED_MSG: L1(" + line1 + ") L2(" + line2 + ")");
  }
  
  unsigned long messageStartTime = millis();
  unsigned long displayDuration = holdBriefly ? 2000 : 750; 

  display.clearDisplay();
  display.setFont(); 
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 20); 
  display.println(line1);
  display.setCursor(5, 35);
  display.println(line2);
  display.display();

  while(millis() - messageStartTime < displayDuration) {
      delay(50);
      yield();
  }
  
  lastDisplayUpdateTime = millis() - DISPLAY_ANIMATION_INTERVAL - 10; 
}

void drawBootScreen() { 
    display.clearDisplay(); 
    display.setTextColor(SSD1306_WHITE); 
    int16_t x1,y1; uint16_t w,h; String msg1, msg2, statusText = "";
    
    const GFXfont* logoFont = &PixelifySans_VariableFont_wght10pt7b; 

    // --- Draw Logo "Spotify Stats" (Centered) ---
    display.setFont(logoFont); 
    msg1="Spotify";
    display.getTextBounds(msg1.c_str(),0,0,&x1,&y1,&w,&h);
    int totalLogoHeight = CUSTOM_FONT_Y_ADVANCE * 2 - (CUSTOM_FONT_Y_ADVANCE - h) *2 ; 
    int logoStartY = (SCREEN_HEIGHT - totalLogoHeight - SYS_FONT_TEXT_HEIGHT - 5) / 2; 
    
    display.setCursor((SCREEN_WIDTH-w)/2, logoStartY + (CUSTOM_FONT_Y_ADVANCE - h)/2 ); 
    display.print(msg1);
    
    msg2="Stats";
    display.getTextBounds(msg2.c_str(),0,0,&x1,&y1,&w,&h);
    display.setCursor((SCREEN_WIDTH-w)/2, logoStartY + CUSTOM_FONT_Y_ADVANCE + (CUSTOM_FONT_Y_ADVANCE - h)/2 ); 
    display.print(msg2);
    
    // --- Status Messages & Developer Tag on Bottom Line ---
    display.setFont(); // Switch to default system font for status
    display.setTextSize(1);
    int bottomLineY = SCREEN_HEIGHT - SYS_FONT_TEXT_HEIGHT - 1; 
    // Developer Tag (Bottom-Left)
    String devTag = "@jrx";
    display.setCursor(2, bottomLineY); 
    display.print(devTag);

    // Determine status text
    if (currentBootStatus == BS_ERROR) { 
        statusText = "ERROR";
    } else if (currentBootStatus == BS_WIFI_CONNECTING) {
        statusText = "WiFi";
        for(int i=0; i < bootAnimFrame % 4; i++) statusText += ".";
    } else if (currentBootStatus == BS_WIFI_DONE) {
        statusText = "WiFi OK";
    } else if (currentBootStatus == BS_TOKEN_FETCHING) {
        statusText = "Auth"; // Shorter for space
        for(int i=0; i < bootAnimFrame % 4; i++) statusText += ".";
    } else if (currentBootStatus == BS_TOKEN_DONE) {
        statusText = "Auth OK";
    }

    if (statusText != "") {
      display.getTextBounds(statusText.c_str(),0,0,&x1,&y1,&w,&h);
      display.setCursor(SCREEN_WIDTH - w - 2, bottomLineY); // Bottom-right
      display.print(statusText);
    }
    
    display.display(); 
    
    if(millis()-lastBootAnimFrameTime > BOOT_ANIM_FRAME_DELAY){ 
        bootAnimFrame++;
        lastBootAnimFrameTime = millis();
    }
}


int getTextWidth(const String& str) {
    int16_t x1, y1;
    uint16_t w, h;
    char str_array[str.length() + 1];
    str.toCharArray(str_array, str.length() + 1);
    display.getTextBounds(str_array, 0, 0, &x1, &y1, &w, &h);
    return w;
}


void updateDisplay() {
  if (bootSequenceActive) { 
      return;
  }

  display.clearDisplay();
  display.setFont(nullptr); 
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false); 
  
  int16_t w,h; 

  int cursor_y_pos = 0; 
  display.setCursor(0, cursor_y_pos); 
  String statusText = isPlaying ? ">> Playing" : "-- Paused ";
  display.print(statusText);
  
  String ipSuffix = "---";
  if(WiFi.status() == WL_CONNECTED) {
    String ipStr = WiFi.localIP().toString();
    int lastDot = ipStr.lastIndexOf('.');
    if (lastDot != -1) ipSuffix = ipStr.substring(lastDot + 1);
    else ipSuffix = "N/A";
  }
  w = getTextWidth(ipSuffix); 
  display.setCursor(SCREEN_WIDTH - w - 1, cursor_y_pos); 
  display.print(ipSuffix);

  cursor_y_pos += SYS_FONT_TEXT_HEIGHT + SYS_FONT_LINE_SPACING + 2; 
  display.setCursor(0, cursor_y_pos);
  w = getTextWidth(currentTrack); 

  if (songDataValid && w > TEXT_AREA_WIDTH_PIXELS) { 
    if (millis() - lastTrackScrollTime > SCROLL_DELAY_TRACK) { 
      trackScrollOffset++;
      if (trackScrollOffset > w + 20) { 
        trackScrollOffset = 0;
      }
      lastTrackScrollTime = millis();
    }
    display.setCursor(-trackScrollOffset, cursor_y_pos);
    display.print(currentTrack);
    if (w - trackScrollOffset < TEXT_AREA_WIDTH_PIXELS - 20) { 
        display.setCursor(w - trackScrollOffset + 20, cursor_y_pos);
        display.print(currentTrack);
    }
  } else {
    display.setCursor(max(0, (SCREEN_WIDTH - w) / 2), cursor_y_pos); 
    display.print(currentTrack); 
  }

  cursor_y_pos += SYS_FONT_TEXT_HEIGHT + SYS_FONT_LINE_SPACING; 
  display.setCursor(0, cursor_y_pos);
  w = getTextWidth(currentArtist); 

  if (songDataValid && w > TEXT_AREA_WIDTH_PIXELS) {
    if (millis() - lastArtistScrollTime > SCROLL_DELAY_ARTIST) { 
      artistScrollOffset++;
      if (artistScrollOffset > w + 20) { 
        artistScrollOffset = 0;
      }
      lastArtistScrollTime = millis();
    }
    display.setCursor(-artistScrollOffset, cursor_y_pos);
    display.print(currentArtist);
    if (w - artistScrollOffset < TEXT_AREA_WIDTH_PIXELS - 20) {
        display.setCursor(w - artistScrollOffset + 20, cursor_y_pos);
        display.print(currentArtist);
    }
  } else {
    display.setCursor(max(0, (SCREEN_WIDTH - w) / 2), cursor_y_pos);
    display.print(currentArtist);
  }
  
  wave_baseline_y = SCREEN_HEIGHT - (wave_amplitude + (wave_thickness / 2) + 2) ; 

  if (songDataValid && apiDurationMs > 0) {
    unsigned long effectiveProgressMs = apiProgressMs;
    if (isPlaying) {
      if (lastApiUpdateTime > 0 && millis() > lastApiUpdateTime) { 
        effectiveProgressMs += (millis() - lastApiUpdateTime); 
      }
    }
    effectiveProgressMs = constrain(effectiveProgressMs, 0, apiDurationMs);

    int current_progress_x = map(effectiveProgressMs, 0, apiDurationMs, 0, SCREEN_WIDTH); 
    current_progress_x = constrain(current_progress_x, 0, SCREEN_WIDTH);

    if (current_progress_x < SCREEN_WIDTH ) { 
        display.drawFastHLine(current_progress_x, wave_baseline_y, SCREEN_WIDTH - current_progress_x, SSD1306_WHITE);
    }

    continuousWavePhase += wavePhaseIncrement; 
    if (continuousWavePhase >= (2.0f * PI)) { 
        continuousWavePhase -= (2.0f * PI); 
    }

    for (int x_coord = 0; x_coord < current_progress_x; x_coord++) { 
      int y_center_of_wave = wave_baseline_y + round(wave_amplitude * sin((x_coord * wave_frequency) + continuousWavePhase));
      
      int current_wave_segment_thickness = wave_thickness;
      int taper_length = wave_thickness * 2 + 2; 
      
      if (x_coord >= current_progress_x - taper_length && current_progress_x > taper_length) { 
           current_wave_segment_thickness = max(1, wave_thickness - ( ( (current_progress_x -1) - x_coord) / 2 ) );
      } else if (x_coord < taper_length && current_progress_x > taper_length) { 
           current_wave_segment_thickness = max(1, (x_coord / 2) + 1);
      } else if (current_progress_x <= taper_length) { 
           current_wave_segment_thickness = max(1, min( (x_coord/2)+1, wave_thickness - ((current_progress_x -1 - x_coord)/2) ) );
      }
      current_wave_segment_thickness = min(current_wave_segment_thickness, wave_thickness);

      for (int i = 0; i < current_wave_segment_thickness; ++i) {
          int y_pixel = y_center_of_wave - (current_wave_segment_thickness / 2) + i; 
          y_pixel = constrain(y_pixel, 0, SCREEN_HEIGHT - 1); 
          display.drawPixel(x_coord, y_pixel, SSD1306_WHITE); 
      }
    }
  } else { 
    display.drawFastHLine(0, wave_baseline_y, SCREEN_WIDTH, SSD1306_WHITE);
  }
  
  display.display();
}


// ==========================================================================
// WIFI & SPOTIFY FUNCTIONS
// ==========================================================================
bool connectWiFiRuntime() { 
  Serial.print("Runtime: Attempting WiFi connection...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    Serial.print(".");
    delay(500);
    attempts++;
    yield(); 
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nRuntime: WiFi Re-connected!");
    return true;
  } else {
    Serial.println("\nRuntime: WiFi Re-connection Failed.");
    return false;
  }
}

bool refreshAccessToken() {
  bool isBoot = bootSequenceActive; 
  if (!isBoot) { Serial.print("Runtime: Refresh Token - Free Heap: "); Serial.println(ESP.getFreeHeap());}
  else { Serial.print("Boot: Refresh Token - Free Heap: "); Serial.println(ESP.getFreeHeap());}
  client.setInsecure(); if (!client.connect(spotifyTokenHost, httpsPort)) { Serial.println(isBoot ? "Boot: Token host connection failed!" : "Runtime: Token host connection failed!"); return false;}
  String postData = "grant_type=refresh_token&refresh_token=" + spotifyRefreshToken;
  String request = "POST /api/token HTTP/1.1\r\n" + String("Host: ") + spotifyTokenHost + "\r\n" + "Authorization: Basic " + spotifyClientCredsB64 + "\r\n" + "Content-Type: application/x-www-form-urlencoded\r\n" + "Content-Length: " + String(postData.length()) + "\r\n" + "Connection: close\r\n\r\n" + postData;
  client.print(request); unsigned long timeout = millis();
  while (client.available() == 0) { if (millis() - timeout > 10000) { Serial.println(isBoot ? "Boot: Token client timeout!" : "Runtime: Token client timeout!"); client.stop(); return false;} yield(); }
  String responseBody = ""; bool headersEnded = false;
  while (client.available()) { String line = client.readStringUntil('\n'); if (line == "\r") headersEnded = true; if (headersEnded) responseBody += line; yield(); }
  client.stop(); responseBody.trim(); DynamicJsonDocument doc(1024); DeserializationError error = deserializeJson(doc, responseBody);
  if (error) { Serial.print(isBoot ? "Boot: " : "Runtime: "); Serial.print(F("deserializeJson() for token failed: ")); Serial.println(error.f_str()); return false; }
  if (doc.containsKey("access_token")) { spotifyAccessToken = doc["access_token"].as<String>(); tokenExpiryTime = millis() + (doc["expires_in"].as<long>() * 1000); Serial.println(isBoot ? "Boot: New Access Token obtained." : "Runtime: New Access Token obtained."); return true;}
  else { Serial.println(isBoot ? "Boot: Could not get access token from response." : "Runtime: Could not get access token from response."); return false; }
}

void getCurrentlyPlaying() {
  bool isBoot = bootSequenceActive; 
  if(!isBoot) { 
  }
  if (spotifyAccessToken == "") { 
    songDataValid = false; 
    return;
  }
  client.setInsecure(); 
  if (!client.connect(spotifyApiHost, httpsPort)) { 
    Serial.println("getCurrentlyPlaying: Connection to API host failed!"); 
    songDataValid = false; 
    return;
  }

  String request = "GET /v1/me/player/currently-playing?additional_types=track,episode HTTP/1.1\r\n" + String("Host: ") + spotifyApiHost + "\r\n" + "Authorization: Bearer " + spotifyAccessToken + "\r\n" + "Connection: close\r\n\r\n";
  client.print(request); 
  unsigned long timeout = millis();
  while (client.available() == 0) { 
    if (millis() - timeout > 10000) { Serial.println("getCurrentlyPlaying: API Client Timeout !"); client.stop(); songDataValid = false; return;} 
    yield(); 
  }

  String httpStatusLine = client.readStringUntil('\n');
  if (!httpStatusLine.startsWith("HTTP/1.1 200 OK") && !httpStatusLine.startsWith("HTTP/1.1 204 No Content")) { 
    Serial.println("getCurrentlyPlaying: Spotify API Error: " + httpStatusLine); 
    if (httpStatusLine.startsWith("HTTP/1.1 401")) tokenExpiryTime = millis(); 
    while (client.available()) { client.read(); yield(); } 
    client.stop(); 
    bool pSV = songDataValid; songDataValid=false; 
    if(httpStatusLine.startsWith("HTTP/1.1 204")){currentTrack="Nothing Playing";currentArtist="-";isPlaying=false;apiProgressMs=0;apiDurationMs=0;}
    else if(!isBoot&&!pSV){currentTrack="API Error";currentArtist="-";isPlaying=false;} 
    return;
  }

  String headerLine; unsigned long contentLength = 0;
  while (client.available()) { headerLine = client.readStringUntil('\n'); headerLine.trim(); if (headerLine.startsWith("Content-Length:")) contentLength = headerLine.substring(headerLine.indexOf(':') + 1).toInt(); if (headerLine == "") break; yield(); }
  
  String responseBody = "";
  if (contentLength > 0) { 
    responseBody.reserve(contentLength + 1); char c; unsigned long r = 0, bT = millis(); 
    while(r < contentLength && (millis()-bT < 5000)) { if(client.available()){ c=client.read(); responseBody+=c; r++; } yield(); } 
  } else { 
    unsigned long bT = millis(); while(client.connected()&&client.available()&&(millis()-bT<5000)) { responseBody+=client.readStringUntil('\n'); yield(); } 
  }
  client.stop(); responseBody.trim();

  if (responseBody.length() == 0 || httpStatusLine.startsWith("HTTP/1.1 204")) { 
    currentTrack = "Nothing Playing"; currentArtist = "-"; isPlaying = false; songDataValid = false; apiProgressMs = 0; apiDurationMs = 0; 
    return;
  }

  StaticJsonDocument<300> filter; filter["is_playing"]=true; filter["progress_ms"]=true; filter["item"]["name"]=true; filter["item"]["duration_ms"]=true; filter["item"]["artists"][0]["name"]=true;
  DynamicJsonDocument doc(1536); DeserializationError error = deserializeJson(doc, responseBody, DeserializationOption::Filter(filter));
  
  if (error) { Serial.print("getCurrentlyPlaying: deserializeJson() failed: "); Serial.println(error.f_str()); songDataValid = false; return;}
  
  if (doc.isNull()) { Serial.println("getCurrentlyPlaying: Filtered document is null."); currentTrack = "Filter Error"; currentArtist = "-"; isPlaying = false; songDataValid = false; apiProgressMs = 0; apiDurationMs = 0;}
  else { 
    isPlaying = doc.containsKey("is_playing") ? doc["is_playing"].as<bool>() : false; 
    apiProgressMs = doc.containsKey("progress_ms") ? doc["progress_ms"].as<unsigned long>() : 0; 
    if (!doc.containsKey("item") || doc["item"].isNull()) { 
      currentTrack = "Nothing Playing"; currentArtist = "-"; 
      apiDurationMs = 0; 
      songDataValid = (apiDurationMs > 0); 
    } else { 
      JsonObject item = doc["item"]; 
      String nT = item.containsKey("name")?item["name"].as<String>():"N/A (T)"; 
      String nA = (item.containsKey("artists")&&item["artists"].is<JsonArray>()&&item["artists"].as<JsonArray>().size()>0&&item["artists"][0].containsKey("name"))?item["artists"][0]["name"].as<String>():"N/A (A)";
      
      if(nT!=prevTrackName){currentTrack=nT;prevTrackName=currentTrack;trackScrollOffset=0;lastTrackScrollTime=millis();}else{currentTrack=nT;} if(prevTrackName=="" && currentTrack!="No Track")prevTrackName=currentTrack;
      if(nA!=prevArtistName){currentArtist=nA;prevArtistName=currentArtist;artistScrollOffset=0;lastArtistScrollTime=millis();}else{currentArtist=nA;} if(prevArtistName=="" && currentArtist!="Spotify")prevArtistName=currentArtist;
      
      apiDurationMs = item.containsKey("duration_ms")?item["duration_ms"].as<unsigned long>():0; songDataValid=true;
    }
  }
  lastApiUpdateTime = millis(); 
}


// ==========================================================================
// BUTTON HANDLING
// ==========================================================================
void sendPlaybackCommand(String command) {
  Serial.print("Runtime: Playback Command: "); Serial.println(command);
  if (spotifyAccessToken == "") return; client.setInsecure();
  if (!client.connect(spotifyApiHost, httpsPort)) { Serial.println("Cmd: API Host Conn Fail"); return;}
  String httpMethod = "POST", path = "/v1/me/player/";
  if (command == "play"){ path += "play"; httpMethod = "PUT";}
  else if (command == "pause"){ path += "pause"; httpMethod = "PUT";}
  else if (command == "next"){ path += "next";}
  else if (command == "previous"){ path += "previous";}
  else { Serial.println("Cmd: Unknown"); client.stop(); return; }
  String request = httpMethod + " " + path + " HTTP/1.1\r\n" + "Host: " + String(spotifyApiHost) + "\r\n" + "Authorization: Bearer " + spotifyAccessToken + "\r\n" + "Content-Length: 0\r\n" + "Connection: close\r\n\r\n";
  client.print(request);
  unsigned long tO = millis(); String rS; while(client.available()==0) { if(millis()-tO >5000){ Serial.println("Cmd: Timeout"); client.stop();return;} yield(); }
  if(client.available()) rS = client.readStringUntil('\n'); Serial.println("Cmd Status: "+rS.substring(0, rS.length()-1)); 
  while(client.available()) { client.read(); yield(); } client.stop();
  lastApiCallAttemptTime = millis() - API_CALL_INTERVAL - 1000; 
}

void handleButtons() {
  for (int i = 0; i < 3; i++) { 
    int currentPin = (i == 0) ? PREV_BUTTON_PIN : ((i == 1) ? PLAYPAUSE_BUTTON_PIN : NEXT_BUTTON_PIN);
    bool r = digitalRead(currentPin); 
    if(r != lastButtonPhysicalState[i]) { 
        lastButtonDebounceTime[i] = millis();
    }
    if((millis() - lastButtonDebounceTime[i]) > BUTTON_DEBOUNCE_DELAY) {
        if(r != currentButtonLogicalState[i]){ 
            currentButtonLogicalState[i] = r; 
            if(currentButtonLogicalState[i] == LOW){ 
                Serial.print("Button "); Serial.print(i); Serial.println(" pressed.");
                String msg = "";
                if(i==0) { msg = "Previous..."; sendPlaybackCommand("previous"); }
                else if(i==1){
                    if(isPlaying) { msg = "Pausing..."; sendPlaybackCommand("pause");}
                    else { msg = "Playing..."; sendPlaybackCommand("play");}
                } 
                else if(i==2) { msg = "Next..."; sendPlaybackCommand("next");}
                if (msg != "") displayMessage(msg, "", true, true); // holdBriefly = true for button messages
            }
        }
    }
    lastButtonPhysicalState[i] = r; 
  }
}
