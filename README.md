
## Raspiberry Pi 3B에 MQTT 브러커 설치
mqtt broker 설치한다.
```bash
   sudo apt-get update
   sudo apt-get install mosquitto mosquitto-clients
   sudo systemctl start mosquitto
   sudo systemctl enable mosquitto
   sudo systemctl status mosquitto
```
/etc/mosquitto/mosquitto.conf 파일 확인을 해보면, conf.d를 포함하므로 conf.d 폴더에 사용하려는 .conf파일을 생성 한다.

/etc/mosquitto/conf.d/mqtt-home-assistant.conf을 생성하여 mqtt 1883 port를 활성화한다.
```
listener 1883
allow_anonymous true
```

## NodeMCU 시리얼통신 (Serial) 방법

## 1. usbipd list

WSL2에서 COM3를 공유받아 사용하기 위해서는 `usbipd` 를 이용하여 공유해야한다.

```powershell
PS C:\WINDOWS\system32> usbipd list
Connected:
BUSID  VID:PID    DEVICE                                                        STATE
1-3    0bda:a729  Realtek Bluetooth 5.3 Adapter                                 Not shared
1-5    8087:0029  인텔(R) 무선 Bluetooth(R)                                     Not shared
5-1    0bda:8152  Realtek USB FE Family Controller                              Not shared
5-3    1a86:7523  USB-SERIAL CH340(COM3)                                        Not shared
5-4    25a7:fa61  USB 입력 장치                                                 Not shared
6-4    05e3:0751  USB 대용량 저장 장치                                          Not shared

Persisted:
GUID                                  DEVICE

PS C:\WINDOWS\system32>
```

## 2. usbipd bind

`usbipd  bind --busid 5-3` 를 입력하여 COM3를 shared 상태로 변경한다.

```powershell
PS C:\WINDOWS\system32> usbipd  bind --busid 5-3
PS C:\WINDOWS\system32> usbipd list
Connected:
BUSID  VID:PID    DEVICE                                                        STATE
1-3    0bda:a729  Realtek Bluetooth 5.3 Adapter                                 Not shared
1-5    8087:0029  인텔(R) 무선 Bluetooth(R)                                     Not shared
5-1    0bda:8152  Realtek USB FE Family Controller                              Not shared
5-3    1a86:7523  USB-SERIAL CH340(COM3)                                        Shared
5-4    25a7:fa61  USB 입력 장치                                                 Not shared
6-4    05e3:0751  USB 대용량 저장 장치                                          Not shared

Persisted:
GUID                                  DEVICE
```

## 3. usbipd attach

COM3의 busid가 5-3 이므로 아래와 같이 입력한다.

그러면, Windows의 장치관리자에 연결되어 있는 COM3 장치가 없어지고 WSL2의 우분투에 연결된다.

```powershell
PS C:\WINDOWS\system32> usbipd  attach --busid 5-3 --wsl Ubuntu-24.04
usbipd: info: Selecting a specific distribution is no longer required. Please file an issue if you believe that the default selection mechanism is not working for you.
usbipd: info: Using WSL distribution 'Ubuntu-24.04' to attach; the device will be available in all WSL 2 distributions.
usbipd: info: Using IP address 172.20.224.1 to reach the host.
PS C:\WINDOWS\system32>
```


참조: wsl 목록은 다음 명령으로 확인 가능

```powershell
PS C:\WINDOWS\system32> wsl --list
Linux용 Windows 하위 시스템 배포:
Ubuntu-24.04(기본값)
Ubuntu-22.04
```

## 4. usbipd detach

COM3를 윈도우로 반환하려면 다음과 같이 입력한다.

```powershell
PS C:\WINDOWS\system32> usbipd  detach --busid 5-3
```

## 5. usbipd unbind

COM3를 윈도우로 반환하려면 다음과 같이 입력한다.

```powershell
PS C:\WINDOWS\system32> usbipd unbind --busid 5-3
```



# WSL2 우분투에서 확인

## dmesg

usb 1-1이 활성화 된 것을 확인

```powershell
$ dmesg
[17516.210016] vhci_hcd vhci_hcd.0: pdev(0) rhport(0) sockfd(3)
[17516.210060] vhci_hcd vhci_hcd.0: devid(327683) speed(2) speed_str(full-speed)
[17516.210164] vhci_hcd vhci_hcd.0: Device attached
[17516.470097] vhci_hcd: vhci_device speed not set
[17516.532410] usb 1-1: new full-speed USB device number 2 using vhci_hcd
[17516.606033] vhci_hcd: vhci_device speed not set
[17516.668781] usb 1-1: SetAddress Request (2) to port 0
[17516.706609] usb 1-1: New USB device found, idVendor=1a86, idProduct=7523, bcdDevice= 2.54
[17516.706614] usb 1-1: New USB device strings: Mfr=0, Product=2, SerialNumber=0
[17516.706615] usb 1-1: Product: USB2.0-Serial
[17516.734797] usbcore: registered new interface driver ch341
[17516.734810] usbserial: USB Serial support registered for ch341-uart
[17516.734819] ch341 1-1:1.0: ch341-uart converter detected
[17516.748190] usb 1-1: ch341-uart converter now attached to ttyUSB0
```

## /dev

ttyUSB0가 연결된 것을 확인할 수 있다.

```powershell
$ ls /dev/ttyU*
/dev/ttyUSB0
```

## minicom 연결
/dev/ttyUSB0를 통해서 minicom으로 시리얼통신이 가능한 것을 확인할 수 있다.

## vscode의 platformio IDE
platfomrio IDE를 이용해서 Erase Flash를 실행해본다.
```bash
 *  작업 실행 중: platformio run --target erase --environment nodemcuv2 

Processing nodemcuv2 (platform: espressif8266; board: nodemcuv2; framework: arduino)
--------------------------------------------------------------------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif8266/nodemcuv2.html
PLATFORM: Espressif 8266 (4.2.1) > NodeMCU 1.0 (ESP-12E Module)
HARDWARE: ESP8266 80MHz, 80KB RAM, 4MB Flash
PACKAGES: 
 - framework-arduinoespressif8266 @ 3.30102.0 (3.1.2) 
 - tool-esptool @ 1.413.0 (4.13) 
 - tool-esptoolpy @ 1.30000.201119 (3.0.0) 
 - toolchain-xtensa @ 2.100300.220621 (10.3.0)
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 38 compatible libraries
Scanning dependencies...
No dependencies
Building in release mode
Looking for serial port...
Auto-detected: /dev/ttyUSB0
Erasing...
esptool.py v3.0
Serial port /dev/ttyUSB0
Connecting....
Chip is ESP8266EX
Features: WiFi
Crystal is 26MHz
MAC: 3c:71:bf:29:a1:0a
Uploading stub...
Running stub...
Stub running...
Erasing flash (this may take a while)...
Chip erase completed successfully in 7.4s
Hard resetting via RTS pin...
======================================================= [SUCCESS] Took 9.34 seconds =======================================================
 *  터미널이 작업에서 다시 사용됩니다. 닫으려면 아무 키나 누르세요. 

```