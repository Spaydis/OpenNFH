#pragma once

#include <string_view>

#include "opennfh/content/model.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/io/xml_fragments.hpp"

namespace opennfh::content {

struct LoadOptions {
    io::XmlParseOptions xml;
};

[[nodiscard]] Result<CampaignCatalog> load_campaign(
    const io::DataRoot& root,
    const LoadOptions& options = {});

[[nodiscard]] Result<LevelDefinition> load_level(
    const io::DataRoot& root,
    std::string_view resource_id,
    const LoadOptions& options = {});

}  // namespace opennfh::content
