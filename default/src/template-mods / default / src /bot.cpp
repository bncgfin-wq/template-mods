#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cctype>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace geode::prelude;

class $modify(BotPlayLayer, PlayLayer) {
    struct Fields {
        bool enabled = false;
        bool playing = false;
        bool holding = false;
        bool finished = false;
        int frame = 0;
        int holdLeft = 0;
        int attempts = 0;
        int bestReached = 0;
        int sel = 0;
        std::vector<char> cur;
        std::vector<char> best;
        std::vector<char> play;
        std::mt19937 rng{std::random_device{}()};
        CCLabelBMFont* label = nullptr;
        CCLabelBMFont* selLabel = nullptr;
    };

    int randInt(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(m_fields->rng);
    }

    void notify(const std::string& text, bool ok) {
        Notification::create(text, ok ? NotificationIcon::Success : NotificationIcon::Error, 1.5f)->show();
    }

    std::string levelKey() {
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
        return key;
    }

    std::vector<std::pair<std::string, std::string>> readList() {
        std::vector<std::pair<std::string, std::string>> out;
        std::string raw = Mod::get()->getSavedValue<std::string>("list_" + levelKey(), "");
        size_t start = 0;
        while (start < raw.size()) {
            size_t end = raw.find('\n', start);
            if (end == std::string::npos) end = raw.size();
            std::string line = raw.substr(start, end - start);
            size_t bar = line.find('|');
            if (bar != std::string::npos && bar + 1 < line.size()) {
                out.push_back({line.substr(0, bar), line.substr(bar + 1)});
            }
            start = end + 1;
        }
        return out;
    }

    void writeList(const std::vector<std::pair<std::string, std::string>>& list) {
        std::string raw;
        for (auto& e : list) {
            raw += e.first + "|" + e.second + "\n";
        }
        Mod::get()->setSavedValue<std::string>("list_" + levelKey(), raw);
        Mod::get()->saveData();
    }

    void clampSel(size_t n) {
        if (n == 0) {
            m_fields->sel = 0;
        } else if (m_fields->sel >= static_cast<int>(n)) {
            m_fields->sel = static_cast<int>(n) - 1;
        } else if (m_fields->sel < 0) {
            m_fields->sel = 0;
        }
    }

    void refreshSel() {
        if (!m_fields->selLabel) return;
        auto list = readList();
        clampSel(list.size());
        if (list.empty()) {
            m_fields->selLabel->setString("Saved: none");
            return;
        }
        auto& e = list[m_fields->sel];
        std::string s = "Saved: " + e.first + " (" + std::to_string(e.second.size()) + "f) [" +
            std::to_string(m_fields->sel + 1) + "/" + std::to_string(list.size()) + "]";
        m_fields->selLabel->setString(s.c_str());
    }

    void updateLabel() {
        if (!m_fields->label) return;
        std::string s;
        if (m_fields->playing) s = "Bot: PLAY";
        else if (m_fields->finished) s = "Bot: DONE!";
        else if (m_fields->enabled) s = "Bot: ON";
        else s = "Bot: OFF";
        s += "  Tries " + std::to_string(m_fields->attempts);
        s += "  Best " + std::to_string(m_fields->bestReached) + "f";
        m_fields->label->setString(s.c_str());
    }

    void stopHold() {
        if (m_fields->holding) {
            m_player1->releaseButton(PlayerButton::Jump);
            m_fields->holding = false;
        }
    }

    std::string saveBest() {
        if (m_fields->best.empty()) return "";
        auto list = readList();
        int n = Mod::get()->getSavedValue<int>("cnt_" + levelKey(), 0) + 1;
        Mod::get()->setSavedValue<int>("cnt_" + levelKey(), n);
        std::string name = "Macro " + std::to_string(n);
        std::string bits;
        bits.reserve(m_fields->best.size());
        for (char c : m_fields->best) bits.push_back(c ? '1' : '0');
        list.push_back({name, bits});
        writeList(list);
        m_fields->sel = static_cast<int>(list.size()) - 1;
        refreshSel();
        return name + " (" + std::to_string(bits.size()) + "f)";
    }

    void prepareAttempt() {
        m_fields->frame = 0;
        m_fields->holdLeft = 0;
        m_fields->holding = false;
        m_fields->attempts++;
        int cut = std::max(0, m_fields->bestReached - randInt(15, 90));
        cut = std::min(cut, static_cast<int>(m_fields->best.size()));
        m_fields->cur.assign(m_fields->best.begin(), m_fields->best.begin() + cut);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        auto win = CCDirector::sharedDirector()->getWinSize();
        float y1 = win.height - 75.f;
        float y2 = win.height - 100.f;

        auto menu = CCMenu::create();
        menu->setPosition(ccp(0.f, 0.f));

        auto addBtn = [&](const char* text, SEL_MenuHandler cb, float x, float y) {
            auto spr = ButtonSprite::create(text);
            spr->setScale(0.6f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
            btn->setPosition(ccp(x, y));
            menu->addChild(btn);
        };

        addBtn("Bot", menu_selector(BotPlayLayer::onToggleBot), 40.f, y1);
        addBtn("Save", menu_selector(BotPlayLayer::onSave), 100.f, y1);
        addBtn("Load", menu_selector(BotPlayLayer::onLoad), 160.f, y1);
        addBtn("Del", menu_selector(BotPlayLayer::onDelete), 220.f, y1);
        addBtn("Prev", menu_selector(BotPlayLayer::onPrev), 40.f, y2);
        addBtn("Next", menu_selector(BotPlayLayer::onNext), 100.f, y2);
        addBtn("Play", menu_selector(BotPlayLayer::onPlay), 160.f, y2);

        this->addChild(menu, 1000);

        m_fields->label = CCLabelBMFont::create("Bot: OFF", "bigFont.fnt");
        m_fields->label->setScale(0.3f);
        m_fields->label->setAnchorPoint(ccp(0.f, 0.5f));
        m_fields->label->setPosition(ccp(15.f, win.height - 125.f));
        this->addChild(m_fields->label, 1000);

        m_fields->selLabel = CCLabelBMFont::create("Saved: none", "bigFont.fnt");
        m_fields->selLabel->setScale(0.3f);
        m_fields->selLabel->setAnchorPoint(ccp(0.f, 0.5f));
        m_fields->selLabel->setPosition(ccp(15.f, win.height - 142.f));
        this->addChild(m_fields->selLabel, 1000);

        updateLabel();
        refreshSel();
    }

    void onToggleBot(CCObject*) {
        m_fields->playing = false;
        m_fields->enabled = !m_fields->enabled;
        if (m_fields->enabled) {
            m_fields->finished = false;
            m_fields->attempts = 0;
            m_fields->bestReached = static_cast<int>(m_fields->best.size());
            m_fields->cur.clear();
            this->resetLevel();
        } else {
            stopHold();
            updateLabel();
        }
    }

    void onSave(CCObject*) {
        std::string name = saveBest();
        if (name.empty()) notify("Nothing to save yet", false);
        else notify("Saved: " + name, true);
    }

    void onLoad(CCObject*) {
        auto list = readList();
        if (list.empty()) {
            notify("No saved macros for this level", false);
            return;
        }
        clampSel(list.size());
        auto& e = list[m_fields->sel];
        m_fields->best.clear();
        for (char c : e.second) m_fields->best.push_back(c == '1' ? 1 : 0);
        m_fields->bestReached = static_cast<int>(m_fields->best.size());
        notify("Loaded: " + e.first, true);
        if (m_fields->enabled) {
            m_fields->finished = false;
            this->resetLevel();
        } else {
            updateLabel();
        }
    }

    void onDelete(CCObject*) {
        auto list = readList();
        if (list.empty()) {
            notify("No saved macros for this level", false);
            return;
        }
        clampSel(list.size());
        std::string name = list[m_fields->sel].first;
        list.erase(list.begin() + m_fields->sel);
        writeList(list);
        clampSel(list.size());
        refreshSel();
        notify("Deleted: " + name, true);
    }

    void onPrev(CCObject*) {
        auto list = readList();
        if (list.empty()) return;
        int n = static_cast<int>(list.size());
        m_fields->sel = (m_fields->sel + n - 1) % n;
        refreshSel();
    }

    void onNext(CCObject*) {
        auto list = readList();
        if (list.empty()) return;
        int n = static_cast<int>(list.size());
        m_fields->sel = (m_fields->sel + 1) % n;
        refreshSel();
    }

    void onPlay(CCObject*) {
        if (m_fields->playing) {
            m_fields->playing = false;
            stopHold();
            updateLabel();
            return;
        }
        auto list = readList();
        if (list.empty()) {
            notify("No saved macros for this level", false);
            return;
        }
        clampSel(list.size());
        auto& e = list[m_fields->sel];
        m_fields->play.clear();
        for (char c : e.second) m_fields->play.push_back(c == '1' ? 1 : 0);
        m_fields->enabled = false;
        stopHold();
        m_fields->playing = true;
        m_fields->finished = false;
        notify("Replaying: " + e.first, true);
        this->resetLevel();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (m_fields->finished) return;
        if (!m_fields->enabled && !m_fields->playing) return;
        if (m_player1->m_isDead) return;

        bool want = false;
        if (m_fields->playing) {
            if (m_fields->frame < static_cast<int>(m_fields->play.size())) {
                want = m_fields->play[m_fields->frame] != 0;
            }
        } else if (m_fields->frame < static_cast<int>(m_fields->cur.size())) {
            want = m_fields->cur[m_fields->frame] != 0;
        } else {
            if (m_fields->holdLeft > 0) {
                want = true;
                m_fields->holdLeft--;
            } else if (randInt(1, 100) <= 4) {
                m_fields->holdLeft = randInt(2, 14);
                want = true;
            }
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

    void resetLevel() {
        if (m_fields->enabled && !m_fields->finished && m_fields->frame > 5) {
            int reached = m_fields->frame;
            if (static_cast<int>(m_fields->cur.size()) > reached) {
                m_fields->cur.resize(reached);
            }
            if (reached >= m_fields->bestReached) {
                m_fields->bestReached = reached;
                m_fields->best = m_fields->cur;
            }
        }
        PlayLayer::resetLevel();
        m_fields->holding = false;
        if (m_fields->enabled && !m_fields->finished) {
            prepareAttempt();
        } else {
            m_fields->frame = 0;
        }
        updateLabel();
    }

    void levelComplete() {
        if ((m_fields->enabled || m_fields->playing) && !m_fields->finished) {
            bool wasLearning = m_fields->enabled;
            m_fields->finished = true;
            stopHold();
            if (wasLearning) {
                int n = std::min(static_cast<int>(m_fields->cur.size()), m_fields->frame);
                m_fields->best.assign(m_fields->cur.begin(), m_fields->cur.begin() + n);
                m_fields->bestReached = n;
                std::string name = saveBest();
                if (!name.empty()) notify("Level complete! Saved: " + name, true);
            } else {
                notify("Replay finished", true);
            }
            updateLabel();
        }
        PlayLayer::levelComplete();
    }
};
