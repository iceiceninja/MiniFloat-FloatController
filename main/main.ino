//  If A fatal error occurred: Could not open /dev/ttyUSB0, the port doesn't exist
// try below command
// sudo chmod a+rw /dev/ttyUSB0


// TODO:
// - DC Motor library and code
// - Temperature sensor library and code
// - Gyroscopic and accelerometer library and code

#include <WiFi.h>
#include <Wire.h>
#include "MS5837.h"
// #include <OneWire.h> 
// #include <SoftwareSerial.h>

struct SensorData
{
  float depth_data[10];
  float pressure_data[10]; 
  float temperature_data[10];
  // add accelerometer and gyroscopic sensor data 
};

// Replace with your network credentials
const char* ssid     = "ESP32-Access-Point";
const char* password = "testing123";

// Set web server port number to 80
WiFiServer server(80);


// Variable to store the HTTP request
String header;

MS5837 depthSensor;

void setup() {
  Serial.begin(115200);
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: http://");
  Serial.println(IP);
  
  Wire.begin(); // look into what this does
  // Init sensors
  while (!depthSensor.init()) {
    Serial.println("Init failed!");
    Serial.println("Are SDA/SCL connected correctly?");
    Serial.println("Blue Robotics Bar30: White=SDA, Green=SCL");
    Serial.println("\n\n\n");
    delay(5000);
  }

  depthSensor.setFluidDensity(997); // double check and see if pool water has diff density. This should be freshwater density. 
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/plain");
            client.println("Connection: close");
            client.println();
            
            if(header.indexOf("GET /start") >= 0)
            {
              Serial.println("Starting vertical Profile...");
              client.println("Starting vertical Profile...");
              // verify that all electrical components are connected. Otherwise output error. allSensorCheck should output the error to client
              if(allSensorCheck()){
                performVerticalProfile();
              }
            }
            else if(header.indexOf("GET /retrieve"))
            {
              Serial.println("Displaying sensor data...");
              client.println("Displaying sensor data...");
              sendSensorData(client);
            }

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void performVerticalProfile()
{
  // Begin by turning the motor in the direction to deflate the bladder causing the float to sink
  descend();
  
  // Occasionally take depth sensor data and when depth is the (relatively) same for n number of reads (or after timeout) proceed to next step
  waitUntilFullyDescent();
  
  // Take all other sensor data. This is the data that we will transmit to the mission control
  getSensorData();// Give this arrays or a struct for each sensor to put its recorded values

  // reverse motor direction and use syringe to inflate bladder to cause float to rise
  ascend();
  // When float has reached surface (or maybe have mission control constantly ping /retrieve and whenever it makes connection) then send data to mission control
  // await further instructions. Probably just stop this funciton and return to loop
}

bool allSensorCheck()
{
  // this should return true if all sensors are able to be read and return a valid value.
  // Should communicate with the client on status. If all sensors are good then send "All sensors checked. beginning descent" otherwise try to communicate which sensor is malfuncitoning.
  // Maybe have an option to override this check just in case we want to proceed regardless
}
void descend()
{
  // logic to turn dc motor which uses syringe to deflate the bladder until the float starts to decend.
}
void ascend()
{
  // logic to turn dc motor in direciton to inflate bladder until the float accends
}
void getSensorData()
{
  // Update pressure and temperature readings
  // TODO: Take lots of sensor readings and store them in an array. then get the average or median?
  
  depthSensor.read();

  Serial.print("Pressure: ");
  Serial.print(depthSensor.pressure());
  Serial.println(" mbar");

  Serial.print("Temperature: ");
  Serial.print(depthSensor.temperature());
  Serial.println(" deg C");

  Serial.print("Depth: ");
  Serial.print(depthSensor.depth());
  Serial.println(" m");

  Serial.print("Altitude: ");
  Serial.print(depthSensor.altitude());    // THis is probably not needed
  Serial.println(" m above mean sea level");

  delay(1000); // have this in a loop? 
}

void sendSensorData(WiFiClient client)
{
  client.println("Example sensor data:");
  client.println("Temp: 65 Degrees Farenheit");
  client.println("PH: 1 billion (idk what would be normal)");
  client.println("");
}

float summedAbsDiff(float num, float nums[])
{
  int array_size = sizeof(nums) / sizeof(nums[0]);
  int sum=0;
  for(int i = 0; i<array_size; i++)
  {
    if(num > nums[i])
    {
      sum += num-nums[i];
    }else
    {
      sum += nums[i]-num;
    }
  }
  return sum;
}

// This function should pretty much trap the program in a while loop until the float is (hopefully) at the bottom. Then we will take all other sensor data
// maybe include a timeout or something that tracks time and when time runs out then it exits function regardless. (10 to 15 secs maybe?)
void waitUntilFullyDescent()
{
  int cache_size = 5;
  float cached_depths[cache_size];
  float curr_depth;
  int counter=0;

  // since depth is measured in meters, have a low tolerance so we know when we have hit the bottom
  // This should be a tolerance of 1cm which means that the summed absolute differences in depth need to be <= 1 cm for use to consider ourselves at the bottom
  const int depth_tolerance = 0.01; 
  
  do
  {
    float curr_depth = depthSensor.depth();
    cached_depths[counter%cache_size] = curr_depth;
    counter++;
  }while(summedAbsDiff(curr_depth, cached_depths) <= depth_tolerance);
}