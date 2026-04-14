#include "globals.h"

String indexPage(int strikes, bool isFailed, std::vector<PuzzleInfo> puzzles) {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3'>";
  html += "<title>Main board</title>";
  html += "</head>";

  html += "<body>";
  html += "<h1> Strikes: " + String(strikes) + "</h1>";

  // list the puzzles
  if (puzzles.size() > 0) {
    html += "<h1>Puzzles</h1>";
    for (int i = 0; i < puzzles.size(); i++) 
      html += "<h1>" + puzzles[i].ID + "</h1>";
  }

  if (isFailed) html += "<h1>Players failed.</h1>";

  html += "</body>";
  html += "</html>";

  return html;
}