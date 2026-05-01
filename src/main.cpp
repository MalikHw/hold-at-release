#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <eclipse.eclipse-menu/include/components.hpp>
#include <eclipse.eclipse-menu/include/config.hpp>
#include <eclipse.eclipse-menu/include/modules.hpp>

using namespace geode::prelude;

static bool isEnabled() {
    return Mod::get()->getSettingValue<bool>("reversed-inputs-enabled");
}
// swap push/release
class $modify(MyPlayLayer, PlayLayer) {
    void autoHold(PlayerObject* p) {
        if (!p) return;
        auto* mp = static_cast<MyPlayerObject*>(typeinfo_cast<PlayerObject*>(p));
        if (!mp) return;
        mp->m_fields->skip = true;
        mp->PlayerObject::pushButton(PlayerButton::Jump);
        mp->m_fields->skip = false;
    }
    void doAutoHold(float) {
        if (m_player1) autoHold(m_player1);
        if (m_gameState.m_isDualMode && m_player2) autoHold(m_player2);
        this->unschedule(schedule_selector(MyPlayLayer::doAutoHold));
    }
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;
        if (!isEnabled())
            return true;

        this->schedule(schedule_selector(MyPlayLayer::doAutoHold), 0.f);

        // warn the player
        Notification::create(
            "Reversed Inputs is ON! just warning tho", NotificationIcon::Warning)->show(); return true;
    }
};
// force hold state at level start so the player isn't just hanging(SAYORI REFERENCE!?!?) there
class $modify(MyPlayLayer, PlayLayer) {
    void autoHold(PlayerObject* p) {
        if (!p) return;
        auto* mp = static_cast<MyPlayerObject*>(typeinfo_cast<PlayerObject*>(p));
        if (!mp) return;
        mp->m_fields->skip = true;
        mp->PlayerObject::pushButton(PlayerButton::Jump);
        mp->m_fields->skip = false;
    }
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;
        if (!isEnabled())
            return true;

        CCScheduler::get()->scheduleOnce([this](float) {
            if (!m_player1) return;
            autoHold(m_player1);
            if (m_gameState.m_isDualMode && m_player2)
                autoHold(m_player2);
        }, this, 0.f, "reversed-inputs-autohold");

    // warn the player
        Notification::create("Reversed Inputs is ON! just warning tho", NotificationIcon::Warning)->show(); return true;
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

// eclipse menu + cheat registration
$on_game(Loaded) {
    if (!Loader::get()->isModLoaded("eclipse.eclipse-menu")) return;

    Loader::get()->queueInMainThread([]() {
        eclipse::modules::registerCheat("Reversed Inputs", []() {
            return isEnabled();
        });

        auto tab = eclipse::MenuTab::find("Reversed-Inputs");
        tab.addToggle("reversed-inputs-enabled", "Reversed Inputs", [](bool val) {
            Mod::get()->setSettingValue("reversed-inputs-enabled", val);
        }).setDescription("Hold to do nothing, tap to jump.");

        eclipse::config::set("reversed-inputs-enabled", Mod::get()->getSettingValue<bool>("reversed-inputs-enabled"));

        geode::listenForSettingChanges<bool>("reversed-inputs-enabled", [](bool val) {
            eclipse::config::set("reversed-inputs-enabled", val);
        });
    });
}
