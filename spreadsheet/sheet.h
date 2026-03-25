#pragma once

#include "cell.h"
#include "common.h"
#include <unordered_map>
#include <memory>

struct PositionHasher {
    size_t operator()(const Position& position) const {
        return std::hash<int>{}(position.row) ^ (std::hash<int>{}(position.col) << 1);
    }
};

class Sheet : public SheetInterface {
public:
    ~Sheet() = default;

    void SetCell(Position pos, std::string text) override;
    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;
    void ClearCell(Position pos) override;
    Size GetPrintableSize() const override;
    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    // Внутренний метод для графа зависимостей
    Cell* GetConcreteCell(Position pos);

private:
    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHasher> cells_;
};