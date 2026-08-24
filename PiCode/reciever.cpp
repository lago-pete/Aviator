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
#include <vector>
#include <algorithm>
#include <set>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
using namespace std;

typedef websocketpp::server<websocketpp::config::asio> server_t;

bool setup();
void loop();
void printDisplay();
void printFile();
void sendToClients();

int udp_fd;
struct sockaddr_in addr;
char buffer[100];
struct sockaddr_in returnAddr;
struct in_addr espIP;
socklen_t addLen;
timeval timeout;
server_t ws_server;

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
    float gasResistance = 0;
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
{
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
fd_set udpSet;
set<websocketpp::connection_hdl, owner_less<websocketpp::connection_hdl>> clients; // ownerless is a wrapped for the hdl allowing the set to now organize it. We need this because the hdl is a weak pointer and the set is unable to compare them. It is considered a rule in the set param

void on_open(websocketpp::connection_hdl hdl)
{
    clients.insert(hdl);
    cout << "Client connected" << "\n";
    cout << "Total Clients: " << clients.size() << "\n";
}
void on_close(websocketpp::connection_hdl hdl)
{
    clients.erase(hdl);
    cout << "Client disconnected" << "\n";
    cout << "Total Clients: " << clients.size() << "\n";
}
void on_message(websocketpp::connection_hdl hdl, server_t::message_ptr msg)
{
    cout << "Message received" << "\n";
}
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

    ////UDP SETUP
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_fd == -1)
    {
        cout << "Error Getting FD" << strerror(errno) << "\n";
        return false;
    }
    fcntl(udp_fd, F_SETFL, O_NONBLOCK); // This is a call to make the certain fd non blocking. We do this because if we call recvfrom() and there is no data yet it will pause and wait for that data. However since we have multiple things going on we want to make sure that it will return rather then waiting so others can run.
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3434);
    addr.sin_addr.s_addr = INADDR_ANY;
    int check = bind(udp_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (check == -1)
    {
        cout << "Error Binding" << strerror(errno) << "\n";
        close(udp_fd);
        return false;
    }
    FD_SET(udp_fd, &udpSet);
    /////

    /// Websocket Setup
    ws_server.init_asio();
    ws_server.set_open_handler(on_open);
    ws_server.set_close_handler(on_close);
    ws_server.set_message_handler(on_message);
    ws_server.listen(8080);
    ws_server.start_accept();

    return true;
}
void loop()
{
    addLen = sizeof(returnAddr);
    fd_set copy = udpSet;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int maxFd = udp_fd;

    int result = select(maxFd + 1, &copy, nullptr, nullptr, &timeout); // The only reason I'm keeping select here is for the timeout, if the esp goes silent then there will be a 1 second delay. If it's not silent it runs full speed.

    if (result == 0)
    {
        cout << "[IDLE]...." << "\n";
    }

    if (result > 0 && FD_ISSET(udp_fd, &copy))
    {
        int readBuff = recvfrom(udp_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&returnAddr, &addLen);
        if (readBuff == -1)
        {
            cout << "Error Getting Data from Kernel" << strerror(errno) << "\n";
        }
        else if (readBuff != sizeof(TelemetryPacket))
        {
            cout << "Incorrect message " << "\n";
        }
        else if (returnAddr.sin_addr.s_addr != espIP.s_addr)
        {
            cout << "Incorrect IP " << "\n";
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
                sendToClients();
            }
        }
    }

    ws_server.poll_one();
    
}
void printFile()
{
    ostringstream j;
    j << std::setprecision(8);
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

    if (packet.sendGps.time.hour == 0 && packet.sendGps.time.minute == 0 && packet.sendGps.time.sec == 0)
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
    else if (gpsTimeCheck.hour == packet.sendGps.time.hour && gpsTimeCheck.minute == packet.sendGps.time.minute && gpsTimeCheck.sec == packet.sendGps.time.sec)
    {
        cout << "!!!!Last Know GPS Reading!!!!" << "\n";
        cout << "Lat: " << packet.sendGps.lat << " " << packet.sendGps.latDir << "\n";
        cout << "Lon: " << packet.sendGps.lon << " " << packet.sendGps.lonDir << "\n";
        cout << "Fix Quality: " << packet.sendGps.fixQual << "\n";
        cout << "Sat Count: " << packet.sendGps.satCount << "\n";
        cout << "HDOP: " << packet.sendGps.hdop << "\n";
        cout << "Altitude: " << packet.sendGps.altitude << "\n";
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

void sendToClients()
{
    ostringstream j;
    j << std::setprecision(8);
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
    string message = j.str();
    for (const auto &client : clients)
    {
        try
        {
            ws_server.send(client, message, websocketpp::frame::opcode::text);
        }
        catch (const websocketpp::exception &e)
        {
            cout << "Error sending message to client: " << e.what() << "\n";
        }
    }
}