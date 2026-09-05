#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "opennfh/core/types.hpp"

namespace opennfh::content {

using RoomId = std::string;
using DoorId = std::string;

enum class LevelState {
    Locked,
    Playable,
    Completed,
};

struct LevelMeta {
    std::string resource_id;
    std::string level_name;
    Vec2i size;
    int angry_time{0};
    int min_quota{0};
    int time_value{0};
    int reachable{0};
};

struct Floor {
    Vec2i offset;
    Vec2i size;
    bool wall{false};
    std::string hotspot;
};

struct Door {
    DoorId id;
    int layer{0};
    Vec2i position;
    bool visible{true};
};

struct NeighborLink {
    std::string name;
    int costs{0};
    DoorId door_in;
    DoorId door_out;
};

struct ActorSpawn {
    std::string name;
    int layer{0};
    Vec2i position;
    std::string animation;
};

struct PlacedObject {
    std::string name;
    int layer{0};
    Vec2i position;
    bool visible{true};
    std::string animation;
};

struct Room {
    RoomId id;
    Vec2i offset;
    Vec2i path1;
    Vec2i path2;
    std::vector<Floor> floors;
    std::vector<Door> doors;
    std::vector<NeighborLink> neighbors;
    std::vector<ActorSpawn> actors;
    std::vector<PlacedObject> objects;
};

struct Hotspot {
    std::string name;
    Vec2i offset;
};

struct SpeedDef {
    std::string name;
    int speed{0};
    int start{0};
    int noise{0};
};

struct ContentItem {
    std::string name;
    int count{0};
};

struct FrameDef {
    std::string gfx;
    std::string sfx;
};

struct RegionDef {
    Vec2i position;
    Vec2i size;
    std::string type;
};

struct AnimationDef {
    std::string name;
    std::string type;
    std::vector<FrameDef> frames;
    std::vector<RegionDef> regions;
};

struct GfxFile {
    std::string image;
    Vec2i offset;
};

struct ActionDef {
    std::string name;
    std::string actor;
    std::string actor_animation;
    std::string actor_next_animation;
    std::string object_animation;
    std::string object_next_animation;
    std::string time;
    int noise{0};
    std::string behavior;
    std::string behavior_actor;
    bool always{false};
};

struct ObjectDef {
    std::string name;
    std::string kind;
    std::string gfx;
    std::string hotspot;
    std::vector<Hotspot> hotspots;
    std::vector<SpeedDef> speeds;
    std::vector<std::string> standard_actions;
    std::vector<ActionDef> actions;
    std::vector<std::string> flags;
    std::vector<ContentItem> contents;
    std::map<std::string, std::string> images;
    std::map<std::string, AnimationDef> animations;
    std::vector<GfxFile> gfx_files;
    std::vector<RegionDef> regions;
};

struct SoundDef {
    std::string file;
    int volume{100};
};

struct StringDef {
    std::string name;
    std::string category;
    std::string text;
};

struct Ingredient {
    std::string name;
    bool remove{false};
};

struct Combination {
    std::string name;
    bool trick{false};
    bool wrong{false};
    std::vector<Ingredient> ingredients;
};

struct Trick {
    std::string name;
    int quota1{0};
    int quota2{0};
    int quota3{0};
    int quota4{0};
    int angry_time{0};
};

struct TriggerRule {
    std::string object;
    std::string position;
    std::string type;
};

struct BehaviorDef {
    std::string actor;
    std::string name;
    std::vector<TriggerRule> triggers;
};

struct CampaignSet {
    std::string id;
    LevelState state{LevelState::Locked};
    std::string next_set;
    std::vector<LevelMeta> levels;
};

struct CampaignCatalog {
    std::vector<CampaignSet> sets;
};

struct LevelDefinition {
    LevelMeta meta;
    std::vector<Room> rooms;
    std::vector<PlacedObject> root_objects;
    std::map<std::string, ObjectDef> objects;
    std::map<std::string, SoundDef> sounds;
    std::map<std::string, StringDef> strings;
    std::vector<Combination> combinations;
    std::vector<Trick> tricks;
    std::vector<BehaviorDef> behaviors;
};

struct ContentCatalog {
    CampaignCatalog campaign;
    std::map<std::string, ObjectDef> generic_objects;
};

}  // namespace opennfh::content
