#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace geode::prelude;

namespace {
    struct Individual {
        std::vector<char> genome;
        int fitness = -1;
        bool evaluated = false;
    };
}

class $modify(BotPlayLayer, PlayLayer) {
    struct Fields {
        bool enabled = false;
        bool holding = false;
        bool finished = false;
        bool improved = false;
        int frame = 0;
        int holdLeft = 0;
        int attempts = 0;
        int stuck = 0;
        int bestReached = 0;
        int popSize = 10;
        int gen = 1;
        int idx = 0;
        std::vector<char> cur;
        std::vector<char> bestGenome;
        std::vector<Individual> pop;
        std::mt19937 rng{std::random_device{}()};
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
        if (m_fields->bestGenome.empty()) return false;
        std::ofstream out(macroPath());
        if (!out) return false;
        std::string bits;
        bits.reserve(m_fields->bestGenome.size());
        for (char c : m_fields->bestGenome) bits.push_back(c ? '1' : '0');
        out << "GDBOT1\n" << m_fields->bestGenome.size() << "\n" << bits << "\n";
        return true;
    }

    bool loadMacro() {
        std::ifstream in(macroPath());
        if (!in) return false;
        std::string magic, bits;
        int reached = 0;
        in >> magic >> reached >> bits;
        if (magic != "GDBOT1" || bits.empty()) return false;
        m_fields->bestGenome.clear();
        for (char c : bits) m_fields->bestGenome.push_back(c == '1' ? 1 : 0);
        m_fields->bestReached = static_cast<int>(m_fields->bestGenome.size());
        return true;
    }

    void updateLabel() {
        if (!m_fields->label) return;
        std::string s = m_fields->finished ? "Bot: DONE!" : (m_fields->enabled ? "Bot: ON" : "Bot: OFF");
        s += "  Pop " + std::to_string(m_fields->popSize);
        if (m_fields->enabled) {
            s += "  Gen " + std::to_string(m_fields->gen);
            s += "  #" + std::to_string(m_fields->idx + 1) + "/" + std::to_string(m_fields->popSize);
        }
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

    Individual makeChild(const Individual& parent) {
        Individual child;
        int far = static_cast<int>(parent.genome.size());
        int back;
        if (randInt(1, 100) <= 10) {
            back = randInt(0, 400);
        } else {
            back = 6 + randInt(0, 24 + m_fields->stuck * 3);
        }
        back = std::min(back, 400);
        int cut = std::max(0, far - back);
        child.genome.assign(parent.genome.begin(), parent.genome.begin() + cut);
        return child;
    }

    void skipEvaluated() {
        while (m_fields->idx < static_cast<int>(m_fields->pop.size()) &&
               m_fields->pop[m_fields->idx].evaluated) {
            m_fields->idx++;
        }
    }

    void initPopulation() {
        m_fields->pop.assign(m_fields->popSize, Individual{});
        m_fields->idx = 0;
        m_fields->gen = 1;
        m_fields->improved = false;
        if (!m_fields->bestGenome.empty()) {
            Individual seed;
            seed.genome = m_fields->bestGenome;
            seed.fitness = static_cast<int>(seed.genome.size());
            seed.evaluated = true;
            m_fields->pop[0] = seed;
            for (int i = 1; i < m_fields->popSize; i++) {
                m_fields->pop[i] = makeChild(seed);
            }
        }
        skipEvaluated();
    }

    void nextGeneration() {
        std::sort(m_fields->pop.begin(), m_fields->pop.end(),
            [](const Individual& a, const Individual& b) { return a.fitness > b.fitness; });

        if (m_fields->improved) m_fields->stuck = 0;
        else m_fields->stuck++;
        m_fields->improved = false;

        int K = std::max(2, m_fields->popSize / 5);
        K = std::min(K, static_cast<int>(m_fields->pop.size()));
        std::vector<Individual> next;
        next.reserve(m_fields->popSize);
        for (int i = 0; i < K; i++) {
            next.push_back(m_fields->pop[i]);
            next.back().evaluated = true;
        }
        while (static_cast<int>(next.size()) < m_fields->popSize) {
            int a = randInt(0, K - 1);
            int b = randInt(0, K - 1);
            Individual child = makeChild(next[std::min(a, b)]);
            next.push_back(std::move(child));
        }
        m_fields->pop = std::move(next);
        m_fields->idx = 0;
        m_fields->gen++;
        skipEvaluated();
    }

    void finishAttempt() {
        int reached = m_fields->frame;
        if (static_cast<int>(m_fields->cur.size()) > reached) {
            m_fields->cur.resize(reached);
        }
        if (m_fields->pop.empty() || m_fields->idx >= static_cast<int>(m_fields->pop.size())) return;

        Individual& ind = m_fields->pop[m_fields->idx];
        ind.genome = m_fields->cur;
        ind.fitness = reached;
        ind.evaluated = true;

        if (reached > m_fields->bestReached) {
            m_fields->bestReached = reached;
            m_fields->bestGenome = m_fields->cur;
            m_fields->improved = true;
        }

        m_fields->idx++;
        skipEvaluated();
        if (m_fields->idx >= static_cast<int>(m_fields->pop.size())) {
            nextGeneration();
        }
    }

    void startBot() {
        m_fields->finished = false;
        m_fields->attempts = 0;
        m_fields->stuck = 0;
        m_fields->frame = 0;
        m_fields->holding = false;
        m_fields->bestReached = 0;
        m_fields->bestGenome.clear();
        m_fields->cur.clear();
        if (loadMacro()) notify("Saved macro loaded", true);
        initPopulation();
    }

    void prepareAttempt() {
        m_fields->frame = 0;
        m_fields->holdLeft = 0;
        m_fields->holding = false;
        m_fields->attempts++;
        if (m_fields->idx < static_cast<int>(m_fields->pop.size())) {
            m_fields->cur = m_fields->pop[m_fields->idx].genome;
        } else {
            m_fields->cur.clear();
        }
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

        auto popSpr = ButtonSprite::create("Pop");
        popSpr->setScale(0.6f);
        auto popBtn = CCMenuItemSpriteExtra::create(popSpr, this, menu_selector(BotPlayLayer::onPop));
        popBtn->setPosition(ccp(220.f, win.height - 75.f));
        menu->addChild(popBtn);

        this->addChild(menu, 1000);

        m_fields->label = CCLabelBMFont::create("Bot: OFF", "bigFont.fnt");
        m_fields->label->setScale(0.3f);
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
            m_fields->stuck = 0;
            initPopulation();
            m_fields->frame = 0;
            this->resetLevel();
        } else {
            updateLabel();
        }
    }

    void onPop(CCObject*) {
        m_fields->popSize = (m_fields->popSize == 10) ? 50 : 10;
        if (m_fields->enabled && !m_fields->finished) {
            initPopulation();
            m_fields->frame = 0;
            this->resetLevel();
        } else {
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
            finishAttempt();
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
        if (m_fields->enabled && !m_fields->finished) {
            m_fields->finished = true;
            stopHold();
            int n = std::min(static_cast<int>(m_fields->cur.size()), m_fields->frame);
            m_fields->bestGenome.assign(m_fields->cur.begin(), m_fields->cur.begin() + n);
            m_fields->bestReached = n;
            if (saveMacro()) notify("Level complete! Macro saved", true);
            updateLabel();
        }
        PlayLayer::levelComplete();
    }
};
