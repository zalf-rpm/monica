// C++ side of the phase 7 checkpoint 3 differential test - exercises
// writeOutputHeaderRows/writeOutput (src/io/csv-format.cpp) directly against
// a handful of hand-built OId/value combinations, without needing a full
// MonicaModel: scalar string/number columns, an organ column, a
// layer-aggregated-NONE range column (3-wide), a layer-aggregated-AVG single
// column, and a couple of string values containing the csv separator and a
// quote, to exercise the escaping paths.
//
// Run via odin/tests/cpp_ref/run_csv_format.sh, which builds this, builds the
// Odin counterpart (odin/tests/csv_format_ref/main.odin), and diffs stdout.
#include <iostream>
#include <vector>

#include "io/csv-format.h"
#include "io/output.h"

using namespace monica;
using namespace json11;
using namespace Tools;
using namespace std;

int main() {
  vector<OId> ids;

  OId dateId;
  dateId.name = "Date";
  dateId.unit = "";
  dateId.jsonInput = "\"Date\"";
  ids.push_back(dateId);

  OId yieldId;
  yieldId.name = "Yield";
  yieldId.unit = "kgDM ha-1";
  yieldId.jsonInput = "\"Yield\"";
  ids.push_back(yieldId);

  OId orgBiomId;
  orgBiomId.name = "OrgBiom";
  orgBiomId.unit = "kgDM ha-1";
  orgBiomId.jsonInput = "[\"OrgBiom\", \"LEAF\"]";
  orgBiomId.organ = OId::LEAF;
  ids.push_back(orgBiomId);

  OId moisRangeId;
  moisRangeId.name = "Mois";
  moisRangeId.unit = "m3 m-3";
  moisRangeId.jsonInput = "[\"Mois\", [1, 3]]";
  moisRangeId.fromLayer = 0;
  moisRangeId.toLayer = 2;
  moisRangeId.layerAggOp = OId::NONE;
  ids.push_back(moisRangeId);

  OId moisAvgId;
  moisAvgId.name = "Mois";
  moisAvgId.unit = "m3 m-3";
  moisAvgId.jsonInput = "[\"Mois\", [1, 3, \"AVG\"]]";
  moisAvgId.fromLayer = 0;
  moisAvgId.toLayer = 2;
  moisAvgId.layerAggOp = OId::AVG;
  ids.push_back(moisAvgId);

  OId cropId;
  cropId.name = "Crop";
  cropId.unit = "";
  cropId.jsonInput = "\"Crop\"";
  cropId.displayName = "Crop, with, commas";
  ids.push_back(cropId);

  cout << "== header, csvSep=,\n";
  writeOutputHeaderRows(cout, ids, ",", true, true, true);

  cout << "== header, csvSep=,, no header/units rows\n";
  writeOutputHeaderRows(cout, ids, ",", false, false, true);

  vector<J11Array> values(ids.size());
  // day 0
  values[0].push_back(Json("2020-09-22"));
  values[1].push_back(Json(1234.5678));
  values[2].push_back(Json(99.99999));
  values[3].push_back(Json(J11Array{Json(0.1234), Json(0.000123456), Json(15000.0)}));
  values[4].push_back(Json(250.333333));
  values[5].push_back(Json("Winter wheat, \"WW\""));
  // day 1
  values[0].push_back(Json("2020-09-23"));
  values[1].push_back(Json(0.0));
  values[2].push_back(Json(-0.0));
  values[3].push_back(Json(J11Array{Json(1e6), Json(1e-5), Json(123456.789)}));
  values[4].push_back(Json(3.14159265));
  values[5].push_back(Json("plain"));
  // day 2
  values[0].push_back(Json("2020-09-24"));
  values[1].push_back(Json(12345678.9));
  values[2].push_back(Json(true));
  values[3].push_back(Json(J11Array{Json(100.0), Json(0.0), Json(1e20)}));
  values[4].push_back(Json(1e-20));
  values[5].push_back(Json(false));

  cout << "== rows, csvSep=,\n";
  writeOutput(cout, ids, values, ",");

  cout << "== rows, csvSep=\\t\n";
  writeOutput(cout, ids, values, "\t");

  return 0;
}
