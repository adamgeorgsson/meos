#include "meos_version.h"

#include <string>
#include <vector>

int getMeosBuild() {
    // Revision embedded at build time; kept as a fixed offset like the legacy code.
    return 174 + 1568;  // matches legacy: 174 + atoi("$Rev: 1568 $".substr(5))
}

std::wstring getMajorVersion() {
    return L"5.0";
}

static std::wstring getBuildType() {
    return L"Beta 1";
}

std::wstring getMeosDate() {
    return L"2026-02-19";
}

std::wstring getMeosFullVersion() {
    std::wstring maj = getMajorVersion();
    std::wstring build = std::to_wstring(getMeosBuild());
    std::wstring date = getMeosDate();
    std::wstring bt = getBuildType();

#if defined(__x86_64__) || defined(_WIN64)
    const wchar_t* bits = L"64-bit";
#else
    const wchar_t* bits = L"32-bit";
#endif

    std::wstring result = L"Version X#" + maj + L"." + build + L" (" + bits + L"), " + date;
    if (!bt.empty())
        result += L", " + bt;
    return result;
}

std::wstring getMeosCompectVersion() {
    std::wstring build = std::to_wstring(getMeosBuild());
    std::wstring bt = getBuildType();
    if (bt.empty())
        return getMajorVersion() + L"." + build;
    return getMajorVersion() + L"." + build + L" (" + bt + L")";
}

void getSupporters(std::vector<std::wstring>& supp, std::vector<std::wstring>& developSupp) {
    supp.emplace_back(L"Zdenko Rohac, KOB ATU Košice");
    supp.emplace_back(L"Hans Carlstedt, Sävedalens AIK");
    supp.emplace_back(L"O-Liceo, Spain");
    developSupp.emplace_back(L"Västerviks OK");
    supp.emplace_back(L"Aarhus 1900 Orientering");
    supp.emplace_back(L"Ljusne Ala OK");
    supp.emplace_back(L"Sävedalens AIK");
    supp.emplace_back(L"Foothills Wanderers Orienteering Club");
    supp.emplace_back(L"OK Gripen");
    supp.emplace_back(L"Per Ågren, OK Enen");
    supp.emplace_back(L"OK Roslagen");
    supp.emplace_back(L"OK Kolmården");
    developSupp.emplace_back(L"Orienteering Queensland Inc.");
    supp.emplace_back(L"Eksjö SOK");
    supp.emplace_back(L"Kolding OK");
    developSupp.emplace_back(L"Alfta-Ösa OK");
    supp.emplace_back(L"Erik Almséus, IFK Hedemora OK");
    supp.emplace_back(L"IK Gandvik, Skara");
    supp.emplace_back(L"Mats Kågeson");
    supp.emplace_back(L"Lerums SOK");
    supp.emplace_back(L"OSC Hamburg");
    developSupp.emplace_back(L"IFK Mora OK");
    supp.emplace_back(L"Big Foot Orienteers");
    developSupp.emplace_back(L"OK Måsen");
    supp.emplace_back(L"Kamil Pipek, OK Lokomotiva Pardubice");
    supp.emplace_back(L"Foothills Wanderers Orienteering Club");
    supp.emplace_back(L"Per Eklöf / PE Design / PE Timing");
    supp.emplace_back(L"Kvarnsvedens GOIF OK");
    supp.emplace_back(L"Ingemar Lindström, OK Österåker");
    supp.emplace_back(L"OK Österåker");
    supp.emplace_back(L"Guntars Mankus, OK Saldus");
    supp.emplace_back(L"Orienteering NSW");
    developSupp.emplace_back(L"OK Enen");
    supp.emplace_back(L"Hästveda OK");
    supp.emplace_back(L"Ingemar Carlsson, Sävedalens AIK");
    supp.emplace_back(L"Lunds OK");
    supp.emplace_back(L"Ramblers Orienteering Club, Canada");
    supp.emplace_back(L"CROCO");
    supp.emplace_back(L"Nässjö OK");
    supp.emplace_back(L"IK Uven");
    supp.emplace_back(L"Attunda OK");
    supp.emplace_back(L"Gunnar Svanberg");
    supp.emplace_back(L"Forsa OK");
    supp.emplace_back(L"Långhundra IF");
    supp.emplace_back(L"Mariestads friluftsklubb");
    supp.emplace_back(L"SV Robotron Dresden");
    supp.emplace_back(L"Mats Holmberg, OK Gränsen");
    supp.emplace_back(L"Milen Marinov");
    supp.emplace_back(L"Miroslav Kollar, KOB Kysak");
    developSupp.emplace_back(L"FIF Hillerød Orientering");
    supp.emplace_back(L"Järla Orientering");
    supp.emplace_back(L"Stein Östby, Malmö OK");
    supp.emplace_back(L"Eric Teutsch (o-store.ca)");
    supp.emplace_back(L"Sportegyesület Hód-Mentor");
    developSupp.emplace_back(L"Täby OK");
    developSupp.emplace_back(L"Skogsluffarnas OK");
    developSupp.emplace_back(L"FK Friskus-Varberg");
    supp.emplace_back(L"Hagaby GoIF");
    supp.emplace_back(L"Waxholms OK");
    supp.emplace_back(L"Mariager Fjord OK");
    supp.emplace_back(L"David Ek, FK Göingarna");
    supp.emplace_back(L"OK73");
    supp.emplace_back(L"Ligue PACA");
    developSupp.emplace_back(L"Sävedalens AIK");
    supp.emplace_back(L"Kamil Pipek, OK Lokomotiva Pardubice");
    supp.emplace_back(L"Autidó");
    supp.emplace_back(L"Tjalve OK");
    supp.emplace_back(L"TV Jahn Wolfsburg");
    developSupp.emplace_back(L"Malmö OK");
    supp.emplace_back(L"Söderhamns OK");
    supp.emplace_back(L"Järla Orientering");
    supp.emplace_back(L"Enebybergs IF");
    supp.emplace_back(L"IK Vikings OK");
    supp.emplace_back(L"Naturfreunde Wien Orienteering");
    supp.emplace_back(L"HEYRIES / ACA Aix en Provence");
    supp.emplace_back(L"Allerød OK");
    supp.emplace_back(L"IF Thor");
    supp.emplace_back(L"OK Rodhen");
    supp.emplace_back(L"OK Tyr, Karlstad");
    supp.emplace_back(L"Nordvest OK");
    developSupp.emplace_back(L"Northeastern Ohio Orienteering Club");
    supp.emplace_back(L"Hjobygdens OK");
    developSupp.emplace_back(L"Bayside Kangaroos Orienteering Club");
    supp.emplace_back(L"OK Skogsfalken");
    supp.emplace_back(L"Hód-Mentor Sportegyesület");
    supp.emplace_back(L"Solna Orienteringsklubb");
    supp.emplace_back(L"SOL Tranås");
    supp.emplace_back(L"Orienteering Ottawa");
    supp.emplace_back(L"ACA AIX en Provence");
    supp.emplace_back(L"Silkeborg Orienteringsklub");
    developSupp.emplace_back(L"KOB Sokol Pezinok");
    supp.emplace_back(L"Ligue PACA");
    supp.emplace_back(L"Nässjö OK");
    supp.emplace_back(L"Tormestorps IF");
}
