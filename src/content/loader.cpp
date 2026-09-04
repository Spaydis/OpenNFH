#include "opennfh/content/loader.hpp"

#include "opennfh/io/text_codec.hpp"

#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opennfh::content {

namespace {

Error make_error(ErrorCode code, std::string message, std::string source = {}) {
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.source = std::move(source);
    return error;
}

std::string attribute(const io::XmlNode& node, std::string_view name) {
    for (const auto& [key, value] : node.attributes) {
        if (key == name) {
            return value;
        }
    }
    return {};
}

int integer(std::string_view text, int fallback = 0) {
    if (text.empty()) {
        return fallback;
    }
    int result = fallback;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? result : fallback;
}

Vec2i vector2(std::string_view text) {
    const auto separator = text.find('/');
    if (separator == std::string_view::npos) {
        return {};
    }
    return {integer(text.substr(0, separator)), integer(text.substr(separator + 1))};
}

bool boolean(std::string_view text, bool fallback = false) {
    if (text == "true" || text == "1") {
        return true;
    }
    if (text == "false" || text == "0") {
        return false;
    }
    return fallback;
}

LevelState state(std::string_view text) {
    if (text == "playable") {
        return LevelState::Playable;
    }
    if (text == "completed") {
        return LevelState::Completed;
    }
    return LevelState::Locked;
}

const io::XmlNode* named_root(const io::XmlFragmentDocument& document, std::string_view name) {
    for (const auto& root : document.roots) {
        if (root.name == name) {
            return &root;
        }
    }
    return nullptr;
}

Result<io::XmlFragmentDocument> read_xml(
    const io::DataRoot& root,
    std::string_view path,
    const LoadOptions& options) {
    const auto bytes = root.game_data().read(path);
    if (!bytes.has_value()) {
        return Result<io::XmlFragmentDocument>::failure(bytes.error());
    }
    const auto text = io::decode_xml_bytes(bytes.value());
    if (!text.has_value()) {
        return Result<io::XmlFragmentDocument>::failure(text.error());
    }
    const auto parsed = io::parse_xml_fragments(path, text.value(), options.xml);
    if (!parsed.has_value()) {
        return Result<io::XmlFragmentDocument>::failure(parsed.error());
    }
    return Result<io::XmlFragmentDocument>::success(parsed.value());
}

Result<std::optional<io::XmlFragmentDocument>> read_optional_xml(
    const io::DataRoot& root,
    std::string_view path,
    const LoadOptions& options) {
    if (!root.game_data().contains(path)) {
        return Result<std::optional<io::XmlFragmentDocument>>::success(std::nullopt);
    }
    const auto parsed = read_xml(root, path, options);
    if (!parsed.has_value()) {
        return Result<std::optional<io::XmlFragmentDocument>>::failure(parsed.error());
    }
    return Result<std::optional<io::XmlFragmentDocument>>::success(parsed.value());
}

LevelMeta level_meta(const io::XmlNode& node, std::string_view resource_id) {
    LevelMeta result;
    result.resource_id = std::string(resource_id);
    result.level_name = attribute(node, "name");
    result.size = vector2(attribute(node, "size"));
    result.angry_time = integer(attribute(node, "angrytime"));
    result.min_quota = integer(attribute(node, "minquota"));
    result.time_value = integer(attribute(node, "time"));
    result.reachable = integer(attribute(node, "reachable"));
    return result;
}

void parse_campaign_levels(CampaignSet& set, const io::XmlNode& node) {
    set.id = attribute(node, "name");
    set.state = state(attribute(node, "state"));
    set.next_set = attribute(node, "nextset");
    for (const auto& child : node.children) {
        if (child.name != "level") {
            continue;
        }
        set.levels.push_back(level_meta(child, attribute(child, "name")));
    }
}

void upsert_action(ObjectDef& object, ActionDef action) {
    for (auto& existing : object.actions) {
        if (existing.name == action.name) {
            existing = std::move(action);
            return;
        }
    }
    object.actions.push_back(std::move(action));
}

ActionDef parse_action(const io::XmlNode& node) {
    ActionDef result;
    result.name = attribute(node, "name");
    result.actor = attribute(node, "actor");
    result.actor_animation = attribute(node, "actoranim");
    result.actor_next_animation = attribute(node, "actornextanim");
    result.object_animation = attribute(node, "objanim");
    result.object_next_animation = attribute(node, "objnextanim");
    result.time = attribute(node, "time");
    result.noise = integer(attribute(node, "noise"));
    result.behavior = attribute(node, "behavior");
    result.behavior_actor = attribute(node, "behavioractor");
    result.always = boolean(attribute(node, "always"));
    return result;
}

void merge_object_node(LevelDefinition& level, const io::XmlNode& node) {
    const auto name = attribute(node, "name");
    if (name.empty()) {
        return;
    }
    auto& object = level.objects[name];
    object.name = name;
    if (object.kind.empty()) {
        object.kind = node.name;
    }
    if (!attribute(node, "gfx").empty()) {
        object.gfx = attribute(node, "gfx");
    }
    if (!attribute(node, "hotspot").empty()) {
        object.hotspot = attribute(node, "hotspot");
    }
    for (const auto& child : node.children) {
        if (child.name == "hotspot") {
            const auto hotspot_name = attribute(child, "name");
            auto found = std::find_if(object.hotspots.begin(), object.hotspots.end(), [&](const Hotspot& hotspot) {
                return hotspot.name == hotspot_name;
            });
            Hotspot value{hotspot_name, vector2(attribute(child, "offset"))};
            if (found == object.hotspots.end()) {
                object.hotspots.push_back(std::move(value));
            } else {
                *found = std::move(value);
            }
        } else if (child.name == "stdaction") {
            const auto action_name = attribute(child, "name");
            if (std::find(object.standard_actions.begin(), object.standard_actions.end(), action_name) == object.standard_actions.end()) {
                object.standard_actions.push_back(action_name);
            }
        } else if (child.name == "action") {
            upsert_action(object, parse_action(child));
        } else if (child.name == "flag") {
            const auto flag_name = attribute(child, "name");
            if (std::find(object.flags.begin(), object.flags.end(), flag_name) == object.flags.end()) {
                object.flags.push_back(flag_name);
            }
        } else if (child.name == "content") {
            const auto content_name = attribute(child, "name");
            const ContentItem value{content_name, integer(attribute(child, "count"), 1)};
            auto found = std::find_if(object.contents.begin(), object.contents.end(), [&](const ContentItem& content) {
                return content.name == content_name;
            });
            if (found == object.contents.end()) {
                object.contents.push_back(value);
            } else {
                *found = value;
            }
        }
    }
}

void merge_objects(LevelDefinition& level, const io::XmlFragmentDocument& document) {
    for (const auto& root : document.roots) {
        if (root.name == "objects") {
            for (const auto& child : root.children) {
                merge_object_node(level, child);
            }
        } else if (root.name == "object" || root.name == "actor" || root.name == "door" ||
                   root.name == "icon" || root.name == "inventar") {
            merge_object_node(level, root);
        }
    }
}

void merge_animations(LevelDefinition& level, const io::XmlFragmentDocument& document) {
    for (const auto& root : document.roots) {
        std::vector<const io::XmlNode*> objects;
        if (root.name == "all_objects") {
            for (const auto& child : root.children) {
                if (child.name == "object") {
                    objects.push_back(&child);
                }
            }
        } else if (root.name == "object") {
            objects.push_back(&root);
        }
        for (const auto* object_node : objects) {
            const auto name = attribute(*object_node, "name");
            if (name.empty()) {
                continue;
            }
            auto& object = level.objects[name];
            object.name = name;
            for (const auto& animation_node : object_node->children) {
                if (animation_node.name != "animation") {
                    continue;
                }
                AnimationDef animation;
                animation.name = attribute(animation_node, "name");
                animation.type = attribute(animation_node, "type");
                for (const auto& child : animation_node.children) {
                    if (child.name == "frame") {
                        animation.frames.push_back(FrameDef{attribute(child, "gfx"), attribute(child, "sfx")});
                    } else if (child.name == "region") {
                        animation.regions.push_back(RegionDef{
                            vector2(attribute(child, "position")),
                            vector2(attribute(child, "size")),
                            attribute(child, "type"),
                        });
                    }
                }
                object.animations[animation.name] = std::move(animation);
            }
        }
    }
}

void merge_gfx(LevelDefinition& level, const io::XmlFragmentDocument& document) {
    for (const auto& root : document.roots) {
        if (root.name != "object") {
            continue;
        }
        const auto name = attribute(root, "name");
        auto& object = level.objects[name];
        object.name = name;
        for (const auto& gfxdata : root.children) {
            if (gfxdata.name != "gfxdata") {
                continue;
            }
            for (const auto& file : gfxdata.children) {
                if (file.name != "file") {
                    continue;
                }
                const GfxFile value{attribute(file, "image"), vector2(attribute(file, "offset"))};
                auto found = std::find_if(object.gfx_files.begin(), object.gfx_files.end(), [&](const GfxFile& existing) {
                    return existing.image == value.image;
                });
                if (found == object.gfx_files.end()) {
                    object.gfx_files.push_back(value);
                } else {
                    *found = value;
                }
            }
        }
    }
}

void merge_sfx(LevelDefinition& level, const io::XmlFragmentDocument& document) {
    for (const auto& root : document.roots) {
        if (root.name != "sfxdata") {
            continue;
        }
        for (const auto& child : root.children) {
            if (child.name == "sfx") {
                const auto file = attribute(child, "file");
                level.sounds[file] = SoundDef{file, integer(attribute(child, "volume"), 100)};
            }
        }
    }
}

void merge_strings(LevelDefinition& level, const io::XmlFragmentDocument& document) {
    for (const auto& root : document.roots) {
        if (root.name != "strings") {
            continue;
        }
        for (const auto& child : root.children) {
            if (child.name == "string") {
                const auto name = attribute(child, "name");
                level.strings[name] = StringDef{name, attribute(child, "category"), attribute(child, "text")};
            }
        }
    }
}

void merge_tricks(LevelDefinition& level, const io::XmlFragmentDocument& document) {
    for (const auto& root : document.roots) {
        if (root.name != "tricks") {
            continue;
        }
        for (const auto& child : root.children) {
            if (child.name != "trick") {
                continue;
            }
            Trick value;
            value.name = attribute(child, "name");
            value.quota1 = integer(attribute(child, "quota1"));
            value.quota2 = integer(attribute(child, "quota2"));
            value.quota3 = integer(attribute(child, "quota3"));
            value.quota4 = integer(attribute(child, "quota4"));
            value.angry_time = integer(attribute(child, "angrytime"));
            auto found = std::find_if(level.tricks.begin(), level.tricks.end(), [&](const Trick& existing) {
                return existing.name == value.name;
            });
            if (found == level.tricks.end()) {
                level.tricks.push_back(std::move(value));
            } else {
                *found = std::move(value);
            }
        }
    }
}

void merge_combinations(LevelDefinition& level, const io::XmlFragmentDocument& document) {
    for (const auto& root : document.roots) {
        if (root.name != "combine") {
            continue;
        }
        for (const auto& child : root.children) {
            if (child.name != "combination") {
                continue;
            }
            Combination value;
            value.name = attribute(child, "name");
            value.trick = boolean(attribute(child, "trick"));
            value.wrong = boolean(attribute(child, "wrong"));
            for (const auto& ingredient : child.children) {
                if (ingredient.name == "ingredient") {
                    value.ingredients.push_back(Ingredient{
                        attribute(ingredient, "name"),
                        boolean(attribute(ingredient, "remove")),
                    });
                }
            }
            auto found = std::find_if(level.combinations.begin(), level.combinations.end(), [&](const Combination& existing) {
                return existing.name == value.name;
            });
            if (found == level.combinations.end()) {
                level.combinations.push_back(std::move(value));
            } else {
                *found = std::move(value);
            }
        }
    }
}

void merge_triggers(LevelDefinition& level, const io::XmlFragmentDocument& document) {
    for (const auto& root : document.roots) {
        if (root.name != "triggers") {
            continue;
        }
        for (const auto& actor : root.children) {
            if (actor.name != "actor") {
                continue;
            }
            for (const auto& behavior_node : actor.children) {
                if (behavior_node.name != "behavior") {
                    continue;
                }
                BehaviorDef value;
                value.actor = attribute(actor, "name");
                value.name = attribute(behavior_node, "name");
                for (const auto& trigger : behavior_node.children) {
                    if (trigger.name == "trigger") {
                        value.triggers.push_back(TriggerRule{
                            attribute(trigger, "object"),
                            attribute(trigger, "position"),
                            attribute(trigger, "type"),
                        });
                    }
                }
                auto found = std::find_if(level.behaviors.begin(), level.behaviors.end(), [&](const BehaviorDef& existing) {
                    return existing.actor == value.actor && existing.name == value.name;
                });
                if (found == level.behaviors.end()) {
                    level.behaviors.push_back(std::move(value));
                } else {
                    *found = std::move(value);
                }
            }
        }
    }
}

template <typename Merge>
Result<bool> merge_optional_role(
    const io::DataRoot& root,
    std::string_view path,
    const LoadOptions& options,
    LevelDefinition& level,
    Merge merge) {
    const auto document = read_optional_xml(root, path, options);
    if (!document.has_value()) {
        return Result<bool>::failure(document.error());
    }
    if (document.value().has_value()) {
        merge(level, document.value().value());
    }
    return Result<bool>::success(document.value().has_value());
}

void parse_level_layout(LevelDefinition& level, const io::XmlNode& root) {
    for (const auto& child : root.children) {
        if (child.name == "object") {
            level.root_objects.push_back(PlacedObject{
                attribute(child, "name"),
                integer(attribute(child, "layer")),
                vector2(attribute(child, "position")),
                boolean(attribute(child, "visible"), true),
            });
            continue;
        }
        if (child.name != "room") {
            continue;
        }
        Room room;
        room.id = attribute(child, "name");
        room.offset = vector2(attribute(child, "offset"));
        room.path1 = vector2(attribute(child, "path1"));
        room.path2 = vector2(attribute(child, "path2"));
        for (const auto& item : child.children) {
            if (item.name == "floor") {
                room.floors.push_back(Floor{
                    vector2(attribute(item, "offset")),
                    vector2(attribute(item, "size")),
                    boolean(attribute(item, "wall")),
                    attribute(item, "hotspot"),
                });
            } else if (item.name == "door") {
                room.doors.push_back(Door{
                    attribute(item, "name"),
                    integer(attribute(item, "layer")),
                    vector2(attribute(item, "position")),
                    boolean(attribute(item, "visible"), true),
                });
            } else if (item.name == "neighbor") {
                room.neighbors.push_back(NeighborLink{
                    attribute(item, "name"),
                    integer(attribute(item, "costs")),
                    attribute(item, "doorin"),
                    attribute(item, "doorout"),
                });
            } else if (item.name == "actor") {
                room.actors.push_back(ActorSpawn{
                    attribute(item, "name"),
                    integer(attribute(item, "layer")),
                    vector2(attribute(item, "position")),
                    attribute(item, "animation"),
                });
            } else if (item.name == "object") {
                room.objects.push_back(PlacedObject{
                    attribute(item, "name"),
                    integer(attribute(item, "layer")),
                    vector2(attribute(item, "position")),
                    boolean(attribute(item, "visible"), true),
                });
            }
        }
        level.rooms.push_back(std::move(room));
    }
}

}  // namespace

Result<CampaignCatalog> load_campaign(const io::DataRoot& root, const LoadOptions& options) {
    const auto document = read_xml(root, "leveldata.xml", options);
    if (!document.has_value()) {
        return Result<CampaignCatalog>::failure(document.error());
    }
    const auto* leveldata = named_root(document.value(), "leveldata");
    if (leveldata == nullptr) {
        return Result<CampaignCatalog>::failure(make_error(ErrorCode::Format, "leveldata.xml root is missing", "leveldata.xml"));
    }

    CampaignCatalog campaign;
    for (const auto& child : leveldata->children) {
        if (child.name != "set") {
            continue;
        }
        CampaignSet set_definition;
        parse_campaign_levels(set_definition, child);
        campaign.sets.push_back(std::move(set_definition));
    }
    return Result<CampaignCatalog>::success(std::move(campaign));
}

Result<LevelDefinition> load_level(
    const io::DataRoot& root,
    std::string_view resource_id,
    const LoadOptions& options) {
    const auto campaign = load_campaign(root, options);
    if (!campaign.has_value()) {
        return Result<LevelDefinition>::failure(campaign.error());
    }

    LevelDefinition level;
    for (const auto& set_definition : campaign.value().sets) {
        for (const auto& meta : set_definition.levels) {
            if (meta.resource_id == resource_id) {
                level.meta = meta;
                break;
            }
        }
    }
    level.meta.resource_id = std::string(resource_id);

    const std::string level_path = std::string(resource_id) + "/level.xml";
    const auto layout_document = read_xml(root, level_path, options);
    if (!layout_document.has_value()) {
        return Result<LevelDefinition>::failure(layout_document.error());
    }
    const auto* layout = named_root(layout_document.value(), "level");
    if (layout == nullptr) {
        return Result<LevelDefinition>::failure(make_error(ErrorCode::Format, "level.xml root is missing", level_path));
    }
    level.meta.level_name = attribute(*layout, "name");
    level.meta.size = vector2(attribute(*layout, "size"));
    level.meta.angry_time = integer(attribute(*layout, "angrytime"));
    parse_level_layout(level, *layout);

    const std::string generic_prefix = "generic/";
    const std::string level_prefix = std::string(resource_id) + "/";
    const auto merge_role = [&](std::string_view role, auto merge) -> Result<bool> {
        auto generic = merge_optional_role(root, generic_prefix + std::string(role) + ".xml", options, level, merge);
        if (!generic.has_value()) {
            return generic;
        }
        return merge_optional_role(root, level_prefix + std::string(role) + ".xml", options, level, merge);
    };

    auto objects = merge_role("objects", merge_objects);
    if (!objects.has_value()) return Result<LevelDefinition>::failure(objects.error());
    auto animations = merge_role("anims", merge_animations);
    if (!animations.has_value()) return Result<LevelDefinition>::failure(animations.error());
    auto gfx = merge_role("gfxdata", merge_gfx);
    if (!gfx.has_value()) return Result<LevelDefinition>::failure(gfx.error());
    auto sfx = merge_role("sfxdata", merge_sfx);
    if (!sfx.has_value()) return Result<LevelDefinition>::failure(sfx.error());
    auto strings = merge_role("strings", merge_strings);
    if (!strings.has_value()) return Result<LevelDefinition>::failure(strings.error());
    auto triggers = merge_role("trigger", merge_triggers);
    if (!triggers.has_value()) return Result<LevelDefinition>::failure(triggers.error());
    auto tricks = merge_role("tricks", merge_tricks);
    if (!tricks.has_value()) return Result<LevelDefinition>::failure(tricks.error());
    auto combinations = merge_role("combine", merge_combinations);
    if (!combinations.has_value()) return Result<LevelDefinition>::failure(combinations.error());

    return Result<LevelDefinition>::success(std::move(level));
}

}  // namespace opennfh::content
