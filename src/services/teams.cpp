#include "services/teams.h"

#include <cctype>
#include <cstring>

namespace services::teams {

namespace {

// Canonical list — one entry per nation, sorted by display name for the
// dropdown. Covers likely WC 2026 participants; extend freely.
constexpr TeamInfo kTeams[] = {
    {"Argentina", "ARG"},   {"Australia", "AUS"},    {"Austria", "AUT"},
    {"Belgium", "BEL"},     {"Brazil", "BRA"},       {"Cameroon", "CMR"},
    {"Canada", "CAN"},      {"Cape Verde", "CPV"},   {"Chile", "CHI"},
    {"Colombia", "COL"},    {"Costa Rica", "CRC"},   {"Croatia", "CRO"},
    {"Denmark", "DEN"},     {"Ecuador", "ECU"},      {"Egypt", "EGY"},
    {"England", "ENG"},     {"France", "FRA"},       {"Germany", "GER"},
    {"Ghana", "GHA"},       {"Iran", "IRN"},         {"Italy", "ITA"},
    {"Ivory Coast", "CIV"}, {"Japan", "JPN"},        {"Mexico", "MEX"},
    {"Morocco", "MAR"},     {"Netherlands", "NED"},  {"New Zealand", "NZL"},
    {"Nigeria", "NGA"},     {"Norway", "NOR"},       {"Panama", "PAN"},
    {"Paraguay", "PAR"},    {"Peru", "PER"},         {"Poland", "POL"},
    {"Portugal", "POR"},    {"Qatar", "QAT"},        {"Saudi Arabia", "KSA"},
    {"Scotland", "SCO"},    {"Senegal", "SEN"},      {"Serbia", "SRB"},
    {"South Korea", "KOR"}, {"Spain", "ESP"},        {"Sweden", "SWE"},
    {"Switzerland", "SUI"}, {"Tunisia", "TUN"},      {"Ukraine", "UKR"},
    {"United States", "USA"}, {"Uruguay", "URU"},    {"Wales", "WAL"},
};

// Alternate names some feeds use -> code (not shown in the dropdown).
// football-data.org names confirmed from live payload are marked (fd).
constexpr TeamInfo kAliases[] = {
    {"Cape Verde Islands", "CPV"},  // fd
    {"Cabo Verde", "CPV"},          {"Korea Republic", "KOR"},
    {"USA", "USA"},                 {"IR Iran", "IRN"},
    {"Côte d'Ivoire", "CIV"},       {"Czechia", "CZE"},
};

}  // namespace

const TeamInfo* list() { return kTeams; }
size_t count() { return sizeof(kTeams) / sizeof(kTeams[0]); }

void codeForName(const char* name, char out[4]) {
  if (name == nullptr) { out[0] = '\0'; return; }

  for (const TeamInfo& t : kTeams) {
    if (strcasecmp(t.name, name) == 0) {
      memcpy(out, t.code, 4);
      return;
    }
  }
  for (const TeamInfo& t : kAliases) {
    if (strcasecmp(t.name, name) == 0) {
      memcpy(out, t.code, 4);
      return;
    }
  }
  // Fallback: first three alpha chars, uppercased.
  int n = 0;
  for (const char* p = name; *p != '\0' && n < 3; ++p) {
    if (std::isalpha(static_cast<unsigned char>(*p))) {
      out[n++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    }
  }
  out[n] = '\0';
}

}  // namespace services::teams
