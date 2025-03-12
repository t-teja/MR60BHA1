/*
Added wifi accesspoint at 192.168.4.1 showing heart rate and breadth rate. 
Added button to avoid false triggering of leds. instead it will show animation only if button is pressed and heart rate is valid. 
*/

#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <60ghzbreathheart.h>

#define LED_PIN     5
#define NUM_LEDS    300  // Set the number of LEDs in your strip
#define LED_TYPE    WS2812B  // Change this based on your LED type
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define btn 19

// Heart rate and breath rate variables
int heartRate = 0;  // Default heart rate (0 means no detection)
int breathRate = 0;  // Default breath rate
int pulseDuration = 60000 / 80;  // Default pulse duration (60 bpm)
int fadeDuration = 60000 / 80;   // Default fade duration (60 bpm)

// Timeout variables
unsigned long personDetectedTime = 0;
unsigned long animationStartTime = 0;
const unsigned long detectionTimeout = 3000; // 3 seconds to validate detection
const unsigned long animationDuration = 10000; // 10 seconds of animation
bool animationInProgress = false; // Flag to track animation state


// Create an instance of the radar sensor
BreathHeart_60GHz radar = BreathHeart_60GHz(&Serial2);


// Variables for smooth animation
unsigned long lastUpdateTime = 0;
unsigned long heartBeatTimeInterval = 0;

const char* ssid = "HeartBeat";
const char* password = "";  // Open network

WebServer server(80);  // Create the server on port 80

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX2=GPIO16, TXD2=GPIO17

pinMode(btn,INPUT_PULLUP);

  // Set up the LED strip
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(255);

  // Wait for the serial connection
  //while (!Serial);
  Serial.println("Ready");

  //radar.ModeSelect_fuc(1);  //1: indicates real-time transmission mode, 2: indicates sleep state mode.

  // Start with all LEDs off
  FastLED.clear();
  FastLED.show();

  WiFi.mode(WIFI_AP);  // Set ESP32 as an Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP Address: ");
  Serial.println(IP);

  // Define the endpoint to serve the main page
server.on("/", HTTP_GET, []() {
    // HTML content to display real-time heart rate and breath rate with improved styling
    
    String html = "<html>";
    html += "<head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";  // Make it mobile-friendly
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #f4f7fc; color: #333; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; flex-direction: column;}"; 
    html += ".container { display: fixed; flex-wrap: wrap; justify-content: center; width: 50%; max-width: 800px; padding: 20px;}"; // Adjust the layout container
    html += ".widget { background-color: #fff; border-radius: 10px; padding: 15px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); text-align: center; margin: 10px; flex: 1 0 45%; min-width: 250px; height: 230px; display: flex; flex-direction: column; justify-content: center; align-items: center;}"; // Reduced height and padding for widgets
    html += ".widget h2 { font-size: 20px; color: #333; margin-bottom: 10px;}";  // Smaller header font size
    html += ".widget p { font-size: 16px; color: #666; margin: 10px 0;}";  // Reduced font size
    html += ".widget .value { font-size: 28px; font-weight: bold; color: #4CAF50;}";  // Slightly smaller value font
    html += ".icon { font-size: 40px; color: #4CAF50; margin-bottom: 20px;}";  // Icon styling if you want to enable it
    html += ".footer { text-align: center; margin-top: 20px; font-size: 18px; color: #888;}";
    
    // Adjust layout for mobile portrait and landscape mode
    html += "@media (max-width: 600px) { .container { flex-direction: column; align-items: center;} .widget { flex: 1 0 90%; height: 200px; } }";  // Portrait mode
    html += "@media (min-width: 601px) { .widget { flex: 1 0 45%; height: 230px; } }";  // Landscape mode (side-by-side)
    html += "</style>";
    html += "<script>";
    html += "function fetchData() {";
    html += "  fetch('/data').then(response => response.json()).then(data => {";
    html += "    document.getElementById('heartRate').innerText = data.heartRate;";
    html += "    document.getElementById('breathRate').innerText = data.breathRate;";
    html += "  });";
    html += "  setTimeout(fetchData, 1000);";  // Fetch data every 1 second
    html += "}";
    html += "fetchData();";  // Initial fetch on page load
    html += "</script>";
    html += "</head>";
    html += "<body>";

    html += "<div class='container'>";
    
    // Heart Rate Widget
    html += "<div class='widget'>";
    html += "<h2>Heart Rate</h2>";
    html += "<p>BPM</p>";
    html += "<div class='value' id='heartRate'>" + String(heartRate) + "</div>";
    html += "</div>";

    // Breath Rate Widget
    html += "<div class='widget'>";
    html += "<h2>Breath Rate</h2>";
    html += "<p>Breaths/min</p>";
    html += "<div class='value' id='breathRate'>" + String(breathRate) + "</div>";
    html += "</div>";
    html += "</div>";  // End of container
    

    html += "<div class='footer'>";
    html += "<p><b> Heartbeat & Breath Rate Monitor</b></p>";
    html += "</div>";
   
    
    html += "</body>";
    html += "</html>";

    // Send the HTML content to the client
    server.send(200, "text/html", html);
});





  // Define the endpoint to serve the data (JSON format)
  server.on("/data", HTTP_GET, []() {
    // Return the heart rate and breath rate as JSON
    String json = "{";
    json += "\"heartRate\": " + String(heartRate) + ",";
    json += "\"breathRate\": " + String(breathRate);
    json += "}";
    
    server.send(200, "application/json", json);
  });

  // Start the server
  server.begin();
}
void loop() {
  // Read heart rate and breath rate from the sensor
  radar.Breath_Heart();

  // If data is available, check if a person is detected
  if (radar.sensor_report != 0x00) {
    // Try to detect valid heart rate and breath rate data
    if (radar.sensor_report == HEARTRATEVAL) {
      heartRate = radar.heart_rate;
      Serial.print("Heart Rate: ");
      Serial.println(heartRate);
    }
    if (radar.sensor_report == BREATHVAL) {
      breathRate = radar.breath_rate;
      Serial.print("Breath Rate: ");
      Serial.println(breathRate);
    }

    // 1) When no human is detected, set all LEDs to white
    if (heartRate == 0 && breathRate == 0 && !animationInProgress) {
      fill_solid(leds, NUM_LEDS, CRGB::White);  // All LEDs white
      FastLED.show();
    }
    // 2) When a human is detected and heart/breath rate are not zero,
    // set the first 10 LEDs to green and the rest to white
    else if (heartRate > 0 && breathRate > 0 && !animationInProgress) {
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i < 10) {
          leds[i] = CRGB::Green;  // First 10 LEDs to green
        } else {
          leds[i] = CRGB::White;  // Rest of LEDs to white
        }
      }
      FastLED.show();

      // Ready to press button to simulate heartbeat effect
      if (digitalRead(btn) == LOW) {
        animationStartTime = millis();  // Start the animation
        animationInProgress = true;     // Set flag to prevent further animation until the 10 seconds is over
        simulateHeartbeat();            // Start heartbeat animation
      }
    }

    // 3) When the button is pressed, show heartbeat simulation for 10 seconds
    if (animationInProgress) {
      if (millis() - animationStartTime < animationDuration) {
        simulateHeartbeat();  // Continue the animation for 10 seconds
      } else {
        animationInProgress = false;
        heartRate = 0;
        breathRate = 0;
        fill_solid(leds, NUM_LEDS, CRGB::White);  // Set LEDs back to solid white after 10 seconds
        FastLED.show();
        personDetectedTime = millis();  // Start waiting for a new person to be detected
      }
    }
    
  } else {
    // If no valid data, continue to show heartbeat effect if animation is in progress
    if (animationInProgress) {
      simulateHeartbeat();
    }
  }

  delay(20); // Add a small delay to prevent serial spam and smooth detection
  server.handleClient();  // Handle incoming client requests



  // Free up heap memory in the loop
  //Serial.print("Free Heap: ");
  //Serial.println(ESP.getFreeHeap());
  //delay(1000);  // Delay for a bit to simulate data updates
}

// Heartbeat simulation function
void simulateHeartbeat() {
   unsigned long currentMillis = millis();
  
  // Update the animation every 20ms for smoother transitions
  if (currentMillis - lastUpdateTime >= 20) {
    lastUpdateTime = currentMillis;
    int beatProgress = (currentMillis - animationStartTime) % fadeDuration;  // Get the time since the last pulse cycle started

    // Systolic phase: Fade in (Heart contraction)
    if (beatProgress < fadeDuration / 2) {
      float progress = (float)beatProgress / (fadeDuration / 2);  // Normalize the progress
      int brightness = progress * 255;  // Gradually increase brightness
      setHeartBeatEffect(brightness);
    }
    // Diastolic phase: Fade out (Heart relaxation)
    else {
      float progress = (float)(beatProgress - fadeDuration / 2) / (fadeDuration / 2);  // Normalize the progress
      int brightness = 255 - (progress * 255);  // Gradually decrease brightness
      setHeartBeatEffect(brightness);
    }
  }
}

// Set heartbeat effect on LEDs
void setHeartBeatEffect(int brightness) {
  // Set the brightness of each LED based on the current brightness value
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(0, 255, brightness);  // White color, variable brightness
  }
  FastLED.show();
}
