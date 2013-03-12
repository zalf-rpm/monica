/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) 2007-2013, Leibniz Centre for Agricultural Landscape Research (ZALF)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
