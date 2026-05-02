#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <eclipse.eclipse-menu/include/components.hpp>
#include <eclipse.eclipse-menu/include/config.hpp>
#include <eclipse.eclipse-menu/include/labels.hpp>
#include <eclipse.eclipse-menu/include/modules.hpp>

using namespace geode::prelude;

static bool isEnabled() {
    return Mod::get()->getSettingValue<bool>("reversed-inputs-enabled");
}
static bool affectsPlayer1() {
    return Mod::get()->getSettingValue<bool>("affect-player1");
}
static bool affectsPlayer2() {
    return Mod::get()->getSettingValue<bool>("affect-player2");
}
static bool allowedInPractice() {
    return Mod::get()->getSettingValue<bool>("allow-in-practice");
}

static void updateRiftVariable() {
    eclipse::label::setVariable<bool>("isReversedInputs", isEnabled());
}

// swap push/release
class $modify(MyPlayerObject, PlayerObject) {
    struct Fields {
        bool skip = false;
    };

    bool isAffectedByMod() const {
        auto* gjbgl = GJBaseGameLayer::get();
        if (!gjbgl || !gjbgl->m_player1) return false;
        auto* pl = PlayLayer::get();
        if (pl && pl->m_isPracticeMode && !allowedInPractice()) return false;
        if (this == gjbgl->m_player1) return affectsPlayer1();
        if (gjbgl->m_gameState.m_isDualMode && this == gjbgl->m_player2) return affectsPlayer2();
        return false;
    }

    bool pushButton(PlayerButton btn) {
        if (!isEnabled() || m_fields->skip || (m_isPlatformer && (btn == PlayerButton::Left || btn == PlayerButton::Right)))
            return PlayerObject::pushButton(btn);
        // check for playlayer nullptr presence before running hooks
        if (!isAffectedByMod())
            return PlayerObject::pushButton(btn);
        // player press = release
        m_fields->skip = true;
        bool r = PlayerObject::releaseButton(btn);
        m_fields->skip = false;
        return r;
    }
    bool releaseButton(PlayerButton btn) {
        if (!isEnabled() || m_fields->skip || (m_isPlatformer && (btn == PlayerButton::Left || btn == PlayerButton::Right)))
            return PlayerObject::releaseButton(btn);
        // check for playlayer nullptr presence before running hooks
        if (!isAffectedByMod())
            return PlayerObject::releaseButton(btn);
        // player release = press
        m_fields->skip = true;
        bool r = PlayerObject::pushButton(btn);
        m_fields->skip = false;
        return r;
    }

    static void forceHold(PlayerObject* p) {
        if (!p) return;
        auto* mp = static_cast<MyPlayerObject*>(p);
        mp->m_fields->skip = true;
        mp->PlayerObject::pushButton(PlayerButton::Jump);
        mp->m_fields->skip = false;
    }

    static void forceRelease(PlayerObject* p) {
        if (!p) return;
        auto* mp = static_cast<MyPlayerObject*>(p);
        mp->m_fields->skip = true;
        mp->PlayerObject::releaseButton(PlayerButton::Jump);
        mp->m_fields->skip = false;
    }
};

// force hold state at level start so the player isn't just hanging(SAYORI REFERENCE!?!?) there
class $modify(MyPlayLayer, PlayLayer) {
    void doAutoHold(float) {
        if (m_player1 && affectsPlayer1()) MyPlayerObject::forceHold(m_player1);
        if (m_gameState.m_isDualMode && m_player2 && affectsPlayer2()) MyPlayerObject::forceHold(m_player2);
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
            "Reversed Inputs is ON! just warning tho",
            NotificationIcon::Warning
        )->show();
        return true;
    }
};

// keybind to toggle the mod on/off
$on_game(Loaded) {
    listenForKeybindSettingPresses("toggle-keybind", [](Keybind const&, bool down, bool repeat, double) {
        if (!down || repeat) return;
        auto mod = Mod::get();
        bool newVal = !mod->getSettingValue<bool>("reversed-inputs-enabled");
        mod->setSettingValue("reversed-inputs-enabled", newVal);

        // if toggled off mid-level, release any forced hold so players aren't stuck
        if (!newVal) {
            auto* gjbgl = GJBaseGameLayer::get();
            if (gjbgl) {
                if (gjbgl->m_player1) MyPlayerObject::forceRelease(gjbgl->m_player1);
                if (gjbgl->m_gameState.m_isDualMode && gjbgl->m_player2) MyPlayerObject::forceRelease(gjbgl->m_player2);
            }
        }
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
            // if toggled off mid-level, release any forced hold so players aren't stuck
            if (!val) {
                auto* gjbgl = GJBaseGameLayer::get();
                if (gjbgl) {
                    if (gjbgl->m_player1) MyPlayerObject::forceRelease(gjbgl->m_player1);
                    if (gjbgl->m_gameState.m_isDualMode && gjbgl->m_player2) MyPlayerObject::forceRelease(gjbgl->m_player2);
                }
            }
        }).setDescription("Hold to do nothing, tap to jump.");

        tab.addToggle("allow-in-practice", "Allow in Practice Mode", [](bool val) {
            Mod::get()->setSettingValue("allow-in-practice", val);
        }).setDescription("Whether reversed inputs applies in practice mode.");

        tab.addLabel("Affect Players");
        tab.addToggle("affect-player1", "P1", [](bool val) {
            Mod::get()->setSettingValue("affect-player1", val);
        });
        tab.addToggle("affect-player2", "P2", [](bool val) {
            Mod::get()->setSettingValue("affect-player2", val);
        });

        // sync all settings into eclipse config
        auto mod = Mod::get();
        eclipse::config::set("reversed-inputs-enabled", mod->getSettingValue<bool>("reversed-inputs-enabled"));
        eclipse::config::set("allow-in-practice", mod->getSettingValue<bool>("allow-in-practice"));
        eclipse::config::set("affect-player1", mod->getSettingValue<bool>("affect-player1"));
        eclipse::config::set("affect-player2", mod->getSettingValue<bool>("affect-player2"));

        geode::listenForSettingChanges<bool>("reversed-inputs-enabled", [](bool val) {
            eclipse::config::set("reversed-inputs-enabled", val);
            updateRiftVariable();
        });
        geode::listenForSettingChanges<bool>("allow-in-practice", [](bool val) {
            eclipse::config::set("allow-in-practice", val);
        });
        geode::listenForSettingChanges<bool>("affect-player1", [](bool val) {
            eclipse::config::set("affect-player1", val);
        });
        geode::listenForSettingChanges<bool>("affect-player2", [](bool val) {
            eclipse::config::set("affect-player2", val);
        });

        // init rift variable
        updateRiftVariable();
    });
}
