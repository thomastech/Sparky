![boot_screen1_500](https://user-images.githubusercontent.com/10354989/67133271-e46eeb00-f1c0-11e9-92cb-bf2c53ea3150.jpg)
# SPARKY Source Code

### Introduction
The SPARKY project uses ESP32 Arduino Libraries. Compiling requires VSCode with the Platformio IDE extension and Espressif32 board platform. It is not intended to be compiled with the Arduino IDE. All the software source files are provided so you can make changes.

Please do NOT create a GitHub issue ticket if you have technical questions or seek advice. The place for
that is at the official project discussion area found at the rc-cam.com forum:
https://www.rc-cam.com/forum/index.php?/topic/4605-sparky-a-little-stick-welder-with-big-features/

### config.h
The entries in the config.h file are provide so you can personalize common default settings.

### The Fine Print
This project's software is intended for programmers with moderate or higher coding skills. Beginners should start with a simpler project; There are many Arduino forums and YouTube instructional videos that will help with the learning process.

### Platformio
The project successfully compiles on Visual Studio Code Version 1.41.1 with Platformio IDE 1.10.0, and platform-espressif32
board platform using stable release version 1.11.1 or higher.

### Project Files
All required files are provided in the zip file, including the library dependencies. Unzip the files (do not change directory structure) into your project folder. Then launch VSCode Platformio and open the folder named /src/PulseWelder. An alternative to using the zip file is to clone the project from github.

### Source Code Formatting
All .cpp and .h files have been formatted with the Uncrustify extension. The uncrustify.cfg file is included in the file package.

### Version History
The PulseWelder.cpp source file contains a summary of the changes for each release. See the "Revision History" notes at the top of the file.

