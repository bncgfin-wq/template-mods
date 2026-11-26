#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <algorithm>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    bool g_botEnabled = false;
}

class $modify(BotPlayLayer, PlayLayer) {
    struct Fields {
        bool enabled = false;
        bool holding = false;
        bool finished = false;
        int frame = 0;
        int holdLeft = 0;
        int attempts = 0;
        int stuck = 0;
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
        m_fields->label->setVisible(m_fields->enabled);
        std::string s = m_fields->finished ? "Bot: SELESAI!" : "Bot: ON";
        s += "  Coba " + std::to_string(m_fields->attempts);
        s += "  Terjauh " + std::to_string(m_fields->bestReached) + "f";
        s += "  Macet " + std::to_string(m_fields->stuck);
        m_fields->label->setString(s.c_str());
    }

    void stopHold() {
        if (m_fields->holding) {
            m_player1->releaseButton(PlayerButton::Jump);
            m_fields->holding = false;
        }
    }

    void startBot() {
        m_fields->enabled = true;
        m_fields->finished = false;
        m_fields->attempts = 0;
        m_fields->stuck = 0;
        m_fields->bestReached = 0;
        m_fields->best.clear();
        m_fields->cur.clear();
    }

    void prepareAttempt() {
        m_fields->frame = 0;
        m_fields->holdLeft = 0;
        m_fields->holding = false;
        m_fields->attempts++;

        int far = m_fields->bestReached;
        int cut = 0;
        if (far > 0) {
            if (randInt(1, 100) <= 15) {
                cut = randInt(std::max(0, far - 600), far);
            } else {
                int window = 10 + randInt(0, 20 + m_fields->stuck * 4);
                window = std::min(window, 400);
                cut = std::max(0, far - window);
            }
        }
        cut = std::min(cut, static_cast<int>(m_fields->best.size()));
        m_fields->cur.assign(m_fields->best.begin(), m_fields->best.begin() + cut);
    }

    bool sampleWant() {
        auto* p = m_player1;
        bool holdMode = p->m_isShip || p->m_isDart || p->m_isSwing;
        if (holdMode) {
            int roll = randInt(1, 100);
            if (m_fields->holding) return roll > 12;
            return roll <= 12;
        }
        if (m_fields->holdLeft > 0) {
            m_fields->holdLeft--;
            return true;
        }
        int chance = 1;
        int maxHold = 14;
        if (p->m_isBird) {
            chance = 10;
            maxHold = 2;
        } else {
            chance = p->m_isOnGround ? 6 : 1;
        }
        if (randInt(1, 100) <= chance) {
            m_fields->holdLeft = randInt(0, maxHold);
            return true;
        }
        return false;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        g_botEnabled = false;
        auto win = CCDirector::sharedDirector()->getWinSize();

        m_fields->label = CCLabelBMFont::create("Bot: OFF", "bigFont.fnt");
        m_fields->label->setScale(0.35f);
        m_fields->label->setAnchorPoint(ccp(0.f, 0.5f));
        m_fields->label->setPosition(ccp(15.f, win.height - 75.f));
        this->addChild(m_fields->label, 1000);
        updateLabel();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!g_botEnabled) {
            if (m_fields->enabled) {
                m_fields->enabled = false;
                stopHold();
                updateLabel();
            }
            return;
        }
        if (!m_fields->enabled || m_fields->finished) return;
        if (m_player1->m_isDead) return;

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

        if (g_botEnabled && !m_fields->enabled) {
            startBot();
        } else if (!g_botEnabled && m_fields->enabled) {
            m_fields->enabled = false;
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

class $modify(BotPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = CCMenu::create();
        menu->setPosition(ccp(0.f, 0.f));

        auto toggler = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(BotPauseLayer::onBotToggle), 0.8f);
        toggler->toggle(g_botEnabled);
        toggler->setPosition(ccp(40.f, 45.f));
        menu->addChild(toggler);

        auto label = CCLabelBMFont::create("Bot", "bigFont.fnt");
        label->setScale(0.5f);
        label->setAnchorPoint(ccp(0.f, 0.5f));
        label->setPosition(ccp(65.f, 45.f));
        menu->addChild(label);

        this->addChild(menu, 100);
    }

    void onBotToggle(CCObject* sender) {
        auto toggler = static_cast<CCMenuItemToggler*>(sender);
        bool on = !toggler->isToggled();
        g_botEnabled = on;
        if (on) {
            this->onRestart(nullptr);
        }
    }
};
