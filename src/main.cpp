#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <eclipse.eclipse-menu/include/components.hpp>
#include <eclipse.eclipse-menu/include/config.hpp>

using namespace geode::prelude;

static bool isEnabled() {
    return Mod::get()->getSettingValue<bool>("reversed-inputs-enabled");
}

// swap push/release
class $modify(MyPlayerObject, PlayerObject) {
    struct Fields {
        bool skip = false;
    };

    bool pushButton(PlayerButton btn) {
        if (!isEnabled() || m_fields->skip)
            return PlayerObject::pushButton(btn);

        // player press = release
        m_fields->skip = true;
        bool r = PlayerObject::releaseButton(btn);
        m_fields->skip = false;
        return r;
    }

    bool releaseButton(PlayerButton btn) {
        if (!isEnabled() || m_fields->skip)
            return PlayerObject::releaseButton(btn);

        // player release = press
        m_fields->skip = true;
        bool r = PlayerObject::pushButton(btn);
        m_fields->skip = false;
        return r;
    }
};

// force hold state at level start so the player isn't just hanging(SAYORI REFERENCE!?!?) there
class $modify(MyPlayLayer, PlayLayer) {
    void autoHold(PlayerObject* p) {
        auto* mp = static_cast<MyPlayerObject*>(p);
        mp->m_fields->skip = true;
        mp->PlayerObject::pushButton(PlayerButton::Jump);
        mp->m_fields->skip = false;
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;

        if (!isEnabled())
            return true;

        if (m_player1) autoHold(m_player1);
        if (m_gameState.m_isDualMode && m_player2) autoHold(m_player2);

        return true;
    }
};

// keybind to toggle the mod on/off
$on_game(Loaded) {
    listenForKeybindSettingPresses("toggle-keybind", [](Keybind const&, bool down, bool repeat, double) {
        if (!down || repeat) return;
        auto mod = Mod::get();
        mod->setSettingValue("reversed-inputs-enabled", !mod->getSettingValue<bool>("reversed-inputs-enabled"));
    });
}

$on_game(Loaded) {
    auto tab = eclipse::MenuTab::find("Global");

    tab.addToggle("reversed-inputs-enabled", "Reversed Inputs", [](bool val) {
        Mod::get()->setSettingValue("reversed-inputs-enabled", val);
    }).setDescription("Hold to do nothing, tap to jump.");

    // sync initial value
    eclipse::config::set("reversed-inputs-enabled", Mod::get()->getSettingValue<bool>("reversed-inputs-enabled"));

    // keep in sync when changed from mod settings
    geode::listenForSettingChanges<bool>("reversed-inputs-enabled", [](bool val) {
        eclipse::config::set("reversed-inputs-enabled", val);
    });
}
