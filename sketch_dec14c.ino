
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNFaceDetectionRecognition.h"
#include "VideoStreamOverlay.h"
#include <WiFi.h>
#include <WiFiSSLClient.h>
#include <vector>
#include <NTPClient.h>

#define CHANNEL   0
#define CHANNELNN 3

// Customised resolution for NN
#define NNWIDTH  576
#define NNHEIGHT 320

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNFaceDetectionRecognition facerecog;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerFDFR(1, 1);
StreamIO videoStreamerRGBFD(1, 1);

char ssid[] = "mm123";    // your network SSID (name)
char pass[] = "aaaaaaaa";        // your network password
int status = WL_IDLE_STATUS;

IPAddress ip;
int rtsp_portnum;

const char* firebaseHost =
  "my-project-134ab-default-rtdb.asia-southeast1.firebasedatabase.app";
const int httpsPort = 443;

WiFiSSLClient client;

unsigned long lastEventTime;
const unsigned long EVENT_COOLDOWN = 5000;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org",0);

unsigned long long lastDay = 0;
unsigned long lastOSD = 0;

void setup()
{
    Serial.begin(115200);
    // Attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }

    ip = WiFi.localIP();
    // 啟動 NTP
    timeClient.begin();

    // 強制同步一次
    while (!timeClient.update()) {
      timeClient.forceUpdate();
    }
    // Configure camera video channels with video format information
    // Adjust the bitrate based on your WiFi network quality
    config.setBitrate(2 * 1024 * 1024);    // Recommend to use 2Mbps for RTSP streaming to prevent network congestion
    Camera.configVideoChannel(CHANNEL, config);
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();

    // Configure RTSP with corresponding video format information
    rtsp.configVideo(config);
    rtsp.begin();
    rtsp_portnum = rtsp.getPort();

    // Configure Face Recognition model
    // Select Neural Network(NN) task and models
    facerecog.configVideo(configNN);
    facerecog.modelSelect(FACE_RECOGNITION, NA_MODEL, DEFAULT_SCRFD, DEFAULT_MOBILEFACENET);
    facerecog.begin();
    facerecog.setResultCallback(FRPostProcess);

    // Configure StreamIO object to stream data from video channel to RTSP
    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }
    // Start data stream from video channel
    Camera.channelBegin(CHANNEL);

    // Configure StreamIO object to stream data from RGB video channel to face detection
    videoStreamerRGBFD.registerInput(Camera.getStream(CHANNELNN));
    videoStreamerRGBFD.setStackSize();
    videoStreamerRGBFD.setTaskPriority();
    videoStreamerRGBFD.registerOutput(facerecog);
    if (videoStreamerRGBFD.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }

    // Start video channel for NN
    Camera.channelBegin(CHANNELNN);

    // Start OSD drawing on RTSP video channel
    OSD.configVideo(CHANNEL, config);
    OSD.begin();
    facerecog.restoreRegisteredFace();
}

void loop()
{
    if (Serial.available() > 0) {
        String input = Serial.readString();
        input.trim();

        if (input.startsWith(String("REG="))) {
            String name = input.substring(4);
            facerecog.registerFace(name);
        } else if (input.startsWith(String("DEL="))) {
            String name = input.substring(4);
            facerecog.removeFace(name);
        } else if (input.startsWith(String("RESET"))) {
            facerecog.resetRegisteredFace();
        } else if (input.startsWith(String("BACKUP"))) {
            facerecog.backupRegisteredFace();
        } else if (input.startsWith(String("RESTORE"))) {
            facerecog.restoreRegisteredFace();
        }
    }

    if (millis() - lastOSD > 2000) {
        lastOSD = millis();
        OSD.createBitmap(CHANNEL);
        OSD.update(CHANNEL);
    }
}

unsigned long long now() {
  return timeClient.getEpochTime();
}

bool IsNewDay(){
    if(lastDay==0){
        lastDay=(now()+28800)/86400;
        return false;
    }
    unsigned long long n=now()+28800;
    unsigned long long currentDay = n / 86400;
    if(currentDay!=lastDay){
        lastDay=currentDay;
        return true;
    }
    return false;
}

void sendTest(String path,String json) {
  if (millis() - lastEventTime < EVENT_COOLDOWN) return;
  lastEventTime = millis();

  Serial.println("Connecting to Firebase...");

  if (!client.connect(firebaseHost, httpsPort)) {
    Serial.println("❌ Firebase HTTPS 連線失敗");
    return;
  }

  client.print(
    String("PUT "+path+".json HTTP/1.1\r\n") +
    "Host: " + firebaseHost + "\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + json.length() + "\r\n" +
    "Connection: close\r\n\r\n" +
    json
  );

  Serial.println("===== Firebase Response =====");

  unsigned long timeout = millis();
  while (client.connected() && millis() - timeout < 5000) {
    while (client.available()) {
      char c = client.read();
      Serial.write(c);
      timeout = millis();
    }
  }

  client.stop();
  Serial.println("\n===== End =====");
}

std::vector<String> GetOwner(){
    String owners;
    Serial.println("Connecting to Firebase...");

    if (!client.connect(firebaseHost, httpsPort)) {
        Serial.println("❌ Firebase HTTPS 連線失敗");
        return std::vector<String>(0);
    }

    client.print(
        String("GET /Config/owners.json HTTP/1.1\r\n") +
        "Host: " + firebaseHost + "\r\n" +
        "Connection: close\r\n\r\n"
    );

    // 讀 response
    String response = "";
    unsigned long timeout = millis();

    while (client.connected() && millis() - timeout < 5000) {
        while (client.available()) {
            char c = client.read();
            response += c;
            timeout = millis();
        }
    }
    client.stop();

    // 找 JSON 本體（跳過 HTTP header）
    int jsonStart = response.indexOf("\r\n\r\n");
    if (jsonStart == -1) {
        Serial.println("❌ 找不到 JSON");
        return std::vector<String>(0);
    }

    String json = response.substring(jsonStart + 4);
    json.trim();   // 例如："Liao,Chen,Wang"

    // Firebase 回來是 JSON string，要去掉 ""
    if (json.startsWith("\"") && json.endsWith("\"")) {
        json = json.substring(1, json.length() - 1);
    }

    owners = json;
    std::vector<String> result;
    int start = 0;

    while (true) {
        int idx = owners.indexOf(',', start);
        if (idx == -1) {
            result.push_back(owners.substring(start));
            break;
        }
        result.push_back(owners.substring(start, idx));
        start = idx + 1;
    }
    // for(auto a:result){
    //   Serial.println(a);
    // }
    // delay(5000);
    return result;
}

void FRPostProcess(std::vector<FaceRecognitionResult> results)
{
    uint16_t im_h = config.height();
    uint16_t im_w = config.width();

    Serial.print("Network URL for RTSP Streaming: ");
    Serial.print("rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp_portnum);
    Serial.println(" ");

    printf("Total number of faces detected = %d\r\n", facerecog.getResultCount());
    OSD.createBitmap(CHANNEL);

    if (facerecog.getResultCount() > 0) {
        //std::vector<String> Owners = GetOwner();
        std::vector<String> Owners(1,"Liao");
        std::vector<std::pair<String,String>> Human;
        for (int i = 0; i < facerecog.getResultCount(); i++) {
            FaceRecognitionResult item = results[i];
            // Result coordinates are floats ranging from 0.00 to 1.00
            // Multiply with RTSP resolution to get coordinates in pixels
            int xmin = (int)(item.xMin() * im_w);
            int xmax = (int)(item.xMax() * im_w);
            int ymin = (int)(item.yMin() * im_h);
            int ymax = (int)(item.yMax() * im_h);

            uint32_t osd_color;
            if (String(item.name()) == String("unknown")) {
                Human.push_back({"unknown","unknown"});
                osd_color = OSD_COLOR_RED;
                //sendTest("unknown");
            } else {
                bool IsOwner=false;
                for(auto a:Owners){
                  if(a==item.name()){
                    Human.push_back({a,"owner"});
                    osd_color = OSD_COLOR_GREEN;
                    IsOwner=true;
                  }
                }
                if(!IsOwner){
                    Human.push_back({item.name(),"visitor"});
                    osd_color = OSD_COLOR_YELLOW;
                    //sendTest(String(item.name()));
                }
            }
            // Draw boundary box
            printf("Face %d name %s:\t%d %d %d %d\n\r", i, item.name(), xmin, xmax, ymin, ymax);
            OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, osd_color);

            // Print identification text above boundary box
            char text_str[40];
            snprintf(text_str, sizeof(text_str), "Face:%s", item.name());
            OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, osd_color);
        }
        if(Human.size()!=0){
          String path="/timeline/"+String((unsigned long)now()),json="{\"people\":[";
          for (size_t i = 0; i < Human.size(); ++i) {
              json += "{";
              json += "\"name\":\"" + Human[i].first + "\",";
              json += "\"role\":\"" + Human[i].second + "\"";
              json += "}";
              if (i != Human.size() - 1)
                  json += ",";
          }
          json += "]}";
          sendTest(path,json);
        }
    }
    OSD.update(CHANNEL);
}

