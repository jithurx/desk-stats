# DESK_STATS [ESP8266 Spotify OLED Display (Version 1.0)]

This project uses a NodeMCU ESP8266 and an I2C OLED display (128x64) to show the currently playing song on Spotify. It also features three buttons for basic playback control (Previous, Play/Pause, Next). Data is fetched from the Spotify Web API, and this setup includes a method for obtaining the necessary Spotify Refresh Token using a Vercel serverless function as a callback.

## Features (Version 1.0)

*   Displays the current track name and artist.
*   Shows current playback status (Playing/Paused).
*   Playback control:
    *   Previous Track
    *   Play/Pause
    *   Next Track
*   Fetches data from the Spotify Web API.
*   Handles Spotify API authentication (OAuth 2.0 Refresh Token flow).
*   Displays the last octet of the ESP8266's IP address.

## Hardware Required

*   NodeMCU ESP8266 (Amica, LoLin, or similar ESP-12E/F based board)
*   128x64 SSD1306 OLED Display (I2C interface)
*   3x Tactile Push Buttons
*   Breadboard
*   Jumper Wires

## Wiring

### OLED Display (I2C - SSD1306)
*   **GND** (OLED) -> **GND** (NodeMCU)
*   **VCC** (OLED) -> **3.3V** (NodeMCU)
*   **SCL** (OLED) -> **D1** (NodeMCU - GPIO5)
*   **SDA** (OLED) -> **D2** (NodeMCU - GPIO4)
    *Note: Default I2C address used is `0x3C`. Adjust in `deskstats_spotify_v.1.ino` if yours is different.*

### Buttons (Using `INPUT_PULLUP`)
*   **Previous Button:**
    *   One leg -> **D5** (NodeMCU - GPIO14)
    *   Other leg -> **GND** (NodeMCU)
*   **Play/Pause Button:**
    *   One leg -> **D6** (NodeMCU - GPIO12)
    *   Other leg -> **GND** (NodeMCU)
*   **Next Button:**
    *   One leg -> **D7** (NodeMCU - GPIO13)
    *   Other leg -> **GND** (NodeMCU)

## Software & Setup Workflow

The setup involves three main parts:
1.  Setting up a Spotify Developer App.
2.  Deploying a Vercel callback function to help with Spotify authentication.
3.  Configuring and uploading the Arduino sketch to the NodeMCU.

### Part 1: Spotify Developer App Setup

1.  **Create a Spotify Developer Account:** Go to [https://developer.spotify.com/dashboard/](https://developer.spotify.com/dashboard/)
2.  **Create an App:**
    *   Give it a name (e.g., "ESP8266 Spotify Display") and a description.
    *   Once created, you will get a **Client ID** and you can view the **Client Secret**. Copy these down.
3.  **Redirect URIs:** You will configure the Redirect URI in Part 2, Step 4 after deploying your Vercel function.

### Part 2: Vercel Callback & Refresh Token Generation

This project uses a Vercel serverless function to securely handle the Spotify OAuth callback and help you obtain the Refresh Token.

1.  **Prerequisites for Vercel & Python Script:**
    *   **Node.js and npm:** For Vercel CLI. ([Install Node.js](https://nodejs.org/))
    *   **Vercel Account:** Sign up at [https://vercel.com/](https://vercel.com/)
    *   **Vercel CLI:** Install globally: `npm install -g vercel`
    *   **Python 3:** For running the `get_spotify_refresh_token.py` script. ([Install Python](https://www.python.org/))
    *   **Python `requests` library:** Install: `pip install requests`

2.  **Project Files for Vercel Callback:**
    Ensure you have the following files in a directory (e.g., `spotify-auth-helpers`):
    *   `api/callback.js` (Provided in this repository)
    *   `vercel.json` (Provided in this repository)
    *   `package.json` (Provided in this repository, primarily for Vercel to recognize the project type)

3.  **Deploy to Vercel:**
    *   Open your terminal, navigate to the directory containing these Vercel files.
    *   Log in to Vercel: `vercel login`
    *   Deploy: `vercel` (or `vercel --prod` for a production deployment)
    *   Vercel will provide a URL for your deployment (e.g., `https://your-project-name-xxxx.vercel.app`). Your callback endpoint will be this URL plus `/api/callback`. Example: `https://my-spotify-callback.vercel.app/api/callback`. **Note this full callback URL.**

4.  **Update Spotify Developer Dashboard Redirect URI:**
    *   Go back to your Spotify App settings on the Developer Dashboard.
    *   Under "Edit Settings", add the **full Vercel callback URL** (e.g., `https://my-spotify-callback.vercel.app/api/callback`) to the "Redirect URIs" list.
    *   Save the settings.

5.  **Get Refresh Token using Python Script:**
    *   Use the `get_spotify_refresh_token.py` script (provided in this repository).
    *   **Important:** Before running, open the script and ensure the `CLIENT_ID`, `CLIENT_SECRET`, and `VERCEL_CALLBACK_URI` variables at the top are correctly set to your values (or be prepared to enter them when prompted).
    *   Run the script: `python get_spotify_refresh_token.py`
    *   It will guide you:
        1.  A browser window will open for Spotify authorization.
        2.  Log in and grant permissions.
        3.  Spotify will redirect to your Vercel callback page.
        4.  Your Vercel page will display an **authorization code**. Copy this code.
        5.  Paste the authorization code back into the Python script in your terminal.
    *   The script will then output your **Access Token** and **Refresh Token**. **Securely copy the Refresh Token.**

6.  **Base64 Encode `Client ID:Client Secret`:**
    *   You'll need a Base64 encoded string of `your_client_id:your_client_secret`.
    *   Concatenate them with a colon: `CLIENT_ID_VALUE:CLIENT_SECRET_VALUE`
    *   Use an online tool like [Base64 Encode](https://www.base64encode.org/) or the following Python command:
        ```python
        import base64
        client_id = "YOUR_CLIENT_ID"
        client_secret = "YOUR_CLIENT_SECRET"
        message = f"{client_id}:{client_secret}"
        message_bytes = message.encode('ascii')
        base64_bytes = base64.b64encode(message_bytes)
        base64_message = base64_bytes.decode('ascii')
        print(base64_message)
        ```
    *   Copy the resulting Base64 string.

### Part 3: Arduino Sketch Setup

1.  **Arduino IDE:**
    *   Install the Arduino IDE.
    *   Install the ESP8266 board support package: Go to `File > Preferences` and add `http://arduino.esp8266.com/stable/package_esp8266com_index.json` to "Additional Boards Manager URLs". Then, go to `Tools > Board > Boards Manager`, search for `esp8266`, and install it.
    *   Select your NodeMCU board from `Tools > Board` (e.g., "NodeMCU 1.0 (ESP-12E Module)").

2.  **Libraries:**
    Install the following libraries via the Arduino Library Manager (`Sketch > Include Library > Manage Libraries...`):
    *   `Adafruit GFX Library` by Adafruit
    *   `Adafruit SSD1306` by Adafruit
    *   `ArduinoJson` by Benoit Blanchon (Version 6.x.x is recommended)

3.  **Configure the Sketch (`spotify_oled_display.ino`):**
    Open the main `.ino` sketch file (e.g., `spotify_oled_display.ino` containing the ESP8266 code) and update the following placeholders:
    ```cpp
    // --- Spotify API Configuration ---
    const char* ssid = "YOUR_WIFI_SSID"; // Your WiFi network name
    const char* password = "YOUR_WIFI_PASSWORD"; // Your WiFi password

    String spotifyClientId = "YOUR_SPOTIFY_CLIENT_ID"; // From Spotify Dashboard
    String spotifyClientCredsB64 = "YOUR_BASE64_ENCODED_CLIENTID_COLON_CLIENTSECRET"; // From Part 2, Step 6
    String spotifyRefreshToken = "YOUR_SPOTIFY_REFRESH_TOKEN"; // From Part 2, Step 5
    ```

## Uploading and Running the ESP8266 Code

1.  Connect your NodeMCU to your computer via USB.
2.  Select the correct Board and Port in the Arduino IDE.
3.  Click the "Upload" button.
4.  Once uploaded, open the Serial Monitor (`Tools > Serial Monitor`) at `115200` baud to see debug messages and status.
5.  The OLED display should initialize, connect to WiFi, fetch the token, and then display Spotify information.

## Troubleshooting

*   **Vercel 404:** Ensure you're accessing the Vercel function at the correct path (e.g., `https://your-deployment.vercel.app/api/callback`). Check Vercel deployment logs.
*   **Python Script Errors:**
    *   `INVALID_CLIENT: Invalid redirect URI`: The `VERCEL_CALLBACK_URI` in your Python script or Spotify App settings is incorrect or doesn't match.
    *   `Error getting tokens`: The authorization code might have been incorrect/expired, or `redirect_uri` mismatch in the token request.
*   **ESP8266 `ESP8266WiFi.h: No such file or directory`**: ESP8266 board support not installed/selected in Arduino IDE.
*   **ESP8266 OLED Not Working:** Check wiring, I2C address, correct Adafruit display library.
*   **ESP8266 Token Failed / API Errors:** Double-check all Spotify credentials in the sketch. Ensure Refresh Token is valid. Check Serial Monitor.
*   **ESP8266 `JSON Parse Err`**: This version uses JSON filtering. If this error persists, check the raw JSON response printed to the Serial Monitor.

## Files in this Repository

*   `spotify_oled_display.ino`: Main Arduino sketch for the ESP8266.
*   `get_spotify_refresh_token.py`: Python script to obtain the Spotify Refresh Token using the Vercel callback.
*   `api/callback.js`: Serverless function for Vercel to handle Spotify OAuth callback.
*   `vercel.json`: Configuration file for Vercel deployment.
*   `package.json`: Node.js package file for the Vercel project.
*   `README.md`: This file.

## Future Ideas (Version 2+)

*   Scrolling text for long track/artist names.
*   Playback progress bar.
*   Album art display.

## License


This project is open-source. Please provide attribution if you find it useful.

---