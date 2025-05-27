# spoti-stats: ESP8266 Spotify OLED Display & Controller

![Project Demo GIF](demo/demo.gif)

Display your currently playing Spotify track, artist, and playback progress on an SSD1306 OLED display using an ESP8266. Control playback with physical buttons for previous, play/pause, and next track.

This project features a custom boot animation with status updates, scrolling text for long track/artist names, a "wavy" animated progress bar, and uses the "Pixelify Sans" custom font for a distinct look.

## Features

*   **Now Playing Info:** Displays current track name and artist.
*   **Playback Status:** Shows if music is playing or paused.
*   **Playback Control:**
    *   Previous Track
    *   Play/Pause
    *   Next Track
*   **Animated Progress Bar:** Visual representation of track progress with a unique wavy animation.
*   **Scrolling Text:** Long track and artist names scroll horizontally for better readability.
*   **WiFi Connectivity:** Connects to your local WiFi network.
*   **Spotify API Integration:** Uses Spotify Web API for track info and control.
*   **OLED Display:** Utilizes a 128x64 SSD1306 I2C OLED display.
*   **Custom Font:** Uses "Pixelify Sans" for the boot screen logo.
*   **Boot Animation:** Displays logo and real-time status for WiFi connection and Spotify authentication.
*   **IP Address Display:** Shows the last octet of the ESP8266's IP address on the main screen.
*   **Automatic Token Refresh:** Handles Spotify access token expiration and renewal using a refresh token.

## Hardware Required

*   **ESP8266 Development Board:** (e.g., NodeMCU Amica/LoLin, Wemos D1 Mini, or similar ESP-12E/F based board)
*   **SSD1306 OLED Display:** 128x64 pixels, I2C interface (ensure it's address `0x3C` or change in code).
*   **Tactile Push Buttons (3)**
*   **Breadboard and Jumper Wires**
*   **Micro USB Cable:** For power and programming.

## Software & Libraries

*   **Arduino IDE:** [Download here](https://www.arduino.cc/en/software)
*   **ESP8266 Core for Arduino:** Follow installation instructions [here](https://github.com/esp8266/Arduino)
*   **Libraries (Install via Arduino Library Manager):**
    *   `Adafruit GFX Library` (by Adafruit)
    *   `Adafruit SSD1306` (by Adafruit)
    *   `ArduinoJson` (by Benoit Blanchon, Version 6.x.x recommended)
*   **Custom Font File:**
    *   `PixelifySans_10pt.h`: This file (containing the font data for "Pixelify Sans 10pt") must be present in the same directory as your main `.ino` sketch file.

## Wiring

### OLED Display (I2C - SSD1306)
*   **GND** (OLED) -> **GND** (NodeMCU)
*   **VCC** (OLED) -> **3.3V** (NodeMCU)
*   **SCL** (OLED) -> **D1** (NodeMCU - GPIO5)
*   **SDA** (OLED) -> **D2** (NodeMCU - GPIO4)
    *Note: The code defines `SCREEN_ADDRESS 0x3C`. Adjust this in your sketch if your display's I2C address is different.*

### Buttons (Using `INPUT_PULLUP`)
*   **Previous Button:**
    *   One leg -> **D5** (NodeMCU - GPIO14) - Corresponds to `PREV_BUTTON_PIN`
    *   Other leg -> **GND** (NodeMCU)
*   **Play/Pause Button:**
    *   One leg -> **D6** (NodeMCU - GPIO12) - Corresponds to `PLAYPAUSE_BUTTON_PIN`
    *   Other leg -> **GND** (NodeMCU)
*   **Next Button:**
    *   One leg -> **D7** (NodeMCU - GPIO13) - Corresponds to `NEXT_BUTTON_PIN`
    *   Other leg -> **GND** (NodeMCU)

## Configuration Workflow

The setup involves three main parts:
1.  Setting up a Spotify Developer App.
2.  Generating necessary Spotify credentials.
3.  Configuring and uploading the Arduino sketch to the NodeMCU.

### Part 1: Spotify Developer App Setup

1.  **Go to Spotify Developer Dashboard:** [https://developer.spotify.com/dashboard/](https://developer.spotify.com/dashboard/)
2.  **Log in** with your Spotify account.
3.  Click **"Create an App"** (or "Create App").
    *   Give it a name (e.g., "spoti-stats ESP8266 Display").
    *   Give it a description.
    *   Agree to the terms.
4.  Once created, you'll see your **Client ID**. Click **"Show client secret"** to see your **Client Secret**.
    *   **Copy your `Client ID`**.
    *   **Copy your `Client Secret`**.
5.  Click **"Edit Settings"**.
6.  Under **"Redirect URIs"**, add `http://localhost/callback` (or any valid URI, e.g., `https://example.com/callback`). This URI is primarily used during the manual refresh token generation process but not directly by the ESP8266 in runtime. Click **"Add"** then **"Save"**.

### Part 2: Generate Spotify Credentials

#### A. `spotifyClientCredsB64`
This is a Base64 encoded string of your `Client ID` and `Client Secret` combined with a colon.
Format: `Base64Encode(YOUR_CLIENT_ID:YOUR_CLIENT_SECRET)`

You can use an online Base64 encoder or a command line tool:
*   **Linux/macOS Terminal:**
    ```bash
    echo -n "YOUR_CLIENT_ID:YOUR_CLIENT_SECRET" | base64
    ```
    (Replace `YOUR_CLIENT_ID` and `YOUR_CLIENT_SECRET` with your actual values. The `-n` is important to prevent a trailing newline.)
*   **Online Tool:** Search for "base64 encode online".
*   **Python:**
    ```python
    import base64
    client_id = "YOUR_CLIENT_ID"
    client_secret = "YOUR_CLIENT_SECRET"
    message = f"{client_id}:{client_secret}"
    message_bytes = message.encode('ascii')
    base64_bytes = base64.b64encode(message_bytes)
    base64_message = base64_bytes.decode('ascii')
    print(f"Base64 Encoded Client Credentials: {base64_message}")
    ```

#### B. `spotifyRefreshToken`
A refresh token allows your ESP8266 to get new access tokens without requiring user login each time. It's typically a one-time generation process.

**Manual Method (Conceptual - using Authorization Code Flow):**
1.  **Construct Authorization URL:**
    Replace `YOUR_CLIENT_ID` and `YOUR_REDIRECT_URI` (from Part 1, Step 6) in the URL below.
    The `scope` parameter defines the permissions your application needs. The one in your code implies: `user-read-currently-playing user-read-playback-state user-modify-playback-state`.
    ```
    https://accounts.spotify.com/authorize?client_id=YOUR_CLIENT_ID&response_type=code&redirect_uri=YOUR_REDIRECT_URI&scope=user-read-currently-playing%20user-read-playback-state%20user-modify-playback-state
    ```
2.  **Authorize:** Open this URL in your browser. Log in to Spotify if prompted and grant permissions. You'll be redirected to your `redirect_uri` with a `code` parameter in the URL (e.g., `http://localhost/callback?code=AQB...`). **Copy this `code` value.** This code is short-lived.
3.  **Exchange Code for Tokens:** Make a POST request to `https://accounts.spotify.com/api/token`. You can use tools like `curl` or Postman.
    *   **URL:** `https://accounts.spotify.com/api/token`
    *   **Method:** `POST`
    *   **Headers:**
        *   `Authorization: Basic YOUR_CLIENT_CREDS_B64` (the Base64 string from Part 2A)
        *   `Content-Type: application/x-www-form-urlencoded`
    *   **Body (form-urlencoded):**
        *   `grant_type=authorization_code`
        *   `code=THE_AUTHORIZATION_CODE_YOU_JUST_GOT`
        *   `redirect_uri=YOUR_REDIRECT_URI`

    Example using `curl`:
    ```bash
    curl -X POST "https://accounts.spotify.com/api/token" \
         -H "Authorization: Basic YOUR_CLIENT_CREDS_B64" \
         -H "Content-Type: application/x-www-form-urlencoded" \
         -d "grant_type=authorization_code&code=THE_AUTHORIZATION_CODE_YOU_JUST_GOT&redirect_uri=YOUR_REDIRECT_URI"
    ```
    The JSON response will contain `access_token`, `expires_in`, and importantly, `refresh_token`. **Securely copy the `refresh_token` value.**

**Note:** There are also helper scripts and online tools available that can simplify obtaining a refresh token by automating parts of this flow. Search for "Spotify API get refresh token script".

**Important:** Keep your `Client Secret` and `Refresh Token` secure!

### Part 3: Arduino Sketch Setup

1.  **Arduino IDE Setup:**
    *   Install the Arduino IDE.
    *   Install the ESP8266 board support package: Go to `File > Preferences` and add `http://arduino.esp8266.com/stable/package_esp8266com_index.json` to "Additional Boards Manager URLs". Then, go to `Tools > Board > Boards Manager`, search for `esp8266`, and install it.
    *   Select your NodeMCU board from `Tools > Board` (e.g., "NodeMCU 1.0 (ESP-12E Module)" or "LOLIN(WEMOS) D1 R2 & mini").

2.  **Install Libraries:** (As listed in "Software & Libraries" section above).

3.  **Add Custom Font:** Download/create `PixelifySans_10pt.h` and place it in the same folder as your main `.ino` sketch file.

4.  **Configure the Sketch (`spoti-stats.ino` or your main sketch file):**
    Open the `.ino` sketch file and update the following placeholders in the `// --- config ---` section:

    ```cpp
    // --- config ---
    const char* ssid = "YOUR_WIFI_SSID";         // Your WiFi network Name
    const char* password = "YOUR_WIFI_PASSWORD"; // Your WiFi network Password

    // Get these from Spotify Developer Dashboard
    String spotifyClientId = "YOUR_SPOTIFY_CLIENT_ID"; // From Part 1

    // Base64 encoded string of "YOUR_CLIENT_ID:YOUR_CLIENT_SECRET"
    String spotifyClientCredsB64 = "YOUR_GENERATED_BASE64_STRING"; // From Part 2A

    // Get this using an external tool/script (see prerequisites)
    String spotifyRefreshToken = "YOUR_GENERATED_REFRESH_TOKEN"; // From Part 2B

    // --- OLED Display Configuration ---
    // ... (Usually 0x3C for 128x64 SSD1306, change if yours is different)
    #define SCREEN_ADDRESS 0x3C

    // --- Button Configuration ---
    // (Default pins, change if your wiring is different)
    const int PREV_BUTTON_PIN = D5;
    const int PLAYPAUSE_BUTTON_PIN = D6;
    const int NEXT_BUTTON_PIN = D7;
    ```

## Installation & Flashing

1.  Connect your NodeMCU to your computer via USB.
2.  In the Arduino IDE, select the correct Board (`Tools > Board`) and Port (`Tools > Port`).
3.  Click the "Upload" button (right arrow icon).
4.  Once uploaded, open the Serial Monitor (`Tools > Serial Monitor`) at `115200` baud to see debug messages, connection status, and API responses.
5.  The OLED display should initialize, show the boot animation, connect to WiFi, authenticate with Spotify, and then start displaying "Now Playing" information.

## How It Works

1.  **Boot Sequence:**
    ![Boot](demo/boot.jpg)
    *   Initializes the OLED display.
    *   Displays a boot logo ("Spotify Stats" with Pixelify Sans font) and status messages for WiFi connection and Spotify authentication, with a simple animation.
    *   Connects to the specified WiFi network.
    *   Fetches an initial Spotify Access Token using the provided Refresh Token.
    *   If WiFi or authentication fails, it displays an error and halts (or retries WiFi).
2.  **Main Loop:**
    *   **Button Handling:** Continuously checks for button presses (with debouncing). If a button is pressed, it sends the corresponding command (previous, play/pause, next) to the Spotify API.
    *   **API Calls:**
        *   Periodically (every `API_CALL_INTERVAL`), it fetches the "currently playing" track information from Spotify.
        *   Before making API calls, it checks if the Access Token is nearing expiry (`TOKEN_REFRESH_LEAD_TIME`). If so, it uses the Refresh Token to get a new Access Token.
        *   If WiFi is disconnected, it attempts to reconnect.
    *   **Display Update:**
        *   Regularly (every `DISPLAY_ANIMATION_INTERVAL`), it updates the OLED display with the latest track, artist, playback status, IP address suffix, and the wavy progress bar.
        *   Handles text scrolling for long track and artist names.
        *   Animates the wavy progress bar based on song progress.

## Troubleshooting

*   **No Display / OLED Not Working:**
    *   Check all wiring carefully (SDA, SCL, VCC, GND).
    *   Ensure the I2C address (`SCREEN_ADDRESS`) in the code matches your OLED module (common addresses are `0x3C` or `0x3D`). You can use an I2C scanner sketch to find the address.
*   **WiFi Connection Issues:**
    *   Double-check SSID and password in the sketch. Case-sensitive!
    *   Ensure your WiFi network is 2.4GHz (ESP8266 does not support 5GHz).
    *   Check router signal strength and proximity.
    *   Look at Serial Monitor output for error messages like "WiFi connection FAILED."
*   **Spotify Authentication Errors (Check Serial Monitor):**
    *   `Token host connection failed!`: ESP8266 cannot reach `accounts.spotify.com`. Check internet connection.
    *   `Token client timeout!`: Slow internet connection or Spotify server issue.
    *   `deserializeJson() for token failed`: The response from Spotify was not valid JSON or an error occurred. Often due to incorrect `spotifyClientCredsB64` or an invalid `spotifyRefreshToken`.
    *   `Could not get access token from response`: The `spotifyRefreshToken` might be invalid/expired, or `spotifyClientCredsB64` is wrong. You might need to generate a new refresh token.
*   **Spotify API Errors (e.g., 401, 403, "API Error" on display):**
    *   `HTTP/1.1 401 Unauthorized`: Access Token is invalid or expired. The code should attempt to refresh it. If this persists, the Refresh Token might be bad.
    *   `HTTP/1.1 403 Forbidden`: You might not have the correct scopes authorized for your refresh token, or your Spotify app might be restricted.
    *   Check Serial Monitor for detailed HTTP status codes and error messages from the API.
*   **"deserializeJson() failed" for `getCurrentlyPlaying`:**
    *   Spotify API might have changed its response structure.
    *   Could be an ESP8266 memory issue if the JSON response is very large, though the code uses filtering and a reasonably sized `DynamicJsonDocument`.
*   **Text Not Scrolling / Progress Bar Issues:**
    *   Check timing constants in the code related to scrolling and animation intervals.
*   **Buttons Not Working:**
    *   Verify wiring. Ensure buttons are connected to the correct ESP8266 pins and to GND.
    *   The Serial Monitor should show "Button X pressed." messages if debouncing and detection are working.
*   **Font Not Displaying (`PixelifySans_10pt.h`):**
    *   Ensure the `PixelifySans_10pt.h` file is in the same directory as your `.ino` sketch file.
    *   Check that the `#include "PixelifySans_10pt.h"` line is present in your sketch.

## Files in this Repository

*   `spoti-stats.ino`: The main sketch for the ESP8266.
*   `PixelifySans_10pt.h`: Custom GFX font file for "Pixelify Sans".
*   `README.md`: This file.

## Credits

*   This project utilizes the [Spotify Web API](https://developer.spotify.com/documentation/web-api/).
*   Font: "Pixelify Sans" (ensure you have the rights to use any custom font or replace it with a freely available GFX font).