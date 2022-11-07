#include "olcPixelGameEngine.h"
#include <cassert>
#include <optional>
#include <ostream>
#include <random>
#include <stack>
#include <string>
#include <type_traits>
#include <vector>

#include <iostream>

static constexpr float UpdateInterval = 0.01; // time between frame updates

std::mt19937 rg_ = std::mt19937{std::random_device{}()};

class Maze : public olc::PixelGameEngine {
  public:
    bool OnUserCreate() override {
        init();
        return true;
    }

    void init() {
        filterColorPct_ = 0;
        maze_.clear();
        path_.clear();
        state_ = State::Building;
        maze_.resize(width_ * height_);
        path_.reserve(width_ * height_);
        buildEnter(0);
    }

    bool OnUserUpdate(float fElapsedTime) override {
        elapsed_ += fElapsedTime;

        if (GetKey(olc::Key::SPACE).bPressed) {
            return false;
        }

        switch (state_) {
            case State::Building: buildMaze(); break;
            case State::Solving: solveMaze(); break;
            case State::Solved: solved(); break;
        }
        draw();
        return true;
    }

  private:
    enum class State { Building, Solving, Solved };
    enum class Flags : int {
        Empty        = 0,
        North        = 1 << 1,
        East         = 1 << 2,
        South        = 1 << 3,
        West         = 1 << 4,
        BuildVisited = 1 << 5, // never unset, has builder been here
        SolveVisited = 1 << 6, // never unset, has solver been here
        SolvePath    = 1 << 7  // cell is on current solution path
    };

    static std::string toString(State s) {
        using enum State;
        switch (s) {
            case Building: return "Building";
            case Solving: return "Solving";
            case Solved: return "Solved";
            default: return "?State?";
        }
    }

    static std::string toString(Flags f) {
        using enum Flags;
        switch (f) {
            case Empty: return "Empty";
            case North: return "North";
            case East: return "East";
            case South: return "South";
            case West: return "West";
            case BuildVisited: return "BuildVisited"; // never unset, has builder seen this cell
            case SolveVisited: return "SolveVisited"; // never unset, has solver seen this cell
            case SolvePath: return "SolvePath"; // set only when this cell is part of current path
            default: return "?Flags?";
        }
    }

    template <typename EnumT>
    friend std::ostream &operator<<(std::ostream &os, EnumT e) requires(std::is_enum_v<EnumT>) {
        return os << toString(e);
    }

    friend int operator+(Flags val) { return static_cast<int>(val); }
    friend Flags operator|=(Flags &lhs, Flags rhs) { return lhs = Flags{+lhs | +rhs}; }
    friend Flags operator&(Flags lhs, Flags rhs) { return Flags{+lhs & +rhs}; }
    friend Flags flip(Flags val) {
        using enum Flags;
        switch (val) {
            case North: return Flags::South;
            case East: return Flags::West;
            case South: return Flags::North;
            case West: return Flags::East;
            default: return Flags::Empty;
        }
    }

    void solved() {
        if (elapsed_ > 0.05f) {
            filterColorPct_ += 20;
            elapsed_ = 0;
        }
        if (filterColorPct_ >= 255) {
            init();
        }
    }

    static bool isFlagSet(Flags flags, Flags flag) { return (flags & flag) != Flags::Empty; }

    void draw() {
        Clear(olc::BLACK);

        int cellWidth  = ScreenWidth() / width_;
        int cellHeight = ScreenHeight() / height_;

        for (int idx = 0; idx < maze_.size(); ++idx) {
            if (visited(idx)) {
                auto color  = inCurrentSolvePath(idx) ? olc::RED : olc::BLUE;
                auto [x, y] = idx2xy(idx);
                FillRect(x * cellWidth, y * cellHeight, cellWidth, cellHeight, color);
                Flags flags = maze_[idx];

                if (not isFlagSet(flags, Flags::North)) {
                    DrawLine(x * cellWidth, y * cellHeight, x * cellWidth + cellWidth,
                             y * cellHeight, olc::WHITE);
                }
                if (not isFlagSet(flags, Flags::South)) {
                    DrawLine(x * cellWidth, y * cellHeight + cellHeight, x * cellWidth + cellWidth,
                             y * cellHeight + cellHeight, olc::WHITE);
                }
                if (not isFlagSet(flags, Flags::East)) {
                    DrawLine(x * cellWidth + cellWidth, y * cellHeight, x * cellWidth + cellWidth,
                             y * cellHeight + cellHeight, olc::WHITE);
                }
                if (not isFlagSet(flags, Flags::West)) {
                    DrawLine(x * cellWidth, y * cellHeight, x * cellWidth,
                             y * cellHeight + cellHeight, olc::WHITE);
                }
            }
        }

        if (filterColorPct_ > 0) {
            auto color = olc::BLACK;
            color.a    = static_cast<std::uint8_t>(filterColorPct_);
            SetPixelMode(olc::Pixel::Mode::ALPHA);
            FillRect(0, 0, ScreenWidth(), ScreenHeight(), color);
            SetPixelMode(olc::Pixel::Mode::NORMAL);
        }
    }

    void solveMaze() {
        do {
            assert(not path_.empty());
            int curIdx = path_.back();
            if (curIdx == height_ * width_ - 1) {
                state_ = State::Solved;
                return;
            }
            int nextIdx = -1;
            for (Flags dir : {Flags::South, Flags::East, Flags::North, Flags::West}) {
                if (isFlagSet(maze_[curIdx], dir)) {
                    auto idx = indexOffset(curIdx, dir);
                    if (idx.has_value() && not solveVisited(*idx)) {
                        nextIdx = *idx;
                        break;
                    }
                }
            }
            if (nextIdx == -1) {
                removeFromPath(curIdx);
            } else {
                solveEnter(nextIdx);
            }
        } while (fastsolve_ && state_ == State::Solving);
    }

    void buildMaze() {
        do {
            int neighbors[4];
            Flags direction[4];
            int neighborCount = 0;
            assert(not path_.empty());
            while (true) {
                int curIdx = path_.back();
                for (Flags dir : {Flags::North, Flags::South, Flags::East, Flags::West}) {
                    if (auto idx = indexOffset(curIdx, dir); idx.has_value() && not visited(*idx)) {
                        direction[neighborCount] = dir;
                        neighbors[neighborCount] = *idx;
                        ++neighborCount;
                    }
                }
                if (neighborCount == 0) {
                    path_.pop_back();
                    if (path_.empty()) {
                        state_ = State::Solving;
                        solveEnter(0);
                        break;
                    }
                } else {
                    std::uniform_int_distribution<int> dist =
                        std::uniform_int_distribution<int>(0, neighborCount - 1);
                    int nextPathIdx = dist(rg_);
                    addEdge(curIdx, direction[nextPathIdx]);
                    buildEnter(neighbors[nextPathIdx]);
                    break;
                }
            }
        } while (fastbuild_ && state_ == State::Building);
    }

    void removeFromPath(int idx) {
        maze_[idx] = Flags{+maze_[idx] & ~(+Flags::SolvePath)};
        path_.pop_back();
    }

    bool visited(int idx) const { return isFlagSet(maze_[idx], Flags::BuildVisited); }
    bool solveVisited(int idx) const { return isFlagSet(maze_[idx], Flags::SolveVisited); }
    bool inCurrentSolvePath(int idx) const { return isFlagSet(maze_[idx], Flags::SolvePath); }

    std::pair<int, int> idx2xy(int idx) const { return {idx % width_, idx / width_}; }

    int toIdx(int x, int y) const { return width_ * y + x; }

    int indexOffsetUnchecked(int idx, Flags direction) const {
        using enum Flags;

        auto [x, y] = idx2xy(idx);
        switch (direction) {
            case North: return toIdx(x, y - 1);
            case East: return toIdx(x + 1, y);
            case South: return toIdx(x, y + 1);
            case West: return toIdx(x - 1, y);
            default: return idx;
        }
    }

    std::optional<int> indexOffset(int idx, Flags direction) const {
        using enum Flags;

        auto [x, y] = idx2xy(idx);
        switch (direction) {
            case North:
                if (y <= 0)
                    return std::nullopt;
                break;
            case East:
                if (x + 1 >= width_)
                    return std::nullopt;
                break;
            case South:
                if (y + 1 >= height_)
                    return std::nullopt;
                break;
            case West:
                if (x <= 0)
                    return std::nullopt;
                break;
        }
        return {indexOffsetUnchecked(idx, direction)};
    }

    void buildEnter(int idx) {
        maze_[idx] |= Flags::BuildVisited;
        path_.push_back(idx);
    }

    void solveEnter(int idx) {
        maze_[idx] |= Flags::SolveVisited;
        maze_[idx] |= Flags::SolvePath;
        path_.push_back(idx);
    }

    void addEdge(int idx, Flags direction) {
        maze_[idx] |= direction;
        maze_[indexOffsetUnchecked(idx, direction)] |= flip(direction);
    }

  private:
    int filterColorPct_ = 0;
    bool fastbuild_         = 1;
    bool fastsolve_         = 0;
    int width_              = 40;
    int height_             = 20;
    float elapsed_          = 0;
    std::vector<Flags> maze_;
    std::vector<int> path_;
    State state_ = State::Building;
};

int main() {
    Maze app;
    bool FULLSCREEN = false;
    int PX_SIZE     = 1;
    if (app.Construct(1920, 1080, PX_SIZE, PX_SIZE, FULLSCREEN)) {
        app.Start();
    }

    return 0;
}
