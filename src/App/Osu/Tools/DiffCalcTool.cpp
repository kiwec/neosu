// standalone PP/SR calculator for .osu files
#include "DiffCalcTool.h"

#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "ModFlags.h"
#include "File.h"
#include "SString.h"
#include "Parsing.h"

#include <iostream>
#include <charconv>
#include <string>

namespace {  // static

struct BeatmapSettings {
    float AR = 5.0f;
    float CS = 5.0f;
    float OD = 5.0f;
    float HP = 5.0f;
};

BeatmapSettings parseDifficultySettings(std::string_view osuFilePath) {
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

        if(line.starts_with('[') && inDifficulty) {
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

std::string modsStringFromMods(ModFlags mods, float speed) {
    using enum ModFlags;

    std::string modsString;

    // only for exact values
    const bool nc = speed == 1.5f && flags::has<NoPitchCorrection>(mods);
    const bool dt = speed == 1.5f && !nc;  // only show dt/nc, not both
    const bool ht = speed == 0.75f;

    if(flags::has<NoFail>(mods)) modsString.append("NF,");
    if(flags::has<Easy>(mods)) modsString.append("EZ,");
    if(flags::has<TouchDevice>(mods)) modsString.append("TD,");
    if(flags::has<Hidden>(mods)) modsString.append("HD,");
    if(flags::has<HardRock>(mods)) modsString.append("HR,");
    if(flags::has<SuddenDeath>(mods)) modsString.append("SD,");
    if(dt) modsString.append("DT,");
    if(nc) modsString.append("NC,");
    if(flags::has<Relax>(mods)) modsString.append("Relax,");
    if(ht) modsString.append("HT,");
    if(flags::has<Flashlight>(mods)) modsString.append("FL,");
    if(flags::has<SpunOut>(mods)) modsString.append("SO,");
    if(flags::has<Autopilot>(mods)) modsString.append("AP,");
    if(flags::has<Perfect>(mods)) modsString.append("PF,");
    if(flags::has<ScoreV2>(mods)) modsString.append("v2,");
    if(flags::has<Target>(mods)) modsString.append("Target,");
    if(flags::has<Nightmare>(mods)) modsString.append("Nightmare,");
    if(flags::any<MirrorHorizontal | MirrorVertical>(mods)) modsString.append("Mirror,");
    if(flags::has<FPoSu>(mods)) modsString.append("FPoSu,");
    if(flags::has<Singletap>(mods)) modsString.append("1K,");
    if(flags::has<NoKeylock>(mods)) modsString.append("4K,");

    if(modsString.length() > 0) modsString.pop_back();  // remove trailing comma

    return modsString;
}

}  // namespace

int NEOSU_run_diffcalc(int argc, char* argv[]) {
    if(argc < 3) {
        std::cerr << "usage: " << argv[0] << "-diffcalc <osu_file> [speed] [mod flags bitmask (0xHEX)]\n";
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

    float speedMultiplier = 1.0f;
    if(argc > 3) {
        std::string_view cur{argv[3]};
        float speedTemp = 1.f;
        auto [ptr, ec] = std::from_chars(cur.data(), cur.data() + cur.size(), speedTemp);
        if(ec == std::errc() && speedTemp >= 0.01f && speedTemp <= 3.f) speedMultiplier = speedTemp;
    }

    ModFlags modFlags = {};
    if(argc > 4) {
        int base = 10;
        ModFlags flagsTemp = {};
        std::string_view cur{argv[4]};
        if(cur.starts_with("0x")) {
            base = 16;
            cur = cur.substr(2);
        }
        auto [ptr, ec] = std::from_chars(cur.data(), cur.data() + cur.size(), (u64&)flagsTemp, base);
        if(ec == std::errc()) modFlags = flagsTemp;
    }

    // load difficulty hitobjects for star calculation
    auto diffResult =
        DatabaseBeatmap::loadDifficultyHitObjects(primitives, settings.AR, settings.CS, speedMultiplier, false);

    if(diffResult.errorCode != 0) {
        std::cerr << "error loading difficulty objects: code " << diffResult.errorCode << '\n';
        return 1;
    }

    // calculate star rating
    DifficultyCalculator::BeatmapDiffcalcData diffcalcData{.sortedHitObjects = diffResult.diffobjects,
                                                           .CS = settings.CS,
                                                           .HP = settings.HP,
                                                           .AR = settings.AR,
                                                           .OD = settings.OD,
                                                           .hidden = flags::has<ModFlags::Hidden>(modFlags),
                                                           .relax = flags::has<ModFlags::Relax>(modFlags),
                                                           .autopilot = flags::has<ModFlags::Autopilot>(modFlags),
                                                           .touchDevice = flags::has<ModFlags::TouchDevice>(modFlags),
                                                           .speedMultiplier = speedMultiplier,
                                                           .breakDuration = diffResult.totalBreakDuration,
                                                           .playableLength = diffResult.playableLength};

    DifficultyCalculator::DifficultyAttributes outAttrs{};

    DifficultyCalculator::StarCalcParams starParams{
        .cachedDiffObjects = {},
        .outAttributes = outAttrs,
        .beatmapData = diffcalcData,
        .outAimStrains = nullptr,
        .outSpeedStrains = nullptr,
        .incremental = nullptr,
        .upToObjectIndex = -1,
        .cancelCheck = nullptr,
    };

    f64 totalStars = DifficultyCalculator::calculateStarDiffForHitObjects(starParams);

    f64 aim = outAttrs.AimDifficulty;
    f64 speed = outAttrs.SpeedDifficulty;

    // calculate PP for SS play
    DifficultyCalculator::PPv2CalcParams ppParams{.attributes = outAttrs,
                                                  .modFlags = modFlags,
                                                  .timescale = speedMultiplier,
                                                  .ar = settings.AR,
                                                  .od = settings.OD,
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
                                                  .legacyTotalScore = 0};

    f64 pp = DifficultyCalculator::calculatePPv2(ppParams);

    // output results
    std::cout << "star rating: " << totalStars << '\n';
    std::cout << "  aim: " << aim << '\n';
    std::cout << "  speed: " << speed << '\n';
    std::cout << "pp (SS): " << pp << '\n';
    std::cout << '\n';
    std::cout << "map info:\n";
    std::cout << "  mods: " << modsStringFromMods(modFlags, speedMultiplier) << '\n';
    std::cout << "  timescale: " << speedMultiplier << '\n';
    std::cout << "  AR: " << settings.AR << '\n';
    std::cout << "  CS: " << settings.CS << '\n';
    std::cout << "  OD: " << settings.OD << '\n';
    std::cout << "  HP: " << settings.HP << '\n';
    std::cout << "  objects: " << primitives.numHitobjects << " (" << primitives.numCircles << "c + "
              << primitives.numSliders << "s + " << primitives.numSpinners << "sp)\n";
    std::cout << "  max combo: " << diffResult.maxPossibleCombo << '\n';

    return 0;
}
