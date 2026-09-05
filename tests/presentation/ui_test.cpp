#include <cassert>
#include <filesystem>

#include "opennfh/io/data_root.hpp"
#include "opennfh/io/xml_fragments.hpp"
#include "support/zip_fixture.hpp"
#include "opennfh/presentation/ui.hpp"

int main() {
    const auto document = opennfh::io::parse_xml_fragments(
        "menu.xml",
        "<dialog><button name=\"ok\" offset=\"1/2\" size=\"30/40\"/><text name=\"caption\" offset=\"4/5\" size=\"50/60\"/></dialog>",
        {});
    assert(document.has_value());

    const auto definition = opennfh::presentation::parse_dialog(document.value(), "menu");
    assert(definition.has_value());
    assert(definition.value().controls.size() == 2);
    assert(definition.value().controls[0].name == "ok");
    assert(definition.value().controls[0].rect.offset.x == 1);
    assert(definition.value().controls[0].rect.size.y == 40);

    const auto hud_document = opennfh::io::parse_xml_fragments(
        "hud.xml",
        R"(<dialog gfx="gui/ingame/interface_m.tga" offset="347/597"><button name="inv00" offset="11/57"><image name="std" gfx="gui/inv/i_pins_norm.tga"/></button></dialog>)",
        {});
    assert(hud_document.has_value());
    const auto hud = opennfh::presentation::parse_dialog(hud_document.value(), "hud");
    assert(hud.has_value());
    assert(hud.value().gfx == "gui/ingame/interface_m.tga");
    assert(hud.value().offset.x == 347);
    assert(hud.value().controls[0].images.size() == 1);
    assert(hud.value().controls[0].images[0].gfx == "gui/inv/i_pins_norm.tga");

    const auto nested = opennfh::io::parse_xml_fragments(
        "nested.xml",
        "<dialog><button name=\"btn\" offset=\"0/0\"><image name=\"hover\"/></button><progressbar name=\"quota\" role=\"progress\" offset=\"2/3\" size=\"4/5\"/></dialog>",
        {});
    assert(nested.has_value());
    const auto nested_definition = opennfh::presentation::parse_dialog(nested.value(), "nested");
    assert(nested_definition.has_value());
    assert(nested_definition.value().controls.size() == 2);
    assert(nested_definition.value().controls[1].role == "progress");
    assert(nested_definition.value().controls[1].rect.size.x == 4);

    const auto malformed = opennfh::io::parse_xml_fragments(
        "malformed.xml", "<dialog><button name=\"bad\" offset=\"x/0\"/></dialog>", {});
    assert(malformed.has_value());
    const auto malformed_definition =
        opennfh::presentation::parse_dialog(malformed.value(), "malformed");
    assert(!malformed_definition.has_value());
    const auto no_dialog_document = opennfh::io::parse_xml_fragments(
        "empty.xml", "<panel/>", {});
    assert(no_dialog_document.has_value());
    const auto missing = opennfh::presentation::parse_dialog(no_dialog_document.value(), "missing");
    assert(!missing.has_value());

    test_support::ZipFixture zip_fixture;
    const auto fixture_root = std::filesystem::temp_directory_path() / "opennfh-ui-data-root";
    std::error_code fixture_error;
    std::filesystem::remove_all(fixture_root, fixture_error);
    std::filesystem::create_directories(fixture_root);
    std::filesystem::copy_file(zip_fixture.path(), fixture_root / "gamedata.bnd");
    std::filesystem::copy_file(zip_fixture.path(), fixture_root / "gfxdata.bnd");
    std::filesystem::copy_file(zip_fixture.path(), fixture_root / "sfxdata.bnd");
    const auto data_root = opennfh::io::DataRoot::open(fixture_root);
    assert(data_root.has_value());
    const auto unsafe = opennfh::presentation::load_dialog(data_root.value(), "../menu");
    assert(!unsafe.has_value());
    assert(unsafe.error().code == opennfh::ErrorCode::InvalidArgument);
    std::filesystem::remove_all(fixture_root, fixture_error);
    using opennfh::presentation::InputAction;
    assert(opennfh::presentation::resolve_shortcut(0x25) == InputAction::ScrollLeft);
    assert(opennfh::presentation::resolve_shortcut(0x27) == InputAction::ScrollRight);
    assert(opennfh::presentation::resolve_shortcut(0x13) == InputAction::Pause);
    assert(opennfh::presentation::resolve_shortcut(0xffff) == InputAction::Unknown);
    return 0;
}
