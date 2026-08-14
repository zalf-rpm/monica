/* Differential-test driver for the Odin jsonx port.
 *
 * Reads each JSON file named on the command line exactly the way MONICA does
 * (Tools::readAndParseJsonFile -> Tools::readFile + json11::Json::parse), dumps
 * the parsed value back out with json11's own dump(), and prints one line per
 * file:
 *
 *     <path><TAB><ok|ERR><TAB><dump>
 *
 * The Odin side (odin/tests/json_ref/) does the same through support/jsonx and
 * the two are diffed. This exercises, on real parameter data:
 *   - readFile's newline-stripping concatenation
 *   - number classification (json11's JsonInt vs JsonDouble)
 *   - "%.17g" double formatting
 *   - sorted object key order (json11's object is a std::map)
 *   - string escaping
 *
 * See odin/tests/cpp_ref/run_json.sh.
 */

#include <cstdio>
#include <string>

#include "json11/json11-helper.h"

using namespace Tools;
using namespace json11;

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);

  for (int i = 1; i < argc; i++) {
    std::string path = argv[i];
    auto r = readAndParseJsonFile(path);
    // normalise the path separator so the two sides agree
    std::string norm;
    for (char c : path) norm += (c == '\\' ? '/' : c);

    if (r.failure()) {
      printf("%s\tERR\t\n", norm.c_str());
    } else {
      printf("%s\tok\t%s\n", norm.c_str(), r.result.dump().c_str());
    }
  }
  return 0;
}
