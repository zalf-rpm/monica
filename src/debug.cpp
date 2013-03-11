#define LOKI_OBJECT_LEVEL_THREADING

#include "loki/Threads.h"

#include "debug.h"

using namespace Monica;
using namespace std;

bool Monica::activateDebug = false;

namespace
{
  struct L : public Loki::ObjectLevelLockable<L> {};
}

ostream& Monica::debug()
{
  //static L lockable;
  static Debug dummy;

  //L::Lock lock(lockable);

  return activateDebug ? cout : dummy;
}

Debug::~Debug()
{
  DebugBuffer* buf = (DebugBuffer*) rdbuf();
  if (buf)
    delete buf;
}
