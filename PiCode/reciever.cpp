#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include <string>
#include <iomanip> // need this cause im losing the last 2 digits of lat and lon
using namespace std;

bool setup();
void loop();
void printDisplay();
void printFile();

int fd;
struct sockaddr_in addr;
char buffer[100];
struct sockaddr_in returnAddr;
struct in_addr espIP;
socklen_t addLen;

struct __attribute__((packed)) MpuData
{
    int16_t accelX = 0, accelY = 0, accelZ = 0;
    int16_t temp = 0;
    int16_t gyroX = 0, gyroY = 0, gyroZ = 0;
    bool valid = false;
} __attribute__((packed));

struct __attribute__((packed)) BmeData
{
    float temperature = 0;
    float humidity = 0;
    float pressure = 0;
    float gasResistance = 0; // TODO: heater/gas-measurement sequence not implemented — no proven reference for this board
    bool valid = false;
} __attribute__((packed));

struct __attribute__((packed)) CompassData
{
    float headingDegrees = 0;
    int16_t rawX = 0, rawY = 0, rawZ = 0;
    bool valid = false;
} __attribute__((packed));

struct __attribute__((packed)) GPSTime
{
    int hour;
    int minute;
    float sec;
} __attribute__((packed));

struct __attribute__((packed)) GPSReading
{
    double lat;
    double lon;
    char latDir;
    char lonDir;
    int fixQual;
    int satCount;
    float hdop;
    float altitude;
    GPSTime time;
} __attribute__((packed));
struct __attribute__((packed)) TelemetryPacket
{ // the attribute packed will remove the padding that the compiler normally places as conveniance. Some data types like to be round numbers so the compiler will add space to push them into desired sizes.
    MpuData sendMpu;
    BmeData sendBme;
    CompassData sendComp;
    GPSReading sendGps;
} __attribute__((packed));

MpuData mpuData;
BmeData bmeData;
CompassData compassData;
GPSReading gpsReading;
TelemetryPacket packet;
GPSTime gpsTimeCheck;

time_t now = time(nullptr);
struct tm *localTime = localtime(&now);
int year = localTime->tm_year + 1900;
int month = localTime->tm_mon + 1;
int day = localTime->tm_mday;
int hour = localTime->tm_hour;
int minute = localTime->tm_min;
int second = localTime->tm_sec;
ofstream myFile("FlightLogs/Log_" + to_string(year) + "-" + to_string(month) + "-" + to_string(day) + "-" + to_string(hour) + ":" + to_string(minute) + ":" + to_string(second) + ".jsonl");

int main()
{
    int count = 1;
    cout << "TelemetryPacket size: " << sizeof(TelemetryPacket) << "\n";
    inet_pton(AF_INET, "10.42.0.89", &espIP);

    while (!setup())
    {
        cout << "Failed Setup...." << "\n";
        if (count == 5)
        {
            cout << "Attempted setup() 5 Times with no Success..... Ending As Error No Connection" << "\n";
            return -1;
        }
        cout << "Retrying Setup...." << "\n";
        sleep(2);
        count++;
    }

    while (1)
    {
        loop();
    }
}

bool setup()
{
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd == -1)
    {
        cout << "Error Getting FD" << strerror(errno) << "\n";
        return false;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(3434);
    addr.sin_addr.s_addr = INADDR_ANY;
    int check = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (check == -1)
    {
        cout << "Error Binding" << strerror(errno) << "\n";
        close(fd);
        return false;
    }

    return true;
}

void loop()
{
    addLen = sizeof(returnAddr);

    int readBuff = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&returnAddr, &addLen);
    if (readBuff == -1)
    {
        cout << "Error Getting Data from Kernel" << strerror(errno) << "\n";
        return;
    }
    else if (readBuff != sizeof(TelemetryPacket))
    {
        cout << "Incorrect message " << "\n";
        return;
    }
    else if (returnAddr.sin_addr.s_addr != espIP.s_addr)
    {
        cout << "Incorrect IP " << "\n";
        return;
    }
    else
    {

        memcpy(&packet, buffer, sizeof(packet));
        printDisplay();
        if (!myFile.is_open())
        {
            cout << "FILE Can NOT BE OPENED!!!!!!!!" << "\n";
        }
        else
        {
            printFile();
        }

        return;
    }
}

void printFile()
{
    ostringstream j;
    j << std::setprecision(8);  // ok apparently this is something caleld sticky and it applied to the rest of the calls, so when I was palcing it over and over it wasnt liking it. 
    j << "{";
    if (!packet.sendMpu.valid)
    {
        j << "\"mpu\":{\"valid\":false}";
    }
    else
    {
        j << "\"mpu\":{"
          << "\"valid\":true,"
          << "\"accelX\":" << (int16_t)packet.sendMpu.accelX << ","
          << "\"accelY\":" << (int16_t)packet.sendMpu.accelY << ","
          << "\"accelZ\":" << (int16_t)packet.sendMpu.accelZ << ","
          << "\"temp\":" << (int16_t)packet.sendMpu.temp << ","
          << "\"gyroX\":" << (int16_t)packet.sendMpu.gyroX << ","
          << "\"gyroY\":" << (int16_t)packet.sendMpu.gyroY << ","
          << "\"gyroZ\":" << (int16_t)packet.sendMpu.gyroZ
          << "}";
    }
    j << ",";

    if (!packet.sendBme.valid)
    {
        j << "\"bme\":{\"valid\":false}";
    }
    else
    {
        j << "\"bme\":{"
          << "\"valid\":true,"
          << "\"temperature\":" << (float)packet.sendBme.temperature << ","
          << "\"humidity\":" << (float)packet.sendBme.humidity << ","
          << "\"pressure\":" << (float)packet.sendBme.pressure << ","
          << "\"gasResistance\":" << (float)packet.sendBme.gasResistance
          << "}";
    }
    j << ",";

    if (!packet.sendComp.valid)
    {
        j << "\"compass\":{\"valid\":false}";
    }
    else
    {
        j << "\"compass\":{"
          << "\"valid\":true,"
          << "\"headingDegrees\":" << (float)packet.sendComp.headingDegrees << ","
          << "\"rawX\":" << (int16_t)packet.sendComp.rawX << ","
          << "\"rawY\":" << (int16_t)packet.sendComp.rawY << ","
          << "\"rawZ\":" << (int16_t)packet.sendComp.rawZ
          << "}";
    }
    j << ",";

    int gpsHour = packet.sendGps.time.hour;
    int gpsMinute = packet.sendGps.time.minute;
    float gpsSec = packet.sendGps.time.sec;
    bool gpsValid = !(gpsHour == gpsTimeCheck.hour && gpsMinute == gpsTimeCheck.minute && gpsSec == gpsTimeCheck.sec);
    if (!gpsValid)
    {
        j << "\"gps\":{"
          << "\"valid\":false,"
          << "\"time\":{"
          << "\"hour\":" << gpsHour << ","
          << "\"minute\":" << gpsMinute << ","
          << "\"sec\":" << gpsSec
          << "}"
          << "}";
    }
    else
    {
        double gpsLat = packet.sendGps.lat;
        double gpsLon = packet.sendGps.lon;
        char gpsLatDir = packet.sendGps.latDir;
        char gpsLonDir = packet.sendGps.lonDir;
        int gpsFixQual = packet.sendGps.fixQual;
        int gpsSatCount = packet.sendGps.satCount;
        float gpsHdop = packet.sendGps.hdop;
        float gpsAltitude = packet.sendGps.altitude;

        j << "\"gps\":{"
          << "\"valid\":true,"
          << "\"lat\":" << gpsLat << ","
          << "\"latDir\":\"" << gpsLatDir << "\","
          << "\"lon\":" << gpsLon << ","
          << "\"lonDir\":\"" << gpsLonDir << "\","
          << "\"fixQuality\":" << gpsFixQual << ","
          << "\"satCount\":" << gpsSatCount << ","
          << "\"hdop\":" << gpsHdop << ","
          << "\"altitude\":" << gpsAltitude << ","
          << "\"time\":{"
          << "\"hour\":" << gpsHour << ","
          << "\"minute\":" << gpsMinute << ","
          << "\"sec\":" << gpsSec
          << "}"
          << "}";
        gpsTimeCheck = packet.sendGps.time;
    }

    j << "}";

    myFile << j.str() << "\n";
}
void printDisplay()
{
    if (!packet.sendMpu.valid)
    {
        cout << "!!!MPU Reading Not Valid!!! " << packet.sendMpu.accelX << "\n";
        cout << "--- MPU ---" << "\n";
        cout << "Accel X: " << "--------------------" << "\n";
        cout << "Accel Y: " << "--------------------" << "\n";
        cout << "Accel Z: " << "--------------------" << "\n";
        cout << "Temp: " << "--------------------" << "\n";
        cout << "Gyro X: " << "--------------------" << "\n";
        cout << "Gyro Y: " << "--------------------" << "\n";
        cout << "Gyro Z: " << "--------------------" << "\n";
        cout << "Valid: " << "--------------------" << "\n";
    }
    else
    {
        cout << "--- MPU ---" << "\n";
        cout << "Accel X: " << packet.sendMpu.accelX << "\n";
        cout << "Accel Y: " << packet.sendMpu.accelY << "\n";
        cout << "Accel Z: " << packet.sendMpu.accelZ << "\n";
        cout << "Temp: " << packet.sendMpu.temp << "\n";
        cout << "Gyro X: " << packet.sendMpu.gyroX << "\n";
        cout << "Gyro Y: " << packet.sendMpu.gyroY << "\n";
        cout << "Gyro Z: " << packet.sendMpu.gyroZ << "\n";
        cout << "Valid: " << packet.sendMpu.valid << "\n";
    }
    if (!packet.sendBme.valid)
    {
        cout << "!!!!BME Reading Not Valid!!!!" << "\n";
        cout << "Temperature: " << "--------------------" << "\n";
        cout << "Humidity: " << "--------------------" << "\n";
        cout << "Pressure: " << "--------------------" << "\n";
        cout << "Gas Resistance: " << "--------------------" << "\n";
        cout << "Valid: " << "--------------------" << "\n";
    }
    else
    {
        cout << "--- BME ---" << "\n";
        cout << "Temperature: " << packet.sendBme.temperature << "\n";
        cout << "Humidity: " << packet.sendBme.humidity << "\n";
        cout << "Pressure: " << packet.sendBme.pressure << "\n";
        cout << "Gas Resistance: " << packet.sendBme.gasResistance << "\n";
        cout << "Valid: " << packet.sendBme.valid << "\n";
    }

    if (!packet.sendComp.valid)
    {
        cout << "!!!!Compass Reading Not Valid!!!!" << "\n";
        cout << "Heading Degrees: " << "--------------------" << "\n";
        cout << "Raw X: " << "--------------------" << "\n";
        cout << "Raw Y: " << "--------------------" << "\n";
        cout << "Raw Z: " << "--------------------" << "\n";
        cout << "Valid: " << "--------------------" << "\n";
    }
    else
    {
        cout << "--- Compass ---" << "\n";
        cout << "Heading Degrees: " << packet.sendComp.headingDegrees << "\n";
        cout << "Raw X: " << packet.sendComp.rawX << "\n";
        cout << "Raw Y: " << packet.sendComp.rawY << "\n";
        cout << "Raw Z: " << packet.sendComp.rawZ << "\n";
        cout << "Valid: " << packet.sendComp.valid << "\n";
    }

    if (packet.sendGps.time.hour == gpsTimeCheck.hour && packet.sendGps.time.minute == gpsTimeCheck.minute && packet.sendGps.time.sec == gpsTimeCheck.sec)
    {
        cout << "!!!!GPS Reading Not Valid!!!!" << "\n";
        cout << "Lat: " << "--------------------" << " " << "--------------------" << "\n";
        cout << "Lon: " << "--------------------" << " " << "--------------------" << "\n";
        cout << "Fix Quality: " << "--------------------" << "\n";
        cout << "Sat Count: " << "--------------------" << "\n";
        cout << "HDOP: " << "--------------------" << "\n";
        cout << "Altitude: " << "--------------------" << "\n";
        cout << "Time: " << packet.sendGps.time.hour << ":" << packet.sendGps.time.minute << ":" << packet.sendGps.time.sec << "\n";
    }
    else
    {
        cout << "--- GPS ---" << "\n";
        cout << "Lat: " << packet.sendGps.lat << " " << packet.sendGps.latDir << "\n";
        cout << "Lon: " << packet.sendGps.lon << " " << packet.sendGps.lonDir << "\n";
        cout << "Fix Quality: " << packet.sendGps.fixQual << "\n";
        cout << "Sat Count: " << packet.sendGps.satCount << "\n";
        cout << "HDOP: " << packet.sendGps.hdop << "\n";
        cout << "Altitude: " << packet.sendGps.altitude << "\n";
        cout << "Time: " << packet.sendGps.time.hour << ":" << packet.sendGps.time.minute << ":" << packet.sendGps.time.sec << "\n";
        gpsTimeCheck = packet.sendGps.time;
    }

    cout << "\n";
}