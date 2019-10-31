/*
   File: bleFobClient.cpp. Bluetooth Client for the FOB Button (https://ebay.to/2M150E0)
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.0
   Creation: Sep-11-2019
   Revised: Oct-24-2019
   Release: Oct-30-2019
   Author: T. Black
   (c) copyright T. Black 2019, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   Based on the library's BLE_Client.ino example.
   This Code was formatted with the uncrustify extension.

   IMPORTANT:
    The official Arduino BLE lib is buggy (crash and hang issues); I have patched it with custom code
    tweaks (do NOT use the official release). It's still not perfect, but is more stable. The patched
    arduino BLE library has been copied into the project's libdeps folder:
    <project base directory>/PulseWelder/.pio/libdeps/lolin_d32_pro/ESP32 BLE Arduino_ID1841/src

    Supported BLE FOB Buttons (lost key finder device):
    1. iTAG. Service UUID 0xffe0. Teardrop shaped enclosure, 52mm long.
    2. TrackerPA. Service UUID 0xfff0. Square enclosure, 38mm x 38mm.

 */

#include <Arduino.h>
#include <string.h>
#include <BLEDevice.h>
#include "PulseWelder.h"
#include "XT_DAC_Audio.h"

// Global Audio Generation
extern XT_DAC_Audio_Class  DacAudio;
extern XT_MusicScore_Class highBeep;
extern XT_MusicScore_Class lowBeep;

// Extern Globals
extern int  Amps;         // Measured Welding Amps.
extern bool bleConnected; // Bluetooth connected flag.
extern byte bleSwitch;    // Menu Switch, Bluetooth On/Off.
extern int  buttonClick;  // Bluetooth FOB Button click type, single or double click.

// BLE FOB Button Type (iTAG or TrackerPA) declarations.
// FOB Type is set in config.h.
#if FOB_TYPE == iTAG_FOB                             // BLE Configured for iTAG FOB
unsigned int   bleType = iTAG_FOB;
static BLEUUID serviceUUID(BLEUUID("ffe0"));         // iTAG FOB Button Service UUID.
static BLEUUID charUUID1(BLEUUID((uint16_t)0xffe1)); // iTAG FOB Button Characteristic UUID.
#elif  FOB_TYPE == TrackerPA_FOB                     // BLE Configured for trackerPA FOB
unsigned int   bleType = TrackerPA_FOB;
static BLEUUID serviceUUID(BLEUUID("fff0"));         // TrackerPA FOB Button Service UUID.
static BLEUUID charUUID1(BLEUUID((uint16_t)0xfff1)); // TrackerPA FOB Button Characteristic UUID.
#else                                                // Unkown FOB device
 # error "Incorrect FOB_TYPE specified in config.h"
#endif                                               // End of FOB define block

// BLE Device declarations
static BLEAdvertisedDevice *pMyDevice                  = nullptr;
static BLERemoteCharacteristic *pRemoteCharacteristic1 = nullptr;

// static BLERemoteCharacteristic *pRemoteCharacteristic2 = nullptr;
static BLEAddress fobAddress = BLEAddress((uint8_t *)"\0\0\0\0\0\0");
static BLEClient *pClient    = nullptr;

// Local Scope Function vars
static bool doConnect      = false;      // Flag that indicates that device connection is allowed.
static bool doScan         = false;      // Flag that indicates that Service scan is allowed.
static bool newFobClick    = false;      // Flag that indicates that FOB Button pressed.
static int  fobClick       = CLICK_NONE; // FOB Button click (press) type.
static int  reconnectCount = 0;          // Counter for number of automatic reconnects.

// *********************************************************************************************
static void notifyCallback(
  BLERemoteCharacteristic *pBLERemoteCharacteristic,
  uint8_t                 *pData,
  size_t                   length,
  bool                     isNotify)
{
  bool validPress          = false;
  unsigned int byteCount   = 0;
  static long  clickMillis = 0;

  // Verbose messages are useful for developing code to new button devices.
  if (isNotify) {
    Serial.print("BLE Notify ");
  }
  Serial.print("Callback for Characteristic ");

  if (pBLERemoteCharacteristic != nullptr) {
    Serial.print(String(pBLERemoteCharacteristic->getUUID().toString().c_str()) + ", ");
  }

  Serial.print("len " + String(length) + ", "); // Debug

  if (length == 0) {
    validPress = false;
    Serial.println("Missing Notify Value!");              // Debug.
  }
  else if ((length == 1) && (bleType == iTAG_FOB)) {      // iTAG fob.
    validPress = true;
    Serial.print("iTAG Button Press Notify");             // Debug.
  }
  else if ((length == 6) && (bleType == TrackerPA_FOB)) { // trackerPA fob, button press.
    validPress = true;
    Serial.print("TrackerPA Button Press Notify");        // Debug.
  }
  else if ((length == 7) && (bleType == TrackerPA_FOB)) { // trackerPA fob, 2-min timed broadcast (battery voltage?).
    validPress = false;
    Serial.print("TrackerPA Auto Notify");                // Debug.
  }
  else {
    validPress = false;
    Serial.print("Unexpected Notify"); // Debug.
  }

  // Print the notification value (if available).
  if (length > 0) {
    byteCount = 0;
    Serial.print(", Value: ");

    while (byteCount < length) {
      Serial.print(String(pData[byteCount++]));
    }
    Serial.println();
  }

  Serial.println("BLE FreeHeap: " + String(ESP.getFreeHeap()) + " bytes"); // Monitor the Mem Leaks.

  if (validPress && !newFobClick && (millis() >= clickMillis + DOUBLE_CLICK_TIME)) {
    newFobClick = true;
    fobClick    = CLICK_SINGLE;
  }
  else if (validPress && (millis() < clickMillis + DOUBLE_CLICK_TIME)) {
    // Serial.println("Double Click Event.");
    if (fobClick == CLICK_SINGLE) {
      fobClick = CLICK_DOUBLE;
    }
  }

  clickMillis = millis();
}

// *********************************************************************************************
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient *pclient)
  {}


  // *********************************************************************************************
  void onDisconnect(BLEClient *pclient)
  {
    bleConnected = false;
    Serial.println("BlueTooth Lost Connection (onDisconnect)");
  }
};

// *********************************************************************************************
// Connect to the BLE Server.
// This funtion is used after Scan has found the Bluetooth FOB Button. It is a blocking call.
// On client restorations a Memory leak will occur, approx 235 bytes.
bool connectToServer()
{
  bool connected                       = false;
  static MyClientCallback *my_callback = new MyClientCallback(); // Fix mem leaks? Nope, no help.

  Serial.print("Forming a connection to ");
  Serial.println(pMyDevice->getAddress().toString().c_str());

  if (pClient == nullptr) { // First connect.
    pClient = BLEDevice::createClient();

    if (pClient == nullptr) {
      Serial.println(" - Client Creation Failed");
      return false;
    }
    Serial.println(" - Created Client");

    // pClient->setClientCallbacks(new MyClientCallback());
    pClient->setClientCallbacks(my_callback);
  }
  else {
    Serial.println(" - Restored Client");
  }

  // Connect to the remote BLE Server.
  connected = pClient->connect(pMyDevice); // Blocking call.
  // connected = pClient->connect(fobAddress, BLE_ADDR_TYPE_PUBLIC); // Blocking call.

  if (!connected) {
    Serial.println(" - Connection to FOB Button Server Failed");
    return false;
  }

  Serial.println(" - Connected to FOB Button Server");

  // Obtain a reference to the desired service in the remote BLE server.
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);

  if (pRemoteService == nullptr) {
    Serial.println(" - Did Not Find FOB Button Service UUID: " + String(serviceUUID.toString().c_str()));
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found FOB Button Service UUID");

  // Obtain a reference to the main characteristic of the server (BLE FOB).
  if (pClient->isConnected()) {
    pRemoteCharacteristic1 = pRemoteService->getCharacteristic(charUUID1);

    if (pRemoteCharacteristic1 == nullptr) {
      Serial.print("Did Not Find FOB Characteristic UUID: ");
      Serial.print(charUUID1.toString().c_str());
      Serial.println(". Disconnect, abort.");
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found FOB Characteristic UUID");
    bleConnected = true;

    /*
       // Read the value of the characteristic.
       if (pRemoteCharacteristic1->canRead())
       {
        std::string value = pRemoteCharacteristic1->readValue();
        Serial.print("The characteristic value was: ");
        byte val = (byte)(value[0]);
        Serial.println(String(val));
        //          Serial.println((value.c_str()));
       }
     */
    if (pRemoteCharacteristic1->canNotify()) {
      pRemoteCharacteristic1->registerForNotify(notifyCallback);
    }

    /*
       // Obtain a reference to the second characteristic in the service of the remote BLE server.
       pRemoteCharacteristic2 = pRemoteService->getCharacteristic(charUUID2);
       if (pRemoteCharacteristic2 == nullptr)
       {
        Serial.print("Failed to find the Alert UUID: ");
        Serial.println(charUUID2.toString().c_str());
       }
       else
       {
        Serial.println(" - Found the Alert Characteristic");
       }

       //    if (pRemoteCharacteristic2->canNotify())
       //        pRemoteCharacteristic2->registerForNotify(notifyCallback2);
     */
  }
  else {
    Serial.println("BLE FreeHeap After Unsuccessful Server Connection: " + String(ESP.getFreeHeap()) + " bytes.");
    return false;
  }

  Serial.println("BLE FreeHeap After Server Connection: " + String(ESP.getFreeHeap()) + " bytes."); // Monitor the mem leaks.
  return true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice)   // Called for each advertising BLE server.
  {
    Serial.println("BLE Advertised Device found: ");
    Serial.print(" -> " + String(advertisedDevice.toString().c_str()));
    Serial.println(", RSSI " + String(advertisedDevice.getRSSI()));

    Serial.print(" -> Require Serv UUID: " + String(serviceUUID.toString().c_str()));

    if (advertisedDevice.haveServiceUUID()) {
      Serial.println(", Detected Serv UUID: " + String(advertisedDevice.getServiceUUID().toString().c_str()));
    }
    else {
      Serial.println(", Serv UUID Not found.");
    }

/*
    // We have found a device. Check for the name we are looking for.
    if (advertisedDevice.haveName() && advertisedDevice.getName() == std::string("iTAG            ")) {
        Serial.println(" -> Found a matching Advertised Name.");
        BLEDevice::getScan()->stop();
        pMyDevice = new BLEAdvertisedDevice(advertisedDevice);
        fobAddress = pMyDevice->getAddress(); // This address could be saved in Flash (future feature).
        doConnect = true;
        doScan = true;
    }
    else {
      Serial.println(" -> Advertised Name does NOT match.");
    }
*/
    // We have found a device. Check for the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      Serial.println(" -> Found a matching Advertised Service.");
      BLEDevice::getScan()->stop();
      pMyDevice  = new BLEAdvertisedDevice(advertisedDevice);
      fobAddress = pMyDevice->getAddress(); // This address could be saved in Flash (future feature).
      doConnect  = true;
      doScan     = true;
    }
    else {
      Serial.println(" -> Advertised Service does NOT match.");
    }
  } // end of onResult().
}; // End of class


// *********************************************************************************************
// Initialize Bluetooth Low Energy Communications and scan for the remote server (BLE FOB).
// Return true if successful.
void scanBlueTooth(void)
{
  if (bleSwitch) {           // Bluetooth mode is enabled.
    Serial.println("Scanning for Bluetooth BLE FOB Button.");
    setupBle(BLE_SCAN_TIME); // Scan bluetooth FOB Button for a few seconds.
  }
  else {
    btStop();
    Serial.println("Scan not permitted, Bluetooth Disabled.");
    bleConnected = false;
  }
}

// *********************************************************************************************
void setupBle(unsigned int scanSeconds)
{
  BLEScan *pBLEScan = nullptr; // Singleton object, never release/delete it.

  Serial.println("Starting BLE Client application ...");
  BLEDevice::init("");         // init() has built-in safeguard to ensure it is only initialized once.

// Retrieve a scanned device and set the callback we want to use to be informed when we
// have detected a new device.  Specify that we want active scanning.

  if (pBLEScan == nullptr) {
    Serial.println("Retrieving BLE Scan Object.");
    pBLEScan = BLEDevice::getScan(); // Retrieve the Scan object.

    if (pBLEScan != nullptr) {
      Serial.println("Obtained BLE Scan Object.");
      pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false); // Ignore duplicate callbacks.
      pBLEScan->setInterval(1500);                                                      // Interval Time to scan, mS.
      pBLEScan->setWindow(500);                                                         // Time to to actively scan, mS.
      pBLEScan->setActiveScan(true);                                                    // true=Show scan Responses.
    }
  }

// Start scanning the scan object.
  if (pBLEScan != nullptr) {             // Valid BLE Object is available to scan.
    pBLEScan->start(scanSeconds, false); // Set scan time, clear any previous stored devices.
  }
  else {
    Serial.println("BLE Client Failed, aborted.");
  }
}

// *********************************************************************************************
// Stop Bluetooth Low Energy Communication.
void stopBle(void)
{
  if (isBleServerConnected()) {
    bleConnected   = false;
    reconnectCount = 0;
    Serial.println("Bluetooth Disconnected.");

    if (bleType == TrackerPA_FOB) {
      delay(375); // Workaround for BLEClient::disconnect() bug, Prevent random crash.
    }

    if (pClient->isConnected()) {
      pClient->disconnect();
    }
  }
}

// *********************************************************************************************
// If the Bluetooth FOB Button has been pressed then check for single or double click.
// This function is called from remoteControl(), do not use elsewhere.
void processFobClick(void)
{
  int click               = CLICK_NONE; // Click Event Type (Single or Double Click).
  static bool busyFlag    = false;      // Click being analyzed.
  static long clickMillis = 0;          // Windowed Click Time.

  if (!isBleServerConnected()) {
    return;                             // Nothing to do here. Exit now.
  }

  if (!busyFlag && newFobClick) {       // New click event.
    busyFlag    = true;
    newFobClick = false;
    clickMillis = millis();
  }

  if (busyFlag && (millis() <= (clickMillis + DOUBLE_CLICK_TIME))) {
    click = CLICK_BUSY;  // Still processing click event.
  }
  else if (busyFlag) {
    click    = fobClick; // Get the click type (single or double).
    fobClick = CLICK_NONE;
    busyFlag = false;    // Free up click processor.
  }

  if ((click == CLICK_SINGLE) || (click == CLICK_DOUBLE)) {
    buttonClick = click;
    // Serial.println("Key Click Event: " + String(buttonClick));
  }
}

// *********************************************************************************************
// Check the Auto-Reconnect timer.
// On entry rst = true to reset timer, else allow timer to run.
// On exit, return true if timer has expired.
bool reconnectTimer(bool rst)
{
  bool result                 = false;
  static long reconnectMillis = 0;

  if ((millis() > reconnectMillis + RECONNECT_DLY_TIME))
  {
    result = true;
  }

  if (rst == true) {
    reconnectMillis = millis();
  }

  return result;
}

// *********************************************************************************************
// Check the Bluetooth FOB Button server connection.
// Perform auto-reconnect if a paired connection has been disconnected.
void checkBleConnection(void)
{
  if (bleSwitch == BLE_OFF) { // Bluetooth is turned off, exit.
    return;
  }

  // If the flag "doConnect" is true a BLE Server had been detected. Now Connect to it.
  if (doConnect == true) {
    doConnect = false;  // Pseudo non-reentrant.

    if (connectToServer()) {
      Serial.println("Connected to BLE Server.");
      reconnectTimer(true); // Reset Auto-Reconnect Timer.
    }
    else {
      Serial.println("BLE Server Connection Failed.");
    }
  }

  // We were connected, but the connection has been lost. Try to find it again.
  // Do not attempt reconnect while burning a rod stick because it is blocking code.
  if ((Amps <= MIN_DET_AMPS) && !bleConnected && doScan && reconnectTimer(false) && (reconnectCount < RECONNECT_TRIES))
  {
    if (++reconnectCount < RECONNECT_TRIES) {
      Serial.println("Attempting BLE Auto-Reconnect #" + String(reconnectCount) + " (of " + String(RECONNECT_TRIES) + ") ...");
    }
    else {
      Serial.println("Final Attempted BLE Auto-Reconnect (#10) ...");
    }

    reconnectBlueTooth(BLE_RESCAN_TIME); // This is a blocking call, use short timeout value.
    reconnectTimer(true);                // Reset Auto-Reconnect Timer.
  }
  else if (isBleServerConnected())
  {
    reconnectCount = 0;
    reconnectTimer(true); // Reset Auto-Reconnect Timer.
  }
}

// *********************************************************************************************
bool isBleServerConnected(void)
{
  return bleConnected;
}

// *********************************************************************************************
bool isBleDoScan(void)
{
  return doScan;
}

// *********************************************************************************************
void reconnectBlueTooth(int secs)
{
  BLEDevice::getScan()->start(secs);
}

// EOF