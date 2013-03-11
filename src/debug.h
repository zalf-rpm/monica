#ifndef MONICA_DEBUG_H_
#define MONICA_DEBUG_H_

/**
 * @file debug.h
 * @author Xenia Specka, Michael Berg
 */

#include <iostream>
#include <streambuf>
#include <ostream>

namespace Monica
{
  std::ostream& debug();
  //global flag to activate debug function
  extern bool activateDebug;

  class DebugBuffer : public std::streambuf
  {
  public:
    DebugBuffer(int=5000){}
    virtual ~DebugBuffer(){}

  protected:
    int_type overflow(int_type){ return 0; }
    int_type sync(){ return 0; }
  };

  class Debug : public std::ostream
  {
  public:
    Debug() : std::ostream(new DebugBuffer()) {}
    Debug(int i) : std::ostream(new DebugBuffer(i)) {}
    ~Debug();
  };
}

#endif /* DEBUG_H_ */
