// standalone PP/SR calculator for .osu files
#include "DiffCalcTool.h"

#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "ModFlags.h"
#include "File.h"
#include "SString.h"
#include "Parsing.h"

#include <iostream>
#include <string>

namespace {  // static

struct BeatmapSettings {
    float AR = 5.0f;
    float CS = 5.0f;
    float OD = 5.0f;
    float HP = 5.0f;
};

static BeatmapSettings parseDifficultySettings(std::string_view osuFilePath) {
    BeatmapSettings settings;

    File file(osuFilePath);
    if(!file.canRead() || (file.getFileSize() == 0)) {
        std::cerr << "warning: could not read file for difficulty parsing, using defaults\n";
        return settings;
    }

    bool foundAR = false;
    bool inDifficulty = false;

    for(auto line = file.readLine(); file.canRead(); line = file.readLine()) {
        SString::trim_inplace(line);

        if(line.empty() || line.starts_with("//")) continue;

        if(line.contains("[Difficulty]")) {
            inDifficulty = true;
            continue;
        }

        if(line.starts_with("[") && inDifficulty) {
            break;
        }

        if(inDifficulty) {
            Parsing::parse(line, "CircleSize", ':', &settings.CS);
            if(Parsing::parse(line, "ApproachRate", ':', &settings.AR)) {
                foundAR = true;
            }
            Parsing::parse(line, "HPDrainRate", ':', &settings.HP);
            Parsing::parse(line, "OverallDifficulty", ':', &settings.OD);
        }
    }

    // old beatmaps: AR = OD
    if(!foundAR) {
        settings.AR = settings.OD;
    }

    return settings;
}

}  // namespace

int NEOSU_run_diffcalc(int argc, char* argv[]) {
    if(argc < 3) {
        std::cerr << "usage: " << argv[0] << "-diffcalc <osu_file>\n";
        return 1;
    }

    std::string osuFilePath = argv[2];

    // parse difficulty settings from file
    auto settings = parseDifficultySettings(osuFilePath);

    // load primitive hitobjects
    auto primitives = DatabaseBeatmap::loadPrimitiveObjects(osuFilePath);
    if(primitives.errorCode != 0) {
        std::cerr << "error loading beatmap primitives: code " << primitives.errorCode << '\n';
        return 1;
    }

    // load difficulty hitobjects for star calculation
    float speedMultiplier = 1.0f;
    auto diffResult =
        DatabaseBeatmap::loadDifficultyHitObjects(primitives, settings.AR, settings.CS, speedMultiplier, false);

    if(diffResult.errorCode != 0) {
        std::cerr << "error loading difficulty objects: code " << diffResult.errorCode << '\n';
        return 1;
    }

    // calculate star rating
    f64 aim = 0.0;
    f64 aimSliderFactor = 0.0;
    f64 speed = 0.0;
    f64 aimDifficultSliders = 0.0;
    f64 difficultAimStrains = 0.0;
    f64 speedNotes = 0.0;
    f64 difficultSpeedStrains = 0.0;

    DifficultyCalculator::StarCalcParams starParams{
        .cachedDiffObjects = {},
        .sortedHitObjects = diffResult.diffobjects,
        .CS = settings.CS,
        .OD = settings.OD,
        .speedMultiplier = speedMultiplier,
        .relax = false,
        .touchDevice = false,
        .aim = &aim,
        .aimSliderFactor = &aimSliderFactor,
        .aimDifficultSliders = &aimDifficultSliders,
        .difficultAimStrains = &difficultAimStrains,
        .speed = &speed,
        .speedNotes = &speedNotes,
        .difficultSpeedStrains = &difficultSpeedStrains,
        .upToObjectIndex = -1,
        .incremental = nullptr,
        .outAimStrains = nullptr,
        .outSpeedStrains = nullptr,
        .cancelCheck = nullptr,
    };

    f64 totalStars = DifficultyCalculator::calculateStarDiffForHitObjects(starParams);

    // calculate PP for SS play
    DifficultyCalculator::PPv2CalcParams ppParams{
        .modFlags = static_cast<ModFlags>(0),
        .speedOverride = speedMultiplier,
        .ar = settings.AR,
        .od = settings.OD,
        .aim = aim,
        .aimSliderFactor = aimSliderFactor,
        .aimDifficultSliders = aimDifficultSliders,
        .aimDifficultStrains = difficultAimStrains,
        .speed = speed,
        .speedNotes = speedNotes,
        .speedDifficultStrains = difficultSpeedStrains,
        .numHitObjects = static_cast<i32>(primitives.numHitobjects),
        .numCircles = static_cast<i32>(primitives.numCircles),
        .numSliders = static_cast<i32>(primitives.numSliders),
        .numSpinners = static_cast<i32>(primitives.numSpinners),
        .maxPossibleCombo = diffResult.maxPossibleCombo,
        .combo = -1,
        .misses = 0,
        .c300 = -1,
        .c100 = 0,
        .c50 = 0,
    };

    f64 pp = DifficultyCalculator::calculatePPv2(ppParams);

    // output results
    std::cout << "star rating: " << totalStars << '\n';
    std::cout << "  aim: " << aim << '\n';
    std::cout << "  speed: " << speed << '\n';
    std::cout << "pp (SS): " << pp << '\n';
    std::cout << '\n';
    std::cout << "map info:\n";
    std::cout << "  AR: " << settings.AR << '\n';
    std::cout << "  CS: " << settings.CS << '\n';
    std::cout << "  OD: " << settings.OD << '\n';
    std::cout << "  HP: " << settings.HP << '\n';
    std::cout << "  objects: " << primitives.numHitobjects << " (" << primitives.numCircles << "c + "
              << primitives.numSliders << "s + " << primitives.numSpinners << "sp)\n";
    std::cout << "  max combo: " << diffResult.maxPossibleCombo << '\n';

    return 0;
}
