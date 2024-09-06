# Note this code is not terminated at 100% , i need to do somethings

# ThreadedLoggerForCPP
A separate thread logger and writter to log file for c++ projects


how to include this library int your project :

1 : Download the latest Release from Github

2 : copy and paste the whole code into YOUR_LIBS_FOLDER 

3 : Add these lines into your CMakeLists.txt

set(LIBRAIRIES_DIR "${CMAKE_SOURCE_DIR}/YOUR_LIBS_FOLDER")

set(ALL_INCLUDE_DIR "${LIBRAIRIES_DIR}/ThreadedLoggerForCPP-VersionX.X/include")

target_include_directories(${PROJECT_NAME} PUBLIC ${ALL_INCLUDE_DIR})


4 : See main.cpp in src/ to see code example to how to use in .cpp files

5 : Make a global for the LoggerInstance that will be used in these calls by example : LoggerGlobals::YourGlobalLoggerInstance.ExitLoggerThread(); LoggerGlobals::YourGlobalLoggerInstance.logMessageAsync etc....
(see CreateGlobalsLoggerInstanceExample.h ,CreateGlobalsLoggerInstanceExample.cpp and main.cpp in src/)
or use the GetLoggerThread if you don't want to use Global.

6 : now it should be okay to use the threaded logger
