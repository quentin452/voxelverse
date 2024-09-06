#include "CreateGlobalsLoggerInstanceExample.h"

#include <ThreadedLoggerForCPP/LoggerThread.hpp>
#include <ThreadedLoggerForCPP/LoggerFileSystem.hpp>
#include <ThreadedLoggerForCPP/LoggerGlobals.hpp>

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

std::string _exe_game = "ThreadedLoggerForCPPTest";
int test = 99;

void testLoggingSpeed() {
  const int numIterations = 10000;

  auto startLogAsync = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < numIterations; ++i) {
    CreateGlobalsLoggerInstanceExample::LoggerInstance.logMessageAsync(
        LogLevel::INFO, __FILE__, __LINE__, "logger test");
  }
  auto endLogAsync = std::chrono::high_resolution_clock::now();
  auto durationLogAsync = std::chrono::duration_cast<std::chrono::milliseconds>(
      endLogAsync - startLogAsync);

  auto startStdCout = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < numIterations; ++i) {
    std::cout << "logger test" << std::endl;
  }
  auto endStdCout = std::chrono::high_resolution_clock::now();
  auto durationStdCout = std::chrono::duration_cast<std::chrono::milliseconds>(
      endStdCout - startStdCout);
  std::string durationStdCoutStr =
      "Time taken by std::cout: " + std::to_string(durationStdCout.count()) +
      " milliseconds\n";
  CreateGlobalsLoggerInstanceExample::LoggerInstance.logMessageAsync(
      LogLevel::INFO, __FILE__, __LINE__, durationStdCoutStr);
  std::string durationLogAsyncStr = "Time taken by logMessageAsync: " +
                                    std::to_string(durationLogAsync.count()) +
                                    " milliseconds\n";
  CreateGlobalsLoggerInstanceExample::LoggerInstance.logMessageAsync(
      LogLevel::INFO, __FILE__, __LINE__, durationLogAsyncStr);
}
void testLoggingGetThreadSpeed() {
  const int numIterations = 1000;

  // Test with calling GetLoggerThread each time
  auto start1 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < numIterations; ++i) {
    LoggerThread::GetLoggerThread().logMessageAsync(LogLevel::INFO, __FILE__,
                                                    __LINE__, "Test message with GetLoggerThread");
  }
  auto end1 = std::chrono::high_resolution_clock::now();
  auto duration1 =
      std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);

  // Test with storing the result of GetLoggerThread
  auto &loggerThread = LoggerThread::GetLoggerThread();
  auto start2 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < numIterations; ++i) {
    loggerThread.logMessageAsync(LogLevel::INFO, __FILE__, __LINE__,
                                 "Test message without GetLoggerThread");
  }
  auto end2 = std::chrono::high_resolution_clock::now();
  auto duration2 =
      std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
  std::string test1 = "Time taken with calling GetLoggerThread each time: " +
                      std::to_string(duration1.count()) + " milliseconds\n";
  CreateGlobalsLoggerInstanceExample::LoggerInstance.logMessageAsync(
      LogLevel::INFO, __FILE__, __LINE__, test1);
  std::string test2 =
      "Time taken with storing the result of GetLoggerThread: " +
      std::to_string(duration2.count()) + " milliseconds\n";
  CreateGlobalsLoggerInstanceExample::LoggerInstance.logMessageAsync(
      LogLevel::INFO, __FILE__, __LINE__, test2);
}

void InitThreadedLoggerForCPP(std::string &ProjectDirectory,
                              std::string &LogFileName,
                              std::string &GameSaveFolder) {
#pragma warning(push)
#pragma warning(disable : 4996) // Disable warning for getenv
#ifdef _WIN32
  LoggerGlobals::UsernameDirectory = std::getenv("USERNAME");
#else
  LoggerGlobals::UsernameDirectory = std::getenv("USER");
#endif
#pragma warning(pop)

  // this is the folder that contains your src files like main.cpp
  LoggerGlobals::SrcProjectDirectory = ProjectDirectory;
  // Create Log File and folder
  LoggerGlobals::LogFolderPath = "C:\\Users\\" +
                                 LoggerGlobals::UsernameDirectory + "\\." +
                                 GameSaveFolder + "\\logging\\";
  LoggerGlobals::LogFilePath =
      "C:\\Users\\" + LoggerGlobals::UsernameDirectory + "\\." +
      GameSaveFolder + "\\logging\\" + LogFileName + ".log";
  LoggerGlobals::LogFolderBackupPath =
      "C:\\Users\\" + LoggerGlobals::UsernameDirectory + "\\." +
      GameSaveFolder + "\\logging\\LogBackup";
  LoggerGlobals::LogFileBackupPath =
      "C:\\Users\\" + LoggerGlobals::UsernameDirectory + "\\." +
      GameSaveFolder + "\\logging\\LogBackup\\" + LogFileName + "-";

  CreateGlobalsLoggerInstanceExample::LoggerInstance.StartLoggerThread(
      LoggerGlobals::LogFolderPath, LoggerGlobals::LogFilePath,
      LoggerGlobals::LogFolderBackupPath, LoggerGlobals::LogFileBackupPath);
}
int main(int argc, char *args[]) {
  InitThreadedLoggerForCPP(_exe_game, _exe_game, _exe_game);
  testLoggingSpeed();
  testLoggingGetThreadSpeed();
  CreateGlobalsLoggerInstanceExample::LoggerInstance.ExitLoggerThread();
  return 0;
}
