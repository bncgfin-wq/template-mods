#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;

class $modify(BotPlayLayer, PlayLayer) {
    struct Fields {
        bool enabled = false;
        bool holding = false;
        bool finished = false;
        bool deathHandled = false;
        int frame = 0;
        int holdLeft = 0;
        int attempts = 0;
        int stuck = 0;
        int bestReached = 0;
        std::vector<char> cur;
        std::vector<char> best;
        std::mt19937 rng{std::random_device{}()};
        std::chrono::steady_clock::time_point lastInstant{};
        CCLabelBMFont* label = nullptr;
    };

    int randInt(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(m_fields->rng);
    }

    void notify(const char* text, bool ok) {
        Notification::create(text, ok ? NotificationIcon::Success : NotificationIcon::Error, 1.5f)->show();
    }

    std::filesystem::path macroPath() {
        int id = m_level->m_levelID.value();
        std::string key;
        if (id > 0) {
            key = std::to_string(id);
        } else {
            std::string name = m_level->m_levelName;
            for (char c : name) {
                if (std::isalnum(static_cast<unsigned char>(c))) key.push_back(c);
            }
            if (key.empty()) key = "local";
        }
        return Mod::get()->getSaveDir() / ("macro_" + key + ".txt");
    }

    bool saveMacro() {
        if (m_fields->best.empty()) return false;
        std::ofstream out(macroPath());
        if (!out) return false;
        std::string bits;
        bits.reserve(m_fields->best.size());
        for (char c : m_fields->best) bits.push_back(c ? '1' : '0');
        out << "GDBOT1\n" << m_fields->best.size() << "\n" << bits << "\n";
        return true;
    }

    bool loadMacro() {
        std::ifstream in(macroPath());
        if (!in) return false;
        std::string magic, bits;
        int reached = 0;
        in >> magic >> reached >> bits;
        if (magic != "GDBOT1" || bits.empty()) return false;
        m_fields->best.clear();
        for (char c : bits) m_fields->best.push_back(c == '1' ? 1 : 0);
        m_fields->bestReached = static_cast<int>(m_fields->best.size());
        m_fields->stuck = 0;
        return true;
    }

    void updateLabel() {
        if (!m_fields->label) return;
        std::string s = m_fields->finished ? "Bot: DONE!" : (m_fields->enabled ? "Bot: ON" : "Bot: OFF");
        s += "  Tries " + std::to_string(m_fields->attempts);
        s += "  Best " + std::to_string(m_fields->bestReached) + "f";
        s += "  Stuck " + std::to_string(m_fields->stuck);
        m_fields->label->setString(s.c_str());
    }

    void stopHold() {
        if (m_fields->holding) {
            m_player1->releaseButton(PlayerButton::Jump);
            m_fields->holding = false;
        }
    }

    void startBot() {
        m_fields->finished = false;
        m_fields->deathHandled = false;
        m_fields->attempts = 0;
        m_fields->stuck = 0;
        m_fields->bestReached = 0;
        m_fields->best.clear();
        m_fields->cur.clear();
        if (loadMacro()) notify("Saved macro loaded", true);
    }

    void prepareAttempt() {
        m_fields->frame = 0;
        m_fields->holdLeft = 0;
        m_fields->holding = false;
        m_fields->attempts++;

        int far = m_fields->bestReached;
        int cut = 0;
        if (far > 0) {
            int back;
            if (randInt(1, 100) <= 10) {
                back = randInt(0, 400);
            } else {
                back = 6 + randInt(0, 24 + m_fields->stuck * 3);
            }
            back = std::min(back, 400);
            cut = std::max(0, far - back);
        }
        cut = std::min(cut, static_cast<int>(m_fields->best.size()));
        m_fields->cur.assign(m_fields->best.begin(), m_fields->best.begin() + cut);
    }

    bool sampleWant() {
        auto* p = m_player1;
        if (p->m_isShip || p->m_isDart || p->m_isSwing) {
            int roll = randInt(1, 100);
            return m_fields->holding ? roll > 10 : roll <= 10;
        }
        if (m_fields->holdLeft > 0) {
            m_fields->holdLeft--;
            return true;
        }
        int chance;
        int maxHold;
        if (p->m_isBird) {
            chance = 8;
            maxHold = 1;
        } else if (p->m_isOnGround) {
            chance = 6;
            maxHold = 14;
        } else {
            chance = 1;
            maxHold = 6;
        }
        if (randInt(1, 100) <= chance) {
            m_fields->holdLeft = randInt(0, maxHold);
            return true;
        }
        return false;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        auto win = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setPosition(ccp(0.f, 0.f));

        auto botSpr = ButtonSprite::create("Bot");
        botSpr->setScale(0.6f);
        auto botBtn = CCMenuItemSpriteExtra::create(botSpr, this, menu_selector(BotPlayLayer::onToggleBot));
        botBtn->setPosition(ccp(40.f, win.height - 75.f));
        menu->addChild(botBtn);

        auto saveSpr = ButtonSprite::create("Save");
        saveSpr->setScale(0.6f);
        auto saveBtn = CCMenuItemSpriteExtra::create(saveSpr, this, menu_selector(BotPlayLayer::onSave));
        saveBtn->setPosition(ccp(100.f, win.height - 75.f));
        menu->addChild(saveBtn);

        auto loadSpr = ButtonSprite::create("Load");
        loadSpr->setScale(0.6f);
        auto loadBtn = CCMenuItemSpriteExtra::create(loadSpr, this, menu_selector(BotPlayLayer::onLoad));
        loadBtn->setPosition(ccp(160.f, win.height - 75.f));
        menu->addChild(loadBtn);

        this->addChild(menu, 1000);

        m_fields->label = CCLabelBMFont::create("Bot: OFF", "bigFont.fnt");
        m_fields->label->setScale(0.35f);
        m_fields->label->setAnchorPoint(ccp(0.f, 0.5f));
        m_fields->label->setPosition(ccp(15.f, win.height - 105.f));
        this->addChild(m_fields->label, 1000);
        updateLabel();
    }

    void onToggleBot(CCObject*) {
        m_fields->enabled = !m_fields->enabled;
        if (m_fields->enabled) {
            startBot();
            this->resetLevel();
        } else {
            stopHold();
            saveMacro();
            updateLabel();
        }
    }

    void onSave(CCObject*) {
        if (saveMacro()) notify("Macro saved", true);
        else notify("Nothing to save yet", false);
    }

    void onLoad(CCObject*) {
        if (!loadMacro()) {
            notify("No saved macro for this level", false);
            return;
        }
        notify("Macro loaded", true);
        if (m_fields->enabled) {
            m_fields->finished = false;
            this->resetLevel();
        } else {
            updateLabel();
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!m_fields->enabled || m_fields->finished) return;

        if (m_player1->m_isDead) {
            if (!m_fields->deathHandled) {
                m_fields->deathHandled = true;
                m_fields->lastInstant = std::chrono::steady_clock::now();
                this->resetLevel();
            }
            return;
        }

        bool want = false;
        if (m_fields->frame < static_cast<int>(m_fields->cur.size())) {
            want = m_fields->cur[m_fields->frame] != 0;
        } else {
            want = sampleWant();
            m_fields->cur.push_back(want ? 1 : 0);
        }

        if (want && !m_fields->holding) {
            m_player1->pushButton(PlayerButton::Jump);
            m_fields->holding = true;
        } else if (!want && m_fields->holding) {
            m_player1->releaseButton(PlayerButton::Jump);
            m_fields->holding = false;
        }
        m_fields->frame++;
    }

    void delayedResetLevel() {
        if (m_fields->enabled) {
            auto diff = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - m_fields->lastInstant).count();
            if (diff < 2.5) return;
        }
        PlayLayer::delayedResetLevel();
    }

    void resetLevel() {
        if (m_fields->enabled && !m_fields->finished && m_fields->frame > 5) {
            int reached = m_fields->frame;
            if (static_cast<int>(m_fields->cur.size()) > reached) {
                m_fields->cur.resize(reached);
            }
            if (reached > m_fields->bestReached) {
                m_fields->bestReached = reached;
                m_fields->best = m_fields->cur;
                m_fields->stuck = 0;
            } else {
                if (reached == m_fields->bestReached) {
                    m_fields->best = m_fields->cur;
                }
                m_fields->stuck++;
            }
        }

        PlayLayer::resetLevel();
        m_fields->holding = false;
        m_fields->deathHandled = false;

        if (m_fields->enabled && !m_fields->finished) {
            prepareAttempt();
        } else {
            m_fields->frame = 0;
        }
        updateLabel();
    }

    void levelComplete() {
        if (m_fields->enabled && !m_fields->finished) {
            m_fields->finished = true;
            stopHold();
            int n = std::min(static_cast<int>(m_fields->cur.size()), m_fields->frame);
            m_fields->best.assign(m_fields->cur.begin(), m_fields->cur.begin() + n);
            m_fields->bestReached = n;
            if (saveMacro()) notify("Level complete! Macro saved", true);
            updateLabel();
        }
        PlayLayer::levelComplete();
    }
};
