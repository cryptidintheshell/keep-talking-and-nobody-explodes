#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* SERVER_IP = "http://192.168.4.1";
bool isHandshake = false;

String performRequest(const String& path, const String& method = "GET", const String& payload = "") {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[-] WiFi Disconnected");
        return "";
    }

    WiFiClient client;
    HTTPClient http;
    String url = String(SERVER_IP) + path;
    String response = "";

    http.begin(client, url);

    if (method == "POST") {
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    }

    int httpCode = (method == "POST") ? http.POST(payload) : http.GET();

    if (httpCode == HTTP_CODE_OK) {
        response = http.getString();
    } else {
        Serial.printf("[!!] Handshake Failed, error: %s\n",http.errorToString(httpCode).c_str());
    }

    http.end();
    return response;
}

bool connectToMainBoard(const char* ssid, const char* passwd) {
    Serial.print("[+] Connecting to AP");
    WiFi.begin(ssid, passwd);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[+] Connected!");
    return true;
}

bool sendHandshake(String id) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, "http://192.168.4.1/handshake?ID=" + id); 
    http.setTimeout(5000);
    int httpResponseCode = http.POST("");
    if (httpResponseCode < 0) {
        Serial.printf("[-] Error: %s\n", http.errorToString(httpResponseCode).c_str());
        return false;
    }
    http.end();
    isHandshake = true;
    return true;
  }
  return false;
}

void sendStrike() {
    performRequest("/strike", "POST");
}

bool sendFinished(String id) {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        HTTPClient http;
        
        // Construct the URL with the ID as a query parameter
        String url = String(SERVER_IP) + "/finished?ID=" + id;
        
        http.begin(client, url);
        http.setTimeout(5000);
        
        // Sending an empty body as the ID is passed in the URL
        int httpResponseCode = http.POST("");
        
        if (httpResponseCode > 0) {
            Serial.printf("[+] Finished signal sent. Response: %d\n", httpResponseCode);
            http.end();
            return true;
        } else {
            Serial.printf("[-] Failed to send finished signal: %s\n", http.errorToString(httpResponseCode).c_str());
            http.end();
            return false;
        }
    }
    return false;
}


int getStrikes() {
    String res = performRequest("/get_strikes", "GET");
    return (res != "") ? res.toInt() : 0;
}

bool getWidgets(JsonDocument& doc) {
    String response = performRequest("/widgets", "GET");
    if (response == "") return false;

    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.print(F("[-] JSON Deserialization failed: "));
        Serial.println(err.f_str());
        return false;
    }

    return true;
}

// Add this helper function to connectionHandler.h
bool hasVowel() {
    JsonDocument doc;
    if (getWidgets(doc)) {
        // Assuming the main board provides a "serial" field in the JSON
        String serial = doc["serialNumber"] | ""; 
        serial.toLowerCase();
        const char* vowels = "aeiou";
        for (int i = 0; i < serial.length(); i++) {
            if (strchr(vowels, serial[i])) return true;
        }
    }
    return false; // Default if check fails or no vowel found
}

bool checkLastDigit() {
    JsonDocument doc;

    bool isOdd = false;
    
    if (getWidgets(doc)) {
        String serialNumber = doc["serialNumber"];
        Serial.println("serialNumber: " + serialNumber);

        if (serialNumber[serialNumber.length() - 1] % 2 == 1) {
            isOdd = true;
        }

    } else Serial.println("[!!] Failed to get widgets.");

    return isOdd;
}