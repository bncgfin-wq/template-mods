#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>
#include <utility>

using namespace geode::prelude;

namespace {
    const int KN[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    const int DIRS[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
}

struct ChessBoard {
    char b[8][8];

    void reset() {
        const char* back = "RNBQKBNR";
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++) b[r][c] = '.';
        for (int c = 0; c < 8; c++) {
            b[0][c] = back[c];
            b[1][c] = 'P';
            b[6][c] = 'p';
            b[7][c] = static_cast<char>(std::tolower(back[c]));
        }
    }

    static bool inside(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
    static bool isWhite(char p) { return p >= 'A' && p <= 'Z'; }
    static bool isBlack(char p) { return p >= 'a' && p <= 'z'; }
    static bool sameSide(char p, bool white) { return white ? isWhite(p) : isBlack(p); }

    bool attacked(int r, int c, bool byWhite) const {
        int pr = byWhite ? r - 1 : r + 1;
        char pawn = byWhite ? 'P' : 'p';
        for (int dc = -1; dc <= 1; dc += 2)
            if (inside(pr, c + dc) && b[pr][c + dc] == pawn) return true;

        char knight = byWhite ? 'N' : 'n';
        for (int i = 0; i < 8; i++) {
            int rr = r + KN[i][0], cc = c + KN[i][1];
            if (inside(rr, cc) && b[rr][cc] == knight) return true;
        }

        char king = byWhite ? 'K' : 'k';
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int rr = r + dr, cc = c + dc;
                if (inside(rr, cc) && b[rr][cc] == king) return true;
            }

        char rook = byWhite ? 'R' : 'r';
        char bishop = byWhite ? 'B' : 'b';
        char queen = byWhite ? 'Q' : 'q';
        for (int i = 0; i < 8; i++) {
            bool straight = i < 4;
            int rr = r + DIRS[i][0], cc = c + DIRS[i][1];
            while (inside(rr, cc)) {
                char p = b[rr][cc];
                if (p != '.') {
                    if (p == queen) return true;
                    if (straight && p == rook) return true;
                    if (!straight && p == bishop) return true;
                    break;
                }
                rr += DIRS[i][0];
                cc += DIRS[i][1];
            }
        }
        return false;
    }

    bool inCheck(bool white) const {
        char king = white ? 'K' : 'k';
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (b[r][c] == king) return attacked(r, c, !white);
        return false;
    }

    std::vector<std::pair<int, int>> pseudo(int r, int c) const {
        std::vector<std::pair<int, int>> out;
        char p = b[r][c];
        if (p == '.') return out;
        bool white = isWhite(p);
        char t = static_cast<char>(std::tolower(p));

        if (t == 'p') {
            int d = white ? 1 : -1;
            int startRow = white ? 1 : 6;
            if (inside(r + d, c) && b[r + d][c] == '.') {
                out.push_back({r + d, c});
                if (r == startRow && b[r + 2 * d][c] == '.') out.push_back({r + 2 * d, c});
            }
            for (int dc = -1; dc <= 1; dc += 2) {
                int rr = r + d, cc = c + dc;
                if (inside(rr, cc) && b[rr][cc] != '.' && !sameSide(b[rr][cc], white))
                    out.push_back({rr, cc});
            }
        } else if (t == 'n') {
            for (int i = 0; i < 8; i++) {
                int rr = r + KN[i][0], cc = c + KN[i][1];
                if (inside(rr, cc) && !sameSide(b[rr][cc], white)) out.push_back({rr, cc});
            }
        } else if (t == 'k') {
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int rr = r + dr, cc = c + dc;
                    if (inside(rr, cc) && !sameSide(b[rr][cc], white)) out.push_back({rr, cc});
                }
        } else {
            for (int i = 0; i < 8; i++) {
                bool straight = i < 4;
                if (t == 'r' && !straight) continue;
                if (t == 'b' && straight) continue;
                int rr = r + DIRS[i][0], cc = c + DIRS[i][1];
                while (inside(rr, cc)) {
                    if (b[rr][cc] == '.') {
                        out.push_back({rr, cc});
                    } else {
                        if (!sameSide(b[rr][cc], white)) out.push_back({rr, cc});
                        break;
                    }
                    rr += DIRS[i][0];
                    cc += DIRS[i][1];
                }
            }
        }
        return out;
    }

    void move(int r, int c, int rr, int cc) {
        char p = b[r][c];
        b[r][c] = '.';
        if (p == 'P' && rr == 7) p = 'Q';
        if (p == 'p' && rr == 0) p = 'q';
        b[rr][cc] = p;
    }

    std::vector<std::pair<int, int>> legal(int r, int c) const {
        std::vector<std::pair<int, int>> out;
        bool white = isWhite(b[r][c]);
        for (auto m : pseudo(r, c)) {
            ChessBoard n = *this;
            n.move(r, c, m.first, m.second);
            if (!n.inCheck(white)) out.push_back(m);
        }
        return out;
    }

    bool hasAnyMove(bool white) const {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (sameSide(b[r][c], white) && !legal(r, c).empty()) return true;
        return false;
    }
};

class ChessLayer : public CCLayer {
protected:
    ChessBoard m_board;
    bool m_whiteTurn = true;
    bool m_over = false;
    int m_selR = -1;
    int m_selC = -1;
    std::vector<std::pair<int, int>> m_moves;
    CCNode* m_boardNode = nullptr;
    CCNode* m_dynamic = nullptr;
    CCLabelBMFont* m_status = nullptr;
    float m_sq = 34.f;

    bool init() override {
        if (!CCLayer::init()) return false;
        m_board.reset();
        auto win = CCDirector::sharedDirector()->getWinSize();

        auto bg = CCLayerColor::create(ccColor4B{25, 25, 40, 255});
        this->addChild(bg);

        m_boardNode = CCNode::create();
        m_boardNode->setPosition(ccp((win.width - 8 * m_sq) / 2.f, 12.f));
        this->addChild(m_boardNode);

        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                bool light = (r + c) % 2 == 1;
                ccColor4B col = light ? ccColor4B{200, 170, 120, 255} : ccColor4B{120, 85, 55, 255};
                auto sq = CCLayerColor::create(col, m_sq, m_sq);
                sq->setPosition(ccp(c * m_sq, r * m_sq));
                m_boardNode->addChild(sq);
            }
        }

        m_dynamic = CCNode::create();
        m_boardNode->addChild(m_dynamic, 5);

        m_status = CCLabelBMFont::create("Giliran Putih", "bigFont.fnt");
        m_status->setScale(0.5f);
        m_status->setPosition(ccp(win.width / 2.f, win.height - 16.f));
        this->addChild(m_status);

        auto menu = CCMenu::create();
        menu->setPosition(ccp(0.f, 0.f));
        auto backSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        auto backBtn = CCMenuItemSpriteExtra::create(backSpr, this, menu_selector(ChessLayer::onBack));
        backBtn->setPosition(ccp(30.f, win.height - 30.f));
        menu->addChild(backBtn);

        auto resSpr = ButtonSprite::create("Ulang");
        auto resBtn = CCMenuItemSpriteExtra::create(resSpr, this, menu_selector(ChessLayer::onRestart));
        resBtn->setPosition(ccp(win.width - 50.f, win.height - 30.f));
        menu->addChild(resBtn);
        this->addChild(menu);

        this->setTouchEnabled(true);
        this->setKeypadEnabled(true);
        refresh();
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, 0, true);
    }

    void keyBackClicked() override {
        onBack(nullptr);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (m_over) return true;
        auto p = m_boardNode->convertToNodeSpace(touch->getLocation());
        int c = static_cast<int>(std::floor(p.x / m_sq));
        int r = static_cast<int>(std::floor(p.y / m_sq));
        if (ChessBoard::inside(r, c)) onSquare(r, c);
        return true;
    }

    void onSquare(int r, int c) {
        char p = m_board.b[r][c];
        if (m_selR >= 0) {
            for (auto& m : m_moves) {
                if (m.first == r && m.second == c) {
                    m_board.move(m_selR, m_selC, r, c);
                    m_selR = -1;
                    m_selC = -1;
                    m_moves.clear();
                    m_whiteTurn = !m_whiteTurn;
                    checkState();
                    refresh();
                    return;
                }
            }
        }
        if (p != '.' && ChessBoard::sameSide(p, m_whiteTurn)) {
            m_selR = r;
            m_selC = c;
            m_moves = m_board.legal(r, c);
        } else {
            m_selR = -1;
            m_selC = -1;
            m_moves.clear();
        }
        refresh();
    }

    void checkState() {
        bool check = m_board.inCheck(m_whiteTurn);
        bool any = m_board.hasAnyMove(m_whiteTurn);
        std::string side = m_whiteTurn ? "Putih" : "Hitam";
        std::string other = m_whiteTurn ? "Hitam" : "Putih";
        if (!any) {
            m_over = true;
            if (check) m_status->setString(("Skakmat! " + other + " menang").c_str());
            else m_status->setString("Remis (stalemate)");
        } else if (check) {
            m_status->setString(("Giliran " + side + " - SKAK!").c_str());
        } else {
            m_status->setString(("Giliran " + side).c_str());
        }
    }

    void refresh() {
        m_dynamic->removeAllChildren();
        auto draw = CCDrawNode::create();
        m_dynamic->addChild(draw);

        auto center = [this](int r, int c) {
            return ccp((c + 0.5f) * m_sq, (r + 0.5f) * m_sq);
        };

        if (m_selR >= 0) {
            draw->drawDot(center(m_selR, m_selC), m_sq * 0.5f, ccc4f(1.f, 1.f, 0.f, 0.6f));
            for (auto& m : m_moves) {
                if (m_board.b[m.first][m.second] == '.')
                    draw->drawDot(center(m.first, m.second), m_sq * 0.18f, ccc4f(0.f, 0.8f, 0.f, 0.9f));
                else
                    draw->drawDot(center(m.first, m.second), m_sq * 0.5f, ccc4f(1.f, 0.f, 0.f, 0.7f));
            }
        }

        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                char p = m_board.b[r][c];
                if (p == '.') continue;
                bool white = ChessBoard::isWhite(p);
                draw->drawDot(center(r, c), m_sq * 0.4f,
                    white ? ccc4f(0.95f, 0.95f, 0.95f, 1.f) : ccc4f(0.1f, 0.1f, 0.1f, 1.f));
                char s[2] = { static_cast<char>(std::toupper(p)), 0 };
                auto lbl = CCLabelBMFont::create(s, "bigFont.fnt");
                lbl->setScale(0.55f);
                lbl->setColor(white ? ccc3(20, 20, 20) : ccc3(240, 240, 240));
                lbl->setPosition(center(r, c));
                m_dynamic->addChild(lbl, 1);
            }
        }
    }

    void onBack(CCObject*) {
        CCDirector::sharedDirector()->popScene();
    }

    void onRestart(CCObject*) {
        m_board.reset();
        m_whiteTurn = true;
        m_over = false;
        m_selR = -1;
        m_selC = -1;
        m_moves.clear();
        m_status->setString("Giliran Putih");
        refresh();
    }

public:
    static ChessLayer* create() {
        auto ret = new ChessLayer();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    static CCScene* scene() {
        auto s = CCScene::create();
        s->addChild(ChessLayer::create());
        return s;
    }
};

class $modify(ChessMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        auto spr = ButtonSprite::create("Chess");
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ChessMenuLayer::onChess));
        menu->addChild(btn);
        menu->setPosition(ccp(win.width - 55.f, 20.f));
        this->addChild(menu);
        return true;
    }

    void onChess(CCObject*) {
        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, ChessLayer::scene()));
    }
};
