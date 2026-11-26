#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;

class $modify(BotPlayLayer, PlayLayer) {
    struct Fields {
        bool enabled = false;
        bool holding = false;
        bool finished = false;
        int frame = 0;
        int holdLeft = 0;
        int attempts = 0;
        int bestReached = 0;
        std::vector<char> cur;
        std::vector<char> best;
        std::mt19937 rng{std::random_device{}()};
        CCLabelBMFont* label = nullptr;
    };

    int randInt(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(m_fields->rng);
    }

    void updateLabel() {
        if (!m_fields->label) return;
        std::string s = m_fields->finished ? "Bot: SELESAI!" : (m_fields->enabled ? "Bot: ON" : "Bot: OFF");
        s += "  Coba " + std::to_string(m_fields->attempts);
        s += "  Terjauh " + std::to_string(m_fields->bestReached) + "f";
        m_fields->label->setString(s.c_str());
    }

    void stopHold() {
        if (m_fields->holding) {
            m_player1->releaseButton(PlayerButton::Jump);
            m_fields->holding = false;
        }
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

        auto menu = CCMenu::create();
        menu->setPosition(ccp(0.f, 0.f));
        auto spr = ButtonSprite::create("Bot");
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BotPlayLayer::onToggleBot));
        btn->setPosition(ccp(45.f, win.height - 75.f));
        menu->addChild(btn);
        this->addChild(menu, 1000);

        m_fields->label = CCLabelBMFont::create("Bot: OFF", "bigFont.fnt");
        m_fields->label->setScale(0.35f);
        m_fields->label->setAnchorPoint(ccp(0.f, 0.5f));
        m_fields->label->setPosition(ccp(85.f, win.height - 75.f));
        this->addChild(m_fields->label, 1000);
        updateLabel();
    }

    void onToggleBot(CCObject*) {
        m_fields->enabled = !m_fields->enabled;
        if (m_fields->enabled) {
            m_fields->finished = false;
            m_fields->attempts = 0;
            m_fields->bestReached = 0;
            m_fields->best.clear();
            m_fields->cur.clear();
            this->resetLevel();
        } else {
            stopHold();
            updateLabel();
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!m_fields->enabled || m_fields->finished) return;
        if (m_player1->m_isDead) return;

        bool want = false;
        if (m_fields->frame < static_cast<int>(m_fields->cur.size())) {
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
        if (m_fields->enabled) {
            m_fields->finished = true;
            stopHold();
            updateLabel();
        }
        PlayLayer::levelComplete();
    }
};
