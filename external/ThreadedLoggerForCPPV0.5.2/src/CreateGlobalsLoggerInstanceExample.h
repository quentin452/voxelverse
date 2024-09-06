#ifndef CreateGlobalsLoggerInstanceExample_H
#define CreateGlobalsLoggerInstanceExample_H
#include <ThreadedLoggerForCPP/LoggerThread.hpp>
#include <thread>

#include <string>
class CreateGlobalsLoggerInstanceExample {
public:
  static LoggerThread LoggerInstance;
};
#endif // CreateGlobalsLoggerInstanceExample_H