#pragma once

#include "common.h"
#include "formula.h"
#include <optional>
#include <unordered_set>

class Sheet;

class Cell : public CellInterface {
public:
    Cell(Sheet& sheet, Position pos);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    bool IsReferenced() const;
    void InvalidateCache();

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    std::unique_ptr<Impl> impl_;
    Sheet& sheet_;
    Position pos_;
    mutable std::optional<Value> cache_;

    std::unordered_set<Cell*> referenced_cells_;
    std::unordered_set<Cell*> dependent_cells_;

    void UpdateDependencies(const std::vector<Position>& new_refs);
    void ClearDependencies();
    void CheckCircularDependencies(const std::vector<Position>& new_refs) const;
};