#include "olcPixelGameEngine.h"
#include <cassert>
#include <optional>
#include <random>
#include <stack>
#include <vector>

#include <iostream>

static constexpr float UpdateInterval = 0.01; // time between frame updates

std::mt19937 rg_ = std::mt19937{std::random_device{}()};

class Maze : public olc::PixelGameEngine {
  public:
    bool OnUserCreate() override {
        maze_.resize(width_ * height_);
        buildEnter(0);
        return true;
    }

    bool OnUserUpdate(float fElapsedTime) override {

        // exit?
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

    void solved() {}

    bool isFlagSet(Flags flags, Flags flag) const { return (flags & flag) != Flags::Empty; }

    void draw() {
        Clear(olc::BLACK);

        int cellWidth  = ScreenWidth() / width_;
        int cellHeight = ScreenHeight() / height_;

        for (int idx = 0; idx < maze_.size(); ++idx) {
            if (visited(idx)) {
                auto color  = solveVisited(idx) ? olc::RED : olc::BLUE;
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
    }

    void solveMaze() {
        assert(not path_.empty());
        int curIdx = path_.top();
        if (curIdx == height_ * width_ - 1) {
            state_ = State::Solved;
            return;
        }
        int nextIdx = -1;
        for (Flags dir : {Flags::North, Flags::East, Flags::South, Flags::West}) {
            if (not isFlagSet(maze_[curIdx], dir)) {
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
    }

    void buildMaze() {
        int neighbors[4];
        Flags direction[4];
        int neighborCount = 0;
        assert(not path_.empty());
        while (true) {
            int curIdx = path_.top();
            for (Flags dir : {Flags::North, Flags::South, Flags::East, Flags::West}) {
                if (auto idx = indexOffset(curIdx, dir); idx.has_value() && not visited(*idx)) {
                    direction[neighborCount] = dir;
                    neighbors[neighborCount] = *idx;
                    ++neighborCount;
                }
            }
            if (neighborCount == 0) {
                path_.pop();
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
    }

    void removeFromPath(int idx) {
        maze_[idx] = Flags{+maze_[idx] & ~(+Flags::SolvePath)};
        path.pop();
    }

    bool visited(int idx) const {
        assert(idx >= 0);
        assert(idx < maze_.size());
        return (maze_[idx] & Flags::BuildVisited) != Flags::Empty;
    }

    bool solveVisited(int idx) const {
        assert(idx >= 0);
        assert(idx < maze_.size());
        return (maze_[idx] & Flags::SolveVisited) != Flags::Empty;
    }

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
        path_.push(idx);
    }

    void solveEnter(int idx) {
        maze_[idx] |= Flags::SolveVisited;
        maze_[idx] |= Flags::SolvePath;
        path_.push(idx);
    }

    void addEdge(int idx, Flags direction) {
        maze_[idx] |= direction;
        maze_[indexOffsetUnchecked(idx, direction)] |= flip(direction);
    }

  private:
    int width_  = 40;
    int height_ = 20;
    std::vector<Flags> maze_;
    std::stack<int> path_;
    State state_ = State::Building;
};

int main() {
    Maze app;
    bool FULLSCREEN = 1;
    int PX_SIZE     = 1;
    if (app.Construct(1920, 1080, PX_SIZE, PX_SIZE, FULLSCREEN)) {
        app.Start();
    }

    return 0;
}
