#include <BluetoothSerial.h>
#include <dummy.h>
#include "esp_camera.h"
#include <WiFi.h>

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const char* ssid = "TEST1";
const char* password = "123456789";
char PC2UnoMessage[5];
char Uno2PCMessage[40];
char BTDetect;
char SerialDetect;
BluetoothSerial SerialBT;
void startCameraServer();

//virtual car mode
bool isRunning = false;
int direction = 0;
int distance = 0;
int targetDistance = 0;
char nowRunningCmd;
void camSetup() { //初始化摄像头
	camera_config_t config;
	config.ledc_channel = LEDC_CHANNEL_0;
	config.ledc_timer = LEDC_TIMER_0;
	config.pin_d0 = Y2_GPIO_NUM;
	config.pin_d1 = Y3_GPIO_NUM;
	config.pin_d2 = Y4_GPIO_NUM;
	config.pin_d3 = Y5_GPIO_NUM;
	config.pin_d4 = Y6_GPIO_NUM;
	config.pin_d5 = Y7_GPIO_NUM;
	config.pin_d6 = Y8_GPIO_NUM;
	config.pin_d7 = Y9_GPIO_NUM;
	config.pin_xclk = XCLK_GPIO_NUM;
	config.pin_pclk = PCLK_GPIO_NUM;
	config.pin_vsync = VSYNC_GPIO_NUM;
	config.pin_href = HREF_GPIO_NUM;
	config.pin_sscb_sda = SIOD_GPIO_NUM;
	config.pin_sscb_scl = SIOC_GPIO_NUM;
	config.pin_pwdn = PWDN_GPIO_NUM;
	config.pin_reset = RESET_GPIO_NUM;
	config.xclk_freq_hz = 20000000;
	config.pixel_format = PIXFORMAT_JPEG;
	//init with high specs to pre-allocate larger buffers
	if (psramFound()) {
		config.frame_size = FRAMESIZE_UXGA;
		config.jpeg_quality = 10;
		config.fb_count = 2;
	}
	else {
		config.frame_size = FRAMESIZE_SVGA;
		config.jpeg_quality = 12;
		config.fb_count = 1;
	}

#if defined(CAMERA_MODEL_ESP_EYE)
	pinMode(13, INPUT_PULLUP);
	pinMode(14, INPUT_PULLUP);
#endif

	// camera init
	esp_err_t err = esp_camera_init(&config);
	if (err != ESP_OK) {
		Serial.printf("Camera init failed with error 0x%x", err);
		return;
	}

	sensor_t * s = esp_camera_sensor_get();
	//initial sensors are flipped vertically and colors are a bit saturated
	if (s->id.PID == OV3660_PID) {
		s->set_vflip(s, 1);//flip it back
		s->set_brightness(s, 1);//up the blightness just a bit
		s->set_saturation(s, -2);//lower the saturation
	}
	//drop down frame size for higher initial frame rate
	s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
	s->set_vflip(s, 1);
	s->set_hmirror(s, 1);
#endif

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");

	startCameraServer();

	Serial.print("Camera Ready! Use 'http://");
	Serial.print(WiFi.localIP());
	Serial.println("' to connect");
}

void serialSetup() { //初始化蓝牙串口和uno通信串口
	Serial.begin(115200);
	Serial.setDebugOutput(true);
	Serial.println(); //这里必须先输出一个换行符才能正常启动Serial
	Serial.println("Serial1.begin");
	SerialBT.begin("ESP32");
	Serial.println("SerialBT loaded.");
	Serial2.begin(9600);
	Serial2.println();
	Serial.println("Serial2 begin");
}

void relayLoop() { //中继循环调用
	//检测蓝牙串口，把接收到的指令发给uno
	if (SerialBT.available() > 0) {
		BTDetect = (char)SerialBT.read();
		if (BTDetect == 'F' || BTDetect == 'B' || BTDetect == 'R' || BTDetect == 'L' || BTDetect == 'A' || BTDetect == 'S') {
			Serial2.print(BTDetect);
			for (int i = 0; i < 4; i++) {
				if (SerialBT.available()) {
					Serial2.print((char)SerialBT.read());
				}
				else {
					Serial.println("Error in sending serialBT data");
				}
			}
		}
	}

	if (Serial2.available() > 0) {
		SerialDetect = (char)Serial2.read();
		if (SerialDetect == '-') {
			SerialBT.print("-");
			bool isError = true;
			while (Serial2.available()) {
				SerialDetect = Serial2.read();
				SerialBT.print(SerialDetect);
				if (SerialDetect == ';') {
					isError = false;
					break;
				}
			}
			if (isError == true) {
				Serial.println("Error in sending Serial2 data.");
			}
		}
	}
}

void virtualCarSetup() //虚拟小车，用于测试GUI
{
	Serial.begin(115200);
	Serial.setDebugOutput(true);
	Serial.println();
	Serial.println("ESP32 virtual car mode.");
}

void reset() {
	distance = 0;
	targetDistance = 0;
}
void virtualCarLoop() {
	if (Serial.available() > 0) {
		BTDetect = (char)Serial.read();
		for (int k = 1; k < 5; k++) {
			PC2UnoMessage[k] = (char)Serial.read();
		}
		int num = (PC2UnoMessage[2] - '0') * 100 + (PC2UnoMessage[3] - '0') * 10 + (PC2UnoMessage[4] - '0');
		switch (BTDetect) {
		case 'F':
			nowRunningCmd = BTDetect;
			isRunning = true;
			direction = 1;
			targetDistance = num;
			break;
		case 'B':
			nowRunningCmd = BTDetect;
			isRunning = true;
			direction = -1;
			targetDistance = num;
			break;
		case 'R':
			nowRunningCmd = BTDetect;
			isRunning = true;
			direction = 1;
			targetDistance = num;
			break;
		case 'L':
			nowRunningCmd = BTDetect;
			isRunning = true;
			direction = -1;
			targetDistance = num;
			break;
		case 'S':
			nowRunningCmd = BTDetect;
			isRunning = false;
			if (PC2UnoMessage[1] == '0' && num == 0) {
				Serial.println("-E1;");
			}
			else {
				Serial.println("-S;");
			}
			reset();
			break;
		default:
			break;
		}
	}
	if (isRunning == true) {
		if (abs(targetDistance) > abs(distance))
		{
			Serial.print("-");
			Serial.print(nowRunningCmd);
			Serial.print(distance);
			Serial.println(";");
			distance += direction;
			delay(50);
		}
		else {
			Serial.println("-E0;");
			isRunning = false;
			reset();
		}
	}
}

void setup() {
	/*
	serialSetup();
	camSetup();
	*/
	virtualCarSetup();
}

void loop() {
	/*
	relayLoop();
	*/
	virtualCarLoop();
}