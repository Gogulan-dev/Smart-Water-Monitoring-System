#include <WiFi.h>

const char* ssid = "";
const char* password = "";

const int flowSensorPin = 2;
const float calibrationFactor = 4.5;

#define TdsSensorPin A0
#define VREF 3.3
#define SCOUNT 30

WiFiServer server(80);

volatile int pulseCount = 0;
float flowRate = 0.0;
float totalLitres = 0.0;
float tdsValue = 0.0;
float temperature = 25.0;  

int analogBuffer[SCOUNT];
int analogIndex = 0;
unsigned long lastFlowCalcTime = 0;
unsigned long lastAnalogReadTime = 0;

void IRAM_ATTR pulseCounter() {
    pulseCount++;
}

int getMedian(int arr[], int size) {
    int temp[size];
    memcpy(temp, arr, sizeof(temp));
    std::sort(temp, temp + size);
    return size % 2 ? temp[size / 2] : (temp[size / 2 - 1] + temp[size / 2]) / 2;
}

void setup() {
    Serial.begin(115200);

    pinMode(flowSensorPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
    pinMode(TdsSensorPin, INPUT);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000); Serial.print(".");
    }
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

    server.begin();
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastFlowCalcTime >= 1000) {
        detachInterrupt(digitalPinToInterrupt(flowSensorPin));
        flowRate = (pulseCount / calibrationFactor) * 60.0; 
        totalLitres += flowRate / 60.0;                     
        pulseCount = 0;
        lastFlowCalcTime = currentMillis;
        attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
    }

    if (currentMillis - lastAnalogReadTime >= 40) {
        lastAnalogReadTime = currentMillis;
        analogBuffer[analogIndex++] = analogRead(TdsSensorPin);
        if (analogIndex >= SCOUNT) analogIndex = 0;
    }


    int medianRaw = getMedian(analogBuffer, SCOUNT);
    float voltage = medianRaw * VREF / 4095.0;
    float compensation = 1.0 + 0.02 * (temperature - 25.0);
    float compensatedVoltage = voltage / compensation;
    tdsValue = (133.42 * pow(compensatedVoltage, 3)
                - 255.86 * pow(compensatedVoltage, 2)
                + 857.39 * compensatedVoltage) * 0.5;

    WiFiClient client = server.available();
    if (client && client.connected()) {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html\n");

        client.println("<!DOCTYPE html><html><head>");
        client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
        client.println("<title>Water Flow and Quality</title>");
        client.println("<link href='https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css' rel='stylesheet'>");
        client.println("</head><body>");
        
        client.println("<div class='container'>");
        client.println("<div class='jumbotron text-center'><h1>Water Flow and Quality</h1>");
        client.println("<p>\"Water is the driving force of all nature.\" - Leonardo da Vinci</p></div>");

        client.println("<div class='card text-center'><div class='card-header'>Water Data</div><div class='card-body'>");
        client.printf("<p><strong>Flow Rate:</strong> %.2f L/min</p>", flowRate);
        client.printf("<p><strong>Total Volume:</strong> %.2f L</p>", totalLitres);
        client.printf("<p><strong>TDS Value:</strong> %.0f ppm</p>", tdsValue);
        client.println("</div></div>");

        client.println("<div class='text-center my-4'><img src='https://via.placeholder.com/400x300.png?text=Water' alt='Water Image' class='img-fluid rounded'></div>");

        client.println("<div class='card text-center my-4'><div class='card-header'>Project Information</div>");
        client.println("<div class='card-body'><p>This project measures flow rate, total volume, and TDS of water in real time using an ESP32.</p></div></div>");
        
        client.println("</div></body></html>");
        client.stop();
    }
}
