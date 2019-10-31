/*
   File: spiffs.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.0
   Creation: Sep-11-2019
   Revised: Oct-24-2019
   Release: Oct-30-2019
   Author: T. Black
   (c) copyright T. Black 2019, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.

   Notes:
    SPIFFS is not used in the welder project. This file is included in case SPIFFS is needed in a custom application.
    The code presented below are examples of using SPIFFS. Please feel free to delete anything not needed in this file.
 */

#include <Arduino.h>
#include "SPIFFS.h"

#define VERBOSE 1 // Set to 1 to Show Verbose debug messages.

// SPIFFS

extern const uint8_t test1_img_start[] asm ("_binary_src_test1_img_start");
extern const uint8_t test1_img_end[] asm ("_binary_src_test1_img_end");
const char *test1_img = (const char *)test1_img_start;

extern const uint8_t test2_bin_start[] asm ("_binary_src_test2_bin_start");
extern const uint8_t test2_bin_end[] asm ("_binary_src_test2_bin_end");
const char *test2_bin = (const char *)test2_bin_start;

// END OF SPIFFS

// Initialize SPIFFS
void spiffsInit(void)
{
  const char *content = "This is the text string that was written to SPIFFS. ";

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS: An Error has occurred while mounting.");
  }
  else {
    Serial.println("SPIFFS: Mounted");
  }

  File file1 = SPIFFS.open("/test.txt",FILE_WRITE);
#if (VERBOSE == 1)

  if (!file1) {
    Serial.println("SPIFFS: There was an error opening the SPIFFS file for writing");
  }
#endif // if (VERBOSE == 1)

  Serial.print("SPIFFS: Content length = ");
  Serial.println(strlen(content));

  if (file1.print(content)) {
#if (VERBOSE == 1)
    Serial.println("SPIFFS: File was written");
#endif // if (VERBOSE == 1)
  }
  else {
#if (VERBOSE == 1)
    Serial.println("SPIFFS: File write failed");
#endif // if (VERBOSE == 1)
  }

  file1.close();

  File file2 = SPIFFS.open("/test.txt",FILE_READ);
#if (VERBOSE == 1)
  if (!file2)
  {
    Serial.println("SPIFFS: Failed to open file test.txt for reading");
  }
  Serial.print("SPIFFS: test.txt File size = ");
  Serial.println(file2.size());
  Serial.print("SPIFFS: Contents of test.txt file = ");

  while (file2.available())
  {
    Serial.write(file2.read()); // Send raw data to serial port.
  }
  Serial.println();
#endif // if (VERBOSE == 1)
  file2.close();

#if (VERBOSE == 1)
  Serial.print("SPIFFS: Test-1 img data = ");
  for (uint16_t k = 0; k < test1_img_end - test1_img_start; k++) {
    Serial.print(*(test1_img + k));
  }
  Serial.println();

  Serial.print("SPIFFS: Test-2 bin data = ");
  for (uint16_t i = 0; i < test2_bin_end - test2_bin_start; i++) {
    Serial.print(*(test2_bin + i));
  }
  Serial.println();

#endif // if (VERBOSE == 1)
}
