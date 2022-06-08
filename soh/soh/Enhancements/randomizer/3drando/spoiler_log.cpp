#include "spoiler_log.hpp"

#include "dungeon.hpp"
#include "item_list.hpp"
#include "item_location.hpp"
#include "entrance.hpp"
#include "random.hpp"
#include "settings.hpp"
#include "trial.hpp"
#include "tinyxml2.h"
#include "utils.hpp"
#include "shops.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

namespace {
std::string placementtxt;
} // namespace

static RandomizerHash randomizerHash;
static SpoilerData spoilerData;

void GenerateHash() {
    for (size_t i = 0; i < Settings::seed.size(); i++) {
        int number = Settings::seed[i] - '0';
        Settings::hashIconIndexes[i] = number;
    }

    // Clear out spoiler log data here, in case we aren't going to re-generate it
    // spoilerData = { 0 };
}

const RandomizerHash& GetRandomizerHash() {
  return randomizerHash;
}

// Returns the randomizer hash as concatenated string, separated by comma.
const std::string GetRandomizerHashAsString() {
  std::string hash = "";
  for (const std::string& str : randomizerHash) {
    hash += str + ", ";
  }
  hash.erase(hash.length() - 2); // Erase last comma
  return hash;
}

const SpoilerData& GetSpoilerData() {
  return spoilerData;
}

static auto GetGeneralPath() {
    return "./randomizer/haha.xml";
}

static auto GetSpoilerLogPath() {
    return GetGeneralPath();
}

static auto GetPlacementLogPath() {
  return GetGeneralPath();
}

void WriteIngameSpoilerLog() {
    uint16_t spoilerItemIndex = 0;
    uint32_t spoilerStringOffset = 0;
    uint16_t spoilerSphereItemoffset = 0;
    uint16_t spoilerGroupOffset = 0;
    // Intentionally junk value so we trigger the 'new group, record some stuff' code
    uint8_t currentGroup = SpoilerCollectionCheckGroup::SPOILER_COLLECTION_GROUP_COUNT;
    bool spoilerOutOfSpace = false;

    // Create map of string data offsets for all _unique_ item locations and names in the playthrough
    // Some item names, like gold skulltula tokens, can appear many times in a playthrough
    std::unordered_map<uint32_t, uint16_t>
        itemLocationsMap; // Map of LocationKey to an index into spoiler data item locations
    itemLocationsMap.reserve(allLocations.size());
    std::unordered_map<std::string, uint16_t>
        stringOffsetMap; // Map of strings to their offset into spoiler string data array
    stringOffsetMap.reserve(allLocations.size() * 2);

    // Sort all locations by their group, so the in-game log can show a group of items by simply starting/ending at
    // certain indices
    std::stable_sort(allLocations.begin(), allLocations.end(), [](const uint32_t& a, const uint32_t& b) {
        auto groupA = Location(a)->GetCollectionCheckGroup();
        auto groupB = Location(b)->GetCollectionCheckGroup();
        return groupA < groupB;
    });

    for (const uint32_t key : allLocations) {
        auto loc = Location(key);

        // Hide excluded locations from ingame tracker
        if (loc->IsExcluded()) {
            continue;
        }
        // Cows
        else if (!Settings::ShuffleCows && loc->IsCategory(Category::cCow)) {
            continue;
        }
        // Merchants
        else if (Settings::ShuffleMerchants.Is(SHUFFLEMERCHANTS_OFF) && loc->IsCategory(Category::cMerchant)) {
            continue;
        }
        // Adult Trade
        else if (!Settings::ShuffleAdultTradeQuest && loc->IsCategory(Category::cAdultTrade)) {
            continue;
        }
        // Chest Minigame
        else if (Settings::ShuffleChestMinigame.Is(SHUFFLECHESTMINIGAME_OFF) &&
                 loc->IsCategory(Category::cChestMinigame)) {
            continue;
        }
        // Gerudo Fortress
        else if ((Settings::GerudoFortress.Is(GERUDOFORTRESS_OPEN) &&
                  (loc->IsCategory(Category::cVanillaGFSmallKey) || loc->GetHintKey() == GF_GERUDO_TOKEN)) ||
                 (Settings::GerudoFortress.Is(GERUDOFORTRESS_FAST) && loc->IsCategory(Category::cVanillaGFSmallKey) &&
                  loc->GetHintKey() != GF_NORTH_F1_CARPENTER)) {
            continue;
        }

        // Copy at most 51 chars from the name and location name to avoid issues with names that don't fit on screen
        const char* nameFormatStr = "%.51s";

        auto locName = loc->GetName();
        if (stringOffsetMap.find(locName) == stringOffsetMap.end()) {
            if (spoilerStringOffset + locName.size() + 1 >= SPOILER_STRING_DATA_SIZE) {
                spoilerOutOfSpace = true;
                break;
            } else {
                stringOffsetMap[locName] = spoilerStringOffset;
                spoilerStringOffset +=
                    sprintf(&spoilerData.StringData[spoilerStringOffset], nameFormatStr, locName.c_str()) + 1;
            }
        }

        auto locItem = loc->GetPlacedItemName().GetEnglish();
        if (loc->GetPlacedItemKey() == ICE_TRAP && loc->IsCategory(Category::cShop)) {
            locItem = NonShopItems[TransformShopIndex(GetShopIndex(key))].Name.GetEnglish();
        }
        if (stringOffsetMap.find(locItem) == stringOffsetMap.end()) {
            if (spoilerStringOffset + locItem.size() + 1 >= SPOILER_STRING_DATA_SIZE) {
                spoilerOutOfSpace = true;
                break;
            } else {
                stringOffsetMap[locItem] = spoilerStringOffset;
                spoilerStringOffset +=
                    sprintf(&spoilerData.StringData[spoilerStringOffset], nameFormatStr, locItem.c_str()) + 1;
            }
        }

        spoilerData.ItemLocations[spoilerItemIndex].LocationStrOffset = stringOffsetMap[locName];
        spoilerData.ItemLocations[spoilerItemIndex].ItemStrOffset = stringOffsetMap[locItem];
        spoilerData.ItemLocations[spoilerItemIndex].LocationStr = locName;
        spoilerData.ItemLocations[spoilerItemIndex].ItemStr = locItem;
        spoilerData.ItemLocations[spoilerItemIndex].CollectionCheckType = loc->GetCollectionCheck().type;
        spoilerData.ItemLocations[spoilerItemIndex].LocationScene = loc->GetCollectionCheck().scene;
        spoilerData.ItemLocations[spoilerItemIndex].LocationFlag = loc->GetCollectionCheck().flag;

        // Collect Type and Reveal Type
        if (key == GANON) {
            spoilerData.ItemLocations[spoilerItemIndex].CollectType = COLLECTTYPE_NEVER;
            spoilerData.ItemLocations[spoilerItemIndex].RevealType = REVEALTYPE_ALWAYS;
        } else if (key == MARKET_BOMBCHU_BOWLING_BOMBCHUS) {
            spoilerData.ItemLocations[spoilerItemIndex].CollectType = COLLECTTYPE_REPEATABLE;
            spoilerData.ItemLocations[spoilerItemIndex].RevealType = REVEALTYPE_ALWAYS;
        }
        // Shops
        else if (loc->IsShop()) {
            if (Settings::Shopsanity.Is(SHOPSANITY_OFF)) {
                spoilerData.ItemLocations[spoilerItemIndex].RevealType = REVEALTYPE_ALWAYS;
            } else {
                spoilerData.ItemLocations[spoilerItemIndex].RevealType = REVEALTYPE_SCENE;
            }
            if (loc->GetPlacedItem().GetItemType() == ITEMTYPE_REFILL ||
                loc->GetPlacedItem().GetItemType() == ITEMTYPE_SHOP ||
                loc->GetPlacedItem().GetHintKey() == PROGRESSIVE_BOMBCHUS) {
                spoilerData.ItemLocations[spoilerItemIndex].CollectType = COLLECTTYPE_REPEATABLE;
            }
        }
        // Gold Skulltulas
        else if (loc->IsCategory(Category::cSkulltula) &&
                 ((Settings::Tokensanity.Is(TOKENSANITY_OFF)) ||
                  (Settings::Tokensanity.Is(TOKENSANITY_DUNGEONS) && !loc->IsDungeon()) ||
                  (Settings::Tokensanity.Is(TOKENSANITY_OVERWORLD) && loc->IsDungeon()))) {
            spoilerData.ItemLocations[spoilerItemIndex].RevealType = REVEALTYPE_ALWAYS;
        }
        // Deku Scrubs
        else if (loc->IsCategory(Category::cDekuScrub) && !loc->IsCategory(Category::cDekuScrubUpgrades) &&
                 Settings::Scrubsanity.Is(SCRUBSANITY_OFF)) {
            spoilerData.ItemLocations[spoilerItemIndex].CollectType = COLLECTTYPE_REPEATABLE;
            spoilerData.ItemLocations[spoilerItemIndex].RevealType = REVEALTYPE_ALWAYS;
        }

        auto checkGroup = loc->GetCollectionCheckGroup();
        spoilerData.ItemLocations[spoilerItemIndex].Group = checkGroup;

        // Group setup
        if (checkGroup != currentGroup) {
            currentGroup = checkGroup;
            spoilerData.GroupOffsets[currentGroup] = spoilerGroupOffset;
        }
        ++spoilerData.GroupItemCounts[currentGroup];
        ++spoilerGroupOffset;

        itemLocationsMap[key] = spoilerItemIndex++;
    }
    spoilerData.ItemLocationsCount = spoilerItemIndex;

    if (Settings::IngameSpoilers) {
        bool playthroughItemNotFound = false;
        // Write playthrough data to in-game spoiler log
        if (!spoilerOutOfSpace) {
            for (uint32_t i = 0; i < playthroughLocations.size(); i++) {
                if (i >= SPOILER_SPHERES_MAX) {
                    spoilerOutOfSpace = true;
                    break;
                }
                spoilerData.Spheres[i].ItemLocationsOffset = spoilerSphereItemoffset;
                for (uint32_t loc = 0; loc < playthroughLocations[i].size(); ++loc) {
                    if (spoilerSphereItemoffset >= SPOILER_ITEMS_MAX) {
                        spoilerOutOfSpace = true;
                        break;
                    }

                    const auto foundItemLoc = itemLocationsMap.find(playthroughLocations[i][loc]);
                    if (foundItemLoc != itemLocationsMap.end()) {
                        spoilerData.SphereItemLocations[spoilerSphereItemoffset++] = foundItemLoc->second;
                    } else {
                        playthroughItemNotFound = true;
                    }
                    ++spoilerData.Spheres[i].ItemCount;
                }
                ++spoilerData.SphereCount;
            }
        }
        if (spoilerOutOfSpace || playthroughItemNotFound) {
            printf("%sError!%s ", YELLOW, WHITE);
        }
    }
}

// Writes the location to the specified node.
static void WriteLocation(
    tinyxml2::XMLElement* parentNode, const uint32_t locationKey,
    const bool withPadding = false
) {
  ItemLocation* location = Location(locationKey);

  auto node = parentNode->InsertNewChildElement("location");
  node->SetAttribute("name", location->GetName().c_str());
  node->SetText(location->GetPlacedItemName().GetEnglish().c_str());

  if (withPadding) {
    constexpr int16_t LONGEST_NAME = 56; // The longest name of a location.
    constexpr int16_t PRICE_ATTRIBUTE = 12; // Length of a 3-digit price attribute.

    // Insert a padding so we get a kind of table in the XML document.
    int16_t requiredPadding = LONGEST_NAME - location->GetName().length();
    if (location->IsCategory(Category::cShop)) {
      // Shop items have short location names, but come with an additional price attribute.
      requiredPadding -= PRICE_ATTRIBUTE;
    }
    if (requiredPadding >= 0) {
      std::string padding(requiredPadding, ' ');
      node->SetAttribute("_", padding.c_str());
    }
  }

  if (location->IsCategory(Category::cShop)) {
    char price[6];
    sprintf(price, "%03d", location->GetPrice());
    node->SetAttribute("price", price);
  }
  if (!location->IsAddedToPool()) {
    #ifdef ENABLE_DEBUG  
      node->SetAttribute("not-added", true);
    #endif
  }
}

//Writes a shuffled entrance to the specified node
static void WriteShuffledEntrance(
  tinyxml2::XMLElement* parentNode,
  Entrance* entrance,
  const bool withPadding = false
) {
  auto node = parentNode->InsertNewChildElement("entrance");
  node->SetAttribute("name", entrance->GetName().c_str());
  auto text = entrance->GetConnectedRegion()->regionName + " from " + entrance->GetReplacement()->GetParentRegion()->regionName;
  node->SetText(text.c_str());

  if (withPadding) {
    constexpr int16_t LONGEST_NAME = 56; //The longest name of a vanilla entrance

    //Insert padding so we get a kind of table in the XML document
    int16_t requiredPadding = LONGEST_NAME - entrance->GetName().length();
    if (requiredPadding > 0) {
      std::string padding(requiredPadding, ' ');
      node->SetAttribute("_", padding.c_str());
    }
  }
}

json jsonData;

// Writes the settings (without excluded locations, starting inventory and tricks) to the spoilerLog document.
static void WriteSettings(const bool printAll = false) {
  // auto parentNode = spoilerLog.NewElement("settings");

  std::vector<Menu*> allMenus = Settings::GetAllOptionMenus();

  for (const Menu* menu : allMenus) {
    //This is a menu of settings, write them
    if (menu->mode == OPTION_SUB_MENU && menu->printInSpoiler) {
      for (const Option* setting : *menu->settingsList) {
        jsonData["settings"][setting->GetName()] = setting->GetSelectedOptionText();
      }


      // for (const Option* setting : *menu->settingsList) {
      //   if (printAll || (!setting->IsHidden() && setting->IsCategory(OptionCategory::Setting))) {
      //     auto node = parentNode->InsertNewChildElement("setting");
      //     node->SetAttribute("name", RemoveLineBreaks(setting->GetName()).c_str());
      //     node->SetText(setting->GetSelectedOptionText().c_str());
      //   }
      // }
    }
  }
  // spoilerLog.RootElement()->InsertEndChild(parentNode);

  //     for (const uint32_t key : allLocations) {
  //       ItemLocation* location = Location(key);
  //       settingsJsonData["locations"][location->GetName()] = location->GetPlacedItemName().english;
  //   }
}

// Writes the excluded locations to the spoiler log, if there are any.
static void WriteExcludedLocations(tinyxml2::XMLDocument& spoilerLog) {
  auto parentNode = spoilerLog.NewElement("excluded-locations");

  for (size_t i = 1; i < Settings::excludeLocationsOptionsVector.size(); i++) {
    for (const auto& location : Settings::excludeLocationsOptionsVector[i]) {
      if (location->GetSelectedOptionIndex() == INCLUDE) {
        continue;
      }

      tinyxml2::XMLElement* node = spoilerLog.NewElement("location");
      node->SetAttribute("name", RemoveLineBreaks(location->GetName()).c_str());
      parentNode->InsertEndChild(node);
    }
  }

  if (!parentNode->NoChildren()) {
    spoilerLog.RootElement()->InsertEndChild(parentNode);
  }
}

// Writes the starting inventory to the spoiler log, if there is any.
static void WriteStartingInventory() {
  std::vector<std::vector<Option *>*> startingInventoryOptions = {
    &Settings::startingItemsOptions,
    &Settings::startingSongsOptions,
    &Settings::startingEquipmentOptions,
    &Settings::startingStonesMedallionsOptions,
  };

  for (std::vector<Option *>* menu : startingInventoryOptions) {
    for (size_t i = 0; i < menu->size(); ++i) {
      const auto setting = menu->at(i);

      // we need to write these every time because we're not clearing jsondata, so
      // the default logic of only writing it when we aren't using the default value
      // doesn't work, and because it'd be bad to set every single possible starting
      // inventory item as "false" in the json, we're just going to check
      // to see if the name is one of the 3 we're using rn
      if(setting->GetName() == "Deku Shield" || setting->GetName() == "Kokiri Sword" || setting->GetName() == "Ocarina") {
        jsonData["settings"]["Start With " + setting->GetName()] = setting->GetSelectedOptionText();
      }
    }
  }
}

// Writes the enabled tricks to the spoiler log, if there are any.
static void WriteEnabledTricks(tinyxml2::XMLDocument& spoilerLog) {
  auto parentNode = spoilerLog.NewElement("enabled-tricks");

  for (const auto& setting : Settings::trickOptions) {
    if (setting->GetSelectedOptionIndex() != TRICK_ENABLED || !setting->IsCategory(OptionCategory::Setting)) {
      continue;
    }

    auto node = parentNode->InsertNewChildElement("trick");
    node->SetAttribute("name", RemoveLineBreaks(setting->GetName()).c_str());
  }

  if (!parentNode->NoChildren()) {
    spoilerLog.RootElement()->InsertEndChild(parentNode);
  }
}

// Writes the enabled glitches to the spoiler log, if there are any.
static void WriteEnabledGlitches(tinyxml2::XMLDocument& spoilerLog) {
  auto parentNode = spoilerLog.NewElement("enabled-glitches");

  for (const auto& setting : Settings::glitchCategories) {
    if (setting->Value<uint8_t>() == 0) {
      continue;
    }

    auto node = parentNode->InsertNewChildElement("glitch-category");
    node->SetAttribute("name", setting->GetName().c_str());
    node->SetText(setting->GetSelectedOptionText().c_str());
  }

  for (const auto& setting : Settings::miscGlitches) {
    if (!setting->Value<bool>()) {
      continue;
    }

    auto node = parentNode->InsertNewChildElement("misc-glitch");
    node->SetAttribute("name", RemoveLineBreaks(setting->GetName()).c_str());
  }

  if (!parentNode->NoChildren()) {
    spoilerLog.RootElement()->InsertEndChild(parentNode);
  }
}

// Writes the Master Quest dungeons to the spoiler log, if there are any.
static void WriteMasterQuestDungeons(tinyxml2::XMLDocument& spoilerLog) {
  auto parentNode = spoilerLog.NewElement("master-quest-dungeons");

  for (const auto* dungeon : Dungeon::dungeonList) {
    if (dungeon->IsVanilla()) {
      continue;
    }

    auto node = parentNode->InsertNewChildElement("dungeon");
    node->SetAttribute("name", dungeon->GetName().c_str());
  }

  if (!parentNode->NoChildren()) {
    spoilerLog.RootElement()->InsertEndChild(parentNode);
  }
}

// Writes the required trails to the spoiler log, if there are any.
static void WriteRequiredTrials(tinyxml2::XMLDocument& spoilerLog) {
  auto parentNode = spoilerLog.NewElement("required-trials");

  for (const auto* trial : Trial::trialList) {
    if (trial->IsSkipped()) {
      continue;
    }

    auto node = parentNode->InsertNewChildElement("trial");
    std::string name = trial->GetName().GetEnglish();
    name[0] = toupper(name[0]); // Capitalize T in "The"
    node->SetAttribute("name", name.c_str());
  }

  if (!parentNode->NoChildren()) {
    spoilerLog.RootElement()->InsertEndChild(parentNode);
  }
}

// Writes the intended playthrough to the spoiler log, separated into spheres.
static void WritePlaythrough(tinyxml2::XMLDocument& spoilerLog) {
  auto playthroughNode = spoilerLog.NewElement("playthrough");

  for (uint32_t i = 0; i < playthroughLocations.size(); ++i) {
    auto sphereNode = playthroughNode->InsertNewChildElement("sphere");
    sphereNode->SetAttribute("level", i + 1);

    for (const uint32_t key : playthroughLocations[i]) {
      WriteLocation(sphereNode, key, true);
    }
  }

  spoilerLog.RootElement()->InsertEndChild(playthroughNode);
}

//Write the randomized entrance playthrough to the spoiler log, if applicable
static void WriteShuffledEntrances(tinyxml2::XMLDocument& spoilerLog) {
    if (!Settings::ShuffleEntrances || noRandomEntrances) {
        return;
    }

    auto playthroughNode = spoilerLog.NewElement("entrance-playthrough");

    for (uint32_t i = 0; i < playthroughEntrances.size(); ++i) {
        auto sphereNode = playthroughNode->InsertNewChildElement("sphere");
        sphereNode->SetAttribute("level", i + 1);

        for (Entrance* entrance : playthroughEntrances[i]) {
            WriteShuffledEntrance(sphereNode, entrance, true);
        }
    }

    spoilerLog.RootElement()->InsertEndChild(playthroughNode);
}

// Writes the WOTH locations to the spoiler log, if there are any.
static void WriteWayOfTheHeroLocation(tinyxml2::XMLDocument& spoilerLog) {
    auto parentNode = spoilerLog.NewElement("way-of-the-hero-locations");

    for (const uint32_t key : wothLocations) {
        WriteLocation(parentNode, key, true);
    }

    if (!parentNode->NoChildren()) {
        spoilerLog.RootElement()->InsertEndChild(parentNode);
    }
}

// Writes the hints to the spoiler log, if they are enabled.
static void WriteHints(tinyxml2::XMLDocument& spoilerLog) {
    if (Settings::GossipStoneHints.Is(HINTS_NO_HINTS)) {
        return;
    }

    auto parentNode = spoilerLog.NewElement("hints");

    for (const uint32_t key : gossipStoneLocations) {
        ItemLocation* location = Location(key);

        auto node = parentNode->InsertNewChildElement("hint");
        node->SetAttribute("location", location->GetName().c_str());

        auto text = location->GetPlacedItemName().GetEnglish();
        std::replace(text.begin(), text.end(), '&', ' ');
        std::replace(text.begin(), text.end(), '^', ' ');
        node->SetText(text.c_str());
    }

    spoilerLog.RootElement()->InsertEndChild(parentNode);
}

static void WriteAllLocations() {
    for (const uint32_t key : allLocations) {
        ItemLocation* location = Location(key);
        jsonData["locations"][location->GetName()] = location->GetPlacedItemName().english;
    }
}

const char* SpoilerLog_Write() {
    auto spoilerLog = tinyxml2::XMLDocument(false);
    spoilerLog.InsertEndChild(spoilerLog.NewDeclaration());

    auto rootNode = spoilerLog.NewElement("spoiler-log");
    spoilerLog.InsertEndChild(rootNode);

    rootNode->SetAttribute("version", Settings::version.c_str());
    rootNode->SetAttribute("seed", Settings::seed.c_str());
    rootNode->SetAttribute("hash", GetRandomizerHashAsString().c_str());

    // Write Hash
    int index = 0;
    for (uint8_t seed_value : Settings::hashIconIndexes) {
        jsonData["file_hash"][index] = seed_value;
        index++;
    }

    WriteSettings();
    //WriteExcludedLocations(spoilerLog);
    WriteStartingInventory();
    //WriteEnabledTricks(spoilerLog);
    //if (Settings::Logic.Is(LOGIC_GLITCHED)) {
    //    WriteEnabledGlitches(spoilerLog);
    //}
    //WriteMasterQuestDungeons(spoilerLog);
    //WriteRequiredTrials(spoilerLog);
    //WritePlaythrough(spoilerLog);
    //WriteWayOfTheHeroLocation(spoilerLog);

    playthroughLocations.clear();
    playthroughBeatable = false;
    wothLocations.clear();

    //WriteHints(spoilerLog);
    //WriteShuffledEntrances(spoilerLog);
    WriteAllLocations();
    
    if (!std::filesystem::exists("./Randomizer")) {
        std::filesystem::create_directory("./Randomizer");
    }

    std::string jsonString = jsonData.dump(4);
    std::ofstream jsonFile("./Randomizer/" + Settings::seed + ".json");
    jsonFile << std::setw(4) << jsonString << std::endl;
    jsonFile.close();

    return Settings::seed.c_str();
}

void PlacementLog_Msg(std::string_view msg) {
    placementtxt += msg;
}

void PlacementLog_Clear() {
    placementtxt = "";
}

bool PlacementLog_Write() {
    auto placementLog = tinyxml2::XMLDocument(false);
    placementLog.InsertEndChild(placementLog.NewDeclaration());

    auto rootNode = placementLog.NewElement("placement-log");
    placementLog.InsertEndChild(rootNode);

    rootNode->SetAttribute("version", Settings::version.c_str());
    rootNode->SetAttribute("seed", Settings::seed.c_str());
    rootNode->SetAttribute("hash", GetRandomizerHashAsString().c_str());

    // WriteSettings(placementLog, true); // Include hidden settings.
    WriteExcludedLocations(placementLog);
    // WriteStartingInventory(placementLog);
    WriteEnabledTricks(placementLog);
    WriteEnabledGlitches(placementLog);
    WriteMasterQuestDungeons(placementLog);
    WriteRequiredTrials(placementLog);

    placementtxt = "\n" + placementtxt;

    auto node = rootNode->InsertNewChildElement("log");
    auto contentNode = node->InsertNewText(placementtxt.c_str());
    contentNode->SetCData(true);

    return true;
}