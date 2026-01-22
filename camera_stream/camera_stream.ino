#include <esp_camera.h>
#include <WiFi.h>
#include <WebServer.h>

// 1) AI-Thinker pin map
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// 2) Your Wi-Fi credentials
const char* STA_SSID = "BakerStreet";
const char* STA_PWD  = "Cookie098";

WebServer server(80);

// Root page: embed the MJPEG stream
void handleRoot() {
  server.send(200, "text/html",
              "<!DOCTYPE html><html><head><title>ESP32-CAM</title></head>"
              "<body style='margin:0;padding:0;'><img src='/stream' style='width:100%;'/>"
              "</body></html>");
}

// MJPEG stream handler
void handleStream() {
  WiFiClient client = server.client();
  client.setNoDelay(true);

  client.print(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) break;

    client.printf(
      "--frame\r\n"
      "Content-Type: image/jpeg\r\n"
      "Content-Length: %u\r\n\r\n",
      fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);

    yield();   // let Wi-Fi + HTTP do their thing
    delay(1);  // small pause for TCP acks
  }
  client.stop();
}

// Dedicated HTTP server task
void httpTask(void*) {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.begin();
  for (;;) {
    server.handleClient();
    vTaskDelay(1);  // ~1 ms
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  camera_config_t cfg{};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = Y2_GPIO_NUM;
  cfg.pin_d1       = Y3_GPIO_NUM;
  cfg.pin_d2       = Y4_GPIO_NUM;
  cfg.pin_d3       = Y5_GPIO_NUM;
  cfg.pin_d4       = Y6_GPIO_NUM;
  cfg.pin_d5       = Y7_GPIO_NUM;
  cfg.pin_d6       = Y8_GPIO_NUM;
  cfg.pin_d7       = Y9_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_pclk     = PCLK_GPIO_NUM;
  cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cfg.pin_href     = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size   = FRAMESIZE_QVGA;  // 320×240
  cfg.jpeg_quality = 10;
  cfg.fb_count     = 2;               // double buffer

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("Camera init failed");
    while (true) delay(100);
  }

  // Flip/mirror
  auto s = esp_camera_sensor_get();
  s->set_hmirror(s, 1);
  s->set_vflip(s, 1);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PWD);
  Serial.printf("Connecting to Wi-Fi \"%s\"", STA_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.printf("Connected! Stream at http://%s/\n", WiFi.localIP().toString().c_str());

  // Start the HTTP server task
  xTaskCreate(
    httpTask,    // function
    "httpTask",  // name
    4096,        // stack size
    nullptr,     // param
    1,           // priority
    nullptr      // handle
  );
}

void loop() {
  // Everything is handled in httpTask
  delay(1000);
}
