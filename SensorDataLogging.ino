//Project by KoloKush

//Include LED Tetris Matrix Libraries
#include "Arduino_LED_Matrix.h"

// Include the RTC library
#include "RTC.h"
//Include the NTP library
#include <NTPClient.h>

//Include the Wifi library
#if defined(ARDUINO_PORTENTA_C33)
#include <WiFiC3.h>
#elif defined(ARDUINO_UNOWIFIR4)
#include <WiFiS3.h>
#endif

//Include Network SSID and Password
#include "arduino_secrets.h"

float tempData[130];     //Array size big enough for testing 30 second intervals over an hour
float prevData[130];
String todayTime[130];
String prevTime[130];
String timeString;      //String form of time for the data tables in JS
int hour, minutes, seconds;
int dataToday = 0;       //Current day's data array size
int dataYesterday;         //Previous day's data array size
int runOnce[2];           //Neccessary for Time Based Logic, especially minutes and hours
boolean prevDay = false;        //Does the previous day's data exist yet?
boolean startup = true;         //code added for when startup happens at midnight
//Temperature data variables
int tempReading;
double tempK;
float tempC;
float tempF;

int tempPin = 0;

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

int wifiStatus = WL_IDLE_STATUS;
WiFiServer server(80);
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp);
RTCTime currentTime;

void setup()
{
  runOnce[0] = 0;
  runOnce[1] = 0;
  // you can also load frames at runtime, without stopping the refresh
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  while (!Serial);

  connectToWiFi();
  RTC.begin();
  Serial.println("\nStarting connection to server...");
  timeClient.begin();
  timeClient.update();

  // Get the current date and time from an NTP server and convert
  // it to UTC -7 by passing the time zone offset in hours.
  // You may change the time zone offset to your local one.
  auto timeZoneOffsetHours = -7;
  auto unixTime = timeClient.getEpochTime() + (timeZoneOffsetHours * 3600);
  Serial.print("Unix time = ");
  Serial.println(unixTime);
  RTCTime timeToSet = RTCTime(unixTime);
  RTC.setTime(timeToSet);

  // Retrieve the date and time from the RTC and print them
  RTC.getTime(currentTime);
  Serial.println("The RTC was just set to: " + String(currentTime));
  server.begin();
}
void loop()
{
  wifiReconnect();
  calcTemp();
  calcTime();
  setTempData();
  displayWebpage();
}
void calcTemp(){
  tempReading = analogRead(tempPin);
  //Temp Math from Arduino Kit
  // This is OK
 // /*
  tempK = log(10000.0 * ((1024.0 / tempReading - 1)));
  tempK = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * tempK * tempK )) * tempK );       //  Temp Kelvin
  tempC = tempK - 273.15;            // Convert Kelvin to Celcius
  tempF = (tempC * 9.0)/ 5.0 + 32.0; // Convert Celcius to Fahrenheit
 // float tempVolts = tempReading * 5.0 / 1024.0;
 // Serial.println("Voltage: " + String(tempVolts));
  //*/
  /*  replaced
    float tempVolts = tempReading * 5.0 / 1024.0;
    float tempC = (tempVolts - 0.5) * 10.0;
    float tempF = tempC * 9.0 / 5.0 + 32.0;
    Serial.println("Temp F: " + String(tempF));
  */
}
void calcTime(){
  RTC.getTime(currentTime);
  delay(1000);
  hour = currentTime.getHour();
  minutes = currentTime.getMinutes();
  seconds = currentTime.getSeconds();
  if (minutes < 10){
    timeString = (String(hour) + ":0" + String(minutes));
  }
  else {
    timeString = (String(hour) + ":" + String(minutes));
  }
}
void setTempData(){          //Sets the data into the array one by one
  if (dataToday == 0){
    tempData[dataToday] = tempF;
    todayTime[dataToday] = timeString;
  }
  if (minutes == 0 | ((minutes % 30) == 0)){
  //if (seconds == 0 | ((seconds % 30) == 0)){    //for testing
    if (runOnce[0] == 0){
      runOnce[0] = 1;
      tempData[dataToday] = tempF;
      todayTime[dataToday] = timeString;
      dataToday++;
      if(WiFi.status() == WL_CONNECTED){
        Serial.println("Wifi is still connected...");
      }
      else{
        Serial.println(WiFi.status());
      }
    }
  }
  if ((hour == 0) && (runOnce[1] == 0) && (startup == false)){
  //if ((minutes == 0) && (runOnce[1] == 0) && (startup == false)){    //for testing
    runOnce[1] = 1;
    pullTimeFromWeb();
    dataYesterday = dataToday;    //datapoints for previous day transfer gets the same size as what the current day collected
    dataToday = 0;             //current day datapoints gets reset to zero
    previousDayTransfer();     //transfers to previous set array
    resetTempData();           //resets all data to current temp
  }
  if ((minutes == 1) | (minutes == 31)){
  //if ((seconds == 1) | (seconds == 31)){    //for testing
    runOnce[0] = 0;
  }
  if (hour == 1){
  //if (minutes == 1){    //for testing
    runOnce[1] = 0;
  }
  if (hour > 0){
  //if (minutes > 0){
    startup = false;
  }
}
void displayWebpage(){
  WiFiClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an HTTP request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the HTTP request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard HTTP response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 100");  // refresh the page automatically every 100 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html lang='en'>");
          client.println("<head style='background-color:aquamarine;'>");
          client.println("<meta charset='UTF-8'>");
          client.println("<meta name='viewport' content='width=device-width, height=device-height initial-scale=1.0'>");
          client.println("<center><title>Real-Time Sensor Temperature Data</title></center>");
          client.println("<!-- Load the Google Charts library -->");
          client.println("<script src='https://www.gstatic.com/charts/loader.js'></script>");        //google charts library
          client.println("</head>");
          client.println("<body style='background-color:aquamarine;'>");
          client.println("<center>");
          client.println("<h1 style='background-color:MediumSeaGreen;'>Real-time Sensor Temperature Data</h1>");
          client.println("<h2 style='background-color:yellow;'>");
          client.println("Temperature readings graphed every 30 minutes over 1 day");
          //client.println("Temperature readings graphed once an hour over 24 hours");
          client.println("<br />");
          client.println("Temperature in Degrees F: ");
          client.print(tempF);
          client.println("<br />");
          client.print("Time: ");
          client.print(currentTime);
          client.println("<br />");
          client.println("</h2>");
          client.println("<div id='temperatureChart1' style='width: device-width; height: 400px;'></div>");
          if (prevDay){
          client.println("<div id='temperatureChart2' style='width: device-width; height: 400px;'></div>");
          }
          client.println("<script>");
          client.println("google.charts.load('current', {'packages':['corechart']});");
          client.println("google.charts.setOnLoadCallback(drawCharts);");
          client.println("function drawCharts() {");
          client.println("var data1 = new google.visualization.DataTable();");
          client.println("data1.addColumn('string', 'Time');");
          client.println("data1.addColumn('number', 'Temperature (°F)');");
          if (prevDay){
          client.println("var data2 = new google.visualization.DataTable();");
          client.println("data2.addColumn('string', 'Time');");
          client.println("data2.addColumn('number', 'Temperature (°F)');");
          }
          client.println("var currentData =[");
          Serial.print("dataToday: ");
          Serial.println(dataToday);
          if (dataToday == 0){
            client.print("['");
            client.print(todayTime[dataToday]);
            client.print("', ");
            client.print(tempData[dataToday]);
            client.println("]");
            client.println("];");
          }
          for(int i=0; i<dataToday; i++){   //Data for Current reading's chart
              client.print("['");
              client.print(todayTime[i]);
              client.print("', ");
              client.print(tempData[i]);
            if (i == (dataToday - 1)){
              client.println("]");
              client.println("];");
            }
            else {
              client.println("],");
            }
          }
          if (prevDay){
          client.println("var prevData =[");
          for(int i=0; i<dataYesterday; i++){    //Data for previous reading's Chart
            client.print("['");
            client.print(prevTime[i]);
            client.print("', ");
            client.print(prevData[i]);
            if (i == (dataYesterday - 1)){
              client.println("]");
              client.println("];");
            }
            else {
              client.println("],");
            }
          }
          }
          client.println("data1.addRows(currentData);");
          if (prevDay){
          client.println("data2.addRows(prevData);");
          }
          client.println("var options1 = {");
          client.println("title: 'Temperature Data Today',");
          client.println("curveType: 'function',");
          client.println("legend: { position: 'bottom' }");
          client.println("};");
          if (prevDay){
          client.println("var options2 = {");
          client.println("title: 'Temperature Data Yesterday',");
          client.println("curveType: 'function',");
          client.println("legend: { position: 'bottom' }");
          client.println("};");
          }
          client.println("var chart1 = new google.visualization.LineChart(document.getElementById('temperatureChart1'));");
          client.println("chart1.draw(data1, options1);");
          if (prevDay){
          client.println("var chart2 = new google.visualization.LineChart(document.getElementById('temperatureChart2'));");
          client.println("chart2.draw(data2, options2);");
          }
          client.println("}");
          client.println("</script>");
          client.println("</center>");
          client.println("</body>");
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}
void resetTempData(){    // resets all data to current reading
  for (int i=0; i<dataYesterday; i++){
    tempData[i] = tempF;
  }
}
void previousDayTransfer(){    //transfers all data to previous set array
  prevDay = true;
  Serial.print("dataYesterday: ");
  Serial.println(dataYesterday);
  for (int i=0; i<dataYesterday; i++){
    prevData[i] = tempData[i];
    prevTime[i] = todayTime[i];
    Serial.println(prevData[i]);
  }
}
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

void connectToWiFi(){
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (wifiStatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    wifiStatus = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi");
  printWifiStatus();
}
void wifiReconnect(){
  if(WiFi.status() != WL_CONNECTED){     //To prevent the Wifi Lease from expiring
    wifiStatus = 0;                      //WL_CONNECTED = 3, didn't know the keyword for Disconnected
    while (wifiStatus != WL_CONNECTED) {                //Actually Reconnects
      Serial.print("Attempting to connect to SSID: ");
      Serial.println(ssid);
      // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
      wifiStatus = WiFi.begin(ssid, pass);

      // wait 10 seconds for connection:
      delay(10000);
    }
  }
}
void pullTimeFromWeb(){
  RTC.begin();
  Serial.println("\nStarting connection to server...");
  timeClient.begin();
  timeClient.update();

  // Get the current date and time from an NTP server and convert
  // it to UTC -7 by passing the time zone offset in hours.
  // You may change the time zone offset to your local one.
  auto timeZoneOffsetHours = -7;
  auto unixTime = timeClient.getEpochTime() + (timeZoneOffsetHours * 3600);
  Serial.print("Unix time = ");
  Serial.println(unixTime);
  RTCTime timeToSet = RTCTime(unixTime);
  RTC.setTime(timeToSet);
  RTC.getTime(currentTime); 
  Serial.println("The RTC was just set to: " + String(currentTime));
}
