/* C++ side of the daily trace dump.
 *
 * Emits the same format as odin/monica/trace/trace.odin:
 *
 *     <day>\t<path.to.field>\t<value>
 *
 * The Odin side walks the model reflectively; the C++ has no reflection, so
 * each struct gets an explicit dump function built from these macros - one line
 * per field. That is tedious but it is also self-policing: the macros stringify
 * the field name with #, and CONVENTIONS §2 requires the Odin fields to keep
 * their exact C++ spelling, so the two sides produce identical paths without a
 * shared manifest. If one side gains or loses a field, the diff shows an
 * added/removed line rather than a silent mismatch.
 *
 * Formatting must match trace.odin exactly:
 *   - doubles via "%.17g"
 *   - ints via "%d", enums cast to int
 *   - bools as "true"/"false"
 *   - a Maybe that is unset prints the literal "unset"
 */

#pragma once

#include <cstdio>
#include <string>

namespace trace {

inline int &current_day() {
  static int day = 0;
  return day;
}

inline void set_day(int d) { current_day() = d; }

inline void line_f64(const std::string &path, double v) {
  printf("%d\t%s\t%.17g\n", current_day(), path.c_str(), v);
}

inline void line_int(const std::string &path, long long v) {
  printf("%d\t%s\t%lld\n", current_day(), path.c_str(), v);
}

inline void line_str(const std::string &path, const std::string &v) {
  printf("%d\t%s\t%s\n", current_day(), path.c_str(), v.c_str());
}

inline void line_bool(const std::string &path, bool v) {
  printf("%d\t%s\t%s\n", current_day(), path.c_str(), v ? "true" : "false");
}

inline std::string join(const std::string &path, const char *name) {
  return path.empty() ? std::string(name) : path + "." + name;
}

inline std::string index(const std::string &path, int i) {
  return path + "[" + std::to_string(i) + "]";
}

} // namespace trace

// One field of the struct currently being dumped. `OBJ` and `PATH` are expected
// to be in scope - see the DUMP_BEGIN helper below.
#define TD(field) trace::line_f64(trace::join(PATH, #field), (double)(OBJ).field)
#define TI(field) trace::line_int(trace::join(PATH, #field), (long long)(OBJ).field)
#define TB(field) trace::line_bool(trace::join(PATH, #field), (OBJ).field)
#define TS(field) trace::line_str(trace::join(PATH, #field), (OBJ).field)
/* an enum member, cast to int exactly as the Odin walk does */
#define TE(field) trace::line_int(trace::join(PATH, #field), (long long)(int)(OBJ).field)

/* A Tools::Maybe<T>/kj::Maybe<T> field. Unset must print "unset", never the
 * zero value - the ff0f0fc regression in plan.md is exactly this distinction. */
#define TM_BOOL(field)                                                                             \
  do {                                                                                             \
    if ((OBJ).field.isValue())                                                                     \
      trace::line_bool(trace::join(PATH, #field), (OBJ).field.value());                            \
    else                                                                                           \
      trace::line_str(trace::join(PATH, #field), "unset");                                         \
  } while (0)

/* A std::vector<double>/<int>/<bool> field, emitted as path[i]. */
#define TD_VEC(field)                                                                              \
  do {                                                                                             \
    for (size_t _i = 0; _i < (OBJ).field.size(); _i++)                                             \
      trace::line_f64(trace::index(trace::join(PATH, #field), (int)_i), (double)(OBJ).field[_i]);   \
  } while (0)
#define TI_VEC(field)                                                                              \
  do {                                                                                             \
    for (size_t _i = 0; _i < (OBJ).field.size(); _i++)                                             \
      trace::line_int(trace::index(trace::join(PATH, #field), (int)_i), (long long)(OBJ).field[_i]); \
  } while (0)
#define TB_VEC(field)                                                                              \
  do {                                                                                             \
    for (size_t _i = 0; _i < (OBJ).field.size(); _i++)                                             \
      trace::line_bool(trace::index(trace::join(PATH, #field), (int)_i), (bool)(OBJ).field[_i]);    \
  } while (0)

/* A raw pointer member. The Odin walk does not follow pointers (the model is a
 * graph; submodules hold back-pointers to SoilColumn) and records only nil vs
 * set, so the C++ must do the same. */
#define TP(field)                                                                                  \
  trace::line_str(trace::join(PATH, #field), (OBJ).field ? "<ptr:set>" : "<ptr:nil>")
